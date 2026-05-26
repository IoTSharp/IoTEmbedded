#include "Interpreter/Inc/basic_mqtt.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Config/Inc/config.h"
#include "Interpreter/Inc/basic.h"
#include "Protocol/Mqtt/Inc/mqtt_client.h"

#include "Board/Inc/bsp_hal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASIC_MQTT_NO_HANDLE 0
#define BASIC_MQTT_MAX_KEEPALIVE_SECONDS UINT16_MAX
#define BASIC_MQTT_DEFAULT_RECEIVE_TIMEOUT_MS 0U
#define BASIC_MQTT_RECEIVE_POLL_SLICE_MS 50U
#define BASIC_MQTT_ERROR_MAX 96U

static int_t basic_mqtt_active_handle = BASIC_MQTT_NO_HANDLE;
static int_t basic_mqtt_next_handle = BASIC_MQTT_NO_HANDLE;
static char basic_mqtt_last_error[BASIC_MQTT_ERROR_MAX];

static int basic_mqtt_connect(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_disconnect(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_connected(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_ping(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_subscribe(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_unsubscribe(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_publish(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_receive(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_last_error_func(struct mb_interpreter_t *s, void **l);
static int basic_sleep(struct mb_interpreter_t *s, void **l);
static int basic_ticks(struct mb_interpreter_t *s, void **l);
static void basic_mqtt_feed_heartbeat(void);
static void basic_mqtt_clear_error(void);
static int basic_mqtt_fail_int(struct mb_interpreter_t *s, void **l, const char *message);
static int basic_mqtt_fail_nil(struct mb_interpreter_t *s, void **l, const char *message);
static bool basic_mqtt_pop_handle(struct mb_interpreter_t *s, void **l, int_t *handle);
static bool basic_mqtt_handle_is_active(int_t handle);
static int_t basic_mqtt_allocate_handle(void);
static int basic_mqtt_configure_session(const char *endpoint, int_t port_arg, const char *client_id,
                                        const char *username, const char *password, int_t keepalive_arg);
static bool basic_mqtt_endpoint_is_valid(const char *endpoint);
static void basic_mqtt_format_uint(char *buffer, size_t buffer_size, unsigned int value);
static int basic_mqtt_push_message(struct mb_interpreter_t *s, void **l, const mqtt_message_t *message);
static int basic_mqtt_set_dict_string(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                      const char *value);
static int basic_mqtt_set_dict_int(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                   int_t value);
static int basic_mqtt_set_dict_bool(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                    bool value);
static void basic_mqtt_release_value(struct mb_interpreter_t *s, mb_value_t value);
static int basic_mqtt_push_nil(struct mb_interpreter_t *s, void **l);
static int basic_mqtt_push_string(struct mb_interpreter_t *s, void **l, const char *value);
static int_t basic_mqtt_u32_to_int(uint32_t value);

ErrorStatus basic_mqtt_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  basic_mqtt_active_handle = BASIC_MQTT_NO_HANDLE;
  basic_mqtt_next_handle = BASIC_MQTT_NO_HANDLE;
  basic_mqtt_clear_error();

  int result = MB_FUNC_OK;
  /* Register only the standard handle-model API. Legacy singleton MQTT_RECV or parameter-compatible calls are absent. */
  result |= mb_register_func(interpreter, "MQTT_CONNECT", basic_mqtt_connect);
  result |= mb_register_func(interpreter, "MQTT_DISCONNECT", basic_mqtt_disconnect);
  result |= mb_register_func(interpreter, "MQTT_CONNECTED", basic_mqtt_connected);
  result |= mb_register_func(interpreter, "MQTT_PING", basic_mqtt_ping);
  result |= mb_register_func(interpreter, "MQTT_PUBLISH", basic_mqtt_publish);
  result |= mb_register_func(interpreter, "MQTT_SUBSCRIBE", basic_mqtt_subscribe);
  result |= mb_register_func(interpreter, "MQTT_UNSUBSCRIBE", basic_mqtt_unsubscribe);
  result |= mb_register_func(interpreter, "MQTT_RECEIVE", basic_mqtt_receive);
  result |= mb_register_func(interpreter, "MQTT_LAST_ERROR", basic_mqtt_last_error_func);
  result |= mb_register_func(interpreter, "DELAY", basic_sleep);
  result |= mb_register_func(interpreter, "SLEEP", basic_sleep);
  result |= mb_register_func(interpreter, "TICKS", basic_ticks);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_mqtt_connect(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  char *endpoint = NULL;
  char *client_id = NULL;
  char *username = NULL;
  char *password = NULL;
  int_t port_arg = 1883;
  int_t keepalive_arg = 30;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &endpoint));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &port_arg));
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &client_id));
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &username));
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &password));
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &keepalive_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  int config_result = basic_mqtt_configure_session(endpoint, port_arg, client_id, username, password, keepalive_arg);
  if (config_result != MB_FUNC_OK) {
    return mb_push_int(s, l, 0);
  }

  mqtt_client_disconnect();
  basic_mqtt_active_handle = BASIC_MQTT_NO_HANDLE;
  if (!mqtt_client_connect()) {
    return basic_mqtt_fail_int(s, l, "MQTT connect failed");
  }

  basic_mqtt_active_handle = basic_mqtt_allocate_handle();
  basic_mqtt_clear_error();
  return mb_push_int(s, l, basic_mqtt_active_handle);
}

static int basic_mqtt_disconnect(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  mb_check(mb_attempt_close_bracket(s, l));

  mqtt_client_disconnect();
  if (handle == basic_mqtt_active_handle) {
    basic_mqtt_active_handle = BASIC_MQTT_NO_HANDLE;
  }
  basic_mqtt_clear_error();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_connected(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return mb_push_int(s, l, 0);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }

  mqtt_client_poll();
  mqtt_client_state_t state = mqtt_client_get_state();
  basic_mqtt_clear_error();
  return mb_push_int(s, l, state >= MQTT_CLIENT_STATE_SESSION_CONNECTED ? 1 : 0);
}

static int basic_mqtt_ping(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  bool ok = mqtt_client_ping();
  if (!ok) {
    return basic_mqtt_fail_int(s, l, "MQTT ping failed");
  }
  basic_mqtt_clear_error();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_subscribe(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  char *topic = NULL;
  int_t qos_arg = 0;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  mb_check(mb_pop_string(s, l, &topic));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &qos_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  if (qos_arg < 0 || qos_arg > 1) {
    return basic_mqtt_fail_int(s, l, "STM32 MQTT supports QoS 0 or 1");
  }
  bool ok = mqtt_client_subscribe_topic_ex(topic, (uint8_t)qos_arg);
  if (!ok) {
    return basic_mqtt_fail_int(s, l, "MQTT subscribe failed");
  }
  basic_mqtt_clear_error();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_unsubscribe(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  char *topic = NULL;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  mb_check(mb_pop_string(s, l, &topic));
  mb_check(mb_attempt_close_bracket(s, l));
  (void)topic;

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }

  bool ok = mqtt_client_unsubscribe_topic(topic);
  if (!ok) {
    return basic_mqtt_fail_int(s, l, "MQTT unsubscribe failed");
  }
  basic_mqtt_clear_error();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_publish(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  char *topic = NULL;
  char *payload = NULL;
  int_t qos_arg = 0;
  int_t retain_arg = 0;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  mb_check(mb_pop_string(s, l, &topic));
  mb_check(mb_pop_string(s, l, &payload));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &qos_arg));
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &retain_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_int(s, l, "MQTT handle not found");
  }
  if (qos_arg < 0 || qos_arg > 1) {
    return basic_mqtt_fail_int(s, l, "STM32 MQTT supports QoS 0 or 1");
  }

  bool ok = mqtt_client_publish_topic_ex(topic, payload, (uint8_t)qos_arg, retain_arg != 0);
  if (!ok) {
    return basic_mqtt_fail_int(s, l, "MQTT publish failed");
  }
  basic_mqtt_clear_error();
  return mb_push_int(s, l, 1);
}

static int basic_mqtt_receive(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  int_t handle = BASIC_MQTT_NO_HANDLE;
  int_t timeout_arg = (int_t)BASIC_MQTT_DEFAULT_RECEIVE_TIMEOUT_MS;
  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_mqtt_pop_handle(s, l, &handle)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_mqtt_fail_nil(s, l, "MQTT handle not found");
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &timeout_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_mqtt_handle_is_active(handle)) {
    return basic_mqtt_fail_nil(s, l, "MQTT handle not found");
  }

  uint32_t timeout_ms = timeout_arg <= 0 ? 0U : (uint32_t)timeout_arg;
  uint32_t start_ms = HAL_GetTick();
  mqtt_message_t message = {0};

  do {
    mqtt_client_poll();
    bool ok = mqtt_client_read_message_ex(&message);
    if (ok) {
      basic_mqtt_clear_error();
      return basic_mqtt_push_message(s, l, &message);
    }
    if (timeout_ms == 0U) {
      break;
    }
    uint32_t elapsed_ms = HAL_GetTick() - start_ms;
    if (elapsed_ms >= timeout_ms) {
      break;
    }
    uint32_t sleep_ms = timeout_ms - elapsed_ms;
    if (sleep_ms > BASIC_MQTT_RECEIVE_POLL_SLICE_MS) {
      sleep_ms = BASIC_MQTT_RECEIVE_POLL_SLICE_MS;
    }
    basic_mqtt_feed_heartbeat();
    HAL_Delay(sleep_ms);
  } while (true);

  basic_mqtt_clear_error();
  return basic_mqtt_push_nil(s, l);
}

static int basic_mqtt_last_error_func(struct mb_interpreter_t *s, void **l) {
  basic_mqtt_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  if (mb_has_arg(s, l)) {
    int_t handle = BASIC_MQTT_NO_HANDLE;
    if (!basic_mqtt_pop_handle(s, l, &handle)) {
      mb_check(mb_attempt_close_bracket(s, l));
      return basic_mqtt_push_string(s, l, "MQTT handle not found");
    }
    (void)handle;
  }
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_mqtt_push_string(s, l, basic_mqtt_last_error);
}

static int basic_sleep(struct mb_interpreter_t *s, void **l) {
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

static void basic_mqtt_clear_error(void) {
  basic_mqtt_last_error[0] = '\0';
}

static int basic_mqtt_fail_int(struct mb_interpreter_t *s, void **l, const char *message) {
  (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", message == NULL ? "" : message);
  return mb_push_int(s, l, 0);
}

static int basic_mqtt_fail_nil(struct mb_interpreter_t *s, void **l, const char *message) {
  (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", message == NULL ? "" : message);
  return basic_mqtt_push_nil(s, l);
}

static bool basic_mqtt_pop_handle(struct mb_interpreter_t *s, void **l, int_t *handle) {
  if (handle == NULL) {
    return false;
  }
  int_t raw_handle = BASIC_MQTT_NO_HANDLE;
  int result = mb_pop_int(s, l, &raw_handle);
  if (result != MB_FUNC_OK || raw_handle <= BASIC_MQTT_NO_HANDLE) {
    return false;
  }
  *handle = raw_handle;
  return true;
}

static bool basic_mqtt_handle_is_active(int_t handle) {
  return handle > BASIC_MQTT_NO_HANDLE && handle == basic_mqtt_active_handle &&
         mqtt_client_get_state() >= MQTT_CLIENT_STATE_SESSION_CONNECTED;
}

static int_t basic_mqtt_allocate_handle(void) {
  if (basic_mqtt_next_handle >= INT_MAX) {
    basic_mqtt_next_handle = BASIC_MQTT_NO_HANDLE;
  }
  basic_mqtt_next_handle++;
  if (basic_mqtt_next_handle <= BASIC_MQTT_NO_HANDLE) {
    basic_mqtt_next_handle = 1;
  }
  return basic_mqtt_next_handle;
}

static int basic_mqtt_configure_session(const char *endpoint, int_t port_arg, const char *client_id,
                                        const char *username, const char *password, int_t keepalive_arg) {
  if (!basic_mqtt_endpoint_is_valid(endpoint)) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT endpoint is required");
    return MB_FUNC_ERR;
  }
  if (port_arg <= 0 || port_arg > (int_t)UINT16_MAX) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT port is invalid");
    return MB_FUNC_ERR;
  }
  if (keepalive_arg < 0 || keepalive_arg > (int_t)BASIC_MQTT_MAX_KEEPALIVE_SECONDS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT keepalive is invalid");
    return MB_FUNC_ERR;
  }

  char port_text[8] = {0};
  char keepalive_text[8] = {0};
  basic_mqtt_format_uint(port_text, sizeof(port_text), (unsigned int)port_arg);
  basic_mqtt_format_uint(keepalive_text, sizeof(keepalive_text), (unsigned int)keepalive_arg);

  if (config_set_value("mqtt_ip", endpoint) != SUCCESS || config_set_value("mqtt_port", port_text) != SUCCESS ||
      config_set_value("mqtt_keepalive", keepalive_text) != SUCCESS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT config update failed");
    return MB_FUNC_ERR;
  }

  if (username != NULL && username[0] != '\0' && config_set_value("mqtt_user", username) != SUCCESS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT username is invalid");
    return MB_FUNC_ERR;
  }
  if (client_id != NULL && client_id[0] != '\0' && config_set_value("mqtt_client_id", client_id) != SUCCESS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT client id is invalid");
    return MB_FUNC_ERR;
  }
  if (password != NULL && password[0] != '\0' && config_set_value("mqtt_password", password) != SUCCESS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT password is invalid");
    return MB_FUNC_ERR;
  }
  if (config_apply_runtime() != SUCCESS) {
    (void)snprintf(basic_mqtt_last_error, sizeof(basic_mqtt_last_error), "%s", "MQTT runtime apply failed");
    return MB_FUNC_ERR;
  }
  return MB_FUNC_OK;
}

static bool basic_mqtt_endpoint_is_valid(const char *endpoint) {
  if (endpoint == NULL || endpoint[0] == '\0') {
    return false;
  }
  return strstr(endpoint, "://") == NULL;
}

static void basic_mqtt_format_uint(char *buffer, size_t buffer_size, unsigned int value) {
  if (buffer == NULL || buffer_size == 0U) {
    return;
  }
  (void)snprintf(buffer, buffer_size, "%u", value);
}

static int basic_mqtt_push_message(struct mb_interpreter_t *s, void **l, const mqtt_message_t *message) {
  if (message == NULL) {
    return basic_mqtt_push_nil(s, l);
  }
  mb_value_t dict;
  mb_make_dict(dict, NULL);
  int result = mb_init_coll(s, l, &dict);
  if (result != MB_FUNC_OK) {
    return result;
  }

  result = basic_mqtt_set_dict_string(s, l, dict, "topic", message->topic);
  if (result == MB_FUNC_OK) {
    result = basic_mqtt_set_dict_string(s, l, dict, "payload", message->payload);
  }
  if (result == MB_FUNC_OK) {
    result = basic_mqtt_set_dict_int(s, l, dict, "qos", (int_t)message->qos);
  }
  if (result == MB_FUNC_OK) {
    result = basic_mqtt_set_dict_bool(s, l, dict, "retain", message->retain);
  }
  if (result != MB_FUNC_OK) {
    mb_dispose_value(s, dict);
    return result;
  }

  return mb_push_value(s, l, dict);
}

static int basic_mqtt_set_dict_string(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                      const char *value) {
  mb_value_t key_value;
  mb_value_t value_value;
  mb_make_nil(key_value);
  mb_make_nil(value_value);

  const char *safe_key = key == NULL ? "" : key;
  const char *safe_value = value == NULL ? "" : value;
  char *key_copy = mb_memdup(safe_key, (unsigned)(strlen(safe_key) + 1U));
  char *value_copy = mb_memdup(safe_value, (unsigned)(strlen(safe_value) + 1U));
  if (key_copy == NULL || value_copy == NULL) {
    if (key_copy != NULL) {
      mb_value_t free_value;
      mb_make_string(free_value, key_copy);
      (void)mb_dispose_value(s, free_value);
    }
    if (value_copy != NULL) {
      mb_value_t free_value;
      mb_make_string(free_value, value_copy);
      (void)mb_dispose_value(s, free_value);
    }
    return MB_FUNC_ERR;
  }

  mb_make_string(key_value, key_copy);
  mb_make_string(value_value, value_copy);
  int result = mb_set_coll(s, l, dict, key_value, value_value);
  basic_mqtt_release_value(s, key_value);
  basic_mqtt_release_value(s, value_value);
  return result;
}

static int basic_mqtt_set_dict_int(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                   int_t value) {
  mb_value_t key_value;
  mb_value_t value_value;
  mb_make_nil(key_value);
  mb_make_int(value_value, value);

  const char *safe_key = key == NULL ? "" : key;
  char *key_copy = mb_memdup(safe_key, (unsigned)(strlen(safe_key) + 1U));
  if (key_copy == NULL) {
    return MB_FUNC_ERR;
  }
  mb_make_string(key_value, key_copy);
  int result = mb_set_coll(s, l, dict, key_value, value_value);
  basic_mqtt_release_value(s, key_value);
  return result;
}

static int basic_mqtt_set_dict_bool(struct mb_interpreter_t *s, void **l, mb_value_t dict, const char *key,
                                    bool value) {
  return basic_mqtt_set_dict_int(s, l, dict, key, value ? 1 : 0);
}

static void basic_mqtt_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_STRING || value.type == MB_DT_LIST || value.type == MB_DT_DICT ||
      value.type == MB_DT_ARRAY) {
    (void)mb_dispose_value(s, value);
  }
}

static int basic_mqtt_push_nil(struct mb_interpreter_t *s, void **l) {
  mb_value_t value;
  mb_make_nil(value);
  return mb_push_value(s, l, value);
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
