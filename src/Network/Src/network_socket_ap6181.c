#include "Network/Inc/network_socket.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Network/Ap6181/Inc/bsp_ap6181.h"

static bool network_socket_ap6181_is_ready(void);
static bool network_socket_ap6181_open(const network_socket_config_t *config);
static void network_socket_ap6181_close(uint8_t socket_index);
static bool network_socket_ap6181_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
static bool network_socket_ap6181_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                          uint16_t length);
static uint16_t network_socket_ap6181_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
static bool network_socket_ap6181_is_tcp_connected(uint8_t socket_index);

const network_socket_ops_t network_socket_ap6181_ops = {
  .link = NETWORK_LINK_AP6181,
  .name = "AP6181 WiFi/SDMMC1",
  .is_ready = network_socket_ap6181_is_ready,
  .open = network_socket_ap6181_open,
  .close = network_socket_ap6181_close,
  .send = network_socket_ap6181_send,
  .send_to = network_socket_ap6181_send_to,
  .recv = network_socket_ap6181_recv,
  .is_tcp_connected = network_socket_ap6181_is_tcp_connected,
};

static bool network_socket_ap6181_is_ready(void) {
#if BSP_HAS_AP6181
  return false;
#else
  return false;
#endif
}

static bool network_socket_ap6181_open(const network_socket_config_t *config) {
  (void)config;
#if BSP_HAS_AP6181
  LOG_WARNING("AP6181 WiFi socket driver is not bound yet; MQTT will not use wired or 4G fallback on this board");
#endif
  return false;
}

static void network_socket_ap6181_close(uint8_t socket_index) {
  (void)socket_index;
}

static bool network_socket_ap6181_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  (void)socket_index;
  (void)data;
  (void)length;
  return false;
}

static bool network_socket_ap6181_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                          uint16_t length) {
  (void)socket_index;
  (void)host;
  (void)port;
  (void)data;
  (void)length;
  return false;
}

static uint16_t network_socket_ap6181_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  (void)socket_index;
  (void)data;
  (void)max_length;
  return 0U;
}

static bool network_socket_ap6181_is_tcp_connected(uint8_t socket_index) {
  (void)socket_index;
  return false;
}
