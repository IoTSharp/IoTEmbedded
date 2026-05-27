#ifndef DISPLAY_API_H
#define DISPLAY_API_H

#include "Common/Inc/app_types.h"
#include "Display/Inc/display_driver.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

ErrorStatus display_api_bind(const display_driver_t *driver, void *context);
bool display_api_is_available(void);
const display_descriptor_t *display_api_descriptor(void);

ErrorStatus display_api_screen(int mode);
ErrorStatus display_api_cls(uint16_t color);
ErrorStatus display_api_color(display_color_pair_t colors);
ErrorStatus display_api_locate(display_text_cursor_t cursor);
ErrorStatus display_api_write_text(const char *text);
ErrorStatus display_api_pset(display_point_t point, uint16_t color);
ErrorStatus display_api_preset(display_point_t point, uint16_t color);
ErrorStatus display_api_line(const display_line_request_t *request);
ErrorStatus display_api_circle(const display_circle_request_t *request);
ErrorStatus display_api_paint(const display_paint_request_t *request);

#ifdef __cplusplus
}
#endif

#endif
