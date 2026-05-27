#include "Display/Inc/display_api.h"

typedef struct {
  const display_driver_t *driver;
  void *context;
} display_api_state_t;

static display_api_state_t display_api_state;

static bool display_api_can_call(void *function_pointer) {
  return display_api_state.driver != NULL && function_pointer != NULL;
}

ErrorStatus display_api_bind(const display_driver_t *driver, void *context) {
  if (driver == NULL) {
    return ERROR;
  }

  display_api_state.driver = driver;
  display_api_state.context = context;
  return SUCCESS;
}

bool display_api_is_available(void) {
  return display_api_state.driver != NULL;
}

const display_descriptor_t *display_api_descriptor(void) {
  return display_api_state.driver == NULL ? NULL : &display_api_state.driver->descriptor;
}

ErrorStatus display_api_screen(int mode) {
  if (!display_api_can_call((void *)display_api_state.driver->ops.configure_screen)) {
    return ERROR;
  }
  return display_api_state.driver->ops.configure_screen(display_api_state.context, mode);
}

ErrorStatus display_api_cls(uint16_t color) {
  if (!display_api_can_call((void *)display_api_state.driver->ops.clear)) {
    return ERROR;
  }
  return display_api_state.driver->ops.clear(display_api_state.context, color);
}

ErrorStatus display_api_color(display_color_pair_t colors) {
  if (!display_api_can_call((void *)display_api_state.driver->ops.set_colors)) {
    return ERROR;
  }
  return display_api_state.driver->ops.set_colors(display_api_state.context, colors);
}

ErrorStatus display_api_locate(display_text_cursor_t cursor) {
  if (!display_api_can_call((void *)display_api_state.driver->ops.set_cursor)) {
    return ERROR;
  }
  return display_api_state.driver->ops.set_cursor(display_api_state.context, cursor);
}

ErrorStatus display_api_write_text(const char *text) {
  if (text == NULL || !display_api_can_call((void *)display_api_state.driver->ops.write_text)) {
    return ERROR;
  }
  return display_api_state.driver->ops.write_text(display_api_state.context, text);
}

ErrorStatus display_api_pset(display_point_t point, uint16_t color) {
  if (!display_api_can_call((void *)display_api_state.driver->ops.draw_pixel)) {
    return ERROR;
  }
  return display_api_state.driver->ops.draw_pixel(display_api_state.context, point, color);
}

ErrorStatus display_api_preset(display_point_t point, uint16_t color) {
  return display_api_pset(point, color);
}

ErrorStatus display_api_line(const display_line_request_t *request) {
  if (request == NULL || !display_api_can_call((void *)display_api_state.driver->ops.draw_line)) {
    return ERROR;
  }
  return display_api_state.driver->ops.draw_line(display_api_state.context, request);
}

ErrorStatus display_api_circle(const display_circle_request_t *request) {
  if (request == NULL || !display_api_can_call((void *)display_api_state.driver->ops.draw_circle)) {
    return ERROR;
  }
  return display_api_state.driver->ops.draw_circle(display_api_state.context, request);
}

ErrorStatus display_api_paint(const display_paint_request_t *request) {
  if (request == NULL || !display_api_can_call((void *)display_api_state.driver->ops.paint)) {
    return ERROR;
  }
  return display_api_state.driver->ops.paint(display_api_state.context, request);
}
