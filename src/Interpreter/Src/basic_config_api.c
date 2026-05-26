#include "Interpreter/Inc/basic_config_api.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Config/Inc/config.h"
#include "Interpreter/Inc/basic.h"
#include "Network/Inc/network_socket.h"

#include "Board/Inc/bsp_hal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASIC_CONFIG_TEXT_MAX 128U

static int basic_config_get(struct mb_interpreter_t *s, void **l);
static int basic_config_set(struct mb_interpreter_t *s, void **l);
static int basic_config_apply(struct mb_interpreter_t *s, void **l);
static int basic_config_save(struct mb_interpreter_t *s, void **l);
static int basic_config_reset(struct mb_interpreter_t *s, void **l);
static int basic_config_network_auto(struct mb_interpreter_t *s, void **l);
static int basic_config_network_ch395(struct mb_interpreter_t *s, void **l);
static int basic_config_network_4g(struct mb_interpreter_t *s, void **l);
static int basic_config_network_use(struct mb_interpreter_t *s, void **l);
static int basic_config_network_mode(struct mb_interpreter_t *s, void **l);
static int basic_config_network_link(struct mb_interpreter_t *s, void **l);
static int basic_config_network_ready(struct mb_interpreter_t *s, void **l);
static int basic_config_mqtt_setup_ch395(struct mb_interpreter_t *s, void **l);
static int basic_config_switch_network_mode(struct mb_interpreter_t *s, void **l, network_mode_t mode);
static bool basic_config_set_text_value(const char *key, const char *value);
static void basic_config_feed_heartbeat(void);
static int basic_config_push_string(struct mb_interpreter_t *s, void **l, const char *value);
static bool basic_config_value_to_text(mb_value_t value, char *buffer, size_t buffer_size);
static void basic_config_release_value(struct mb_interpreter_t *s, mb_value_t value);
static int_t basic_config_bool_to_int(bool value);

ErrorStatus basic_config_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "CONFIG_GET", basic_config_get);
  result |= mb_register_func(interpreter, "CONFIG_SET", basic_config_set);
  result |= mb_register_func(interpreter, "CONFIG_APPLY", basic_config_apply);
  result |= mb_register_func(interpreter, "CONFIG_SAVE", basic_config_save);
  result |= mb_register_func(interpreter, "CONFIG_RESET", basic_config_reset);
  result |= mb_register_func(interpreter, "NETWORK_AUTO", basic_config_network_auto);
  result |= mb_register_func(interpreter, "NETWORK_CH395", basic_config_network_ch395);
  result |= mb_register_func(interpreter, "NETWORK_4G", basic_config_network_4g);
  result |= mb_register_func(interpreter, "NETWORK_USE", basic_config_network_use);
  result |= mb_register_func(interpreter, "NETWORK_MODE", basic_config_network_mode);
  result |= mb_register_func(interpreter, "NETWORK_LINK", basic_config_network_link);
  result |= mb_register_func(interpreter, "NETWORK_READY", basic_config_network_ready);
  result |= mb_register_func(interpreter, "MQTT_USE_AUTO", basic_config_network_auto);
  result |= mb_register_func(interpreter, "MQTT_USE_CH395", basic_config_network_ch395);
  result |= mb_register_func(interpreter, "MQTT_USE_4G", basic_config_network_4g);
  result |= mb_register_func(interpreter, "MQTT_SETUP_CH395", basic_config_mqtt_setup_ch395);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_config_get(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  char *key = NULL;
  char *default_value = NULL;
  char value[BASIC_CONFIG_TEXT_MAX] = {0};

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &key));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &default_value));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (key != NULL && config_get_value(key, value, sizeof(value)) == SUCCESS) {
    return basic_config_push_string(s, l, value);
  }
  return basic_config_push_string(s, l, default_value == NULL ? "" : default_value);
}

static int basic_config_set(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  char *key = NULL;
  mb_value_t value;
  char value_text[BASIC_CONFIG_TEXT_MAX] = {0};
  int result = MB_FUNC_OK;

  mb_make_nil(value);
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &key));
  result = mb_pop_value(s, l, &value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (key == NULL || !basic_config_value_to_text(value, value_text, sizeof(value_text))) {
    basic_config_release_value(s, value);
    return mb_push_int(s, l, 0);
  }

  ErrorStatus status = config_set_value(key, value_text);
  basic_config_release_value(s, value);
  return mb_push_int(s, l, status == SUCCESS ? 1 : 0);
}

static int basic_config_apply(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, config_apply_runtime() == SUCCESS ? 1 : 0);
}

static int basic_config_save(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, config_write_into_eeprom() == SUCCESS ? 1 : 0);
}

static int basic_config_reset(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  config_reset_to_default();
  return mb_push_int(s, l, 1);
}

static int basic_config_network_auto(struct mb_interpreter_t *s, void **l) {
  return basic_config_switch_network_mode(s, l, NETWORK_MODE_AUTO);
}

static int basic_config_network_ch395(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  int_t persist_arg = 0;

  mb_check(mb_attempt_open_bracket(s, l));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &persist_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  return mb_push_int(s, l, config_apply_ch395q_network(persist_arg != 0) == SUCCESS ? 1 : 0);
}

static int basic_config_network_4g(struct mb_interpreter_t *s, void **l) {
  return basic_config_switch_network_mode(s, l, NETWORK_MODE_AIR724UG);
}

static int basic_config_network_use(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  char *mode = NULL;
  int_t persist_arg = 0;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &mode));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &persist_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  network_mode_t network_mode = NETWORK_MODE_AUTO;
  if (!config_parse_network_mode(mode, &network_mode)) {
    return mb_push_int(s, l, 0);
  }
  if (network_mode == NETWORK_MODE_CH395Q) {
    return mb_push_int(s, l, config_apply_ch395q_network(persist_arg != 0) == SUCCESS ? 1 : 0);
  }
  return mb_push_int(s, l, config_apply_network_mode(network_mode, persist_arg != 0) == SUCCESS ? 1 : 0);
}

static int basic_config_mqtt_setup_ch395(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  char *mqtt_host = NULL;
  char *mqtt_user = NULL;
  char *mqtt_password = NULL;
  char *local_ip = NULL;
  char *gateway_ip = NULL;
  char *mask_ip = NULL;
  int_t mqtt_port = 0;
  int_t persist_arg = 0;
  char port_text[BASIC_CONFIG_TEXT_MAX] = {0};

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &mqtt_host));
  mb_check(mb_pop_int(s, l, &mqtt_port));
  mb_check(mb_pop_string(s, l, &mqtt_user));
  mb_check(mb_pop_string(s, l, &mqtt_password));
  mb_check(mb_pop_string(s, l, &local_ip));
  mb_check(mb_pop_string(s, l, &gateway_ip));
  mb_check(mb_pop_string(s, l, &mask_ip));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &persist_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (mqtt_host == NULL || mqtt_user == NULL || mqtt_password == NULL || local_ip == NULL || gateway_ip == NULL ||
      mask_ip == NULL || mqtt_port <= 0 || mqtt_port > (int_t)UINT16_MAX) {
    return mb_push_int(s, l, 0);
  }

  (void)snprintf(port_text, sizeof(port_text), "%u", (unsigned int)mqtt_port);
  if (!basic_config_set_text_value("mqtt_ip", mqtt_host) || !basic_config_set_text_value("mqtt_port", port_text) ||
      !basic_config_set_text_value("mqtt_user", mqtt_user) ||
      !basic_config_set_text_value("mqtt_password", mqtt_password) ||
      !basic_config_set_text_value("local_ip", local_ip) || !basic_config_set_text_value("gateway", gateway_ip) ||
      !basic_config_set_text_value("mask", mask_ip)) {
    return mb_push_int(s, l, 0);
  }

  return mb_push_int(s, l, config_apply_ch395q_network(persist_arg != 0) == SUCCESS ? 1 : 0);
}

static int basic_config_network_mode(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_config_push_string(s, l, config_network_mode_name(config_get_network_mode()));
}

static int basic_config_network_link(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_config_push_string(s, l, network_socket_active_link_name());
}

static int basic_config_network_ready(struct mb_interpreter_t *s, void **l) {
  basic_config_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, basic_config_bool_to_int(network_socket_active_link_ready()));
}

static int basic_config_switch_network_mode(struct mb_interpreter_t *s, void **l, network_mode_t mode) {
  basic_config_feed_heartbeat();
  int_t persist_arg = 0;

  mb_check(mb_attempt_open_bracket(s, l));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &persist_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  return mb_push_int(s, l, config_apply_network_mode(mode, persist_arg != 0) == SUCCESS ? 1 : 0);
}

static void basic_config_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}

static int basic_config_push_string(struct mb_interpreter_t *s, void **l, const char *value) {
  const char *safe_value = value == NULL ? "" : value;
  char *copy = mb_memdup(safe_value, (unsigned)(strlen(safe_value) + 1U));
  if (copy == NULL) {
    return MB_FUNC_ERR;
  }
  return mb_push_string(s, l, copy);
}

static bool basic_config_value_to_text(mb_value_t value, char *buffer, size_t buffer_size) {
  int written = 0;
  if (buffer == NULL || buffer_size == 0U) {
    return false;
  }

  buffer[0] = '\0';
  switch (value.type) {
  case MB_DT_STRING:
    if (value.value.string == NULL) {
      return true;
    }
    written = snprintf(buffer, buffer_size, "%s", value.value.string);
    break;
  case MB_DT_INT:
    written = snprintf(buffer, buffer_size, "%ld", (long)value.value.integer);
    break;
  case MB_DT_REAL:
    if (value.value.float_point >= (real_t)INT_MIN && value.value.float_point <= (real_t)INT_MAX &&
        (real_t)((int_t)value.value.float_point) == value.value.float_point) {
      written = snprintf(buffer, buffer_size, "%ld", (long)((int_t)value.value.float_point));
    } else {
      written = snprintf(buffer, buffer_size, "%g", (double)value.value.float_point);
    }
    break;
  case MB_DT_NIL:
    return true;
  default:
    return false;
  }

  return written >= 0 && (size_t)written < buffer_size;
}

static void basic_config_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_ARRAY
#ifdef MB_ENABLE_USERTYPE_REF
      || value.type == MB_DT_USERTYPE_REF
#endif
  ) {
    (void)mb_dispose_value(s, value);
  }
}

static bool basic_config_set_text_value(const char *key, const char *value) {
  return key != NULL && value != NULL && config_set_value(key, value) == SUCCESS;
}

static int_t basic_config_bool_to_int(bool value) {
  return value ? 1 : 0;
}
