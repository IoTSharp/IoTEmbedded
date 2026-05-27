#include "Display/Inc/display_st7789.h"

#define ST7789_WIDTH_DEFAULT     240U
#define ST7789_HEIGHT_DEFAULT    240U
#define ST7789_COLOR_WHITE       0xFFFFU
#define ST7789_COLOR_BLACK       0x0000U
#define ST7789_WRITE_CHUNK_BYTES 64U
#define ST7789_TEXT_COLUMNS      40U
#define ST7789_TEXT_ROWS         30U
#define ST7789_QB45_SCREEN_MODE  12

#define ST7789_CMD_SWRESET 0x01U
#define ST7789_CMD_SLPOUT  0x11U
#define ST7789_CMD_COLMOD  0x3AU
#define ST7789_CMD_MADCTL  0x36U
#define ST7789_CMD_INVON   0x21U
#define ST7789_CMD_NORON   0x13U
#define ST7789_CMD_CASET   0x2AU
#define ST7789_CMD_RASET   0x2BU
#define ST7789_CMD_RAMWR   0x2CU
#define ST7789_CMD_DISPON    0x29U
#define ST7789_CMD_PORCTRL   0xB2U
#define ST7789_CMD_GCTRL     0xB7U
#define ST7789_CMD_VCOMS     0xBBU
#define ST7789_CMD_LCMCTRL   0xC0U
#define ST7789_CMD_VDVVRHEN  0xC2U
#define ST7789_CMD_VRHS      0xC3U
#define ST7789_CMD_VDVS      0xC4U
#define ST7789_CMD_FRCTRL2   0xC6U
#define ST7789_CMD_PWCTRL1   0xD0U
#define ST7789_CMD_PVGAMCTRL 0xE0U
#define ST7789_CMD_NVGAMCTRL 0xE1U

#define ST7789_MADCTL_RGB 0x00U
#define ST7789_COLMOD_16B 0x55U

static ErrorStatus display_st7789_configure_screen(void *context, int mode);
static ErrorStatus display_st7789_clear(void *context, uint16_t color);
static ErrorStatus display_st7789_set_colors(void *context, display_color_pair_t colors);
static ErrorStatus display_st7789_set_cursor(void *context, display_text_cursor_t cursor);
static ErrorStatus display_st7789_write_text(void *context, const char *text);
static ErrorStatus display_st7789_draw_pixel(void *context, display_point_t point, uint16_t color);
static ErrorStatus display_st7789_draw_line(void *context, const display_line_request_t *request);
static ErrorStatus display_st7789_draw_circle(void *context, const display_circle_request_t *request);
static ErrorStatus display_st7789_paint(void *context, const display_paint_request_t *request);

static ErrorStatus display_st7789_init(display_st7789_context_t *driver);
static ErrorStatus display_st7789_reset_panel(display_st7789_context_t *driver);
static ErrorStatus display_st7789_write_command(display_st7789_context_t *driver, uint8_t command);
static ErrorStatus display_st7789_write_data(display_st7789_context_t *driver, const uint8_t *data, size_t length);
static ErrorStatus display_st7789_write_selected_data(display_st7789_context_t *driver, const uint8_t *data,
                                                      size_t length);
static ErrorStatus display_st7789_write_command_data(display_st7789_context_t *driver, uint8_t command,
                                                     const uint8_t *data, size_t length);
static ErrorStatus display_st7789_set_window(display_st7789_context_t *driver, uint16_t x0, uint16_t y0,
                                             uint16_t x1, uint16_t y1);
static ErrorStatus display_st7789_fill_rect(display_st7789_context_t *driver, int16_t x, int16_t y,
                                            int16_t width, int16_t height, uint16_t color);
static ErrorStatus display_st7789_write_color_run(display_st7789_context_t *driver, uint16_t color,
                                                  uint32_t pixel_count);
static bool display_st7789_can_use(display_st7789_context_t *driver);
static bool display_st7789_point_in_bounds(const display_st7789_context_t *driver, display_point_t point);
static uint16_t display_st7789_width(const display_st7789_context_t *driver);
static uint16_t display_st7789_height(const display_st7789_context_t *driver);
static void display_st7789_delay(display_st7789_context_t *driver, uint32_t delay_ms);
static ErrorStatus display_st7789_draw_fast_hline(display_st7789_context_t *driver, int16_t x, int16_t y,
                                                  int16_t width, uint16_t color);
static ErrorStatus display_st7789_draw_fast_vline(display_st7789_context_t *driver, int16_t x, int16_t y,
                                                  int16_t height, uint16_t color);
static ErrorStatus display_st7789_draw_rect(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                            int16_t x1, int16_t y1, uint16_t color);
static ErrorStatus display_st7789_fill_box(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                           int16_t x1, int16_t y1, uint16_t color);
static ErrorStatus display_st7789_draw_circle_points(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                                     int16_t x, int16_t y, uint16_t color);
static ErrorStatus display_st7789_fill_circle_spans(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                                    int16_t x, int16_t y, uint16_t color);
static int16_t display_st7789_min_i16(int16_t a, int16_t b);
static int16_t display_st7789_abs_i16(int16_t value);

static const display_driver_t display_st7789_driver_instance = {
  .descriptor =
    {
      .name = "ST7789 LCD",
      .controller = "ST7789",
      .size = {ST7789_WIDTH_DEFAULT, ST7789_HEIGHT_DEFAULT},
      .qb45 =
        {
          .can_draw_text = false,
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
          .qb45_paint = false,
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
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  if (!display_st7789_can_use(driver) || (mode != 0 && mode != ST7789_QB45_SCREEN_MODE)) {
    return ERROR;
  }

  if (driver->size.width == 0U) {
    driver->size.width = ST7789_WIDTH_DEFAULT;
  }
  if (driver->size.height == 0U) {
    driver->size.height = ST7789_HEIGHT_DEFAULT;
  }

  if (display_st7789_init(driver) != SUCCESS) {
    return ERROR;
  }

  driver->colors.fg = ST7789_COLOR_WHITE;
  driver->colors.bg = ST7789_COLOR_BLACK;
  driver->cursor.row = 1U;
  driver->cursor.col = 1U;
  return display_st7789_clear(driver, driver->colors.bg);
}

static ErrorStatus display_st7789_clear(void *context, uint16_t color) {
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  return display_st7789_fill_rect(driver, 0, 0, (int16_t)display_st7789_width(driver),
                                  (int16_t)display_st7789_height(driver), color);
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
  if (driver == NULL || cursor.row == 0U || cursor.col == 0U || cursor.row > ST7789_TEXT_ROWS ||
      cursor.col > ST7789_TEXT_COLUMNS) {
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
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  if (!display_st7789_can_use(driver)) {
    return ERROR;
  }
  if (!display_st7789_point_in_bounds(driver, point)) {
    return SUCCESS;
  }
  if (display_st7789_init(driver) != SUCCESS ||
      display_st7789_set_window(driver, (uint16_t)point.x, (uint16_t)point.y, (uint16_t)point.x,
                                (uint16_t)point.y) != SUCCESS) {
    return ERROR;
  }

  return display_st7789_write_color_run(driver, color, 1U);
}

static ErrorStatus display_st7789_draw_line(void *context, const display_line_request_t *request) {
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  int16_t x0 = 0;
  int16_t y0 = 0;
  int16_t x1 = 0;
  int16_t y1 = 0;

  if (request == NULL || !display_st7789_can_use(driver)) {
    return ERROR;
  }

  x0 = request->start.x;
  y0 = request->start.y;
  x1 = request->end.x;
  y1 = request->end.y;

  if (request->fill) {
    return display_st7789_fill_box(driver, x0, y0, x1, y1, request->color);
  }
  if (request->box) {
    return display_st7789_draw_rect(driver, x0, y0, x1, y1, request->color);
  }
  if (y0 == y1) {
    return display_st7789_draw_fast_hline(driver, display_st7789_min_i16(x0, x1), y0,
                                          (int16_t)(display_st7789_abs_i16((int16_t)(x1 - x0)) + 1),
                                          request->color);
  }
  if (x0 == x1) {
    return display_st7789_draw_fast_vline(driver, x0, display_st7789_min_i16(y0, y1),
                                          (int16_t)(display_st7789_abs_i16((int16_t)(y1 - y0)) + 1),
                                          request->color);
  }

  int16_t dx = display_st7789_abs_i16((int16_t)(x1 - x0));
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = (int16_t)-display_st7789_abs_i16((int16_t)(y1 - y0));
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t error = (int16_t)(dx + dy);

  for (;;) {
    display_point_t point = {x0, y0};
    if (display_st7789_point_in_bounds(driver, point) &&
        display_st7789_draw_pixel(driver, point, request->color) != SUCCESS) {
      return ERROR;
    }
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int16_t e2 = (int16_t)(2 * error);
    if (e2 >= dy) {
      error = (int16_t)(error + dy);
      x0 = (int16_t)(x0 + sx);
    }
    if (e2 <= dx) {
      error = (int16_t)(error + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }

  return SUCCESS;
}

static ErrorStatus display_st7789_draw_circle(void *context, const display_circle_request_t *request) {
  display_st7789_context_t *driver = (display_st7789_context_t *)context;
  int16_t x = 0;
  int16_t y = 0;
  int16_t decision = 0;

  if (request == NULL || !display_st7789_can_use(driver) || request->radius > (uint16_t)INT16_MAX) {
    return ERROR;
  }

  x = (int16_t)request->radius;
  y = 0;
  decision = 1 - x;

  while (x >= y) {
    ErrorStatus status = request->fill ? display_st7789_fill_circle_spans(driver, request->center.x,
                                                                           request->center.y, x, y, request->color)
                                       : display_st7789_draw_circle_points(driver, request->center.x,
                                                                            request->center.y, x, y,
                                                                            request->color);
    if (status != SUCCESS) {
      return ERROR;
    }

    y++;
    if (decision <= 0) {
      decision = (int16_t)(decision + (2 * y) + 1);
    } else {
      x--;
      decision = (int16_t)(decision + (2 * (y - x)) + 1);
    }
  }

  return SUCCESS;
}

static ErrorStatus display_st7789_paint(void *context, const display_paint_request_t *request) {
  (void)context;
  (void)request;
  return ERROR;
}

static ErrorStatus display_st7789_init(display_st7789_context_t *driver) {
  uint8_t data = 0U;
  const uint8_t porctrl[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
  const uint8_t pwctrl1[] = {0xA4U, 0xA1U};
  const uint8_t positive_gamma[] = {0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
                                    0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U};
  const uint8_t negative_gamma[] = {0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
                                    0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U};

  if (!display_st7789_can_use(driver)) {
    return ERROR;
  }
  if (driver->initialized) {
    return SUCCESS;
  }

  if (driver->bus.set_power != NULL && driver->bus.set_power(driver->bus_context, true) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 20U);

  if (display_st7789_reset_panel(driver) != SUCCESS) {
    return ERROR;
  }

  if (display_st7789_write_command(driver, ST7789_CMD_SWRESET) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 150U);
  if (display_st7789_write_command(driver, ST7789_CMD_SLPOUT) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 120U);

  data = ST7789_COLMOD_16B;
  if (display_st7789_write_command_data(driver, ST7789_CMD_COLMOD, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 10U);

  data = ST7789_MADCTL_RGB;
  if (display_st7789_write_command_data(driver, ST7789_CMD_MADCTL, &data, 1U) != SUCCESS) {
    return ERROR;
  }

  if (display_st7789_write_command_data(driver, ST7789_CMD_PORCTRL, porctrl, sizeof(porctrl)) != SUCCESS) {
    return ERROR;
  }
  data = 0x35U;
  if (display_st7789_write_command_data(driver, ST7789_CMD_GCTRL, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x19U;
  if (display_st7789_write_command_data(driver, ST7789_CMD_VCOMS, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x2CU;
  if (display_st7789_write_command_data(driver, ST7789_CMD_LCMCTRL, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x01U;
  if (display_st7789_write_command_data(driver, ST7789_CMD_VDVVRHEN, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x12U;
  if (display_st7789_write_command_data(driver, ST7789_CMD_VRHS, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x20U;
  if (display_st7789_write_command_data(driver, ST7789_CMD_VDVS, &data, 1U) != SUCCESS) {
    return ERROR;
  }
  data = 0x0FU;
  if (display_st7789_write_command_data(driver, ST7789_CMD_FRCTRL2, &data, 1U) != SUCCESS ||
      display_st7789_write_command_data(driver, ST7789_CMD_PWCTRL1, pwctrl1, sizeof(pwctrl1)) != SUCCESS ||
      display_st7789_write_command_data(driver, ST7789_CMD_PVGAMCTRL, positive_gamma, sizeof(positive_gamma)) !=
        SUCCESS ||
      display_st7789_write_command_data(driver, ST7789_CMD_NVGAMCTRL, negative_gamma, sizeof(negative_gamma)) !=
        SUCCESS ||
      display_st7789_write_command(driver, ST7789_CMD_INVON) != SUCCESS ||
      display_st7789_write_command(driver, ST7789_CMD_NORON) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 10U);

  if (display_st7789_write_command(driver, ST7789_CMD_DISPON) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 120U);

  driver->initialized = true;
  return SUCCESS;
}

static ErrorStatus display_st7789_reset_panel(display_st7789_context_t *driver) {
  if (driver->bus.set_chip_select != NULL && driver->bus.set_chip_select(driver->bus_context, false) != SUCCESS) {
    return ERROR;
  }
  if (driver->bus.set_reset == NULL) {
    return SUCCESS;
  }

  if (driver->bus.set_reset(driver->bus_context, false) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 10U);
  if (driver->bus.set_reset(driver->bus_context, true) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 20U);
  if (driver->bus.set_reset(driver->bus_context, false) != SUCCESS) {
    return ERROR;
  }
  display_st7789_delay(driver, 120U);
  return SUCCESS;
}

static ErrorStatus display_st7789_write_command(display_st7789_context_t *driver, uint8_t command) {
  ErrorStatus status = ERROR;

  if (!display_st7789_can_use(driver) ||
      (driver->bus.set_chip_select != NULL && driver->bus.set_chip_select(driver->bus_context, true) != SUCCESS) ||
      driver->bus.set_data_command(driver->bus_context, false) != SUCCESS) {
    return ERROR;
  }

  status = driver->bus.write_bytes(driver->bus_context, &command, 1U);
  if (driver->bus.set_chip_select != NULL &&
      driver->bus.set_chip_select(driver->bus_context, false) != SUCCESS) {
    status = ERROR;
  }
  return status;
}

static ErrorStatus display_st7789_write_data(display_st7789_context_t *driver, const uint8_t *data, size_t length) {
  ErrorStatus status = ERROR;

  if (!display_st7789_can_use(driver) || (data == NULL && length > 0U) ||
      (driver->bus.set_chip_select != NULL && driver->bus.set_chip_select(driver->bus_context, true) != SUCCESS) ||
      driver->bus.set_data_command(driver->bus_context, true) != SUCCESS) {
    return ERROR;
  }

  status = length == 0U ? SUCCESS : driver->bus.write_bytes(driver->bus_context, data, length);
  if (driver->bus.set_chip_select != NULL &&
      driver->bus.set_chip_select(driver->bus_context, false) != SUCCESS) {
    status = ERROR;
  }
  return status;
}

static ErrorStatus display_st7789_write_selected_data(display_st7789_context_t *driver, const uint8_t *data,
                                                      size_t length) {
  if (!display_st7789_can_use(driver) || (data == NULL && length > 0U) ||
      driver->bus.set_data_command(driver->bus_context, true) != SUCCESS) {
    return ERROR;
  }

  return length == 0U ? SUCCESS : driver->bus.write_bytes(driver->bus_context, data, length);
}

static ErrorStatus display_st7789_write_command_data(display_st7789_context_t *driver, uint8_t command,
                                                     const uint8_t *data, size_t length) {
  if (display_st7789_write_command(driver, command) != SUCCESS) {
    return ERROR;
  }
  return length == 0U ? SUCCESS : display_st7789_write_data(driver, data, length);
}

static ErrorStatus display_st7789_set_window(display_st7789_context_t *driver, uint16_t x0, uint16_t y0,
                                             uint16_t x1, uint16_t y1) {
  uint16_t column_start = (uint16_t)(x0 + driver->x_offset);
  uint16_t column_end = (uint16_t)(x1 + driver->x_offset);
  uint16_t row_start = (uint16_t)(y0 + driver->y_offset);
  uint16_t row_end = (uint16_t)(y1 + driver->y_offset);
  uint8_t columns[4U] = {
    (uint8_t)(column_start >> 8),
    (uint8_t)column_start,
    (uint8_t)(column_end >> 8),
    (uint8_t)column_end,
  };
  uint8_t rows[4U] = {
    (uint8_t)(row_start >> 8),
    (uint8_t)row_start,
    (uint8_t)(row_end >> 8),
    (uint8_t)row_end,
  };

  return display_st7789_write_command_data(driver, ST7789_CMD_CASET, columns, sizeof(columns)) == SUCCESS &&
             display_st7789_write_command_data(driver, ST7789_CMD_RASET, rows, sizeof(rows)) == SUCCESS &&
             display_st7789_write_command(driver, ST7789_CMD_RAMWR) == SUCCESS
           ? SUCCESS
           : ERROR;
}

static ErrorStatus display_st7789_fill_rect(display_st7789_context_t *driver, int16_t x, int16_t y,
                                            int16_t width, int16_t height, uint16_t color) {
  int16_t x_end = 0;
  int16_t y_end = 0;
  uint16_t clipped_width = 0U;
  uint16_t clipped_height = 0U;

  if (!display_st7789_can_use(driver)) {
    return ERROR;
  }
  if (width <= 0 || height <= 0) {
    return SUCCESS;
  }
  if (display_st7789_init(driver) != SUCCESS) {
    return ERROR;
  }

  x_end = (int16_t)(x + width - 1);
  y_end = (int16_t)(y + height - 1);
  if (x_end < 0 || y_end < 0 || x >= (int16_t)display_st7789_width(driver) ||
      y >= (int16_t)display_st7789_height(driver)) {
    return SUCCESS;
  }

  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x_end >= (int16_t)display_st7789_width(driver)) {
    x_end = (int16_t)(display_st7789_width(driver) - 1U);
  }
  if (y_end >= (int16_t)display_st7789_height(driver)) {
    y_end = (int16_t)(display_st7789_height(driver) - 1U);
  }

  clipped_width = (uint16_t)(x_end - x + 1);
  clipped_height = (uint16_t)(y_end - y + 1);
  if (display_st7789_set_window(driver, (uint16_t)x, (uint16_t)y, (uint16_t)x_end, (uint16_t)y_end) != SUCCESS) {
    return ERROR;
  }

  return display_st7789_write_color_run(driver, color, (uint32_t)clipped_width * (uint32_t)clipped_height);
}

static ErrorStatus display_st7789_write_color_run(display_st7789_context_t *driver, uint16_t color,
                                                  uint32_t pixel_count) {
  uint8_t buffer[ST7789_WRITE_CHUNK_BYTES];
  uint8_t high = (uint8_t)(color >> 8);
  uint8_t low = (uint8_t)color;
  size_t pixels_per_chunk = sizeof(buffer) / 2U;
  ErrorStatus status = SUCCESS;

  if (!display_st7789_can_use(driver)) {
    return ERROR;
  }

  for (size_t i = 0U; i < sizeof(buffer); i += 2U) {
    buffer[i] = high;
    buffer[i + 1U] = low;
  }

  if (driver->bus.set_chip_select != NULL && driver->bus.set_chip_select(driver->bus_context, true) != SUCCESS) {
    return ERROR;
  }

  while (pixel_count > 0U) {
    size_t pixels = pixel_count > pixels_per_chunk ? pixels_per_chunk : (size_t)pixel_count;
    if (display_st7789_write_selected_data(driver, buffer, pixels * 2U) != SUCCESS) {
      status = ERROR;
      break;
    }
    pixel_count -= (uint32_t)pixels;
  }

  if (driver->bus.set_chip_select != NULL &&
      driver->bus.set_chip_select(driver->bus_context, false) != SUCCESS) {
    status = ERROR;
  }

  return status;
}

static bool display_st7789_can_use(display_st7789_context_t *driver) {
  return driver != NULL && driver->bus.write_bytes != NULL && driver->bus.set_data_command != NULL;
}

static bool display_st7789_point_in_bounds(const display_st7789_context_t *driver, display_point_t point) {
  return driver != NULL && point.x >= 0 && point.y >= 0 && point.x < (int16_t)display_st7789_width(driver) &&
         point.y < (int16_t)display_st7789_height(driver);
}

static uint16_t display_st7789_width(const display_st7789_context_t *driver) {
  return driver == NULL || driver->size.width == 0U ? ST7789_WIDTH_DEFAULT : driver->size.width;
}

static uint16_t display_st7789_height(const display_st7789_context_t *driver) {
  return driver == NULL || driver->size.height == 0U ? ST7789_HEIGHT_DEFAULT : driver->size.height;
}

static void display_st7789_delay(display_st7789_context_t *driver, uint32_t delay_ms) {
  if (driver != NULL && driver->bus.delay_ms != NULL) {
    driver->bus.delay_ms(driver->bus_context, delay_ms);
  }
}

static ErrorStatus display_st7789_draw_fast_hline(display_st7789_context_t *driver, int16_t x, int16_t y,
                                                  int16_t width, uint16_t color) {
  return display_st7789_fill_rect(driver, x, y, width, 1, color);
}

static ErrorStatus display_st7789_draw_fast_vline(display_st7789_context_t *driver, int16_t x, int16_t y,
                                                  int16_t height, uint16_t color) {
  return display_st7789_fill_rect(driver, x, y, 1, height, color);
}

static ErrorStatus display_st7789_draw_rect(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                            int16_t x1, int16_t y1, uint16_t color) {
  int16_t left = display_st7789_min_i16(x0, x1);
  int16_t top = display_st7789_min_i16(y0, y1);
  int16_t right = (int16_t)(x0 > x1 ? x0 : x1);
  int16_t bottom = (int16_t)(y0 > y1 ? y0 : y1);

  if (display_st7789_draw_fast_hline(driver, left, top, (int16_t)(right - left + 1), color) != SUCCESS ||
      display_st7789_draw_fast_hline(driver, left, bottom, (int16_t)(right - left + 1), color) != SUCCESS ||
      display_st7789_draw_fast_vline(driver, left, top, (int16_t)(bottom - top + 1), color) != SUCCESS ||
      display_st7789_draw_fast_vline(driver, right, top, (int16_t)(bottom - top + 1), color) != SUCCESS) {
    return ERROR;
  }

  return SUCCESS;
}

static ErrorStatus display_st7789_fill_box(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                           int16_t x1, int16_t y1, uint16_t color) {
  int16_t left = display_st7789_min_i16(x0, x1);
  int16_t top = display_st7789_min_i16(y0, y1);
  int16_t right = (int16_t)(x0 > x1 ? x0 : x1);
  int16_t bottom = (int16_t)(y0 > y1 ? y0 : y1);

  return display_st7789_fill_rect(driver, left, top, (int16_t)(right - left + 1),
                                  (int16_t)(bottom - top + 1), color);
}

static ErrorStatus display_st7789_draw_circle_points(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                                     int16_t x, int16_t y, uint16_t color) {
  display_point_t points[8U] = {
    {(int16_t)(x0 + x), (int16_t)(y0 + y)},
    {(int16_t)(x0 + y), (int16_t)(y0 + x)},
    {(int16_t)(x0 - y), (int16_t)(y0 + x)},
    {(int16_t)(x0 - x), (int16_t)(y0 + y)},
    {(int16_t)(x0 - x), (int16_t)(y0 - y)},
    {(int16_t)(x0 - y), (int16_t)(y0 - x)},
    {(int16_t)(x0 + y), (int16_t)(y0 - x)},
    {(int16_t)(x0 + x), (int16_t)(y0 - y)},
  };

  for (size_t i = 0U; i < sizeof(points) / sizeof(points[0]); i++) {
    if (display_st7789_point_in_bounds(driver, points[i]) &&
        display_st7789_draw_pixel(driver, points[i], color) != SUCCESS) {
      return ERROR;
    }
  }

  return SUCCESS;
}

static ErrorStatus display_st7789_fill_circle_spans(display_st7789_context_t *driver, int16_t x0, int16_t y0,
                                                    int16_t x, int16_t y, uint16_t color) {
  if (display_st7789_draw_fast_hline(driver, (int16_t)(x0 - x), (int16_t)(y0 + y), (int16_t)((2 * x) + 1),
                                     color) != SUCCESS ||
      display_st7789_draw_fast_hline(driver, (int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)((2 * x) + 1),
                                     color) != SUCCESS ||
      display_st7789_draw_fast_hline(driver, (int16_t)(x0 - y), (int16_t)(y0 + x), (int16_t)((2 * y) + 1),
                                     color) != SUCCESS ||
      display_st7789_draw_fast_hline(driver, (int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)((2 * y) + 1),
                                     color) != SUCCESS) {
    return ERROR;
  }

  return SUCCESS;
}

static int16_t display_st7789_min_i16(int16_t a, int16_t b) {
  return a < b ? a : b;
}

static int16_t display_st7789_abs_i16(int16_t value) {
  return value < 0 ? (int16_t)-value : value;
}
