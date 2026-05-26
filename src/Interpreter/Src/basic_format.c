#include "Interpreter/Inc/basic_format.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Interpreter/Inc/basic.h"

#include "Board/Inc/bsp_hal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASIC_FORMAT_MAX_ARGS 12U
#define BASIC_FORMAT_OUTPUT_MAX 256U

static int basic_format_func(struct mb_interpreter_t *s, void **l);
static int basic_format_push_string(struct mb_interpreter_t *s, void **l, const char *value);
static void basic_format_feed_heartbeat(void);
static void basic_format_release_value(struct mb_interpreter_t *s, mb_value_t value);
static bool basic_format_uses_indexed_placeholders(const char *template_text);
static bool basic_format_indexed(const char *template_text, const mb_value_t *args, size_t arg_count, char *output,
                                 size_t output_size);
static bool basic_format_printf(const char *template_text, const mb_value_t *args, size_t arg_count, char *output,
                                size_t output_size);
static bool basic_format_append_char(char *output, size_t output_size, size_t *offset, char value);
static bool basic_format_append_text(char *output, size_t output_size, size_t *offset, const char *value);
static bool basic_format_append_value(char *output, size_t output_size, size_t *offset, mb_value_t value);
static bool basic_format_append_integer(char *output, size_t output_size, size_t *offset, int_t value);
static bool basic_format_append_real(char *output, size_t output_size, size_t *offset, real_t value);
static bool basic_format_append_printf_value(char *output, size_t output_size, size_t *offset, const char *format_spec,
                                             char specifier, mb_value_t value);
static bool basic_format_value_to_text(mb_value_t value, char *buffer, size_t buffer_size);
static int_t basic_format_value_to_int(mb_value_t value);
static real_t basic_format_value_to_real(mb_value_t value);
static bool basic_format_is_printf_modifier(char value);

ErrorStatus basic_format_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "FORMAT", basic_format_func);
  result |= mb_register_func(interpreter, "FMT", basic_format_func);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_format_func(struct mb_interpreter_t *s, void **l) {
  basic_format_feed_heartbeat();

  char *template_text = NULL;
  mb_value_t args[BASIC_FORMAT_MAX_ARGS];
  size_t arg_count = 0U;
  char output[BASIC_FORMAT_OUTPUT_MAX];
  bool ok = false;

  memset(args, 0, sizeof(args));
  output[0] = '\0';

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &template_text));
  while (mb_has_arg(s, l)) {
    if (arg_count >= BASIC_FORMAT_MAX_ARGS) {
      for (size_t index = 0U; index < arg_count; index++) {
        basic_format_release_value(s, args[index]);
      }
      mb_check(mb_attempt_close_bracket(s, l));
      return MB_FUNC_ERR;
    }
    mb_check(mb_pop_value(s, l, &args[arg_count]));
    arg_count++;
  }
  mb_check(mb_attempt_close_bracket(s, l));

  ok = basic_format_uses_indexed_placeholders(template_text == NULL ? "" : template_text)
         ? basic_format_indexed(template_text == NULL ? "" : template_text, args, arg_count, output, sizeof(output))
         : basic_format_printf(template_text == NULL ? "" : template_text, args, arg_count, output, sizeof(output));

  for (size_t index = 0U; index < arg_count; index++) {
    basic_format_release_value(s, args[index]);
  }

  return ok ? basic_format_push_string(s, l, output) : MB_FUNC_ERR;
}

static bool basic_format_uses_indexed_placeholders(const char *template_text) {
  if (template_text == NULL) {
    return false;
  }

  for (size_t index = 0U; template_text[index] != '\0'; index++) {
    if (template_text[index] != '{' || !isdigit((unsigned char)template_text[index + 1U])) {
      continue;
    }

    size_t cursor = index + 2U;
    while (isdigit((unsigned char)template_text[cursor])) {
      cursor++;
    }
    if (template_text[cursor] == '}') {
      return true;
    }
  }

  return false;
}

static bool basic_format_indexed(const char *template_text, const mb_value_t *args, size_t arg_count, char *output,
                                 size_t output_size) {
  size_t offset = 0U;

  for (size_t index = 0U; template_text[index] != '\0'; index++) {
    if (template_text[index] == '{' && template_text[index + 1U] == '{') {
      if (!basic_format_append_char(output, output_size, &offset, '{')) {
        return false;
      }
      index++;
      continue;
    }

    if (template_text[index] == '}' && template_text[index + 1U] == '}') {
      if (!basic_format_append_char(output, output_size, &offset, '}')) {
        return false;
      }
      index++;
      continue;
    }

    if (template_text[index] == '{' && isdigit((unsigned char)template_text[index + 1U])) {
      size_t cursor = index + 1U;
      unsigned int arg_index = 0U;
      while (isdigit((unsigned char)template_text[cursor])) {
        arg_index = (arg_index * 10U) + (unsigned int)(template_text[cursor] - '0');
        cursor++;
      }
      if (template_text[cursor] == '}') {
        if (arg_index < arg_count && !basic_format_append_value(output, output_size, &offset, args[arg_index])) {
          return false;
        }
        index = cursor;
        continue;
      }
    }

    if (!basic_format_append_char(output, output_size, &offset, template_text[index])) {
      return false;
    }
  }

  output[offset] = '\0';
  return true;
}

static bool basic_format_printf(const char *template_text, const mb_value_t *args, size_t arg_count, char *output,
                                size_t output_size) {
  size_t offset = 0U;
  size_t arg_index = 0U;

  for (size_t index = 0U; template_text[index] != '\0'; index++) {
    if (template_text[index] != '%' || template_text[index + 1U] == '\0') {
      if (!basic_format_append_char(output, output_size, &offset, template_text[index])) {
        return false;
      }
      continue;
    }

    size_t spec_start = index;
    index++;
    if (template_text[index] == '%') {
      if (!basic_format_append_char(output, output_size, &offset, '%')) {
        return false;
      }
      continue;
    }

    while (template_text[index] != '\0' && basic_format_is_printf_modifier(template_text[index])) {
      index++;
    }
    while (template_text[index] == 'l' || template_text[index] == 'h') {
      index++;
    }
    if (template_text[index] == '\0') {
      return basic_format_append_char(output, output_size, &offset, '%');
    }

    mb_value_t value;
    mb_make_nil(value);
    if (arg_index < arg_count) {
      value = args[arg_index];
      arg_index++;
    }

    char format_spec[24];
    size_t spec_offset = 0U;
    for (size_t cursor = spec_start; cursor <= index; cursor++) {
      if (template_text[cursor] == 'l' || template_text[cursor] == 'h') {
        continue;
      }
      if (spec_offset + 1U >= sizeof(format_spec)) {
        return false;
      }
      format_spec[spec_offset] = template_text[cursor];
      spec_offset++;
    }
    format_spec[spec_offset] = '\0';

    switch (template_text[index]) {
    case 'd':
    case 'i':
    case 'u':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 's':
      if (!basic_format_append_printf_value(output, output_size, &offset, format_spec, template_text[index], value)) {
        return false;
      }
      break;
    default:
      if (!basic_format_append_char(output, output_size, &offset, '%') ||
          !basic_format_append_char(output, output_size, &offset, template_text[index])) {
        return false;
      }
      break;
    }
  }

  output[offset] = '\0';
  return true;
}

static bool basic_format_append_char(char *output, size_t output_size, size_t *offset, char value) {
  if (output == NULL || offset == NULL || *offset + 1U >= output_size) {
    return false;
  }

  output[*offset] = value;
  (*offset)++;
  output[*offset] = '\0';
  return true;
}

static bool basic_format_append_text(char *output, size_t output_size, size_t *offset, const char *value) {
  const char *safe_value = value == NULL ? "" : value;
  while (*safe_value != '\0') {
    if (!basic_format_append_char(output, output_size, offset, *safe_value)) {
      return false;
    }
    safe_value++;
  }
  return true;
}

static bool basic_format_append_value(char *output, size_t output_size, size_t *offset, mb_value_t value) {
  switch (value.type) {
  case MB_DT_STRING:
    return basic_format_append_text(output, output_size, offset, value.value.string);
  case MB_DT_INT:
    return basic_format_append_integer(output, output_size, offset, value.value.integer);
  case MB_DT_REAL:
    return basic_format_append_real(output, output_size, offset, value.value.float_point);
  case MB_DT_NIL:
    return true;
  default:
    return basic_format_append_text(output, output_size, offset, "");
  }
}

static bool basic_format_append_integer(char *output, size_t output_size, size_t *offset, int_t value) {
  char buffer[24];
  int written = snprintf(buffer, sizeof(buffer), "%d", (int)value);
  return written >= 0 && (size_t)written < sizeof(buffer) && basic_format_append_text(output, output_size, offset, buffer);
}

static bool basic_format_append_real(char *output, size_t output_size, size_t *offset, real_t value) {
  char buffer[32];
  int written = snprintf(buffer, sizeof(buffer), "%g", (double)value);
  return written >= 0 && (size_t)written < sizeof(buffer) && basic_format_append_text(output, output_size, offset, buffer);
}

static bool basic_format_append_printf_value(char *output, size_t output_size, size_t *offset, const char *format_spec,
                                             char specifier, mb_value_t value) {
  char buffer[64];
  char text[64];
  int written = -1;

  switch (specifier) {
  case 'd':
  case 'i':
    written = snprintf(buffer, sizeof(buffer), format_spec, (int)basic_format_value_to_int(value));
    break;
  case 'u':
    written = snprintf(buffer, sizeof(buffer), format_spec, (unsigned int)basic_format_value_to_int(value));
    break;
  case 'f':
  case 'F':
  case 'g':
  case 'G':
    written = snprintf(buffer, sizeof(buffer), format_spec, (double)basic_format_value_to_real(value));
    break;
  case 's':
    if (!basic_format_value_to_text(value, text, sizeof(text))) {
      return false;
    }
    written = snprintf(buffer, sizeof(buffer), format_spec, text);
    break;
  default:
    return false;
  }

  return written >= 0 && (size_t)written < sizeof(buffer) && basic_format_append_text(output, output_size, offset, buffer);
}

static bool basic_format_value_to_text(mb_value_t value, char *buffer, size_t buffer_size) {
  int written = 0;
  if (buffer == NULL || buffer_size == 0U) {
    return false;
  }

  buffer[0] = '\0';
  switch (value.type) {
  case MB_DT_STRING:
    written = snprintf(buffer, buffer_size, "%s", value.value.string == NULL ? "" : value.value.string);
    break;
  case MB_DT_INT:
    written = snprintf(buffer, buffer_size, "%d", (int)value.value.integer);
    break;
  case MB_DT_REAL:
    written = snprintf(buffer, buffer_size, "%g", (double)value.value.float_point);
    break;
  case MB_DT_NIL:
    return true;
  default:
    return true;
  }

  return written >= 0 && (size_t)written < buffer_size;
}

static int_t basic_format_value_to_int(mb_value_t value) {
  switch (value.type) {
  case MB_DT_INT:
    return value.value.integer;
  case MB_DT_REAL:
    return (int_t)value.value.float_point;
  case MB_DT_STRING: {
    long parsed = 0L;
    if (value.value.string != NULL && sscanf(value.value.string, "%ld", &parsed) == 1) {
      return (int_t)parsed;
    }
    return 0;
  }
  default:
    return 0;
  }
}

static real_t basic_format_value_to_real(mb_value_t value) {
  switch (value.type) {
  case MB_DT_REAL:
    return value.value.float_point;
  case MB_DT_INT:
    return (real_t)value.value.integer;
  case MB_DT_STRING: {
    double parsed = 0.0;
    if (value.value.string != NULL && sscanf(value.value.string, "%lf", &parsed) == 1) {
      return (real_t)parsed;
    }
    return (real_t)0;
  }
  default:
    return (real_t)0;
  }
}

static bool basic_format_is_printf_modifier(char value) {
  return value == '-' || value == '+' || value == ' ' || value == '#' || value == '0' || value == '.' ||
         isdigit((unsigned char)value);
}

static int basic_format_push_string(struct mb_interpreter_t *s, void **l, const char *value) {
  const char *safe_value = value == NULL ? "" : value;
  char *copy = mb_memdup(safe_value, (unsigned)(strlen(safe_value) + 1U));
  if (copy == NULL) {
    return MB_FUNC_ERR;
  }
  return mb_push_string(s, l, copy);
}

static void basic_format_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}

static void basic_format_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_STRING || value.type == MB_DT_LIST || value.type == MB_DT_DICT || value.type == MB_DT_ARRAY) {
    (void)mb_dispose_value(s, value);
  }
}
