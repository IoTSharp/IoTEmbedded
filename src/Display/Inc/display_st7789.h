#ifndef DISPLAY_ST7789_H
#define DISPLAY_ST7789_H

#include "Common/Inc/app_types.h"
#include "Display/Inc/display_driver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  ErrorStatus (*set_power)(void *context, bool enabled);
  ErrorStatus (*set_reset)(void *context, bool asserted);
  ErrorStatus (*set_chip_select)(void *context, bool selected);
  ErrorStatus (*set_data_command)(void *context, bool data_mode);
  ErrorStatus (*write_bytes)(void *context, const uint8_t *data, size_t length);
  void (*delay_ms)(void *context, uint32_t delay_ms);
} display_st7789_bus_t;

typedef struct {
  void *bus_context;
  display_st7789_bus_t bus;
  display_size_t size;
  uint16_t x_offset;
  uint16_t y_offset;
  display_color_pair_t colors;
  display_text_cursor_t cursor;
  bool initialized;
} display_st7789_context_t;

const display_driver_t *display_st7789_driver(void);

#ifdef __cplusplus
}
#endif

#endif
