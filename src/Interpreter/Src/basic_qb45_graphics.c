#include "Interpreter/Inc/basic_qb45_graphics.h"

#include "Interpreter/Inc/app_basic.h"
#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Common/Inc/log.h"
#include "Display/Inc/display_api.h"
#include "Interpreter/Inc/basic.h"

#include "Board/Inc/bsp_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BASIC_QB45_SCREEN_TEXT    0
#define BASIC_QB45_SCREEN_DEFAULT 12
#define BASIC_QB45_TEXT_ROW_MAX   30U
#define BASIC_QB45_TEXT_COL_MAX   40U
#define BASIC_QB45_STYLE_BOX      "B"
#define BASIC_QB45_STYLE_BOX_FILL "BF"
#define BASIC_QB45_STYLE_FILL     "F"
#define BASIC_QB45_LINE_ARG_MAX   6U
#define BASIC_QB45_COLOR_COUNT    16U
#define BASIC_QB45_LOCATE_ARG_MAX 5U

typedef struct {
  uint16_t fg;
  uint16_t bg;
  uint16_t cursor_row;
  uint16_t cursor_col;
  int16_t last_x;
  int16_t last_y;
  bool has_last_point;
} basic_qb45_graphics_state_t;

static basic_qb45_graphics_state_t basic_qb45_state = {
  .fg = 0xFFFFU,
  .bg = 0x0000U,
  .cursor_row = 1U,
  .cursor_col = 1U,
};

static const uint16_t basic_qb45_palette[BASIC_QB45_COLOR_COUNT] = {
  0x0000U, 0x0015U, 0x0540U, 0x0555U, 0xA800U, 0xA815U, 0xAAA0U, 0xAD55U,
  0x52AAU, 0x001FU, 0x07E0U, 0x07FFU, 0xF800U, 0xF81FU, 0xFFE0U, 0xFFFFU,
};

static int basic_qb45_screen(struct mb_interpreter_t *s, void **l);
static int basic_qb45_cls(struct mb_interpreter_t *s, void **l);
static int basic_qb45_color(struct mb_interpreter_t *s, void **l);
static int basic_qb45_locate(struct mb_interpreter_t *s, void **l);
static int basic_qb45_pset(struct mb_interpreter_t *s, void **l);
static int basic_qb45_preset(struct mb_interpreter_t *s, void **l);
static int basic_qb45_line(struct mb_interpreter_t *s, void **l);
static int basic_qb45_circle(struct mb_interpreter_t *s, void **l);
static int basic_qb45_paint(struct mb_interpreter_t *s, void **l);

static int basic_qb45_pop_point(struct mb_interpreter_t *s, void **l, display_point_t *point);
static bool basic_qb45_pop_value(struct mb_interpreter_t *s, void **l, mb_value_t *value, bool *has_value);
static bool basic_qb45_pop_optional_color(struct mb_interpreter_t *s, void **l, uint16_t default_color,
                                          uint16_t *color);
static bool basic_qb45_pop_ignored_optional_color(struct mb_interpreter_t *s, void **l);
static bool basic_qb45_pop_ignored_optional_numeric(struct mb_interpreter_t *s, void **l, bool *has_value);
static bool basic_qb45_expect_no_more_args(struct mb_interpreter_t *s, void **l);
static bool basic_qb45_value_to_u16(mb_value_t value, uint16_t *out);
static bool basic_qb45_value_to_i16(mb_value_t value, int16_t *out);
static bool basic_qb45_value_to_color(mb_value_t value, uint16_t default_color, uint16_t *out);
static bool basic_qb45_value_to_style(mb_value_t value, bool *box, bool *fill);
static bool basic_qb45_value_is_omitted(mb_value_t value);
static bool basic_qb45_pop_values(struct mb_interpreter_t *s, void **l, mb_value_t *values, size_t max_count,
                                  size_t *count);
static bool basic_qb45_apply_line_options(const mb_value_t *values, size_t count, uint16_t default_color,
                                          display_line_request_t *request);
static bool basic_qb45_string_equals_ignore_case(const char *left, const char *right);
static void basic_qb45_release_value(struct mb_interpreter_t *s, mb_value_t value);
static void basic_qb45_release_values(struct mb_interpreter_t *s, mb_value_t *values, size_t count);
static int basic_qb45_push_status(struct mb_interpreter_t *s, void **l, ErrorStatus status);
static int basic_qb45_fail(struct mb_interpreter_t *s, void **l);
static void basic_qb45_advance_cursor_for_text(const char *text);
static void basic_qb45_feed_heartbeat(void);
static uint16_t basic_qb45_color_from_attribute(uint16_t attribute);
static void basic_qb45_reset_state(void);

ErrorStatus basic_qb45_graphics_register(struct mb_interpreter_t *interpreter) {
  const display_descriptor_t *display = NULL;

  if (interpreter == NULL || !display_api_is_available()) {
    return ERROR;
  }

  display = display_api_descriptor();
  if (display == NULL) {
    LOG_ERROR("BASIC QB4.5 graphics registration failed: display driver not bound");
    return ERROR;
  }

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_SCREEN, basic_qb45_screen);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_CLS, basic_qb45_cls);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_COLOR, basic_qb45_color);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_LOCATE, basic_qb45_locate);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_PSET, basic_qb45_pset);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_PRESET, basic_qb45_preset);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_LINE, basic_qb45_line);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_CIRCLE, basic_qb45_circle);
  result |= mb_register_func(interpreter, BASIC_QB45_FUNC_PAINT, basic_qb45_paint);
  if (result != MB_FUNC_OK) {
    return ERROR;
  }

  LOG_INFO("BASIC QB4.5 graphics registered for %s controller=%s size=%ux%u", display->name,
           display->controller, (unsigned int)display->size.width, (unsigned int)display->size.height);
  LOG_INFO("BASIC QB4.5 display caps text=%u graphics=%u read_pixels=%u paint=%u",
           display->qb45.can_draw_text ? 1U : 0U, display->qb45.can_draw_graphics ? 1U : 0U,
           display->qb45.can_read_pixels ? 1U : 0U, display->qb45.qb45_paint ? 1U : 0U);
  return SUCCESS;
}

ErrorStatus basic_qb45_graphics_write_text(const char *text) {
  if (text == NULL) {
    return ERROR;
  }
  if (display_api_write_text(text) != SUCCESS) {
    return ERROR;
  }
  basic_qb45_advance_cursor_for_text(text);
  return SUCCESS;
}

static int basic_qb45_screen(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  int_t mode = BASIC_QB45_SCREEN_DEFAULT;
  mb_value_t value;
  bool has_value = false;

  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (has_value && !basic_qb45_value_is_omitted(value)) {
    int16_t raw = 0;
    if (!basic_qb45_value_to_i16(value, &raw)) {
      basic_qb45_release_value(s, value);
      mb_check(mb_attempt_close_bracket(s, l));
      return basic_qb45_fail(s, l);
    }
    mode = (int_t)raw;
  }
  basic_qb45_release_value(s, value);
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  basic_qb45_reset_state();
  if (mode == BASIC_QB45_SCREEN_TEXT) {
    app_basic_set_print_target(APP_BASIC_PRINT_TARGET_DEFAULT);
    return mb_push_int(s, l, 1);
  }

  ErrorStatus status = display_api_screen((int)mode);
  if (status == SUCCESS) {
    app_basic_set_print_target(APP_BASIC_PRINT_TARGET_DISPLAY);
  }
  return basic_qb45_push_status(s, l, status);
}

static int basic_qb45_cls(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  uint16_t color = basic_qb45_state.bg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_qb45_pop_optional_color(s, l, basic_qb45_state.bg, &color)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  basic_qb45_state.has_last_point = false;
  if (display_api_cls(color) != SUCCESS || display_api_locate((display_text_cursor_t){1U, 1U}) != SUCCESS) {
    return basic_qb45_fail(s, l);
  }
  basic_qb45_state.cursor_row = 1U;
  basic_qb45_state.cursor_col = 1U;
  return mb_push_int(s, l, 1);
}

static int basic_qb45_color(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  uint16_t fg = basic_qb45_state.fg;
  uint16_t bg = basic_qb45_state.bg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_qb45_pop_optional_color(s, l, basic_qb45_state.fg, &fg)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (!basic_qb45_pop_optional_color(s, l, basic_qb45_state.bg, &bg)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (!basic_qb45_pop_ignored_optional_color(s, l) || !basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  display_color_pair_t colors = {fg, bg};
  if (display_api_color(colors) != SUCCESS) {
    return basic_qb45_fail(s, l);
  }

  basic_qb45_state.fg = fg;
  basic_qb45_state.bg = bg;
  return mb_push_int(s, l, 1);
}

static int basic_qb45_locate(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  uint16_t row = basic_qb45_state.cursor_row;
  uint16_t col = basic_qb45_state.cursor_col;
  mb_value_t value;
  bool has_value = false;
  size_t arg_count = 0U;
  mb_make_nil(value);

  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (has_value) {
    int16_t raw = 0;
    if (!basic_qb45_value_is_omitted(value)) {
      if (!basic_qb45_value_to_i16(value, &raw) || raw <= 0 || raw > (int16_t)BASIC_QB45_TEXT_ROW_MAX) {
        basic_qb45_release_value(s, value);
        mb_check(mb_attempt_close_bracket(s, l));
        return basic_qb45_fail(s, l);
      }
      row = (uint16_t)raw;
    }
    basic_qb45_release_value(s, value);
    mb_make_nil(value);
    arg_count = 1U;
  }
  if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (has_value) {
    int16_t raw = 0;
    if (!basic_qb45_value_is_omitted(value)) {
      if (!basic_qb45_value_to_i16(value, &raw) || raw <= 0 || raw > (int16_t)BASIC_QB45_TEXT_COL_MAX) {
        basic_qb45_release_value(s, value);
        mb_check(mb_attempt_close_bracket(s, l));
        return basic_qb45_fail(s, l);
      }
      col = (uint16_t)raw;
    }
    basic_qb45_release_value(s, value);
    mb_make_nil(value);
    arg_count = 2U;
  }
  while (arg_count < BASIC_QB45_LOCATE_ARG_MAX) {
    if (!basic_qb45_pop_ignored_optional_numeric(s, l, &has_value)) {
      mb_check(mb_attempt_close_bracket(s, l));
      return basic_qb45_fail(s, l);
    }
    if (!has_value) {
      break;
    }
    arg_count++;
  }
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  display_text_cursor_t cursor = {row, col};
  if (display_api_locate(cursor) != SUCCESS) {
    return basic_qb45_fail(s, l);
  }
  basic_qb45_state.cursor_row = row;
  basic_qb45_state.cursor_col = col;
  return mb_push_int(s, l, 1);
}

static int basic_qb45_pset(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  display_point_t point = {0};
  uint16_t color = basic_qb45_state.fg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (basic_qb45_pop_point(s, l, &point) != MB_FUNC_OK ||
      !basic_qb45_pop_optional_color(s, l, basic_qb45_state.fg, &color)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (display_api_pset(point, color) != SUCCESS) {
    return basic_qb45_fail(s, l);
  }

  basic_qb45_state.last_x = point.x;
  basic_qb45_state.last_y = point.y;
  basic_qb45_state.has_last_point = true;
  return mb_push_int(s, l, 1);
}

static int basic_qb45_preset(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  display_point_t point = {0};
  uint16_t color = basic_qb45_state.bg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (basic_qb45_pop_point(s, l, &point) != MB_FUNC_OK ||
      !basic_qb45_pop_optional_color(s, l, basic_qb45_state.bg, &color)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (display_api_preset(point, color) != SUCCESS) {
    return basic_qb45_fail(s, l);
  }

  basic_qb45_state.last_x = point.x;
  basic_qb45_state.last_y = point.y;
  basic_qb45_state.has_last_point = true;
  return mb_push_int(s, l, 1);
}

static int basic_qb45_line(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  display_line_request_t request = {0};
  mb_value_t values[BASIC_QB45_LINE_ARG_MAX];
  size_t count = 0U;
  size_t option_start = 0U;
  int result = MB_FUNC_OK;

  for (size_t i = 0U; i < BASIC_QB45_LINE_ARG_MAX; i++) {
    mb_make_nil(values[i]);
  }
  request.color = basic_qb45_state.fg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (!basic_qb45_pop_values(s, l, values, BASIC_QB45_LINE_ARG_MAX, &count)) {
    result = mb_attempt_close_bracket(s, l);
    if (result == MB_FUNC_OK) {
      result = basic_qb45_fail(s, l);
    }
    goto exit;
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  if (count >= 4U && basic_qb45_value_to_i16(values[0], &request.start.x) &&
      basic_qb45_value_to_i16(values[1], &request.start.y) &&
      basic_qb45_value_to_i16(values[2], &request.end.x) &&
      basic_qb45_value_to_i16(values[3], &request.end.y)) {
    option_start = 4U;
  } else if (count >= 2U && basic_qb45_state.has_last_point &&
             basic_qb45_value_to_i16(values[0], &request.end.x) &&
             basic_qb45_value_to_i16(values[1], &request.end.y)) {
    request.start.x = basic_qb45_state.last_x;
    request.start.y = basic_qb45_state.last_y;
    option_start = 2U;
  } else {
    result = basic_qb45_fail(s, l);
    goto exit;
  }

  if (!basic_qb45_apply_line_options(&values[option_start], count - option_start, basic_qb45_state.fg, &request)) {
    result = basic_qb45_fail(s, l);
    goto exit;
  }

  if (display_api_line(&request) != SUCCESS) {
    result = basic_qb45_fail(s, l);
    goto exit;
  }

  basic_qb45_state.last_x = request.end.x;
  basic_qb45_state.last_y = request.end.y;
  basic_qb45_state.has_last_point = true;
  result = mb_push_int(s, l, 1);

exit:
  basic_qb45_release_values(s, values, count);
  return result;
}

static int basic_qb45_circle(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  display_circle_request_t request = {0};
  int_t radius = 0;

  request.color = basic_qb45_state.fg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (basic_qb45_pop_point(s, l, &request.center) != MB_FUNC_OK) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_pop_int(s, l, &radius));
  if (radius < 0 || radius > INT16_MAX) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  request.radius = (uint16_t)radius;
  if (!basic_qb45_pop_optional_color(s, l, basic_qb45_state.fg, &request.color)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_value_t fill_value;
  bool has_fill = false;
  mb_make_nil(fill_value);
  if (!basic_qb45_pop_value(s, l, &fill_value, &has_fill)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  if (has_fill && !basic_qb45_value_is_omitted(fill_value)) {
    int16_t fill = 0;
    if (!basic_qb45_value_to_i16(fill_value, &fill)) {
      basic_qb45_release_value(s, fill_value);
      mb_check(mb_attempt_close_bracket(s, l));
      return basic_qb45_fail(s, l);
    }
    request.fill = fill != 0;
  }
  basic_qb45_release_value(s, fill_value);
  if (!basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  return basic_qb45_push_status(s, l, display_api_circle(&request));
}

static int basic_qb45_paint(struct mb_interpreter_t *s, void **l) {
  basic_qb45_feed_heartbeat();
  display_paint_request_t request = {0};

  request.fill_color = basic_qb45_state.fg;
  request.border_color = basic_qb45_state.fg;

  mb_check(mb_attempt_open_bracket(s, l));
  if (basic_qb45_pop_point(s, l, &request.point) != MB_FUNC_OK ||
      !basic_qb45_pop_optional_color(s, l, basic_qb45_state.fg, &request.fill_color) ||
      !basic_qb45_pop_optional_color(s, l, request.fill_color, &request.border_color) ||
      !basic_qb45_pop_ignored_optional_color(s, l) || !basic_qb45_expect_no_more_args(s, l)) {
    mb_check(mb_attempt_close_bracket(s, l));
    return basic_qb45_fail(s, l);
  }
  mb_check(mb_attempt_close_bracket(s, l));

  return basic_qb45_push_status(s, l, display_api_paint(&request));
}

static int basic_qb45_pop_point(struct mb_interpreter_t *s, void **l, display_point_t *point) {
  int_t x = 0;
  int_t y = 0;

  if (point == NULL) {
    return MB_FUNC_ERR;
  }

  mb_check(mb_pop_int(s, l, &x));
  mb_check(mb_pop_int(s, l, &y));
  if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX) {
    return MB_FUNC_ERR;
  }

  point->x = (int16_t)x;
  point->y = (int16_t)y;
  return MB_FUNC_OK;
}

static bool basic_qb45_pop_value(struct mb_interpreter_t *s, void **l, mb_value_t *value, bool *has_value) {
  bool_t has_arg = false;

  if (value == NULL || has_value == NULL) {
    return false;
  }
  mb_make_nil(*value);
  *has_value = false;
  if (mb_pop_optional_value(s, l, value, &has_arg) != MB_FUNC_OK) {
    return false;
  }

  *has_value = has_arg ? true : false;
  return true;
}

static bool basic_qb45_pop_optional_color(struct mb_interpreter_t *s, void **l, uint16_t default_color,
                                          uint16_t *color) {
  mb_value_t value;
  bool has_value = false;
  bool ok = false;

  if (color == NULL) {
    return false;
  }
  *color = default_color;
  mb_make_nil(value);
  if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
    return false;
  }
  if (!has_value) {
    return true;
  }

  ok = basic_qb45_value_to_color(value, default_color, color);
  basic_qb45_release_value(s, value);
  return ok;
}

static bool basic_qb45_pop_ignored_optional_color(struct mb_interpreter_t *s, void **l) {
  uint16_t unused = 0U;
  return basic_qb45_pop_optional_color(s, l, 0U, &unused);
}

static bool basic_qb45_pop_ignored_optional_numeric(struct mb_interpreter_t *s, void **l, bool *has_value) {
  mb_value_t value;
  bool local_has_value = false;
  bool ok = true;

  if (has_value == NULL) {
    return false;
  }
  *has_value = false;
  mb_make_nil(value);
  if (!basic_qb45_pop_value(s, l, &value, &local_has_value)) {
    return false;
  }

  if (local_has_value && !basic_qb45_value_is_omitted(value)) {
    int16_t unused = 0;
    ok = basic_qb45_value_to_i16(value, &unused);
  }
  basic_qb45_release_value(s, value);
  *has_value = local_has_value;
  return ok;
}

static bool basic_qb45_expect_no_more_args(struct mb_interpreter_t *s, void **l) {
  mb_value_t value;
  bool has_value = false;
  bool has_extra = false;

  for (;;) {
    mb_make_nil(value);
    if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
      return false;
    }
    if (!has_value) {
      break;
    }
    has_extra = true;
    basic_qb45_release_value(s, value);
  }

  return !has_extra;
}

static bool basic_qb45_value_to_u16(mb_value_t value, uint16_t *out) {
  int_t integer = 0;

  if (out == NULL) {
    return false;
  }

  switch (value.type) {
  case MB_DT_INT:
    integer = value.value.integer;
    break;
  case MB_DT_REAL:
    integer = (int_t)value.value.float_point;
    if ((real_t)integer != value.value.float_point) {
      return false;
    }
    break;
  default:
    return false;
  }

  if (integer < 0 || integer > (int_t)UINT16_MAX) {
    return false;
  }
  *out = (uint16_t)integer;
  return true;
}

static bool basic_qb45_value_to_i16(mb_value_t value, int16_t *out) {
  int_t integer = 0;

  if (out == NULL) {
    return false;
  }

  switch (value.type) {
  case MB_DT_INT:
    integer = value.value.integer;
    break;
  case MB_DT_REAL:
    integer = (int_t)value.value.float_point;
    if ((real_t)integer != value.value.float_point) {
      return false;
    }
    break;
  default:
    return false;
  }

  if (integer < INT16_MIN || integer > INT16_MAX) {
    return false;
  }
  *out = (int16_t)integer;
  return true;
}

static bool basic_qb45_value_to_color(mb_value_t value, uint16_t default_color, uint16_t *out) {
  uint16_t raw = 0U;

  if (out == NULL) {
    return false;
  }
  if (basic_qb45_value_is_omitted(value)) {
    *out = default_color;
    return true;
  }
  if (!basic_qb45_value_to_u16(value, &raw)) {
    return false;
  }

  *out = basic_qb45_color_from_attribute(raw);
  return true;
}

static bool basic_qb45_value_to_style(mb_value_t value, bool *box, bool *fill) {
  int16_t numeric = 0;

  if (box == NULL || fill == NULL) {
    return false;
  }

  *box = false;
  *fill = false;

  if (value.type == MB_DT_NIL) {
    return true;
  }
  if (value.type == MB_DT_STRING) {
    const char *text = value.value.string == NULL ? "" : value.value.string;
    if (text[0] == '\0') {
      return true;
    }
    if (basic_qb45_string_equals_ignore_case(text, BASIC_QB45_STYLE_BOX)) {
      *box = true;
      return true;
    }
    if (basic_qb45_string_equals_ignore_case(text, BASIC_QB45_STYLE_BOX_FILL)) {
      *box = true;
      *fill = true;
      return true;
    }
    if (basic_qb45_string_equals_ignore_case(text, BASIC_QB45_STYLE_FILL)) {
      *fill = true;
      return true;
    }
    return false;
  }

  if (!basic_qb45_value_to_i16(value, &numeric)) {
    return false;
  }
  *box = (numeric & 0x01) != 0;
  *fill = (numeric & 0x02) != 0;
  return true;
}

static bool basic_qb45_value_is_omitted(mb_value_t value) {
  return value.type == MB_DT_NIL;
}

static bool basic_qb45_pop_values(struct mb_interpreter_t *s, void **l, mb_value_t *values, size_t max_count,
                                  size_t *count) {
  bool ok = true;

  if (values == NULL || count == NULL) {
    return false;
  }

  *count = 0U;
  for (;;) {
    mb_value_t value;
    bool has_value = false;

    mb_make_nil(value);
    if (!basic_qb45_pop_value(s, l, &value, &has_value)) {
      return false;
    }
    if (!has_value) {
      break;
    }
    if (*count < max_count) {
      values[*count] = value;
      (*count)++;
    } else {
      basic_qb45_release_value(s, value);
      ok = false;
    }
  }

  return ok;
}

static bool basic_qb45_apply_line_options(const mb_value_t *values, size_t count, uint16_t default_color,
                                          display_line_request_t *request) {
  if (request == NULL || count > 2U) {
    return false;
  }
  while (count > 0U && basic_qb45_value_is_omitted(values[count - 1U])) {
    count--;
  }
  if (count == 0U) {
    return true;
  }

  if (basic_qb45_value_is_omitted(values[0])) {
    request->color = default_color;
  } else if (values[0].type == MB_DT_STRING) {
    return count == 1U && basic_qb45_value_to_style(values[0], &request->box, &request->fill);
  } else if (!basic_qb45_value_to_color(values[0], default_color, &request->color)) {
    return false;
  }

  if (count == 2U && !basic_qb45_value_to_style(values[1], &request->box, &request->fill)) {
    return false;
  }

  return true;
}

static bool basic_qb45_string_equals_ignore_case(const char *left, const char *right) {
  if (left == NULL || right == NULL) {
    return false;
  }

  while (*left != '\0' && *right != '\0') {
    char lch = *left;
    char rch = *right;
    if (lch >= 'a' && lch <= 'z') {
      lch = (char)(lch - 'a' + 'A');
    }
    if (rch >= 'a' && rch <= 'z') {
      rch = (char)(rch - 'a' + 'A');
    }
    if (lch != rch) {
      return false;
    }
    left++;
    right++;
  }

  return *left == '\0' && *right == '\0';
}

static void basic_qb45_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_STRING || value.type == MB_DT_LIST || value.type == MB_DT_DICT ||
      value.type == MB_DT_ARRAY
#ifdef MB_ENABLE_USERTYPE_REF
      || value.type == MB_DT_USERTYPE_REF
#endif
  ) {
    (void)mb_dispose_value(s, value);
  }
}

static void basic_qb45_release_values(struct mb_interpreter_t *s, mb_value_t *values, size_t count) {
  if (values == NULL) {
    return;
  }

  for (size_t i = 0U; i < count; i++) {
    basic_qb45_release_value(s, values[i]);
    mb_make_nil(values[i]);
  }
}

static int basic_qb45_push_status(struct mb_interpreter_t *s, void **l, ErrorStatus status) {
  return mb_push_int(s, l, status == SUCCESS ? 1 : 0);
}

static int basic_qb45_fail(struct mb_interpreter_t *s, void **l) {
  return mb_push_int(s, l, 0);
}

static void basic_qb45_advance_cursor_for_text(const char *text) {
  if (text == NULL) {
    return;
  }

  while (*text != '\0') {
    char ch = *text++;
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      basic_qb45_state.cursor_row++;
      basic_qb45_state.cursor_col = 1U;
    } else if (ch == '\t') {
      do {
        basic_qb45_state.cursor_col++;
      } while (basic_qb45_state.cursor_col <= BASIC_QB45_TEXT_COL_MAX &&
               ((basic_qb45_state.cursor_col - 1U) % 4U) != 0U);
    } else {
      if (basic_qb45_state.cursor_col > BASIC_QB45_TEXT_COL_MAX) {
        basic_qb45_state.cursor_row++;
        basic_qb45_state.cursor_col = 1U;
      }
      basic_qb45_state.cursor_col++;
    }

    if (basic_qb45_state.cursor_row > BASIC_QB45_TEXT_ROW_MAX) {
      basic_qb45_state.cursor_row = BASIC_QB45_TEXT_ROW_MAX;
      basic_qb45_state.cursor_col = BASIC_QB45_TEXT_COL_MAX + 1U;
      break;
    }
  }
}

static void basic_qb45_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}

static uint16_t basic_qb45_color_from_attribute(uint16_t attribute) {
  if (attribute < BASIC_QB45_COLOR_COUNT) {
    return basic_qb45_palette[attribute];
  }
  return attribute;
}

static void basic_qb45_reset_state(void) {
  basic_qb45_state.fg = basic_qb45_color_from_attribute(15U);
  basic_qb45_state.bg = basic_qb45_color_from_attribute(0U);
  basic_qb45_state.cursor_row = 1U;
  basic_qb45_state.cursor_col = 1U;
  basic_qb45_state.has_last_point = false;
}
