#include "Network/Ch395/Inc/ch395_socket.h"

#include "Board/Inc/bsp_board.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Network/Ch395/Inc/ch395_defs.h"
#include "Network/Ch395/Inc/ch395_driver.h"

#include <stddef.h>

#define CH395_SOCKET_SEND_TIMEOUT_MS 1000U

static bool ch395_socket_wait_int(uint8_t socket_index, uint8_t mask, uint32_t timeout_ms);

bool ch395_socket_open(const ch395_socket_config_t *config) {
  if (config == NULL || config->remote_port == 0U || config->local_port == 0U) {
    return false;
  }

  (void)ch395_tcp_disconnect(config->socket_index);
  (void)ch395_close_socket(config->socket_index);
  (void)ch395_set_socket_proto_type(config->socket_index, (uint8_t)config->proto);
  (void)ch395_set_socket_desip(config->socket_index, config->remote_ip);
  (void)ch395_set_socket_desport(config->socket_index, config->remote_port);
  (void)ch395_set_socket_sourport(config->socket_index, config->local_port);

  uint8_t result = ch395_open_socket(config->socket_index);
  if (result != CH395_CMD_ERR_SUCCESS && result != CH395_ERR_OPEN) {
    return false;
  }

  if (config->proto == CH395_SOCKET_PROTO_TCP) {
    result = ch395_tcp_connect(config->socket_index);
    if (result != CH395_CMD_ERR_SUCCESS && result != CH395_ERR_ISCONN) {
      (void)ch395_close_socket(config->socket_index);
      return false;
    }
    // CH395Q 的 TCP_CONNECT 只是发起建连，真正可用要等 socket 状态进入 ESTABLISHED。
    if (!ch395_wait_socket_tcp_connected(config->socket_index, CH395_TCP_CONNECT_TIMEOUT_MS)) {
      (void)ch395_tcp_disconnect(config->socket_index);
      (void)ch395_close_socket(config->socket_index);
      return false;
    }
  }

  ch395_clear_recv_buf(config->socket_index);
  bsp_delay_ms(20U);
  return true;
}

void ch395_socket_close(uint8_t socket_index) {
  (void)ch395_tcp_disconnect(socket_index);
  (void)ch395_close_socket(socket_index);
}

bool ch395_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  if (!ch395_write_send_buf(socket_index, data, length)) {
    return false;
  }

  /* WRITE_SEND_BUF 只表示数据写进 CH395Q 发送缓冲区；必须等 SEND_OK，
   * 否则 QoS0 MQTT 会过早返回，Broker 侧可能根本收不到 PUBLISH。 */
  return ch395_socket_wait_int(socket_index, CH395_SOCKET_INT_SEND_OK, CH395_SOCKET_SEND_TIMEOUT_MS);
}

bool ch395_socket_send_to(uint8_t socket_index, const uint8_t ip[4], uint16_t port, const uint8_t *data, uint16_t length) {
  if (ip == NULL || port == 0U) {
    return false;
  }

  (void)ch395_set_socket_desip(socket_index, ip);
  (void)ch395_set_socket_desport(socket_index, port);
  return ch395_socket_send(socket_index, data, length);
}

uint16_t ch395_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  if (data == NULL || max_length == 0U) {
    return 0U;
  }

  uint16_t available = ch395_get_recv_length(socket_index);
  if (available == 0U) {
    return 0U;
  }
  if (available > max_length) {
    available = max_length;
  }

  uint16_t read_len = ch395_read_recv_buf(socket_index, data, available);
  ch395_clear_recv_buf(socket_index);
  return read_len;
}

bool ch395_socket_is_tcp_connected(uint8_t socket_index) {
  uint8_t state = CH395_TCP_CLOSED;
  return ch395_get_socket_tcp_state(socket_index, &state) && state == CH395_TCP_ESTABLISHED;
}

static bool ch395_socket_wait_int(uint8_t socket_index, uint8_t mask, uint32_t timeout_ms) {
  uint32_t start = bsp_get_tick_ms();

  do {
    /* SEND_OK 等待受网线、Broker 和 CH395Q 状态影响；这里刷新 IWDG 只表示底层仍在推进等待。 */
    (void)bsp_watchdog_refresh();
    uint8_t socket_int = ch395_get_socket_int(socket_index);
    if ((socket_int & mask) != 0U) {
      return true;
    }
    bsp_delay_ms(5U);
  } while ((bsp_get_tick_ms() - start) < timeout_ms);

  return false;
}
