#include "Network/Inc/network_socket.h"

#include "Modem/Inc/bsp_air724.h"
#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"

#include <stdio.h>
#include <string.h>

#define AIR724_AT_RESPONSE_SIZE 256U
#define AIR724_RX_RESPONSE_SIZE 1024U
#define AIR724_COMMAND_TIMEOUT_MS 3000U
#define AIR724_CONNECT_TIMEOUT_MS 30000U
#define AIR724_SEND_TIMEOUT_MS 10000U
#define AIR724_RECV_TIMEOUT_MS 250U

typedef struct {
  bool opened;
  network_socket_proto_t proto;
  char remote_host[64];
  uint16_t remote_port;
} air724_socket_state_t;

static air724_socket_state_t air724_sockets[NETWORK_SOCKET_MAX_COUNT];
static bool air724_ip_ready;
static volatile bool air724_socket_busy;

static bool network_socket_air724_is_ready(void);
static bool network_socket_air724_open(const network_socket_config_t *config);
static void network_socket_air724_close(uint8_t socket_index);
static bool network_socket_air724_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
static bool network_socket_air724_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                          uint16_t length);
static uint16_t network_socket_air724_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
static bool network_socket_air724_is_tcp_connected(uint8_t socket_index);
static bool air724_socket_lock(uint32_t timeout_ms);
static void air724_socket_unlock(void);
static void air724_socket_close_unlocked(uint8_t socket_index);
static bool air724_socket_prepare_ip(void);
static bool air724_socket_at(const char *command, char *response, uint16_t response_size, uint32_t timeout_ms);
static bool air724_socket_response_ok(const char *response);
static bool air724_socket_wait_pattern(const char *pattern, uint32_t timeout_ms, char *response, uint16_t response_size);
static bool air724_socket_wait_send_ack(uint32_t timeout_ms);
static uint16_t air724_socket_read_response(uint8_t *buffer, uint16_t buffer_size, uint32_t timeout_ms);
static int32_t air724_socket_find_pattern(const uint8_t *buffer, uint16_t buffer_len, const char *pattern);
static bool air724_socket_parse_available(const char *response, uint8_t socket_index, uint16_t *available);
static bool air724_socket_parse_read_header(const uint8_t *buffer, uint16_t buffer_len, uint8_t socket_index,
                                            uint16_t *payload_offset, uint16_t *payload_len);

const network_socket_ops_t network_socket_air724_ops = {
  .link = NETWORK_LINK_AIR724UG,
  .name = "Air724UG/UART4",
  .is_ready = network_socket_air724_is_ready,
  .open = network_socket_air724_open,
  .close = network_socket_air724_close,
  .send = network_socket_air724_send,
  .send_to = network_socket_air724_send_to,
  .recv = network_socket_air724_recv,
  .is_tcp_connected = network_socket_air724_is_tcp_connected,
};

static bool network_socket_air724_is_ready(void) {
#if BSP_HAS_AIR724UG
  return !bsp_air724_is_reset_asserted() && bsp_air724_read_netstate() == GPIO_PIN_SET;
#else
  return false;
#endif
}

static bool network_socket_air724_open(const network_socket_config_t *config) {
  bool connected = false;

  if (config == NULL || config->socket_index >= NETWORK_SOCKET_MAX_COUNT || config->remote_host == NULL ||
      config->remote_host[0] == '\0' || config->remote_port == 0U) {
    return false;
  }

  if (!air724_socket_lock(AIR724_CONNECT_TIMEOUT_MS)) {
    return false;
  }

  if (!air724_socket_prepare_ip()) {
    LOG_WARNING("Air724UG IP context not ready");
    air724_socket_unlock();
    return false;
  }

  air724_socket_close_unlocked(config->socket_index);

  char command[AIR724_AT_RESPONSE_SIZE] = {0};
  char response[AIR724_AT_RESPONSE_SIZE] = {0};
  const char *proto = config->proto == NETWORK_SOCKET_PROTO_TCP ? "TCP" : "UDP";
  (void)snprintf(command, sizeof(command), "AT+CIPSTART=%u,\"%s\",\"%s\",%u", config->socket_index, proto,
                 config->remote_host, config->remote_port);

  if (!air724_socket_at(command, response, sizeof(response), AIR724_CONNECT_TIMEOUT_MS)) {
    LOG_WARNING("Air724UG socket open failed: %s", response);
    air724_socket_unlock();
    return false;
  }

  connected = strstr(response, "CONNECT OK") != NULL || strstr(response, "ALREADY CONNECT") != NULL;
  if (!connected && air724_socket_response_ok(response)) {
    connected = air724_socket_wait_pattern("CONNECT OK", AIR724_CONNECT_TIMEOUT_MS, response, sizeof(response)) ||
                strstr(response, "ALREADY CONNECT") != NULL;
  }
  if (!connected) {
    LOG_WARNING("Air724UG socket open failed: %s", response);
    air724_socket_unlock();
    return false;
  }

  air724_sockets[config->socket_index].opened = true;
  air724_sockets[config->socket_index].proto = config->proto;
  (void)snprintf(air724_sockets[config->socket_index].remote_host, sizeof(air724_sockets[config->socket_index].remote_host),
                 "%s", config->remote_host);
  air724_sockets[config->socket_index].remote_port = config->remote_port;
  air724_socket_unlock();
  return true;
}

static void network_socket_air724_close(uint8_t socket_index) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT) {
    return;
  }

  if (!air724_socket_lock(AIR724_COMMAND_TIMEOUT_MS)) {
    return;
  }
  air724_socket_close_unlocked(socket_index);
  air724_socket_unlock();
}

static bool network_socket_air724_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  bool ok = false;

  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !air724_sockets[socket_index].opened || data == NULL || length == 0U) {
    return false;
  }

  if (!air724_socket_lock(AIR724_SEND_TIMEOUT_MS)) {
    return false;
  }

  char command[32] = {0};
  char response[AIR724_AT_RESPONSE_SIZE] = {0};
  (void)snprintf(command, sizeof(command), "AT+CIPSEND=%u,%u", socket_index, length);
  if (bsp_air724_uart_write((const uint8_t *)command, (uint16_t)strlen(command), AIR724_COMMAND_TIMEOUT_MS) != HAL_OK ||
      bsp_air724_uart_write((const uint8_t *)"\r\n", 2U, AIR724_COMMAND_TIMEOUT_MS) != HAL_OK) {
    air724_socket_unlock();
    return false;
  }
  if (!air724_socket_wait_pattern(">", AIR724_COMMAND_TIMEOUT_MS, response, sizeof(response))) {
    air724_socket_unlock();
    return false;
  }

  if (bsp_air724_uart_write(data, length, AIR724_SEND_TIMEOUT_MS) != HAL_OK) {
    air724_sockets[socket_index].opened = false;
    air724_socket_unlock();
    return false;
  }

  ok = air724_socket_wait_send_ack(AIR724_SEND_TIMEOUT_MS);
  if (!ok) {
    air724_sockets[socket_index].opened = false;
  }
  air724_socket_unlock();
  return ok;
}

static bool network_socket_air724_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                                          uint16_t length) {
  if (host == NULL || socket_index >= NETWORK_SOCKET_MAX_COUNT || !air724_sockets[socket_index].opened) {
    return false;
  }

  /* Air724UG 的 AT UDP 发送依赖 CIPSTART 绑定的远端；目标变更时重建同一个 socket。 */
  if (strncmp(air724_sockets[socket_index].remote_host, host, sizeof(air724_sockets[socket_index].remote_host)) != 0 ||
      air724_sockets[socket_index].remote_port != port) {
    network_socket_config_t config = {
      .socket_index = socket_index,
      .proto = NETWORK_SOCKET_PROTO_UDP,
      .remote_host = host,
      .remote_port = port,
      .local_port = port,
    };
    if (!network_socket_air724_open(&config)) {
      return false;
    }
  }
  return network_socket_air724_send(socket_index, data, length);
}

static uint16_t network_socket_air724_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  uint16_t payload_len = 0U;

  if (socket_index >= NETWORK_SOCKET_MAX_COUNT || !air724_sockets[socket_index].opened || data == NULL || max_length == 0U) {
    return 0U;
  }

  if (!air724_socket_lock(AIR724_COMMAND_TIMEOUT_MS)) {
    return 0U;
  }

  char command[32] = {0};
  char response[AIR724_AT_RESPONSE_SIZE] = {0};
  uint16_t available = 0U;
  (void)snprintf(command, sizeof(command), "AT+CIPRXGET=4,%u", socket_index);
  if (!air724_socket_at(command, response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS) ||
      !air724_socket_parse_available(response, socket_index, &available) || available == 0U) {
    air724_socket_unlock();
    return 0U;
  }

  uint16_t read_len = available > max_length ? max_length : available;
  (void)snprintf(command, sizeof(command), "AT+CIPRXGET=2,%u,%u", socket_index, read_len);
  if (bsp_air724_uart_write((const uint8_t *)command, (uint16_t)strlen(command), AIR724_COMMAND_TIMEOUT_MS) != HAL_OK ||
      bsp_air724_uart_write((const uint8_t *)"\r\n", 2U, AIR724_COMMAND_TIMEOUT_MS) != HAL_OK) {
    air724_socket_unlock();
    return 0U;
  }

  uint8_t buffer[AIR724_RX_RESPONSE_SIZE] = {0};
  uint16_t received = air724_socket_read_response(buffer, sizeof(buffer), AIR724_RECV_TIMEOUT_MS);
  uint16_t payload_offset = 0U;
  if (!air724_socket_parse_read_header(buffer, received, socket_index, &payload_offset, &payload_len) ||
      payload_offset >= received) {
    air724_socket_unlock();
    return 0U;
  }

  if (payload_len > max_length) {
    payload_len = max_length;
  }
  if ((uint32_t)payload_offset + payload_len > received) {
    payload_len = (uint16_t)(received - payload_offset);
  }
  memcpy(data, &buffer[payload_offset], payload_len);
  air724_socket_unlock();
  return payload_len;
}

static bool network_socket_air724_is_tcp_connected(uint8_t socket_index) {
  return socket_index < NETWORK_SOCKET_MAX_COUNT && air724_sockets[socket_index].opened &&
         air724_sockets[socket_index].proto == NETWORK_SOCKET_PROTO_TCP;
}

static bool air724_socket_lock(uint32_t timeout_ms) {
  uint32_t start = bsp_get_tick_ms();
  for (;;) {
    bool acquired = false;
    __disable_irq();
    if (!air724_socket_busy) {
      air724_socket_busy = true;
      acquired = true;
    }
    __enable_irq();
    if (acquired) {
      return true;
    }

    if ((bsp_get_tick_ms() - start) >= timeout_ms) {
      return false;
    }
    bsp_delay_ms(1U);
  }
}

static void air724_socket_unlock(void) {
  __disable_irq();
  air724_socket_busy = false;
  __enable_irq();
}

static void air724_socket_close_unlocked(uint8_t socket_index) {
  if (socket_index >= NETWORK_SOCKET_MAX_COUNT) {
    return;
  }

  if (air724_sockets[socket_index].opened) {
    char command[32] = {0};
    char response[AIR724_AT_RESPONSE_SIZE] = {0};
    (void)snprintf(command, sizeof(command), "AT+CIPCLOSE=%u", socket_index);
    (void)air724_socket_at(command, response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS);
  }
  memset(&air724_sockets[socket_index], 0, sizeof(air724_sockets[socket_index]));
}

static bool air724_socket_prepare_ip(void) {
  if (air724_ip_ready && network_socket_air724_is_ready()) {
    return true;
  }

  char response[AIR724_AT_RESPONSE_SIZE] = {0};
  if (!air724_socket_at("AT", response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS) || !air724_socket_response_ok(response)) {
    return false;
  }

  /* 这些 AT 命令只建立 4G PDP/socket 基础环境，不改变业务 MQTT/NTP 协议内容。 */
  (void)air724_socket_at("ATE0", response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS);
  (void)air724_socket_at("AT+CGATT=1", response, sizeof(response), AIR724_CONNECT_TIMEOUT_MS);
  (void)air724_socket_at("AT+CGACT=1,1", response, sizeof(response), AIR724_CONNECT_TIMEOUT_MS);
  (void)air724_socket_at("AT+CIPMUX=1", response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS);
  (void)air724_socket_at("AT+CIPQSEND=0", response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS);
  (void)air724_socket_at("AT+CIPRXGET=1", response, sizeof(response), AIR724_COMMAND_TIMEOUT_MS);

  air724_ip_ready = network_socket_air724_is_ready();
  return air724_ip_ready;
}

static bool air724_socket_at(const char *command, char *response, uint16_t response_size, uint32_t timeout_ms) {
  HAL_StatusTypeDef status = bsp_air724_at_command(command, response, response_size, timeout_ms);
  return status == HAL_OK && response != NULL && response[0] != '\0';
}

static bool air724_socket_response_ok(const char *response) {
  return response != NULL && strstr(response, "ERROR") == NULL &&
         (strstr(response, "\r\nOK\r\n") != NULL || strstr(response, "\nOK\r\n") != NULL ||
          strstr(response, "OK\r\n") != NULL);
}

static bool air724_socket_wait_pattern(const char *pattern, uint32_t timeout_ms, char *response, uint16_t response_size) {
  if (pattern == NULL || response == NULL || response_size == 0U) {
    return false;
  }

  response[0] = '\0';
  uint16_t received = 0U;
  uint32_t start = bsp_get_tick_ms();
  while (received < (uint16_t)(response_size - 1U) && (bsp_get_tick_ms() - start) < timeout_ms) {
    uint8_t byte = 0U;
    if (bsp_air724_uart_read(&byte, 1U, 20U) == HAL_OK) {
      response[received++] = (char)byte;
      response[received] = '\0';
      if (strstr(response, pattern) != NULL) {
        return true;
      }
      if (strstr(response, "ERROR") != NULL || strstr(response, "FAIL") != NULL) {
        return false;
      }
    }
  }
  return false;
}

static bool air724_socket_wait_send_ack(uint32_t timeout_ms) {
  char response[AIR724_AT_RESPONSE_SIZE] = {0};
  if (!air724_socket_wait_pattern("OK", timeout_ms, response, sizeof(response))) {
    return false;
  }
  return strstr(response, "SEND OK") != NULL || strstr(response, "DATA ACCEPT") != NULL || strstr(response, "\r\nOK\r\n") != NULL ||
         strstr(response, "\nOK\r\n") != NULL;
}

static uint16_t air724_socket_read_response(uint8_t *buffer, uint16_t buffer_size, uint32_t timeout_ms) {
  if (buffer == NULL || buffer_size == 0U) {
    return 0U;
  }

  uint16_t received = 0U;
  uint32_t start = bsp_get_tick_ms();
  while (received < buffer_size && (bsp_get_tick_ms() - start) < timeout_ms) {
    uint8_t byte = 0U;
    if (bsp_air724_uart_read(&byte, 1U, 20U) == HAL_OK) {
      buffer[received++] = byte;
      start = bsp_get_tick_ms();
      if (received >= 4U && air724_socket_find_pattern(buffer, received, "\r\nOK\r\n") >= 0) {
        break;
      }
      if (air724_socket_find_pattern(buffer, received, "ERROR") >= 0) {
        break;
      }
    }
  }
  return received;
}

static int32_t air724_socket_find_pattern(const uint8_t *buffer, uint16_t buffer_len, const char *pattern) {
  if (buffer == NULL || pattern == NULL) {
    return -1;
  }

  uint16_t pattern_len = (uint16_t)strlen(pattern);
  if (pattern_len == 0U || buffer_len < pattern_len) {
    return -1;
  }

  for (uint16_t index = 0U; index <= (uint16_t)(buffer_len - pattern_len); index++) {
    if (memcmp(&buffer[index], pattern, pattern_len) == 0) {
      return (int32_t)index;
    }
  }
  return -1;
}

static bool air724_socket_parse_available(const char *response, uint8_t socket_index, uint16_t *available) {
  if (response == NULL || available == NULL) {
    return false;
  }

  char prefix[24] = {0};
  (void)snprintf(prefix, sizeof(prefix), "+CIPRXGET: 4,%u,", socket_index);
  const char *cursor = strstr(response, prefix);
  if (cursor == NULL) {
    return false;
  }
  cursor += strlen(prefix);
  unsigned int value = 0U;
  if (sscanf(cursor, "%u", &value) != 1 || value > 65535U) {
    return false;
  }
  *available = (uint16_t)value;
  return true;
}

static bool air724_socket_parse_read_header(const uint8_t *buffer, uint16_t buffer_len, uint8_t socket_index,
                                            uint16_t *payload_offset, uint16_t *payload_len) {
  if (buffer == NULL || payload_offset == NULL || payload_len == NULL) {
    return false;
  }

  char prefix[24] = {0};
  (void)snprintf(prefix, sizeof(prefix), "+CIPRXGET: 2,%u,", socket_index);
  int32_t header_pos = air724_socket_find_pattern(buffer, buffer_len, prefix);
  if (header_pos < 0) {
    return false;
  }

  int32_t line_end = air724_socket_find_pattern(&buffer[header_pos], (uint16_t)(buffer_len - (uint16_t)header_pos), "\r\n");
  if (line_end < 0 || line_end >= 64) {
    return false;
  }

  char header[80] = {0};
  memcpy(header, &buffer[header_pos], (uint16_t)line_end);
  unsigned int length = 0U;
  unsigned int remain = 0U;
  if (sscanf(header, "+CIPRXGET: 2,%*u,%u,%u", &length, &remain) < 1 || length > 65535U) {
    (void)remain;
    return false;
  }

  *payload_offset = (uint16_t)((uint16_t)header_pos + (uint16_t)line_end + 2U);
  *payload_len = (uint16_t)length;
  return true;
}
