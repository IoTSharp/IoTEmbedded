#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "Common/Inc/app_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t width;
  uint16_t height;
} display_size_t;

typedef struct {
  bool can_draw_text;
  bool can_draw_graphics;
  bool can_read_pixels;
  bool qb45_screen;
  bool qb45_color;
  bool qb45_cls;
  bool qb45_locate;
  bool qb45_pset;
  bool qb45_preset;
  bool qb45_line;
  bool qb45_circle;
  bool qb45_paint;
} display_qb45_capabilities_t;

typedef struct {
  uint16_t fg;
  uint16_t bg;
} display_color_pair_t;

typedef struct {
  uint16_t row;
  uint16_t col;
} display_text_cursor_t;

typedef struct {
  uint16_t x;
  uint16_t y;
} display_point_t;

typedef struct {
  display_point_t start;
  display_point_t end;
  uint16_t color;
  bool box;
  bool fill;
} display_line_request_t;

typedef struct {
  display_point_t center;
  uint16_t radius;
  uint16_t color;
  bool fill;
} display_circle_request_t;

typedef struct {
  display_point_t point;
  uint16_t fill_color;
  uint16_t border_color;
} display_paint_request_t;

typedef struct {
  const char *name;
  const char *controller;
  display_size_t size;
  display_qb45_capabilities_t qb45;
} display_descriptor_t;

typedef struct {
  ErrorStatus (*configure_screen)(void *context, int mode);
  ErrorStatus (*clear)(void *context, uint16_t color);
  ErrorStatus (*set_colors)(void *context, display_color_pair_t colors);
  ErrorStatus (*set_cursor)(void *context, display_text_cursor_t cursor);
  ErrorStatus (*write_text)(void *context, const char *text);
  ErrorStatus (*draw_pixel)(void *context, display_point_t point, uint16_t color);
  ErrorStatus (*draw_line)(void *context, const display_line_request_t *request);
  ErrorStatus (*draw_circle)(void *context, const display_circle_request_t *request);
  ErrorStatus (*paint)(void *context, const display_paint_request_t *request);
} display_driver_ops_t;

typedef struct {
  display_descriptor_t descriptor;
  display_driver_ops_t ops;
} display_driver_t;

#ifdef __cplusplus
}
#endif

#endif
