#include "Network/Inc/network_socket.h"

#include "Board/Inc/bsp_board.h"
#include "Network/Ap6181/Inc/ap6181_socket.h"

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
  return ap6181_socket_is_ready();
#else
  return false;
#endif
}

static bool network_socket_ap6181_open(const network_socket_config_t *config) {
#if BSP_HAS_AP6181
  return ap6181_socket_open(config);
#else
  (void)config;
  return false;
#endif
}

static void network_socket_ap6181_close(uint8_t socket_index) {
#if BSP_HAS_AP6181
  ap6181_socket_close(socket_index);
#else
  (void)socket_index;
#endif
}

static bool network_socket_ap6181_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
#if BSP_HAS_AP6181
  return ap6181_socket_send(socket_index, data, length);
#else
  (void)socket_index;
  (void)data;
  (void)length;
  return false;
#endif
}

static bool network_socket_ap6181_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                          uint16_t length) {
#if BSP_HAS_AP6181
  return ap6181_socket_send_to(socket_index, host, port, data, length);
#else
  (void)socket_index;
  (void)host;
  (void)port;
  (void)data;
  (void)length;
  return false;
#endif
}

static uint16_t network_socket_ap6181_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
#if BSP_HAS_AP6181
  return ap6181_socket_recv(socket_index, data, max_length);
#else
  (void)socket_index;
  (void)data;
  (void)max_length;
  return 0U;
#endif
}

static bool network_socket_ap6181_is_tcp_connected(uint8_t socket_index) {
#if BSP_HAS_AP6181
  return ap6181_socket_is_tcp_connected(socket_index);
#else
  (void)socket_index;
  return false;
#endif
}
