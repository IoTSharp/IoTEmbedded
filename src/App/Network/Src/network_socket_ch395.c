#include "network_socket.h"

#include "bsp_ch395.h"
#include "ch395_board.h"
#include "ch395_probe.h"
#include "ch395_socket.h"

static bool network_socket_ch395_is_ready(void);
static bool network_socket_ch395_open(const network_socket_config_t *config);
static void network_socket_ch395_close(uint8_t socket_index);
static bool network_socket_ch395_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
static bool network_socket_ch395_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                         uint16_t length);
static uint16_t network_socket_ch395_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
static bool network_socket_ch395_is_tcp_connected(uint8_t socket_index);

const network_socket_ops_t network_socket_ch395_ops = {
  .link = NETWORK_LINK_CH395Q,
  .name = "CH395Q/UART4/CN2",
  .is_ready = network_socket_ch395_is_ready,
  .open = network_socket_ch395_open,
  .close = network_socket_ch395_close,
  .send = network_socket_ch395_send,
  .send_to = network_socket_ch395_send_to,
  .recv = network_socket_ch395_recv,
  .is_tcp_connected = network_socket_ch395_is_tcp_connected,
};

static bool network_socket_ch395_is_ready(void) {
  ch395_board_status_t status = ch395_board_get_status();
  return status.present && !bsp_ch395_is_reset_asserted();
}

static bool network_socket_ch395_open(const network_socket_config_t *config) {
  uint8_t ip[4] = {0};
  if (config == NULL || !ch395_parse_ipv4(config->remote_host, ip)) {
    return false;
  }

  ch395_socket_config_t ch395_config = {
    .socket_index = config->socket_index,
    .proto = config->proto == NETWORK_SOCKET_PROTO_TCP ? CH395_SOCKET_PROTO_TCP : CH395_SOCKET_PROTO_UDP,
    .remote_ip = {ip[0], ip[1], ip[2], ip[3]},
    .remote_port = config->remote_port,
    .local_port = config->local_port,
  };
  return ch395_socket_open(&ch395_config);
}

static void network_socket_ch395_close(uint8_t socket_index) {
  ch395_socket_close(socket_index);
}

static bool network_socket_ch395_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  return ch395_socket_send(socket_index, data, length);
}

static bool network_socket_ch395_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                         uint16_t length) {
  uint8_t ip[4] = {0};
  if (!ch395_parse_ipv4(host, ip)) {
    return false;
  }
  return ch395_socket_send_to(socket_index, ip, port, data, length);
}

static uint16_t network_socket_ch395_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  return ch395_socket_recv(socket_index, data, max_length);
}

static bool network_socket_ch395_is_tcp_connected(uint8_t socket_index) {
  return ch395_socket_is_tcp_connected(socket_index);
}
