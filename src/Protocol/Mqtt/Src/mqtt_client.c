#include "Protocol/Mqtt/Inc/mqtt_client.h"

#include "Board/Inc/bsp_board.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Common/Inc/log.h"
#include "Common/Inc/md5.h"
#include "Network/Inc/network_socket.h"
#include <stdio.h>
#include <string.h>

#if APP_ENABLE_CMSIS_RTOS
#include "cmsis_os2.h"
#endif

#define MQTT_SOCKET_INDEX 0U
#define MQTT_TX_BUF_SIZE 1792U
#define MQTT_RX_BUF_SIZE 768U
#define MQTT_FIXED_HEADER_CONNECT 0x10U
#define MQTT_FIXED_HEADER_CONNACK 0x20U
#define MQTT_FIXED_HEADER_PUBLISH_QOS0 0x30U
#define MQTT_FIXED_HEADER_PUBLISH_QOS1 0x32U
#define MQTT_FIXED_HEADER_PUBACK 0x40U
#define MQTT_FIXED_HEADER_SUBSCRIBE 0x82U
#define MQTT_FIXED_HEADER_SUBACK 0x90U
#define MQTT_FIXED_HEADER_PINGREQ 0xC0U
#define MQTT_FIXED_HEADER_PINGRESP 0xD0U
#define MQTT_FIXED_HEADER_DISCONNECT 0xE0U
#define MQTT_PROTO_LEVEL_3_1_1 4U
#define MQTT_CONNECT_FLAG_CLEAN_SESSION 0x02U
#define MQTT_CONNECT_FLAG_PASSWORD 0x40U
#define MQTT_CONNECT_FLAG_USERNAME 0x80U
#define MQTT_SUBSCRIBE_PACKET_ID 1U
#define MQTT_PUBACK_FIXED_LEN 4U
#define MQTT_CONNECT_TIMEOUT_POLLS 8U
#define MQTT_SUBACK_TIMEOUT_POLLS 8U
#define MQTT_PINGRESP_TIMEOUT_POLLS 8U
#define MQTT_PUBACK_TIMEOUT_POLLS 10U
#define MQTT_POLL_LOCK_TIMEOUT_MS 1U
#define MQTT_INBOX_DEPTH 2U

#define TOPIC_CLIENT_TO_SERVER_GET_DEVICE_INFO "/v1/devices/up/getDeviceInfo/"
#define TOPIC_SERVER_TO_CLIENT_REGISTER_INFO "/v1/devices/down/registerInfo/"
// 平台对 update 上报的确认回执，旧流程里也会单独关注，漏订阅会丢掉更新确认。
#define TOPIC_SERVER_TO_CLIENT_UPDATE_RESP "/v1/devices/down/updateResponse/"
#define TOPIC_CLIENT_TO_SERVER_UPDATE "/v1/devices/up/update/"
#define TOPIC_CLIENT_TO_SERVER_DATA "/v1/devices/up/datas/"
#define TOPIC_SERVER_TO_CLIENT_CMD "/v1/devices/down/command/"
#define TOPIC_CLIENT_TO_SERVER_CMD_RESP "/v1/devices/up/commandResponse/"

typedef struct {
  char topic[MQTT_TOPIC_MAX_LEN];
  char payload[MQTT_PAYLOAD_MAX_LEN];
} mqtt_inbox_message_t;

static mqtt_config_t *mqtt_config;
static mqtt_client_state_t mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
static mqtt_message_handler_t mqtt_message_handler;
static uint16_t mqtt_packet_flags;
static uint16_t mqtt_packet_id = MQTT_SUBSCRIBE_PACKET_ID;
static uint16_t mqtt_last_puback_id;
static uint32_t mqtt_last_reconnect_tick;
static uint32_t mqtt_last_ping_tick;
static uint8_t mqtt_rx_accum[MQTT_RX_BUF_SIZE];
static uint16_t mqtt_rx_accum_len;
static mqtt_inbox_message_t mqtt_inbox[MQTT_INBOX_DEPTH];
static uint8_t mqtt_inbox_head;
static uint8_t mqtt_inbox_tail;
static uint8_t mqtt_inbox_count;
static uint32_t mqtt_inbox_dropped;
/*
 * PUBLISH 需要同时组装变量头+payload 和最终 MQTT 包。启用多 RS485 设备后数据包明显变大，
 * 这里放到静态区并由 mqtt_lock 串行保护，避免两块大缓冲叠在 pem_main 线程栈上。
 */
static uint8_t mqtt_tx_variable[MQTT_TX_BUF_SIZE];
static uint8_t mqtt_tx_packet[MQTT_TX_BUF_SIZE];
#if APP_ENABLE_CMSIS_RTOS
static osMutexId_t mqtt_lock;
#endif

static void mqtt_lock_init(void);
static bool mqtt_lock_acquire(void);
static bool mqtt_lock_acquire_wait(uint32_t timeout_ms);
static void mqtt_lock_release(void);
static bool mqtt_client_connect_locked(void);
static void mqtt_client_disconnect_locked(void);
static bool mqtt_client_subscribe_platform_topics_locked(void);
static bool mqtt_client_ping_locked(void);
static void mqtt_client_poll_locked(void);
static bool mqtt_client_maintain_locked(uint32_t now_ms, uint32_t reconnect_interval_ms);
static bool mqtt_ensure_credentials(void);
static bool mqtt_open_tcp(void);
static bool mqtt_send_connect(void);
static bool mqtt_subscribe_topic_prefix(const char *topic_prefix);
static bool mqtt_subscribe_full_topic(const char *topic);
static bool mqtt_publish_topic_prefix(const char *topic_prefix, const char *payload);
static bool mqtt_publish_full_topic(const char *topic, const char *payload);
static bool mqtt_send_packet(const uint8_t *packet, uint16_t length);
static uint16_t mqtt_next_packet_id(void);
static uint16_t mqtt_encode_remaining_length(uint8_t *buffer, uint32_t length);
static bool mqtt_try_decode_frame_len(const uint8_t *packet, uint16_t available_len, uint16_t *frame_len);
static bool mqtt_write_utf8(uint8_t *buffer, uint16_t buffer_len, uint16_t *offset, const char *value);
static bool mqtt_wait_for_flag(uint16_t flag, uint8_t polls);
static void mqtt_handle_packet(const uint8_t *packet, uint16_t length);
static void mqtt_handle_publish(const uint8_t *packet, uint16_t length, uint8_t header);
static void mqtt_send_puback(uint16_t packet_id);
static uint16_t mqtt_decode_remaining_length(const uint8_t *packet, uint16_t length, uint16_t *remaining_offset);
static void mqtt_inbox_reset(void);
static void mqtt_inbox_push(const char *topic, const char *payload);
static bool mqtt_inbox_pop(char *topic, uint16_t topic_len, char *payload, uint16_t payload_len);

static void mqtt_lock_init(void) {
#if APP_ENABLE_CMSIS_RTOS
  if (mqtt_lock == NULL && osKernelGetState() == osKernelRunning) {
    const osMutexAttr_t attr = {
      .name = "mqtt_lock",
      .attr_bits = osMutexRecursive | osMutexPrioInherit,
    };
    mqtt_lock = osMutexNew(&attr);
  }
#endif
}

static bool mqtt_lock_acquire(void) {
#if APP_ENABLE_CMSIS_RTOS
  return mqtt_lock_acquire_wait(osWaitForever);
#else
  return mqtt_lock_acquire_wait(0U);
#endif
}

static bool mqtt_lock_acquire_wait(uint32_t timeout_ms) {
#if APP_ENABLE_CMSIS_RTOS
  mqtt_lock_init();
  if (mqtt_lock != NULL) {
    return osMutexAcquire(mqtt_lock, timeout_ms) == osOK;
  }
#else
  (void)timeout_ms;
#endif
  return true;
}

static void mqtt_lock_release(void) {
#if APP_ENABLE_CMSIS_RTOS
  if (mqtt_lock != NULL) {
    (void)osMutexRelease(mqtt_lock);
  }
#endif
}

void mqtt_client_init(mqtt_config_t *config) {
  mqtt_lock_init();
  if (!mqtt_lock_acquire()) {
    return;
  }
  mqtt_config = config;
  mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
  mqtt_packet_flags = 0U;
  mqtt_packet_id = MQTT_SUBSCRIBE_PACKET_ID;
  mqtt_last_puback_id = 0U;
  mqtt_last_reconnect_tick = 0U;
  mqtt_last_ping_tick = 0U;
  mqtt_rx_accum_len = 0U;
  mqtt_inbox_reset();
  (void)mqtt_ensure_credentials();
  mqtt_lock_release();
}

bool mqtt_client_connect(void) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_client_connect_locked();
  mqtt_lock_release();
  return ok;
}

void mqtt_client_disconnect(void) {
  if (!mqtt_lock_acquire()) {
    return;
  }
  mqtt_client_disconnect_locked();
  mqtt_lock_release();
}

bool mqtt_client_subscribe_platform_topics(void) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_client_subscribe_platform_topics_locked();
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_subscribe_topic(const char *topic) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_subscribe_full_topic(topic);
  if (ok) {
    mqtt_state = MQTT_CLIENT_STATE_SUBSCRIBED;
  }
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_publish_get_device_info(const char *payload) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_publish_topic_prefix(TOPIC_CLIENT_TO_SERVER_GET_DEVICE_INFO, payload);
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_publish_update(const char *payload) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_publish_topic_prefix(TOPIC_CLIENT_TO_SERVER_UPDATE, payload);
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_publish_data(const char *payload) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_publish_topic_prefix(TOPIC_CLIENT_TO_SERVER_DATA, payload);
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_publish_command_response(const char *payload) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_publish_topic_prefix(TOPIC_CLIENT_TO_SERVER_CMD_RESP, payload);
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_publish_topic(const char *topic, const char *payload) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_publish_full_topic(topic, payload);
  mqtt_lock_release();
  return ok;
}

bool mqtt_client_ping(void) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_client_ping_locked();
  mqtt_lock_release();
  return ok;
}

void mqtt_client_poll(void) {
  /*
   * mqtt_rx 线程只负责“有包就取”，不能在主线程重连/订阅时无限等锁。
   * Broker 不可达时重连路径可能持续数秒；这里短等待失败后让线程继续心跳，
   * 避免把正常网络超时误判成 RTOS 线程死亡。
   */
  if (!mqtt_lock_acquire_wait(MQTT_POLL_LOCK_TIMEOUT_MS)) {
    return;
  }
  mqtt_client_poll_locked();
  mqtt_lock_release();
}

bool mqtt_client_maintain(uint32_t now_ms, uint32_t reconnect_interval_ms) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_client_maintain_locked(now_ms, reconnect_interval_ms);
  mqtt_lock_release();
  return ok;
}

void mqtt_client_set_message_handler(mqtt_message_handler_t handler) {
  if (!mqtt_lock_acquire()) {
    return;
  }
  mqtt_message_handler = handler;
  mqtt_lock_release();
}

mqtt_client_state_t mqtt_client_get_state(void) {
  mqtt_client_state_t state;
  if (!mqtt_lock_acquire()) {
    return MQTT_CLIENT_STATE_DISCONNECTED;
  }
  state = mqtt_state;
  mqtt_lock_release();
  return state;
}

uint16_t mqtt_client_get_packet_flags(void) {
  uint16_t flags;
  if (!mqtt_lock_acquire()) {
    return 0U;
  }
  flags = mqtt_packet_flags;
  mqtt_lock_release();
  return flags;
}

uint16_t mqtt_client_get_last_puback_id(void) {
  uint16_t last_puback_id;
  if (!mqtt_lock_acquire()) {
    return 0U;
  }
  last_puback_id = mqtt_last_puback_id;
  mqtt_lock_release();
  return last_puback_id;
}

void mqtt_client_clear_packet_flags(uint16_t flags) {
  mqtt_packet_flags = (uint16_t)(mqtt_packet_flags & ~flags);
}

bool mqtt_client_is_ready(void) {
  bool ready;
  if (!mqtt_lock_acquire()) {
    return false;
  }
  ready = mqtt_state == MQTT_CLIENT_STATE_SUBSCRIBED;
  mqtt_lock_release();
  return ready;
}

void mqtt_client_build_topic(char *buffer, uint16_t buffer_len, const char *prefix) {
  if (buffer == NULL || buffer_len == 0U) {
    return;
  }
  if (!mqtt_lock_acquire()) {
    buffer[0] = '\0';
    return;
  }
  const char *user_name = mqtt_config == NULL ? "" : mqtt_config->user_name;
  (void)snprintf(buffer, buffer_len, "%s%s", prefix == NULL ? "" : prefix, user_name);
  mqtt_lock_release();
}

uint8_t mqtt_client_pending_messages(void) {
  uint8_t count;
  if (!mqtt_lock_acquire()) {
    return 0U;
  }
  count = mqtt_inbox_count;
  mqtt_lock_release();
  return count;
}

bool mqtt_client_read_message(char *topic, uint16_t topic_len, char *payload, uint16_t payload_len) {
  if (!mqtt_lock_acquire()) {
    return false;
  }
  bool ok = mqtt_inbox_pop(topic, topic_len, payload, payload_len);
  mqtt_lock_release();
  return ok;
}

uint32_t mqtt_client_get_dropped_message_count(void) {
  uint32_t dropped;
  if (!mqtt_lock_acquire()) {
    return 0U;
  }
  dropped = mqtt_inbox_dropped;
  mqtt_lock_release();
  return dropped;
}

static bool mqtt_client_connect_locked(void) {
  if (mqtt_config == NULL || !mqtt_ensure_credentials()) {
    return false;
  }
  if (!mqtt_open_tcp() || !mqtt_send_connect()) {
    mqtt_client_disconnect_locked();
    return false;
  }
  mqtt_client_clear_packet_flags(MQTT_PACKET_FLAG_CONNACK);
  if (!mqtt_wait_for_flag(MQTT_PACKET_FLAG_CONNACK, MQTT_CONNECT_TIMEOUT_POLLS)) {
    LOG_WARNING("MQTT CONNACK timeout");
    mqtt_client_disconnect_locked();
    return false;
  }
  mqtt_state = MQTT_CLIENT_STATE_SESSION_CONNECTED;
  mqtt_last_ping_tick = HAL_GetTick();
  return true;
}

static void mqtt_client_disconnect_locked(void) {
  uint8_t packet[] = {MQTT_FIXED_HEADER_DISCONNECT, 0U};
  if (mqtt_state != MQTT_CLIENT_STATE_DISCONNECTED) {
    (void)mqtt_send_packet(packet, sizeof(packet));
  }
  network_socket_close(MQTT_SOCKET_INDEX);
  mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
  mqtt_rx_accum_len = 0U;
}

static bool mqtt_client_subscribe_platform_topics_locked(void) {
  if (mqtt_state < MQTT_CLIENT_STATE_SESSION_CONNECTED) {
    return false;
  }
  if (!mqtt_subscribe_topic_prefix(TOPIC_SERVER_TO_CLIENT_REGISTER_INFO) ||
      !mqtt_subscribe_topic_prefix(TOPIC_SERVER_TO_CLIENT_UPDATE_RESP) ||
      !mqtt_subscribe_topic_prefix(TOPIC_SERVER_TO_CLIENT_CMD)) {
    return false;
  }
  mqtt_state = MQTT_CLIENT_STATE_SUBSCRIBED;
  return true;
}

static bool mqtt_client_ping_locked(void) {
  uint8_t packet[] = {MQTT_FIXED_HEADER_PINGREQ, 0U};
  mqtt_client_clear_packet_flags(MQTT_PACKET_FLAG_PINGRESP);
  if (!mqtt_send_packet(packet, sizeof(packet))) {
    return false;
  }
  return mqtt_wait_for_flag(MQTT_PACKET_FLAG_PINGRESP, MQTT_PINGRESP_TIMEOUT_POLLS);
}

static void mqtt_client_poll_locked(void) {
  uint8_t buffer[MQTT_RX_BUF_SIZE] = {0};
  uint16_t length = network_socket_recv(MQTT_SOCKET_INDEX, buffer, sizeof(buffer));
  if (length > 0U) {
    if ((uint32_t)mqtt_rx_accum_len + length > sizeof(mqtt_rx_accum)) {
      LOG_WARNING("MQTT RX overflow, drop buffered frame");
      mqtt_rx_accum_len = 0U;
    }
    memcpy(&mqtt_rx_accum[mqtt_rx_accum_len], buffer, length);
    mqtt_rx_accum_len = (uint16_t)(mqtt_rx_accum_len + length);
    while (mqtt_rx_accum_len > 0U) {
      uint16_t frame_len = 0U;
      if (!mqtt_try_decode_frame_len(mqtt_rx_accum, mqtt_rx_accum_len, &frame_len)) {
        break;
      }
      mqtt_handle_packet(mqtt_rx_accum, frame_len);
      mqtt_rx_accum_len = (uint16_t)(mqtt_rx_accum_len - frame_len);
      if (mqtt_rx_accum_len > 0U) {
        memmove(mqtt_rx_accum, &mqtt_rx_accum[frame_len], mqtt_rx_accum_len);
      }
    }
  }
  if (mqtt_state != MQTT_CLIENT_STATE_DISCONNECTED && !network_socket_is_tcp_connected(MQTT_SOCKET_INDEX)) {
    mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
    mqtt_rx_accum_len = 0U;
  }
}

static bool mqtt_client_maintain_locked(uint32_t now_ms, uint32_t reconnect_interval_ms) {
  mqtt_client_poll_locked();
  if (mqtt_config == NULL) {
    return false;
  }
  if (reconnect_interval_ms == 0U) {
    reconnect_interval_ms = 15000U;
  }

  if (mqtt_state == MQTT_CLIENT_STATE_DISCONNECTED) {
    if (mqtt_last_reconnect_tick != 0U && (now_ms - mqtt_last_reconnect_tick) < reconnect_interval_ms) {
      return false;
    }
    mqtt_last_reconnect_tick = now_ms;
    LOG_INFO("MQTT reconnecting to %s:%u", mqtt_config->ip, mqtt_config->port);
    if (!mqtt_client_connect_locked()) {
      return false;
    }
  }

  if (mqtt_state == MQTT_CLIENT_STATE_SESSION_CONNECTED && !mqtt_client_subscribe_platform_topics_locked()) {
    LOG_WARNING("MQTT subscribe platform topics failed");
    mqtt_client_disconnect_locked();
    return false;
  }

  if (mqtt_state == MQTT_CLIENT_STATE_SUBSCRIBED) {
    uint32_t keepalive_ms = (uint32_t)mqtt_config->keepalive * 1000U;
    if (keepalive_ms < 10000U) {
      keepalive_ms = 10000U;
    }
    // MQTT KeepAlive 要在超时前主动发 PING；这里取一半周期，给底层 TCP 链路留出重试余量。
    if (mqtt_last_ping_tick == 0U || (now_ms - mqtt_last_ping_tick) >= (keepalive_ms / 2U)) {
      if (!mqtt_client_ping_locked()) {
        LOG_WARNING("MQTT ping failed, reconnect later");
        mqtt_client_disconnect_locked();
        return false;
      }
      mqtt_last_ping_tick = now_ms;
    }
  }
  return mqtt_state == MQTT_CLIENT_STATE_SUBSCRIBED;
}

static bool mqtt_ensure_credentials(void) {
  if (mqtt_config == NULL || mqtt_config->user_name[0] == '\0') {
    return false;
  }

  if (mqtt_config->client_id[0] == '\0') {
    /* 未显式配置 clientId 时，沿用旧平台规则：clientId = "c" + collectorId。 */
    int written = snprintf(mqtt_config->client_id, sizeof(mqtt_config->client_id), "c%s", mqtt_config->user_name);
    if (written <= 0 || (size_t)written >= sizeof(mqtt_config->client_id)) {
      return false;
    }
  }

  if (mqtt_config->password[0] == '\0') {
    /* 密码为空表示自动生成平台 MD5 密码；上位机写入自定义密码时这里不会覆盖。 */
    char plain[MQTT_USER_NAME_BUF_SIZE + 10U] = {0};
    (void)snprintf(plain, sizeof(plain), "identify:%s", mqtt_config->user_name);
    Md5GenerateStr(plain, (unsigned int)strlen(plain), mqtt_config->password);
  }
  return true;
}

static bool mqtt_open_tcp(void) {
  if (!network_socket_active_link_ready()) {
    LOG_WARNING("MQTT skipped: active network %s not ready", network_socket_active_link_name());
    return false;
  }
  network_socket_config_t socket_config = {
    .socket_index = MQTT_SOCKET_INDEX,
    .proto = NETWORK_SOCKET_PROTO_TCP,
    .remote_host = mqtt_config->ip,
    .remote_port = mqtt_config->port,
    .local_port = mqtt_config->local_port,
  };
  if (!network_socket_open(&socket_config)) {
    return false;
  }
  mqtt_state = MQTT_CLIENT_STATE_TCP_CONNECTED;
  return true;
}

static bool mqtt_send_connect(void) {
  uint8_t payload[MQTT_TX_BUF_SIZE] = {0};
  uint16_t offset = 0U;
  if (!mqtt_write_utf8(payload, sizeof(payload), &offset, "MQTT")) {
    return false;
  }
  payload[offset++] = MQTT_PROTO_LEVEL_3_1_1;
  payload[offset++] = MQTT_CONNECT_FLAG_CLEAN_SESSION | MQTT_CONNECT_FLAG_USERNAME | MQTT_CONNECT_FLAG_PASSWORD;
  payload[offset++] = (uint8_t)(mqtt_config->keepalive >> 8U);
  payload[offset++] = (uint8_t)mqtt_config->keepalive;
  if (!mqtt_write_utf8(payload, sizeof(payload), &offset, mqtt_config->client_id) ||
      !mqtt_write_utf8(payload, sizeof(payload), &offset, mqtt_config->user_name) ||
      !mqtt_write_utf8(payload, sizeof(payload), &offset, mqtt_config->password)) {
    return false;
  }

  uint8_t packet[MQTT_TX_BUF_SIZE] = {0};
  packet[0] = MQTT_FIXED_HEADER_CONNECT;
  uint16_t rl_len = mqtt_encode_remaining_length(&packet[1], offset);
  if ((1U + rl_len + offset) > sizeof(packet)) {
    return false;
  }
  memcpy(&packet[1U + rl_len], payload, offset);
  return mqtt_send_packet(packet, (uint16_t)(1U + rl_len + offset));
}

static bool mqtt_subscribe_topic_prefix(const char *topic_prefix) {
  char topic[MQTT_TOPIC_MAX_LEN] = {0};
  mqtt_client_build_topic(topic, sizeof(topic), topic_prefix);
  return mqtt_subscribe_full_topic(topic);
}

static bool mqtt_subscribe_full_topic(const char *topic) {
  uint8_t payload[MQTT_TX_BUF_SIZE] = {0};
  uint16_t offset = 0U;
  if (mqtt_state < MQTT_CLIENT_STATE_SESSION_CONNECTED || topic == NULL || topic[0] == '\0') {
    return false;
  }
  uint16_t packet_id = mqtt_next_packet_id();
  payload[offset++] = (uint8_t)(packet_id >> 8U);
  payload[offset++] = (uint8_t)packet_id;
  if (!mqtt_write_utf8(payload, sizeof(payload), &offset, topic)) {
    return false;
  }
  payload[offset++] = mqtt_config->sub_qos;

  uint8_t packet[MQTT_TX_BUF_SIZE] = {0};
  packet[0] = MQTT_FIXED_HEADER_SUBSCRIBE;
  uint16_t rl_len = mqtt_encode_remaining_length(&packet[1], offset);
  memcpy(&packet[1U + rl_len], payload, offset);
  mqtt_client_clear_packet_flags(MQTT_PACKET_FLAG_SUBACK);
  bool sent = mqtt_send_packet(packet, (uint16_t)(1U + rl_len + offset));
  if (!sent) {
    LOG_WARNING("MQTT subscribe send failed topic=%s state=%u link=%s", topic, (unsigned int)mqtt_state,
                network_socket_active_link_name());
    return false;
  }
  if (!mqtt_wait_for_flag(MQTT_PACKET_FLAG_SUBACK, MQTT_SUBACK_TIMEOUT_POLLS)) {
    LOG_WARNING("MQTT SUBACK timeout topic=%s state=%u link=%s", topic, (unsigned int)mqtt_state,
                network_socket_active_link_name());
    return false;
  }
  return true;
}

static bool mqtt_publish_topic_prefix(const char *topic_prefix, const char *payload) {
  char topic[MQTT_TOPIC_MAX_LEN] = {0};
  mqtt_client_build_topic(topic, sizeof(topic), topic_prefix);
  return mqtt_publish_full_topic(topic, payload);
}

static bool mqtt_publish_full_topic(const char *topic, const char *payload) {
  if (mqtt_state < MQTT_CLIENT_STATE_SESSION_CONNECTED || topic == NULL || topic[0] == '\0' || payload == NULL) {
    return false;
  }
  uint16_t offset = 0U;
  uint16_t payload_len = (uint16_t)strlen(payload);
  memset(mqtt_tx_variable, 0, sizeof(mqtt_tx_variable));
  memset(mqtt_tx_packet, 0, sizeof(mqtt_tx_packet));
  if (!mqtt_write_utf8(mqtt_tx_variable, sizeof(mqtt_tx_variable), &offset, topic)) {
    return false;
  }
  uint16_t packet_id = 0U;
  uint8_t qos = mqtt_config == NULL ? 0U : mqtt_config->pub_qos;
  if (qos > 1U) {
    qos = 1U;
  }
  if (qos == 1U) {
    packet_id = mqtt_next_packet_id();
    if ((offset + 2U) > sizeof(mqtt_tx_variable)) {
      return false;
    }
    mqtt_tx_variable[offset++] = (uint8_t)(packet_id >> 8U);
    mqtt_tx_variable[offset++] = (uint8_t)packet_id;
  }
  if ((offset + payload_len) > sizeof(mqtt_tx_variable)) {
    return false;
  }
  memcpy(&mqtt_tx_variable[offset], payload, payload_len);
  offset = (uint16_t)(offset + payload_len);

  mqtt_tx_packet[0] = qos == 1U ? MQTT_FIXED_HEADER_PUBLISH_QOS1 : MQTT_FIXED_HEADER_PUBLISH_QOS0;
  uint16_t rl_len = mqtt_encode_remaining_length(&mqtt_tx_packet[1], offset);
  memcpy(&mqtt_tx_packet[1U + rl_len], mqtt_tx_variable, offset);
  mqtt_client_clear_packet_flags(MQTT_PACKET_FLAG_PUBACK);
  if (!mqtt_send_packet(mqtt_tx_packet, (uint16_t)(1U + rl_len + offset))) {
    LOG_WARNING("MQTT publish send failed topic=%s state=%u link=%s", topic, (unsigned int)mqtt_state,
                network_socket_active_link_name());
    return false;
  }
  // QoS1 发布必须等待 PUBACK，保证平台命令响应/数据上报至少到达 Broker 一次。
  if (qos == 0U) {
    return true;
  }
  if (!mqtt_wait_for_flag(MQTT_PACKET_FLAG_PUBACK, MQTT_PUBACK_TIMEOUT_POLLS)) {
    LOG_WARNING("MQTT PUBACK timeout topic=%s state=%u link=%s", topic, (unsigned int)mqtt_state,
                network_socket_active_link_name());
    return false;
  }
  return mqtt_last_puback_id == packet_id;
}

static bool mqtt_send_packet(const uint8_t *packet, uint16_t length) {
  return packet != NULL && length > 0U && network_socket_send(MQTT_SOCKET_INDEX, packet, length);
}

static uint16_t mqtt_next_packet_id(void) {
  mqtt_packet_id++;
  if (mqtt_packet_id == 0U) {
    mqtt_packet_id = 1U;
  }
  return mqtt_packet_id;
}

static uint16_t mqtt_encode_remaining_length(uint8_t *buffer, uint32_t length) {
  uint16_t offset = 0U;
  do {
    uint8_t encoded = (uint8_t)(length % 128U);
    length /= 128U;
    if (length > 0U) {
      encoded |= 0x80U;
    }
    buffer[offset++] = encoded;
  } while (length > 0U && offset < 4U);
  return offset;
}

static bool mqtt_try_decode_frame_len(const uint8_t *packet, uint16_t available_len, uint16_t *frame_len) {
  uint32_t multiplier = 1U;
  uint32_t remaining_len = 0U;
  uint16_t offset = 1U;
  uint8_t encoded = 0U;
  if (packet == NULL || frame_len == NULL || available_len < 2U) {
    return false;
  }
  do {
    if (offset >= available_len || offset > 4U) {
      return false;
    }
    encoded = packet[offset++];
    remaining_len += (uint32_t)(encoded & 0x7FU) * multiplier;
    multiplier *= 128U;
  } while ((encoded & 0x80U) != 0U);
  if ((uint32_t)offset + remaining_len > available_len) {
    return false;
  }
  *frame_len = (uint16_t)(offset + remaining_len);
  return true;
}

static bool mqtt_write_utf8(uint8_t *buffer, uint16_t buffer_len, uint16_t *offset, const char *value) {
  if (buffer == NULL || offset == NULL || value == NULL) {
    return false;
  }
  uint16_t value_len = (uint16_t)strlen(value);
  if ((*offset + 2U + value_len) > buffer_len) {
    return false;
  }
  buffer[(*offset)++] = (uint8_t)(value_len >> 8U);
  buffer[(*offset)++] = (uint8_t)value_len;
  memcpy(&buffer[*offset], value, value_len);
  *offset = (uint16_t)(*offset + value_len);
  return true;
}

static bool mqtt_wait_for_flag(uint16_t flag, uint8_t polls) {
  for (uint8_t i = 0U; i < polls; i++) {
    /* CONNACK/SUBACK/PUBACK/PINGRESP 等待时仍由当前线程轮询 socket，必须避免无响应 Broker 触发 IWDG。 */
    (void)bsp_watchdog_refresh();
    mqtt_client_poll();
    if ((mqtt_packet_flags & flag) != 0U) {
      return true;
    }
    HAL_Delay(100U);
  }
  return false;
}

static void mqtt_handle_packet(const uint8_t *packet, uint16_t length) {
  if (packet == NULL || length < 2U) {
    return;
  }
  uint8_t type = (uint8_t)(packet[0] & 0xF0U);
  switch (type) {
  case MQTT_FIXED_HEADER_CONNACK:
    if (length >= 4U && packet[3] == 0U) {
      mqtt_packet_flags |= MQTT_PACKET_FLAG_CONNACK;
    }
    break;
  case MQTT_FIXED_HEADER_PUBLISH_QOS0:
  case MQTT_FIXED_HEADER_PUBLISH_QOS1:
    mqtt_handle_publish(packet, length, packet[0]);
    mqtt_packet_flags |= MQTT_PACKET_FLAG_PUBLISH;
    break;
  case MQTT_FIXED_HEADER_PUBACK:
    if (length >= MQTT_PUBACK_FIXED_LEN) {
      mqtt_last_puback_id = (uint16_t)(((uint16_t)packet[2] << 8U) | packet[3]);
      mqtt_packet_flags |= MQTT_PACKET_FLAG_PUBACK;
    }
    break;
  case MQTT_FIXED_HEADER_SUBACK:
    mqtt_packet_flags |= MQTT_PACKET_FLAG_SUBACK;
    break;
  case MQTT_FIXED_HEADER_PINGRESP:
    mqtt_packet_flags |= MQTT_PACKET_FLAG_PINGRESP;
    break;
  case MQTT_FIXED_HEADER_DISCONNECT:
    mqtt_packet_flags |= MQTT_PACKET_FLAG_DISCONNECT;
    mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
    break;
  default:
    break;
  }
}

static void mqtt_handle_publish(const uint8_t *packet, uint16_t length, uint8_t header) {
  uint16_t remaining_offset = 0U;
  uint16_t remaining_len = mqtt_decode_remaining_length(packet, length, &remaining_offset);
  uint16_t pos = (uint16_t)(1U + remaining_offset);
  if (remaining_len == 0U || (pos + 2U) > length) {
    return;
  }
  uint16_t topic_len = (uint16_t)((packet[pos] << 8U) | packet[pos + 1U]);
  pos = (uint16_t)(pos + 2U);
  if ((pos + topic_len) > length || topic_len >= MQTT_TOPIC_MAX_LEN) {
    return;
  }
  char topic[MQTT_TOPIC_MAX_LEN] = {0};
  memcpy(topic, &packet[pos], topic_len);
  pos = (uint16_t)(pos + topic_len);
  uint8_t qos = (uint8_t)((header >> 1U) & 0x03U);
  uint16_t packet_id = 0U;
  if (qos > 0U) {
    if ((pos + 2U) > length) {
      return;
    }
    packet_id = (uint16_t)(((uint16_t)packet[pos] << 8U) | packet[pos + 1U]);
    pos = (uint16_t)(pos + 2U);
  }
  if (pos > length) {
    return;
  }
  uint16_t payload_len = (uint16_t)(length - pos);
  if (payload_len >= MQTT_PAYLOAD_MAX_LEN) {
    payload_len = MQTT_PAYLOAD_MAX_LEN - 1U;
  }
  char payload[MQTT_PAYLOAD_MAX_LEN] = {0};
  memcpy(payload, &packet[pos], payload_len);
  mqtt_inbox_push(topic, payload);
  if (mqtt_message_handler != NULL) {
    mqtt_message_handler(topic, payload);
  }
  if (qos == 1U) {
    // 平台若使用 QoS1 下发命令，固件必须回 PUBACK，否则 Broker 会重复投递。
    mqtt_send_puback(packet_id);
  }
}

static void mqtt_send_puback(uint16_t packet_id) {
  uint8_t packet[] = {MQTT_FIXED_HEADER_PUBACK, 0x02U, (uint8_t)(packet_id >> 8U), (uint8_t)packet_id};
  (void)mqtt_send_packet(packet, sizeof(packet));
}

static uint16_t mqtt_decode_remaining_length(const uint8_t *packet, uint16_t length, uint16_t *remaining_offset) {
  uint32_t multiplier = 1U;
  uint32_t value = 0U;
  uint16_t offset = 1U;
  uint8_t encoded = 0U;
  do {
    if (offset >= length || offset > 4U) {
      return 0U;
    }
    encoded = packet[offset++];
    value += (uint32_t)(encoded & 0x7FU) * multiplier;
    multiplier *= 128U;
  } while ((encoded & 0x80U) != 0U);
  *remaining_offset = (uint16_t)(offset - 1U);
  return (uint16_t)value;
}

static void mqtt_inbox_reset(void) {
  memset(mqtt_inbox, 0, sizeof(mqtt_inbox));
  mqtt_inbox_head = 0U;
  mqtt_inbox_tail = 0U;
  mqtt_inbox_count = 0U;
  mqtt_inbox_dropped = 0U;
}

static void mqtt_inbox_push(const char *topic, const char *payload) {
  if (topic == NULL || payload == NULL) {
    return;
  }
  if (mqtt_inbox_count >= MQTT_INBOX_DEPTH) {
    mqtt_inbox_tail = (uint8_t)((mqtt_inbox_tail + 1U) % MQTT_INBOX_DEPTH);
    mqtt_inbox_count--;
    mqtt_inbox_dropped++;
  }

  (void)snprintf(mqtt_inbox[mqtt_inbox_head].topic, sizeof(mqtt_inbox[mqtt_inbox_head].topic), "%s", topic);
  (void)snprintf(mqtt_inbox[mqtt_inbox_head].payload, sizeof(mqtt_inbox[mqtt_inbox_head].payload), "%s", payload);
  mqtt_inbox_head = (uint8_t)((mqtt_inbox_head + 1U) % MQTT_INBOX_DEPTH);
  mqtt_inbox_count++;
}

static bool mqtt_inbox_pop(char *topic, uint16_t topic_len, char *payload, uint16_t payload_len) {
  if (topic == NULL || topic_len == 0U || payload == NULL || payload_len == 0U || mqtt_inbox_count == 0U) {
    return false;
  }

  (void)snprintf(topic, topic_len, "%s", mqtt_inbox[mqtt_inbox_tail].topic);
  (void)snprintf(payload, payload_len, "%s", mqtt_inbox[mqtt_inbox_tail].payload);
  memset(&mqtt_inbox[mqtt_inbox_tail], 0, sizeof(mqtt_inbox[mqtt_inbox_tail]));
  mqtt_inbox_tail = (uint8_t)((mqtt_inbox_tail + 1U) % MQTT_INBOX_DEPTH);
  mqtt_inbox_count--;
  return true;
}
