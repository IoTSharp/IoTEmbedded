#include "Network/Ap6181/Inc/ap6181_socket.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Network/Ap6181/Inc/bsp_ap6181.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define AP6181_LEGACY_STA_NAME "wlan0"
#define AP6181_LEGACY_STATION_MODE 1
#define AP6181_INIT_WAIT_MS 500U
#define AP6181_IP_READY_WAIT_MS 15000U
#define AP6181_RETRY_INTERVAL_MS 30000U
#define AP6181_SOCKET_RECV_TIMEOUT_MS 1U
#define AP6181_AF_INET 2
#define AP6181_SOCK_STREAM 1
#define AP6181_SOCK_DGRAM 2
#define AP6181_IPPROTO_TCP 6
#define AP6181_IPPROTO_UDP 17
#define AP6181_SOL_SOCKET 0x0fff
#define AP6181_SO_RCVTIMEO 0x1006
#define AP6181_SO_SNDTIMEO 0x1005
#define AP6181_MSG_DONTWAIT 0x08
#define AP6181_INVALID_FD (-1)

#ifndef AP6181_SOCKET_SOCKADDR_HAS_LEN
#define AP6181_SOCKET_SOCKADDR_HAS_LEN 1
#endif

typedef uint32_t ap6181_socklen_t;

typedef struct {
  uint32_t s_addr;
} ap6181_in_addr_t;

typedef struct {
#if AP6181_SOCKET_SOCKADDR_HAS_LEN
  uint8_t sin_len;
  uint8_t sin_family;
#else
  uint16_t sin_family;
#endif
  uint16_t sin_port;
  ap6181_in_addr_t sin_addr;
  char sin_zero[8];
} ap6181_sockaddr_in_t;

typedef struct {
  long tv_sec;
  long tv_usec;
} ap6181_timeval_t;

typedef struct {
  bool opened;
  network_socket_proto_t proto;
  int fd;
} ap6181_legacy_socket_t;

extern int rt_hw_wlan_init(void) __attribute__((weak));
extern int rt_hw_wlan_wait_init_done(uint32_t time_ms) __attribute__((weak));
extern int rt_wlan_set_mode(const char *dev_name, int mode) __attribute__((weak));
extern int rt_wlan_connect(const char *ssid, const char *password) __attribute__((weak));
extern int rt_wlan_disconnect(void) __attribute__((weak));
extern int rt_wlan_is_connected(void) __attribute__((weak));
extern int rt_wlan_is_ready(void) __attribute__((weak));
extern void wwd_thread_notify_irq(void) __attribute__((weak));

extern int socket(int domain, int type, int protocol) __attribute__((weak));
extern int bind(int socket_fd, const void *name, ap6181_socklen_t namelen) __attribute__((weak));
extern int connect(int socket_fd, const void *name, ap6181_socklen_t namelen) __attribute__((weak));
extern int send(int socket_fd, const void *dataptr, size_t size, int flags) __attribute__((weak));
extern int sendto(int socket_fd, const void *dataptr, size_t size, int flags, const void *to,
                  ap6181_socklen_t tolen) __attribute__((weak));
extern int recv(int socket_fd, void *mem, size_t len, int flags) __attribute__((weak));
extern int setsockopt(int socket_fd, int level, int optname, const void *optval,
                      ap6181_socklen_t optlen) __attribute__((weak));
extern int closesocket(int socket_fd) __attribute__((weak));
extern int close(int socket_fd) __attribute__((weak));

static const ap6181_socket_backend_ops_t *registered_backend;
static network_wifi_config_t wifi_config;
static bool wifi_configured;
static ap6181_socket_status_t current_status = AP6181_SOCKET_STATUS_DISABLED;
static char current_detail[96] = "not initialized";
static uint32_t last_ready_attempt_ms;
static bool backend_initialized;
static bool legacy_initialized;
static ap6181_legacy_socket_t legacy_sockets[NETWORK_SOCKET_MAX_COUNT];

static const ap6181_socket_backend_ops_t *ap6181_socket_select_backend(void);
static void ap6181_socket_set_status(ap6181_socket_status_t status, const char *detail);
static bool ap6181_socket_can_retry(void);
static bool ap6181_socket_config_valid(void);
static void ap6181_socket_close_all(void);

static bool legacy_backend_available(void);
static bool legacy_backend_init(const network_wifi_config_t *config);
static bool legacy_backend_is_ready(void);
static bool legacy_backend_open(const network_socket_config_t *config);
static void legacy_backend_close(uint8_t socket_index);
static bool legacy_backend_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
static bool legacy_backend_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                   uint16_t length);
static uint16_t legacy_backend_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
static bool legacy_backend_is_tcp_connected(uint8_t socket_index);
static bool legacy_socket_api_available(void);
static void legacy_disconnect_if_connected(void);
static bool legacy_wait_ip_ready(uint32_t timeout_ms);
static bool legacy_fill_remote_addr(const char *host, uint16_t port, ap6181_sockaddr_in_t *addr);
static bool legacy_bind_local_port(int fd, uint16_t local_port);
static bool legacy_socket_set_timeouts(int fd);
static void legacy_socket_close_fd(int fd);
static bool ap6181_parse_ipv4(const char *text, uint32_t *network_order_addr);
static uint16_t ap6181_htons(uint16_t value);
static uint32_t ap6181_htonl(uint32_t value);

static const ap6181_socket_backend_ops_t legacy_backend = {
  .name = "Pandora old RT-WLAN/SAL",
  .init = legacy_backend_init,
  .is_ready = legacy_backend_is_ready,
  .open = legacy_backend_open,
  .close = legacy_backend_close,
  .send = legacy_backend_send,
  .send_to = legacy_backend_send_to,
  .recv = legacy_backend_recv,
  .is_tcp_connected = legacy_backend_is_tcp_connected,
};

void ap6181_socket_configure(const network_wifi_config_t *config) {
  ap6181_socket_close_all();
  memset(&wifi_config, 0, sizeof(wifi_config));
  backend_initialized = false;
  legacy_initialized = false;

  if (config != NULL) {
    wifi_config = *config;
    wifi_config.ssid[sizeof(wifi_config.ssid) - 1U] = '\0';
    wifi_config.password[sizeof(wifi_config.password) - 1U] = '\0';
    wifi_config.local_ip[sizeof(wifi_config.local_ip) - 1U] = '\0';
    wifi_config.gateway_ip[sizeof(wifi_config.gateway_ip) - 1U] = '\0';
    wifi_config.mask_ip[sizeof(wifi_config.mask_ip) - 1U] = '\0';
    wifi_config.dns_ip[sizeof(wifi_config.dns_ip) - 1U] = '\0';
    wifi_configured = true;
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_POWER_OFF, "configured");
  } else {
    wifi_configured = false;
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_CONFIG_MISSING, "WiFi config missing");
  }
}

bool ap6181_socket_register_backend(const ap6181_socket_backend_ops_t *ops) {
  if (ops == NULL || ops->init == NULL || ops->is_ready == NULL || ops->open == NULL || ops->close == NULL ||
      ops->send == NULL || ops->recv == NULL || ops->is_tcp_connected == NULL) {
    return false;
  }

  registered_backend = ops;
  backend_initialized = false;
  legacy_initialized = false;
  ap6181_socket_close_all();
  ap6181_socket_set_status(AP6181_SOCKET_STATUS_POWER_OFF, "external backend registered");
  return true;
}

bool ap6181_socket_is_ready(void) {
#if !BSP_HAS_AP6181
  ap6181_socket_set_status(AP6181_SOCKET_STATUS_DISABLED, "board profile has no AP6181");
  return false;
#else
  if (!wifi_configured || !ap6181_socket_config_valid()) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_CONFIG_MISSING, "set wifi_ssid");
    return false;
  }

  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend == NULL) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_BACKEND_MISSING, "old AP6181 WLAN/SAL backend not linked");
    return false;
  }

  if (backend_initialized && backend->is_ready()) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_READY, "WiFi joined and IP ready");
    return true;
  }

  if (!ap6181_socket_can_retry()) {
    return false;
  }
  last_ready_attempt_ms = bsp_get_tick_ms();

  if (!bsp_ap6181_is_enabled()) {
    bsp_ap6181_prepare_pins();
    bsp_ap6181_enable();
  }

  if (!backend->init(&wifi_config)) {
    backend_initialized = false;
    return false;
  }
  backend_initialized = true;

  if (backend->is_ready()) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_READY, "WiFi joined and IP ready");
    return true;
  }

  ap6181_socket_set_status(AP6181_SOCKET_STATUS_IP_DOWN, "WiFi backend initialized but IP is not ready");
  return false;
#endif
}

bool ap6181_socket_open(const network_socket_config_t *config) {
  if (config == NULL || config->socket_index >= NETWORK_SOCKET_MAX_COUNT) {
    return false;
  }
  if (!ap6181_socket_is_ready()) {
    return false;
  }

  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend == NULL || !backend->open(config)) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_SOCKET_ERROR, "socket open failed");
    return false;
  }
  return true;
}

void ap6181_socket_close(uint8_t socket_index) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend != NULL && backend->close != NULL) {
    backend->close(socket_index);
  }
}

bool ap6181_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend == NULL || backend->send == NULL || data == NULL || length == 0U) {
    return false;
  }
  return backend->send(socket_index, data, length);
}

bool ap6181_socket_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                           uint16_t length) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend == NULL || backend->send_to == NULL || host == NULL || port == 0U || data == NULL || length == 0U) {
    return false;
  }
  return backend->send_to(socket_index, host, port, data, length);
}

uint16_t ap6181_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  if (backend == NULL || backend->recv == NULL || data == NULL || max_length == 0U) {
    return 0U;
  }
  return backend->recv(socket_index, data, max_length);
}

bool ap6181_socket_is_tcp_connected(uint8_t socket_index) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  return backend != NULL && backend->is_tcp_connected != NULL && backend->is_tcp_connected(socket_index);
}

void ap6181_socket_irq_notify(void) {
  if (wwd_thread_notify_irq != NULL) {
    wwd_thread_notify_irq();
  }
}

#if BSP_HAS_AP6181 && defined(EXTI9_5_IRQn)
__attribute__((weak)) void EXTI9_5_IRQHandler(void) {
  if (__HAL_GPIO_EXTI_GET_IT(BSP_AP6181_IRQ_Pin) != 0x00U) {
    __HAL_GPIO_EXTI_CLEAR_IT(BSP_AP6181_IRQ_Pin);
    ap6181_socket_irq_notify();
  }
}
#endif

ap6181_socket_status_t ap6181_socket_get_status(void) {
  return current_status;
}

const char *ap6181_socket_status_name(ap6181_socket_status_t status) {
  switch (status) {
  case AP6181_SOCKET_STATUS_DISABLED:
    return "disabled";
  case AP6181_SOCKET_STATUS_POWER_OFF:
    return "power_off";
  case AP6181_SOCKET_STATUS_CONFIG_MISSING:
    return "config_missing";
  case AP6181_SOCKET_STATUS_BACKEND_MISSING:
    return "backend_missing";
  case AP6181_SOCKET_STATUS_HARDWARE_INIT_FAILED:
    return "hardware_init_failed";
  case AP6181_SOCKET_STATUS_WIFI_JOIN_FAILED:
    return "wifi_join_failed";
  case AP6181_SOCKET_STATUS_IP_DOWN:
    return "ip_down";
  case AP6181_SOCKET_STATUS_SOCKET_ERROR:
    return "socket_error";
  case AP6181_SOCKET_STATUS_READY:
    return "ready";
  default:
    return "unknown";
  }
}

const char *ap6181_socket_status_detail(void) {
  return current_detail;
}

const char *ap6181_socket_backend_name(void) {
  const ap6181_socket_backend_ops_t *backend = ap6181_socket_select_backend();
  return backend == NULL || backend->name == NULL ? "none" : backend->name;
}

const char *ap6181_socket_configured_ssid(void) {
  return wifi_config.ssid;
}

static const ap6181_socket_backend_ops_t *ap6181_socket_select_backend(void) {
  if (registered_backend != NULL) {
    return registered_backend;
  }
  return legacy_backend_available() ? &legacy_backend : NULL;
}

static void ap6181_socket_set_status(ap6181_socket_status_t status, const char *detail) {
  current_status = status;
  if (detail == NULL) {
    current_detail[0] = '\0';
    return;
  }
  (void)snprintf(current_detail, sizeof(current_detail), "%s", detail);
}

static bool ap6181_socket_can_retry(void) {
  uint32_t now = bsp_get_tick_ms();
  if (last_ready_attempt_ms == 0U) {
    return true;
  }
  return (now - last_ready_attempt_ms) >= AP6181_RETRY_INTERVAL_MS;
}

static bool ap6181_socket_config_valid(void) {
  return wifi_config.ssid[0] != '\0';
}

static void ap6181_socket_close_all(void) {
  for (uint8_t index = 0U; index < NETWORK_SOCKET_MAX_COUNT; index++) {
    ap6181_socket_close(index);
  }
  memset(legacy_sockets, 0, sizeof(legacy_sockets));
  for (uint8_t index = 0U; index < NETWORK_SOCKET_MAX_COUNT; index++) {
    legacy_sockets[index].fd = AP6181_INVALID_FD;
  }
}

static bool legacy_backend_available(void) {
  return rt_hw_wlan_init != NULL && rt_wlan_connect != NULL && rt_wlan_is_ready != NULL && legacy_socket_api_available();
}

static bool legacy_backend_init(const network_wifi_config_t *config) {
  if (config == NULL || config->ssid[0] == '\0') {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_CONFIG_MISSING, "set wifi_ssid before WiFi join");
    return false;
  }
  if (!legacy_backend_available()) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_BACKEND_MISSING, "old RT-WLAN/SAL symbols missing");
    return false;
  }

  bsp_ap6181_prepare_pins();
  bsp_ap6181_prepare_irq_interrupt();
  bsp_ap6181_enable();

  if (!legacy_initialized) {
    if (rt_hw_wlan_init() != 0) {
      ap6181_socket_set_status(AP6181_SOCKET_STATUS_HARDWARE_INIT_FAILED, "rt_hw_wlan_init failed");
      return false;
    }
    if (rt_hw_wlan_wait_init_done != NULL && rt_hw_wlan_wait_init_done(AP6181_INIT_WAIT_MS) != 0) {
      ap6181_socket_set_status(AP6181_SOCKET_STATUS_HARDWARE_INIT_FAILED, "wifi_hw_init timeout");
      return false;
    }
    if (rt_wlan_set_mode != NULL && rt_wlan_set_mode(AP6181_LEGACY_STA_NAME, AP6181_LEGACY_STATION_MODE) != 0) {
      ap6181_socket_set_status(AP6181_SOCKET_STATUS_HARDWARE_INIT_FAILED, "rt_wlan_set_mode(STA) failed");
      return false;
    }
    legacy_initialized = true;
  }

  if (!backend_initialized) {
    legacy_disconnect_if_connected();
  }

  if (rt_wlan_is_connected == NULL || !rt_wlan_is_connected()) {
    if (rt_wlan_connect(config->ssid, config->password) != 0) {
      ap6181_socket_set_status(AP6181_SOCKET_STATUS_WIFI_JOIN_FAILED, "rt_wlan_connect failed");
      return false;
    }
  }

  if (!legacy_wait_ip_ready(AP6181_IP_READY_WAIT_MS)) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_IP_DOWN, "WiFi joined but IP ready event timed out");
    return false;
  }

  ap6181_socket_set_status(AP6181_SOCKET_STATUS_READY, "WiFi joined and IP ready");
  return true;
}

static bool legacy_backend_is_ready(void) {
  return rt_wlan_is_ready != NULL && rt_wlan_is_ready() != 0;
}

static bool legacy_backend_open(const network_socket_config_t *config) {
  if (config == NULL || config->socket_index >= NETWORK_SOCKET_MAX_COUNT || config->remote_host == NULL ||
      config->remote_port == 0U) {
    return false;
  }

  ap6181_sockaddr_in_t remote_addr;
  if (!legacy_fill_remote_addr(config->remote_host, config->remote_port, &remote_addr)) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_SOCKET_ERROR, "remote host must be an IPv4 address");
    return false;
  }

  legacy_backend_close(config->socket_index);

  int type = config->proto == NETWORK_SOCKET_PROTO_TCP ? AP6181_SOCK_STREAM : AP6181_SOCK_DGRAM;
  int proto = config->proto == NETWORK_SOCKET_PROTO_TCP ? AP6181_IPPROTO_TCP : AP6181_IPPROTO_UDP;
  int fd = socket(AP6181_AF_INET, type, proto);
  if (fd < 0) {
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_SOCKET_ERROR, "socket() failed");
    return false;
  }

  (void)legacy_socket_set_timeouts(fd);
  if (!legacy_bind_local_port(fd, config->local_port)) {
    legacy_socket_close_fd(fd);
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_SOCKET_ERROR, "bind(local_port) failed");
    return false;
  }

  if (connect(fd, &remote_addr, (ap6181_socklen_t)sizeof(remote_addr)) != 0) {
    legacy_socket_close_fd(fd);
    ap6181_socket_set_status(AP6181_SOCKET_STATUS_SOCKET_ERROR, "connect() failed");
    return false;
  }

  legacy_sockets[config->socket_index].opened = true;
  legacy_sockets[config->socket_index].proto = config->proto;
  legacy_sockets[config->socket_index].fd = fd;
  return true;
}

static void legacy_backend_close(uint8_t socket_index) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !legacy_sockets[socket_index].opened) {
    return;
  }
  legacy_socket_close_fd(legacy_sockets[socket_index].fd);
  memset(&legacy_sockets[socket_index], 0, sizeof(legacy_sockets[socket_index]));
  legacy_sockets[socket_index].fd = AP6181_INVALID_FD;
}

static bool legacy_backend_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !legacy_sockets[socket_index].opened || data == NULL || length == 0U) {
    return false;
  }

  uint16_t sent_total = 0U;
  while (sent_total < length) {
    int sent = send(legacy_sockets[socket_index].fd, &data[sent_total], (size_t)(length - sent_total), 0);
    if (sent <= 0) {
      legacy_backend_close(socket_index);
      return false;
    }
    sent_total = (uint16_t)(sent_total + (uint16_t)sent);
  }
  return true;
}

static bool legacy_backend_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                   uint16_t length) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !legacy_sockets[socket_index].opened || host == NULL || port == 0U ||
      data == NULL || length == 0U) {
    return false;
  }

  ap6181_sockaddr_in_t remote_addr;
  if (!legacy_fill_remote_addr(host, port, &remote_addr)) {
    return false;
  }

  int sent = sendto(legacy_sockets[socket_index].fd, data, length, 0, &remote_addr,
                    (ap6181_socklen_t)sizeof(remote_addr));
  if (sent != (int)length) {
    legacy_backend_close(socket_index);
    return false;
  }
  return true;
}

static uint16_t legacy_backend_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !legacy_sockets[socket_index].opened || data == NULL || max_length == 0U) {
    return 0U;
  }

  int received = recv(legacy_sockets[socket_index].fd, data, max_length, AP6181_MSG_DONTWAIT);
  if (received > 0) {
    return (uint16_t)received;
  }
  if (received == 0 && legacy_sockets[socket_index].proto == NETWORK_SOCKET_PROTO_TCP) {
    legacy_backend_close(socket_index);
  }
  return 0U;
}

static bool legacy_backend_is_tcp_connected(uint8_t socket_index) {
  return socket_index < NETWORK_SOCKET_MAX_COUNT && legacy_sockets[socket_index].opened &&
         legacy_sockets[socket_index].proto == NETWORK_SOCKET_PROTO_TCP && legacy_backend_is_ready();
}

static bool legacy_socket_api_available(void) {
  return socket != NULL && bind != NULL && connect != NULL && send != NULL && sendto != NULL && recv != NULL &&
         (closesocket != NULL || close != NULL);
}

static void legacy_disconnect_if_connected(void) {
  if (rt_wlan_disconnect != NULL && rt_wlan_is_connected != NULL && rt_wlan_is_connected()) {
    (void)rt_wlan_disconnect();
    bsp_delay_ms(200U);
  }
}

static bool legacy_wait_ip_ready(uint32_t timeout_ms) {
  uint32_t start = bsp_get_tick_ms();
  do {
    if (legacy_backend_is_ready()) {
      return true;
    }
    bsp_delay_ms(100U);
  } while ((bsp_get_tick_ms() - start) < timeout_ms);
  return false;
}

static bool legacy_fill_remote_addr(const char *host, uint16_t port, ap6181_sockaddr_in_t *addr) {
  uint32_t ip = 0U;
  if (addr == NULL || !ap6181_parse_ipv4(host, &ip)) {
    return false;
  }

  memset(addr, 0, sizeof(*addr));
#if AP6181_SOCKET_SOCKADDR_HAS_LEN
  addr->sin_len = (uint8_t)sizeof(*addr);
#endif
  addr->sin_family = AP6181_AF_INET;
  addr->sin_port = ap6181_htons(port);
  addr->sin_addr.s_addr = ip;
  return true;
}

static bool legacy_bind_local_port(int fd, uint16_t local_port) {
  if (local_port == 0U) {
    return true;
  }

  ap6181_sockaddr_in_t local_addr;
  memset(&local_addr, 0, sizeof(local_addr));
#if AP6181_SOCKET_SOCKADDR_HAS_LEN
  local_addr.sin_len = (uint8_t)sizeof(local_addr);
#endif
  local_addr.sin_family = AP6181_AF_INET;
  local_addr.sin_port = ap6181_htons(local_port);
  local_addr.sin_addr.s_addr = 0U;
  return bind(fd, &local_addr, (ap6181_socklen_t)sizeof(local_addr)) == 0;
}

static bool legacy_socket_set_timeouts(int fd) {
  if (setsockopt == NULL) {
    return false;
  }

  ap6181_timeval_t timeout = {
    .tv_sec = 0,
    .tv_usec = (long)AP6181_SOCKET_RECV_TIMEOUT_MS * 1000L,
  };
  bool recv_ok = setsockopt(fd, AP6181_SOL_SOCKET, AP6181_SO_RCVTIMEO, &timeout,
                            (ap6181_socklen_t)sizeof(timeout)) == 0;
  bool send_ok = setsockopt(fd, AP6181_SOL_SOCKET, AP6181_SO_SNDTIMEO, &timeout,
                            (ap6181_socklen_t)sizeof(timeout)) == 0;
  return recv_ok && send_ok;
}

static void legacy_socket_close_fd(int fd) {
  if (fd < 0) {
    return;
  }
  if (closesocket != NULL) {
    (void)closesocket(fd);
  } else if (close != NULL) {
    (void)close(fd);
  }
}

static bool ap6181_parse_ipv4(const char *text, uint32_t *network_order_addr) {
  if (text == NULL || network_order_addr == NULL) {
    return false;
  }

  uint32_t parts[4] = {0};
  const char *cursor = text;
  for (uint8_t index = 0U; index < 4U; index++) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }

    uint32_t value = 0U;
    while (*cursor >= '0' && *cursor <= '9') {
      value = value * 10U + (uint32_t)(*cursor - '0');
      if (value > 255U) {
        return false;
      }
      cursor++;
    }
    parts[index] = value;

    if (index < 3U) {
      if (*cursor != '.') {
        return false;
      }
      cursor++;
    } else if (*cursor != '\0') {
      return false;
    }
  }

  uint32_t host_order = (parts[0] << 24U) | (parts[1] << 16U) | (parts[2] << 8U) | parts[3];
  *network_order_addr = ap6181_htonl(host_order);
  return true;
}

static uint16_t ap6181_htons(uint16_t value) {
  return (uint16_t)((value << 8U) | (value >> 8U));
}

static uint32_t ap6181_htonl(uint32_t value) {
  return ((value & 0x000000ffUL) << 24U) | ((value & 0x0000ff00UL) << 8U) |
         ((value & 0x00ff0000UL) >> 8U) | ((value & 0xff000000UL) >> 24U);
}
