#include "Display/Inc/display_st7789.h"

static ErrorStatus display_st7789_configure_screen(void *context, int mode);
static ErrorStatus display_st7789_clear(void *context, uint16_t color);
static ErrorStatus display_st7789_set_colors(void *context, display_color_pair_t colors);
static ErrorStatus display_st7789_set_cursor(void *context, display_text_cursor_t cursor);
static ErrorStatus display_st7789_write_text(void *context, const char *text);
static ErrorStatus display_st7789_draw_pixel(void *context, display_point_t point, uint16_t color);
static ErrorStatus display_st7789_draw_line(void *context, const display_line_request_t *request);
static ErrorStatus display_st7789_draw_circle(void *context, const display_circle_request_t *request);
static ErrorStatus display_st7789_paint(void *context, const display_paint_request_t *request);

static const display_driver_t display_st7789_driver_instance = {
  .descriptor =
    {
      .name = "ST7789-like LCD",
      .controller = "ST7789-like",
      .size = {240U, 240U},
      .qb45 =
        {
          .can_draw_text = true,
          .can_draw_graphics = true,
          .can_read_pixels = false,
          .qb45_screen = true,
          .qb45_color = true,
          .qb45_cls = true,
          .qb45_locate = true,
          .qb45_pset = true,
          .qb45_preset = true,
          .qb45_line = true,
          .qb45_circle = true,
          .qb45_paint = true,
        },
    },
  .ops =
    {
      .configure_screen = display_st7789_configure_screen,
      .clear = display_st7789_clear,
      .set_colors = display_st7789_set_colors,
      .set_cursor = display_st7789_set_cursor,
      .write_text = display_st7789_write_text,
      .draw_pixel = display_st7789_draw_pixel,
      .draw_line = display_st7789_draw_line,
      .draw_circle = display_st7789_draw_circle,
      .paint = display_st7789_paint,
    },
};

const display_driver_t *display_st7789_driver(void) {
  return &display_st7789_driver_instance;
}

static ErrorStatus display_st7789_configure_screen(void *context, int mode) {
  (void)context;
  (void)mode;
  return SUCCESS;
}

static ErrorStatus display_st7789_clear(void *context, uint16_t color) {
  (void)context;
  (void)color;
  return ERROR;
}

static ErrorStatus display_st7789_set_colors(void *context, display_color_pair_t colors) {
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  if (driver == NULL) {
    return ERROR;
  }

  driver->colors = colors;
  return SUCCESS;
}

static ErrorStatus display_st7789_set_cursor(void *context, display_text_cursor_t cursor) {
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  if (driver == NULL) {
    return ERROR;
  }

  driver->cursor = cursor;
  return SUCCESS;
}

static ErrorStatus display_st7789_write_text(void *context, const char *text) {
  (void)context;
  (void)text;
  return ERROR;
}

static ErrorStatus display_st7789_draw_pixel(void *context, display_point_t point, uint16_t color) {
  (void)context;
  (void)point;
  (void)color;
  return ERROR;
}

static ErrorStatus display_st7789_draw_line(void *context, const display_line_request_t *request) {
  (void)context;
  (void)request;
  return ERROR;
}

static ErrorStatus display_st7789_draw_circle(void *context, const display_circle_request_t *request) {
  (void)context;
  (void)request;
  return ERROR;
}

static ErrorStatus display_st7789_paint(void *context, const display_paint_request_t *request) {
  (void)context;
  (void)request;
  return ERROR;
}
