#include "Network/Ch395/Inc/ch395_probe.h"

#include "Network/Ch395/Inc/ch395_board.h"
#include "Network/Ch395/Inc/ch395_driver.h"
#include "Common/Inc/log.h"
#include <stdlib.h>

#define CH395_PROBE_SOURCE_PORT 49000U

static bool ch395_probe_socket_connect(const uint8_t ip[4], uint16_t port);

bool ch395_probe_tcp_port(const char *host, uint16_t port) {
  uint8_t ip[4] = {0};

  if (!ch395_parse_ipv4(host, ip) || port == 0U) {
    LOG_WARNING("CH395Q probe config invalid: %s:%u", host == NULL ? "" : host, port);
    return false;
  }

  ch395_board_status_t status = ch395_board_get_status();
  if (!status.present && !ch395_check_exist()) {
    LOG_WARNING("CH395Q check failed");
    return false;
  }

  return ch395_probe_socket_connect(ip, port);
}

bool ch395_parse_ipv4(const char *host, uint8_t ip[4]) {
  if (host == NULL || ip == NULL) {
    return false;
  }

  const char *cursor = host;
  for (uint8_t i = 0; i < 4U; i++) {
    char *end = NULL;
    unsigned long part = strtoul(cursor, &end, 10);
    if (end == cursor || part > 255UL) {
      return false;
    }
    ip[i] = (uint8_t)part;
    if (i < 3U) {
      if (*end != '.') {
        return false;
      }
      cursor = end + 1;
    } else if (*end != '\0') {
      return false;
    }
  }
  return true;
}

bool network_probe_ch395q_port(const char *host, uint16_t port) {
  return ch395_probe_tcp_port(host, port);
}

static bool ch395_probe_socket_connect(const uint8_t ip[4], uint16_t port) {
  uint8_t tcp_state = 0U;
  uint8_t socket_index = CH395_PROBE_SOCKET_INDEX;

  (void)ch395_tcp_disconnect(socket_index);
  (void)ch395_close_socket(socket_index);
  (void)ch395_set_socket_proto_type(socket_index, CH395_PROTO_TYPE_TCP);
  (void)ch395_set_socket_desip(socket_index, ip);
  (void)ch395_set_socket_desport(socket_index, port);
  (void)ch395_set_socket_sourport(socket_index, (uint16_t)(CH395_PROBE_SOURCE_PORT + socket_index));

  if (ch395_open_socket(socket_index) != CH395_CMD_ERR_SUCCESS) {
    return false;
  }

  if (ch395_tcp_connect(socket_index) != CH395_CMD_ERR_SUCCESS) {
    (void)ch395_close_socket(socket_index);
    return false;
  }

  // 直连探测不能只看 TCP_CONNECT 返回值，要等 socket 真正进入 ESTABLISHED。
  bool ok = ch395_wait_socket_tcp_connected(socket_index, CH395_TCP_CONNECT_TIMEOUT_MS) &&
            ch395_get_socket_tcp_state(socket_index, &tcp_state) && tcp_state == CH395_TCP_ESTABLISHED;
  (void)ch395_tcp_disconnect(socket_index);
  (void)ch395_close_socket(socket_index);
  return ok;
}
