#include "Interpreter/Inc/basic_mqtt.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Interpreter/Inc/basic.h"
#include "Protocol/Mqtt/Inc/mqtt_client.h"

#include "stm32f1xx_hal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static char basic_mqtt_last_topic[MQTT_TOPIC_MAX_LEN];
static char basic_mqtt_last_payload[MQTT_PAYLOAD_MAX_LEN];

static int basic_mqtt_connect(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_disconnect(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_connected(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_ready(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_state(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_maintain(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_ping(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_poll(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_subscribe(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_publish(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_build_topic(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_message_count(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_recv(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_recv_topic(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_recv_payload(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_overflow(struct mb_interpreter_t *s, void **l);
static int basic_delay(struct mb_interpreter_t *s, void **l);
static int basic_ticks(struct mb_interpreter_t *s, void **l);
static void basic_mqtt_feed_heartbeat(void);
static int basic_mqtt_push_string(struct mb_interpreter_t *s, void **l, const char *value);
static int_t basic_mqtt_u32_to_int(uint32_t value);

ErrorStatus basic_mqtt_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  memset(basic_mqtt_last_topic, 0, sizeof(basic_mqtt_last_topic));
  memset(basic_mqtt_last_payload, 0, sizeof(basic_mqtt_last_payload));

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "MQTT_CONNECT", basic_mqtt_connect);
  result |= mb_register_func(interpreter, "MQTT_DISCONNECT", basic_mqtt_disconnect);
  result |= mb_register_func(interpreter, "MQTT_CONNECTED", basic_mqtt_connected);
  result |= mb_register_func(interpreter, "MQTT_READY", basic_mqtt_ready);
  result |= mb_register_func(interpreter, "MQTT_STATE", basic_mqtt_state);
  result |= mb_register_func(interpreter, "MQTT_MAINTAIN", basic_mqtt_maintain);
  result |= mb_register_func(interpreter, "MQTT_PING", basic_mqtt_ping);
  result |= mb_register_func(interpreter, "MQTT_POLL", basic_mqtt_poll);
  result |= mb_register_func(interpreter, "MQTT_SUBSCRIBE", basic_mqtt_subscribe);
  result |= mb_register_func(interpreter, "MQTT_PUBLISH", basic_mqtt_publish);
  result |= mb_register_func(interpreter, "MQTT_BUILD_TOPIC", basic_mqtt_build_topic);
  result |= mb_register_func(interpreter, "MQTT_MESSAGE_COUNT", basic_mqtt_message_count);
  result |= mb_register_func(interpreter, "MQTT_RECV", basic_mqtt_recv);
  result |= mb_register_func(interpreter, "MQTT_RECV_TOPIC", basic_mqtt_recv_topic);
  result |= mb_register_func(interpreter, "MQTT_RECV_PAYLOAD", basic_mqtt_recv_payload);
  result |= mb_register_func(interpreter, "MQTT_OVERFLOW", basic_mqtt_overflow);
  result |= mb_register_func(interpreter, "BASIC_DELAY", basic_delay);
  result |= mb_register_func(interpreter, "BASIC_TICKS", basic_ticks);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_mqtt_connect(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_connect() ? 1 : 0);
}

static int basic_mqtt_disconnect(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  mqtt_client_disconnect();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_connected(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  mqtt_client_state_t state = mqtt_client_get_state();
  return mb_push_int(s, l, state >= MQTT_CLIENT_STATE_SESSION_CONNECTED ? 1 : 0);
}

static int basic_mqtt_ready(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_is_ready() ? 1 : 0);
}

static int basic_mqtt_state(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, (int_t)mqtt_client_get_state());
}

static int basic_mqtt_maintain(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t reconnect_interval_ms = 0;
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_int(s, l, &reconnect_interval_ms));
  mb_check(mb_attempt_close_bracket(s, l));
  if (reconnect_interval_ms < 0) {
    reconnect_interval_ms = 0;
  }
  bool connected = mqtt_client_maintain_connection(HAL_GetTick(), (uint32_t)reconnect_interval_ms);
  return mb_push_int(s, l, connected ? 1 : 0);
}

static int basic_mqtt_ping(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_ping() ? 1 : 0);
}

static int basic_mqtt_poll(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  mqtt_client_poll();
  return mb_push_int(s, l, mqtt_client_pending_messages());
}

static int basic_mqtt_subscribe(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  char *topic = NULL;
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &topic));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_subscribe_topic(topic) ? 1 : 0);
}

static int basic_mqtt_publish(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  char *topic = NULL;
  char *payload = NULL;
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &topic));
  mb_check(mb_pop_string(s, l, &payload));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_publish_topic(topic, payload) ? 1 : 0);
}

static int basic_mqtt_build_topic(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  char *prefix = NULL;
  char topic[MQTT_TOPIC_MAX_LEN] = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &prefix));
  mb_check(mb_attempt_close_bracket(s, l));
  mqtt_client_build_topic(topic, sizeof(topic), prefix);
  return basic_mqtt_push_string(s, l, topic);
}

static int basic_mqtt_message_count(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, mqtt_client_pending_messages());
}

static int basic_mqtt_recv(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  memset(basic_mqtt_last_topic, 0, sizeof(basic_mqtt_last_topic));
  memset(basic_mqtt_last_payload, 0, sizeof(basic_mqtt_last_payload));
  bool ok = mqtt_client_read_message(basic_mqtt_last_topic, sizeof(basic_mqtt_last_topic), basic_mqtt_last_payload,
                                     sizeof(basic_mqtt_last_payload));
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_mqtt_recv_topic(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_mqtt_push_string(s, l, basic_mqtt_last_topic);
}

static int basic_mqtt_recv_payload(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_mqtt_push_string(s, l, basic_mqtt_last_payload);
}

static int basic_mqtt_overflow(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, basic_mqtt_u32_to_int(mqtt_client_get_dropped_message_count()));
}

static int basic_delay(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t delay_ms = 0;
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_int(s, l, &delay_ms));
  mb_check(mb_attempt_close_bracket(s, l));
  if (delay_ms < 0) {
    delay_ms = 0;
  }

  uint32_t remaining_ms = (uint32_t)delay_ms;
  do {
    uint32_t slice_ms = remaining_ms > 1000U ? 1000U : remaining_ms;
    basic_mqtt_feed_heartbeat();
    (void)bsp_watchdog_refresh();
    HAL_Delay(slice_ms);
    remaining_ms -= slice_ms;
  } while (remaining_ms > 0U);
  return mb_push_int(s, l, 1);
}

static int basic_ticks(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, basic_mqtt_u32_to_int(HAL_GetTick()));
}

static void basic_mqtt_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}

static int basic_mqtt_push_string(struct mb_interpreter_t *s, void **l, const char *value) {
  const char *safe_value = value == NULL ? "" : value;
  char *copy = mb_memdup(safe_value, (unsigned)(strlen(safe_value) + 1U));
  if (copy == NULL) {
    return MB_FUNC_ERR;
  }
  return mb_push_string(s, l, copy);
}

static int_t basic_mqtt_u32_to_int(uint32_t value) {
  return value > (uint32_t)INT_MAX ? (int_t)INT_MAX : (int_t)value;
}
