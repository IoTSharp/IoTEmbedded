#include "Interpreter/Inc/basic_json.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Interpreter/Inc/basic.h"
#include "ThirdParty/Parson/parson.h"

#include "stm32f1xx_hal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BASIC_JSON_MAGIC 0x4A534F4EU

typedef struct {
  uint32_t magic;
  JSON_Value *value;
} basic_json_handle_t;

static int basic_json_parse(struct mb_interpreter_t *s, void **l);
static int basic_json_valid(struct mb_interpreter_t *s, void **l);
static int basic_json_object(struct mb_interpreter_t *s, void **l);
static int basic_json_array(struct mb_interpreter_t *s, void **l);
static int basic_json_string(struct mb_interpreter_t *s, void **l);
static int basic_json_number(struct mb_interpreter_t *s, void **l);
static int basic_json_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_null(struct mb_interpreter_t *s, void **l);
static int basic_json_type(struct mb_interpreter_t *s, void **l);
static int basic_json_stringify(struct mb_interpreter_t *s, void **l);
static int basic_json_has(struct mb_interpreter_t *s, void **l);
static int basic_json_count(struct mb_interpreter_t *s, void **l);
static int basic_json_key(struct mb_interpreter_t *s, void **l);
static int basic_json_get(struct mb_interpreter_t *s, void **l);
static int basic_json_get_string(struct mb_interpreter_t *s, void **l);
static int basic_json_get_number(struct mb_interpreter_t *s, void **l);
static int basic_json_get_int(struct mb_interpreter_t *s, void **l);
static int basic_json_get_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_at(struct mb_interpreter_t *s, void **l);
static int basic_json_at_string(struct mb_interpreter_t *s, void **l);
static int basic_json_at_number(struct mb_interpreter_t *s, void **l);
static int basic_json_at_int(struct mb_interpreter_t *s, void **l);
static int basic_json_at_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_set_string(struct mb_interpreter_t *s, void **l);
static int basic_json_set_number(struct mb_interpreter_t *s, void **l);
static int basic_json_set_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_set_null(struct mb_interpreter_t *s, void **l);
static int basic_json_set_json(struct mb_interpreter_t *s, void **l);
static int basic_json_set_at_string(struct mb_interpreter_t *s, void **l);
static int basic_json_set_at_number(struct mb_interpreter_t *s, void **l);
static int basic_json_set_at_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_set_at_null(struct mb_interpreter_t *s, void **l);
static int basic_json_set_at_json(struct mb_interpreter_t *s, void **l);
static int basic_json_append_string(struct mb_interpreter_t *s, void **l);
static int basic_json_append_number(struct mb_interpreter_t *s, void **l);
static int basic_json_append_bool(struct mb_interpreter_t *s, void **l);
static int basic_json_append_null(struct mb_interpreter_t *s, void **l);
static int basic_json_append_json(struct mb_interpreter_t *s, void **l);
static int basic_json_remove(struct mb_interpreter_t *s, void **l);
static int basic_json_remove_at(struct mb_interpreter_t *s, void **l);
static int basic_json_clear(struct mb_interpreter_t *s, void **l);

static void *basic_json_clone_handle(struct mb_interpreter_t *s, void *value);
static void basic_json_destroy_handle(struct mb_interpreter_t *s, void *value);
static bool basic_json_handle_is_valid(const basic_json_handle_t *handle);
static bool basic_json_pop_handle_value(struct mb_interpreter_t *s, void **l, mb_value_t *value,
                                        basic_json_handle_t **handle);
static void basic_json_release_value(struct mb_interpreter_t *s, mb_value_t value);
static int basic_json_push_nil(struct mb_interpreter_t *s, void **l);
static int basic_json_push_string(struct mb_interpreter_t *s, void **l, const char *value);
static int basic_json_push_handle(struct mb_interpreter_t *s, void **l, JSON_Value *value);
static int basic_json_push_handle_copy(struct mb_interpreter_t *s, void **l, const JSON_Value *value);
static int basic_json_push_real(struct mb_interpreter_t *s, void **l, double value);
static int basic_json_push_int(struct mb_interpreter_t *s, void **l, int_t value);
static int basic_json_push_bool(struct mb_interpreter_t *s, void **l, bool value);
static int basic_json_push_count(struct mb_interpreter_t *s, void **l, size_t value);
static int basic_json_push_type(struct mb_interpreter_t *s, void **l, JSON_Value_Type value);
static const JSON_Value *basic_json_resolve_value(const JSON_Value *root, const char *path);
static JSON_Object *basic_json_resolve_object(const JSON_Value *root, const char *path);
static JSON_Array *basic_json_resolve_array(const JSON_Value *root, const char *path);
static bool basic_json_replace_root(basic_json_handle_t *handle, JSON_Value *value);
static bool basic_json_read_string(const JSON_Value *value, const char **text);
static bool basic_json_read_number(const JSON_Value *value, double *number);
static bool basic_json_read_bool(const JSON_Value *value, bool *value_out);
static bool basic_json_index_from_int(int_t value, size_t *index);
static int_t basic_json_size_to_int(size_t value);
static void basic_json_feed_heartbeat(void);

ErrorStatus basic_json_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "JSON_PARSE", basic_json_parse);
  result |= mb_register_func(interpreter, "JSON_VALID", basic_json_valid);
  result |= mb_register_func(interpreter, "JSON_OBJECT", basic_json_object);
  result |= mb_register_func(interpreter, "JSON_ARRAY", basic_json_array);
  result |= mb_register_func(interpreter, "JSON_STRING", basic_json_string);
  result |= mb_register_func(interpreter, "JSON_NUMBER", basic_json_number);
  result |= mb_register_func(interpreter, "JSON_BOOL", basic_json_bool);
  result |= mb_register_func(interpreter, "JSON_NULL", basic_json_null);
  result |= mb_register_func(interpreter, "JSON_TYPE", basic_json_type);
  result |= mb_register_func(interpreter, "JSON_STRINGIFY", basic_json_stringify);
  result |= mb_register_func(interpreter, "JSON_HAS", basic_json_has);
  result |= mb_register_func(interpreter, "JSON_COUNT", basic_json_count);
  result |= mb_register_func(interpreter, "JSON_KEY", basic_json_key);
  result |= mb_register_func(interpreter, "JSON_GET", basic_json_get);
  result |= mb_register_func(interpreter, "JSON_GET_STRING", basic_json_get_string);
  result |= mb_register_func(interpreter, "JSON_GET_NUMBER", basic_json_get_number);
  result |= mb_register_func(interpreter, "JSON_GET_INT", basic_json_get_int);
  result |= mb_register_func(interpreter, "JSON_GET_BOOL", basic_json_get_bool);
  result |= mb_register_func(interpreter, "JSON_AT", basic_json_at);
  result |= mb_register_func(interpreter, "JSON_AT_STRING", basic_json_at_string);
  result |= mb_register_func(interpreter, "JSON_AT_NUMBER", basic_json_at_number);
  result |= mb_register_func(interpreter, "JSON_AT_INT", basic_json_at_int);
  result |= mb_register_func(interpreter, "JSON_AT_BOOL", basic_json_at_bool);
  result |= mb_register_func(interpreter, "JSON_SET_STRING", basic_json_set_string);
  result |= mb_register_func(interpreter, "JSON_SET_NUMBER", basic_json_set_number);
  result |= mb_register_func(interpreter, "JSON_SET_BOOL", basic_json_set_bool);
  result |= mb_register_func(interpreter, "JSON_SET_NULL", basic_json_set_null);
  result |= mb_register_func(interpreter, "JSON_SET_JSON", basic_json_set_json);
  result |= mb_register_func(interpreter, "JSON_SET_AT_STRING", basic_json_set_at_string);
  result |= mb_register_func(interpreter, "JSON_SET_AT_NUMBER", basic_json_set_at_number);
  result |= mb_register_func(interpreter, "JSON_SET_AT_BOOL", basic_json_set_at_bool);
  result |= mb_register_func(interpreter, "JSON_SET_AT_NULL", basic_json_set_at_null);
  result |= mb_register_func(interpreter, "JSON_SET_AT_JSON", basic_json_set_at_json);
  result |= mb_register_func(interpreter, "JSON_APPEND_STRING", basic_json_append_string);
  result |= mb_register_func(interpreter, "JSON_APPEND_NUMBER", basic_json_append_number);
  result |= mb_register_func(interpreter, "JSON_APPEND_BOOL", basic_json_append_bool);
  result |= mb_register_func(interpreter, "JSON_APPEND_NULL", basic_json_append_null);
  result |= mb_register_func(interpreter, "JSON_APPEND_JSON", basic_json_append_json);
  result |= mb_register_func(interpreter, "JSON_REMOVE", basic_json_remove);
  result |= mb_register_func(interpreter, "JSON_REMOVE_AT", basic_json_remove_at);
  result |= mb_register_func(interpreter, "JSON_CLEAR", basic_json_clear);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_json_parse(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  char *text = NULL;
  JSON_Value *value = NULL;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));

  value = json_parse_string(text == NULL ? "" : text);
  if (value == NULL) {
    return basic_json_push_nil(s, l);
  }
  return basic_json_push_handle(s, l, value);
}

static int basic_json_valid(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  char *text = NULL;
  JSON_Value *value = NULL;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));

  value = json_parse_string(text == NULL ? "" : text);
  if (value != NULL) {
    json_value_free(value);
  }
  return mb_push_int(s, l, value != NULL ? 1 : 0);
}

static int basic_json_object(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_object());
}

static int basic_json_array(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_array());
}

static int basic_json_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  char *text = NULL;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_string(text == NULL ? "" : text));
}

static int basic_json_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  real_t number = 0.0;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_real(s, l, &number));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_number((double)number));
}

static int basic_json_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  int_t value = 0;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_int(s, l, &value));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_boolean(value != 0 ? 1 : 0));
}

static int basic_json_null(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return basic_json_push_handle(s, l, json_value_init_null());
}

static int basic_json_type(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  const JSON_Value *target = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &path));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, JSONError);
  }
  target = basic_json_resolve_value(handle->value, path);
  basic_json_release_value(s, handle_value);
  return basic_json_push_type(s, l, target == NULL ? JSONError : json_value_get_type(target));
}

static int basic_json_stringify(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  const JSON_Value *target = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &path));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, "");
  }
  target = basic_json_resolve_value(handle->value, path);
  if (target == NULL) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, "");
  }

  char *serialized = json_serialize_to_string(target);
  if (serialized == NULL) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, "");
  }

  result = basic_json_push_string(s, l, serialized);
  json_free_serialized_string(serialized);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_has(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  const JSON_Value *target = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }
  target = basic_json_resolve_value(handle->value, path);
  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, target != NULL ? 1 : 0);
}

static int basic_json_count(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  const JSON_Value *target = NULL;
  size_t count = 0U;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &path));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_count(s, l, 0U);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (target != NULL) {
    switch (json_value_get_type(target)) {
    case JSONObject:
      count = json_object_get_count(json_value_get_object(target));
      break;
    case JSONArray:
      count = json_array_get_count(json_value_get_array(target));
      break;
    default:
      count = 0U;
      break;
    }
  }

  basic_json_release_value(s, handle_value);
  return basic_json_push_count(s, l, count);
}

static int basic_json_key(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  JSON_Object *object = NULL;
  const char *key = NULL;
  size_t index = 0U;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, "");
  }

  object = basic_json_resolve_object(handle->value, path);
  if (object != NULL) {
    key = json_object_get_name(object, index);
  }

  basic_json_release_value(s, handle_value);
  return basic_json_push_string(s, l, key == NULL ? "" : key);
}

static int basic_json_get(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  const JSON_Value *target = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_nil(s, l);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (target == NULL) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_nil(s, l);
  }

  result = basic_json_push_handle_copy(s, l, target);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_get_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  char *default_text = NULL;
  const JSON_Value *target = NULL;
  const char *text = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &default_text));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, default_text == NULL ? "" : default_text);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (!basic_json_read_string(target, &text)) {
    text = default_text == NULL ? "" : default_text;
  }

  result = basic_json_push_string(s, l, text);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_get_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  real_t default_number = 0.0;
  bool has_default = false;
  const JSON_Value *target = NULL;
  double number = 0.0;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_real(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_real(s, l, default_number);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (!basic_json_read_number(target, &number)) {
    number = (double)(has_default ? default_number : 0.0);
  }

  result = basic_json_push_real(s, l, (real_t)number);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_get_int(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t default_number = 0;
  bool has_default = false;
  const JSON_Value *target = NULL;
  int_t value = 0;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_int(s, l, default_number);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (target != NULL) {
    switch (json_value_get_type(target)) {
    case JSONNumber:
      value = (int_t)json_value_get_number(target);
      break;
    case JSONBoolean:
      value = json_value_get_boolean(target) != 0 ? 1 : 0;
      break;
    default:
      value = has_default ? default_number : 0;
      break;
    }
  } else {
    value = has_default ? default_number : 0;
  }

  result = basic_json_push_int(s, l, value);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_get_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t default_number = 0;
  bool has_default = false;
  const JSON_Value *target = NULL;
  bool value = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_bool(s, l, default_number != 0);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (!basic_json_read_bool(target, &value)) {
    value = has_default ? (default_number != 0) : false;
  }

  result = basic_json_push_bool(s, l, value);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_at(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  const JSON_Value *target = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_nil(s, l);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    target = json_array_get_value(array, index);
  }

  if (target == NULL) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_nil(s, l);
  }

  result = basic_json_push_handle_copy(s, l, target);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_at_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  char *default_text = NULL;
  size_t index = 0U;
  JSON_Array *array = NULL;
  const JSON_Value *target = NULL;
  const char *text = NULL;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &default_text));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_string(s, l, default_text == NULL ? "" : default_text);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    target = json_array_get_value(array, index);
  }
  if (!basic_json_read_string(target, &text)) {
    text = default_text == NULL ? "" : default_text;
  }

  result = basic_json_push_string(s, l, text);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_at_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  real_t default_number = 0.0;
  bool has_default = false;
  size_t index = 0U;
  JSON_Array *array = NULL;
  const JSON_Value *target = NULL;
  double number = 0.0;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_real(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_real(s, l, default_number);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    target = json_array_get_value(array, index);
  }
  if (!basic_json_read_number(target, &number)) {
    number = has_default ? (double)default_number : 0.0;
  }

  result = basic_json_push_real(s, l, (real_t)number);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_at_int(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  int_t default_number = 0;
  bool has_default = false;
  size_t index = 0U;
  JSON_Array *array = NULL;
  const JSON_Value *target = NULL;
  int_t value = 0;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_int(s, l, default_number);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    target = json_array_get_value(array, index);
  }
  if (target != NULL) {
    switch (json_value_get_type(target)) {
    case JSONNumber:
      value = (int_t)json_value_get_number(target);
      break;
    case JSONBoolean:
      value = json_value_get_boolean(target) != 0 ? 1 : 0;
      break;
    default:
      value = has_default ? default_number : 0;
      break;
    }
  } else {
    value = has_default ? default_number : 0;
  }

  result = basic_json_push_int(s, l, value);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_at_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  int_t default_number = 0;
  bool has_default = false;
  size_t index = 0U;
  JSON_Array *array = NULL;
  const JSON_Value *target = NULL;
  bool value = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &default_number));
    has_default = true;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return basic_json_push_bool(s, l, default_number != 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    target = json_array_get_value(array, index);
  }
  if (!basic_json_read_bool(target, &value)) {
    value = has_default ? (default_number != 0) : false;
  }

  result = basic_json_push_bool(s, l, value);
  basic_json_release_value(s, handle_value);
  return result;
}

static int basic_json_set_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  char *text = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  if (handle->value != NULL) {
    if (path == NULL || path[0] == '\0') {
      ok = basic_json_replace_root(handle, json_value_init_string(text == NULL ? "" : text));
    } else {
      JSON_Object *object = json_value_get_object(handle->value);
      ok = object != NULL && json_object_dotset_string(object, path, text == NULL ? "" : text) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  real_t number = 0.0;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_real(s, l, &number));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  if (handle->value != NULL) {
    if (path == NULL || path[0] == '\0') {
      ok = basic_json_replace_root(handle, json_value_init_number((double)number));
    } else {
      JSON_Object *object = json_value_get_object(handle->value);
      ok = object != NULL && json_object_dotset_number(object, path, (double)number) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t raw_value = 0;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &raw_value));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  if (handle->value != NULL) {
    if (path == NULL || path[0] == '\0') {
      ok = basic_json_replace_root(handle, json_value_init_boolean(raw_value != 0 ? 1 : 0));
    } else {
      JSON_Object *object = json_value_get_object(handle->value);
      ok = object != NULL && json_object_dotset_boolean(object, path, raw_value != 0 ? 1 : 0) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_null(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  if (handle->value != NULL) {
    if (path == NULL || path[0] == '\0') {
      ok = basic_json_replace_root(handle, json_value_init_null());
    } else {
      JSON_Object *object = json_value_get_object(handle->value);
      ok = object != NULL && json_object_dotset_null(object, path) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_json(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t target_value;
  mb_value_t source_value;
  basic_json_handle_t *target_handle = NULL;
  basic_json_handle_t *source_handle = NULL;
  char *path = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(target_value);
  mb_make_nil(source_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &target_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  result = mb_pop_value(s, l, &source_value);
  if (result != MB_FUNC_OK) {
    basic_json_release_value(s, target_value);
    return result;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &target_value, &target_handle) ||
      !basic_json_pop_handle_value(s, l, &source_value, &source_handle)) {
    basic_json_release_value(s, target_value);
    basic_json_release_value(s, source_value);
    return mb_push_int(s, l, 0);
  }

  if (target_handle->value != NULL && source_handle->value != NULL) {
    JSON_Value *copy = json_value_deep_copy(source_handle->value);
    if (copy != NULL) {
      if (path == NULL || path[0] == '\0') {
        ok = basic_json_replace_root(target_handle, copy);
      } else {
        JSON_Object *object = json_value_get_object(target_handle->value);
        ok = object != NULL && json_object_dotset_value(object, path, copy) == JSONSuccess;
        if (!ok) {
          json_value_free(copy);
        }
      }
    }
  }

  basic_json_release_value(s, target_value);
  basic_json_release_value(s, source_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_at_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  char *text = NULL;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    size_t count = json_array_get_count(array);
    if (index < count) {
      ok = json_array_replace_string(array, index, text == NULL ? "" : text) == JSONSuccess;
    } else if (index == count) {
      ok = json_array_append_string(array, text == NULL ? "" : text) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_at_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  real_t number = 0.0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_pop_real(s, l, &number));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    size_t count = json_array_get_count(array);
    if (index < count) {
      ok = json_array_replace_number(array, index, (double)number) == JSONSuccess;
    } else if (index == count) {
      ok = json_array_append_number(array, (double)number) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_at_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  int_t raw_value = 0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_pop_int(s, l, &raw_value));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    size_t count = json_array_get_count(array);
    if (index < count) {
      ok = json_array_replace_boolean(array, index, raw_value != 0 ? 1 : 0) == JSONSuccess;
    } else if (index == count) {
      ok = json_array_append_boolean(array, raw_value != 0 ? 1 : 0) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_at_null(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    size_t count = json_array_get_count(array);
    if (index < count) {
      ok = json_array_replace_null(array, index) == JSONSuccess;
    } else if (index == count) {
      ok = json_array_append_null(array) == JSONSuccess;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_set_at_json(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t target_value;
  mb_value_t source_value;
  basic_json_handle_t *target_handle = NULL;
  basic_json_handle_t *source_handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(target_value);
  mb_make_nil(source_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &target_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  result = mb_pop_value(s, l, &source_value);
  if (result != MB_FUNC_OK) {
    basic_json_release_value(s, target_value);
    return result;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &target_value, &target_handle) ||
      !basic_json_pop_handle_value(s, l, &source_value, &source_handle)) {
    basic_json_release_value(s, target_value);
    basic_json_release_value(s, source_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(target_handle->value, path);
  if (array != NULL && source_handle->value != NULL) {
    JSON_Value *copy = json_value_deep_copy(source_handle->value);
    if (copy != NULL) {
      size_t count = json_array_get_count(array);
      if (index < count) {
        ok = json_array_replace_value(array, index, copy) == JSONSuccess;
        if (!ok) {
          json_value_free(copy);
        }
      } else if (index == count) {
        ok = json_array_append_value(array, copy) == JSONSuccess;
        if (!ok) {
          json_value_free(copy);
        }
      } else {
        json_value_free(copy);
      }
    }
  }

  basic_json_release_value(s, target_value);
  basic_json_release_value(s, source_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_append_string(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  char *text = NULL;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_string(s, l, &text));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    ok = json_array_append_string(array, text == NULL ? "" : text) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_append_number(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  real_t number = 0.0;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_real(s, l, &number));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    ok = json_array_append_number(array, (double)number) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_append_bool(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t raw_value = 0;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &raw_value));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    ok = json_array_append_boolean(array, raw_value != 0 ? 1 : 0) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_append_null(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    ok = json_array_append_null(array) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_append_json(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t target_value;
  mb_value_t source_value;
  basic_json_handle_t *target_handle = NULL;
  basic_json_handle_t *source_handle = NULL;
  char *path = NULL;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(target_value);
  mb_make_nil(source_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &target_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  result = mb_pop_value(s, l, &source_value);
  if (result != MB_FUNC_OK) {
    basic_json_release_value(s, target_value);
    return result;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &target_value, &target_handle) ||
      !basic_json_pop_handle_value(s, l, &source_value, &source_handle)) {
    basic_json_release_value(s, target_value);
    basic_json_release_value(s, source_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(target_handle->value, path);
  if (array != NULL && source_handle->value != NULL) {
    JSON_Value *copy = json_value_deep_copy(source_handle->value);
    if (copy != NULL) {
      ok = json_array_append_value(array, copy) == JSONSuccess;
      if (!ok) {
        json_value_free(copy);
      }
    }
  }

  basic_json_release_value(s, target_value);
  basic_json_release_value(s, source_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_remove(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  JSON_Object *object = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_attempt_close_bracket(s, l));

  if (path == NULL || path[0] == '\0' ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  object = json_value_get_object(handle->value);
  if (object != NULL) {
    ok = json_object_dotremove(object, path) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_remove_at(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  int_t index_arg = 0;
  size_t index = 0U;
  JSON_Array *array = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  mb_check(mb_pop_string(s, l, &path));
  mb_check(mb_pop_int(s, l, &index_arg));
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_index_from_int(index_arg, &index) ||
      !basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  array = basic_json_resolve_array(handle->value, path);
  if (array != NULL) {
    ok = json_array_remove(array, index) == JSONSuccess;
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_json_clear(struct mb_interpreter_t *s, void **l) {
  basic_json_feed_heartbeat();
  mb_value_t handle_value;
  basic_json_handle_t *handle = NULL;
  char *path = NULL;
  JSON_Value *target = NULL;
  bool ok = false;
  int result = MB_FUNC_OK;

  mb_make_nil(handle_value);
  mb_check(mb_attempt_open_bracket(s, l));
  result = mb_pop_value(s, l, &handle_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_string(s, l, &path));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_json_pop_handle_value(s, l, &handle_value, &handle)) {
    basic_json_release_value(s, handle_value);
    return mb_push_int(s, l, 0);
  }

  target = basic_json_resolve_value(handle->value, path);
  if (target != NULL) {
    switch (json_value_get_type(target)) {
    case JSONObject:
      ok = json_object_clear(json_value_get_object(target)) == JSONSuccess;
      break;
    case JSONArray:
      ok = json_array_clear(json_value_get_array(target)) == JSONSuccess;
      break;
    default:
      ok = false;
      break;
    }
  }

  basic_json_release_value(s, handle_value);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static void *basic_json_clone_handle(struct mb_interpreter_t *s, void *value) {
  basic_json_handle_t *source = (basic_json_handle_t *)value;
  basic_json_handle_t *copy = NULL;
  JSON_Value *json_copy = NULL;

  (void)s;
  if (!basic_json_handle_is_valid(source) || source->value == NULL) {
    return NULL;
  }

  json_copy = json_value_deep_copy(source->value);
  if (json_copy == NULL) {
    return NULL;
  }

  copy = (basic_json_handle_t *)malloc(sizeof(*copy));
  if (copy == NULL) {
    json_value_free(json_copy);
    return NULL;
  }

  copy->magic = BASIC_JSON_MAGIC;
  copy->value = json_copy;
  return copy;
}

static void basic_json_destroy_handle(struct mb_interpreter_t *s, void *value) {
  basic_json_handle_t *handle = (basic_json_handle_t *)value;

  (void)s;
  if (handle == NULL) {
    return;
  }

  if (handle->magic == BASIC_JSON_MAGIC) {
    json_value_free(handle->value);
    handle->magic = 0U;
  }
  free(handle);
}

static bool basic_json_handle_is_valid(const basic_json_handle_t *handle) {
  return handle != NULL && handle->magic == BASIC_JSON_MAGIC;
}

static bool basic_json_pop_handle_value(struct mb_interpreter_t *s, void **l, mb_value_t *value,
                                        basic_json_handle_t **handle) {
  void *raw = NULL;

  if (handle != NULL) {
    *handle = NULL;
  }
  if (value == NULL) {
    return false;
  }
  if (value->type != MB_DT_USERTYPE_REF) {
    return false;
  }
  if (mb_get_ref_value(s, l, *value, &raw) != MB_FUNC_OK || !basic_json_handle_is_valid((basic_json_handle_t *)raw)) {
    return false;
  }
  if (handle != NULL) {
    *handle = (basic_json_handle_t *)raw;
  }
  return true;
}

static void basic_json_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_USERTYPE_REF) {
    (void)mb_dispose_value(s, value);
  }
}

static int basic_json_push_nil(struct mb_interpreter_t *s, void **l) {
  mb_value_t value;

  mb_make_nil(value);
  return mb_push_value(s, l, value);
}

static int basic_json_push_string(struct mb_interpreter_t *s, void **l, const char *value) {
  const char *safe_value = value == NULL ? "" : value;
  char *copy = mb_memdup(safe_value, (unsigned)(strlen(safe_value) + 1U));

  if (copy == NULL) {
    return MB_FUNC_ERR;
  }
  return mb_push_string(s, l, copy);
}

static int basic_json_push_handle(struct mb_interpreter_t *s, void **l, JSON_Value *value) {
  basic_json_handle_t *handle = NULL;
  mb_value_t ref_value;
  int result = MB_FUNC_OK;

  if (value == NULL) {
    return basic_json_push_nil(s, l);
  }

  handle = (basic_json_handle_t *)malloc(sizeof(*handle));
  if (handle == NULL) {
    json_value_free(value);
    return basic_json_push_nil(s, l);
  }

  handle->magic = BASIC_JSON_MAGIC;
  handle->value = value;
  result = mb_make_ref_value(s, handle, &ref_value, basic_json_destroy_handle, basic_json_clone_handle, NULL, NULL, NULL);
  if (result != MB_FUNC_OK) {
    json_value_free(value);
    free(handle);
    return MB_FUNC_ERR;
  }

  result = mb_push_value(s, l, ref_value);
  (void)mb_dispose_value(s, ref_value);
  return result;
}

static int basic_json_push_handle_copy(struct mb_interpreter_t *s, void **l, const JSON_Value *value) {
  JSON_Value *copy = NULL;

  if (value == NULL) {
    return basic_json_push_nil(s, l);
  }
  copy = json_value_deep_copy(value);
  if (copy == NULL) {
    return basic_json_push_nil(s, l);
  }
  return basic_json_push_handle(s, l, copy);
}

static int basic_json_push_real(struct mb_interpreter_t *s, void **l, double value) {
  return mb_push_real(s, l, (real_t)value);
}

static int basic_json_push_int(struct mb_interpreter_t *s, void **l, int_t value) {
  return mb_push_int(s, l, value);
}

static int basic_json_push_bool(struct mb_interpreter_t *s, void **l, bool value) {
  return mb_push_int(s, l, value ? 1 : 0);
}

static int basic_json_push_count(struct mb_interpreter_t *s, void **l, size_t value) {
  return mb_push_int(s, l, basic_json_size_to_int(value));
}

static int basic_json_push_type(struct mb_interpreter_t *s, void **l, JSON_Value_Type value) {
  return mb_push_int(s, l, (int_t)value);
}

static const JSON_Value *basic_json_resolve_value(const JSON_Value *root, const char *path) {
  if (root == NULL) {
    return NULL;
  }
  if (path == NULL || path[0] == '\0') {
    return root;
  }

  JSON_Object *object = json_value_get_object(root);
  if (object == NULL) {
    return NULL;
  }
  return json_object_dotget_value(object, path);
}

static JSON_Object *basic_json_resolve_object(const JSON_Value *root, const char *path) {
  if (root == NULL) {
    return NULL;
  }
  if (path == NULL || path[0] == '\0') {
    return json_value_get_object(root);
  }

  JSON_Object *object = json_value_get_object(root);
  if (object == NULL) {
    return NULL;
  }
  return json_object_dotget_object(object, path);
}

static JSON_Array *basic_json_resolve_array(const JSON_Value *root, const char *path) {
  if (root == NULL) {
    return NULL;
  }
  if (path == NULL || path[0] == '\0') {
    return json_value_get_array(root);
  }

  JSON_Object *object = json_value_get_object(root);
  if (object == NULL) {
    return NULL;
  }
  return json_object_dotget_array(object, path);
}

static bool basic_json_replace_root(basic_json_handle_t *handle, JSON_Value *value) {
  if (!basic_json_handle_is_valid(handle) || value == NULL) {
    if (value != NULL) {
      json_value_free(value);
    }
    return false;
  }

  if (handle->value != NULL) {
    json_value_free(handle->value);
  }
  handle->value = value;
  return true;
}

static bool basic_json_read_string(const JSON_Value *value, const char **text) {
  if (text != NULL) {
    *text = NULL;
  }
  if (value == NULL || json_value_get_type(value) != JSONString) {
    return false;
  }
  if (text != NULL) {
    *text = json_value_get_string(value);
  }
  return true;
}

static bool basic_json_read_number(const JSON_Value *value, double *number) {
  if (number != NULL) {
    *number = 0.0;
  }
  if (value == NULL) {
    return false;
  }

  switch (json_value_get_type(value)) {
  case JSONNumber:
    if (number != NULL) {
      *number = json_value_get_number(value);
    }
    return true;
  case JSONBoolean:
    if (number != NULL) {
      *number = json_value_get_boolean(value) != 0 ? 1.0 : 0.0;
    }
    return true;
  default:
    return false;
  }
}

static bool basic_json_read_bool(const JSON_Value *value, bool *value_out) {
  if (value_out != NULL) {
    *value_out = false;
  }
  if (value == NULL) {
    return false;
  }

  switch (json_value_get_type(value)) {
  case JSONBoolean:
    if (value_out != NULL) {
      *value_out = json_value_get_boolean(value) != 0;
    }
    return true;
  case JSONNumber:
    if (value_out != NULL) {
      *value_out = json_value_get_number(value) != 0.0;
    }
    return true;
  default:
    return false;
  }
}

static bool basic_json_index_from_int(int_t value, size_t *index) {
  if (index == NULL || value < 0) {
    return false;
  }
  *index = (size_t)value;
  return true;
}

static int_t basic_json_size_to_int(size_t value) {
  return value > (size_t)INT_MAX ? (int_t)INT_MAX : (int_t)value;
}

static void basic_json_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}
