#include "Board/Pandora/Inc/pandora_display.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Display/Inc/display_api.h"
#include "Display/Inc/display_st7789.h"

static ErrorStatus pandora_display_set_power(void *context, bool enabled);
static ErrorStatus pandora_display_set_reset(void *context, bool asserted);
static ErrorStatus pandora_display_set_chip_select(void *context, bool selected);
static ErrorStatus pandora_display_set_data_command(void *context, bool data_mode);
static ErrorStatus pandora_display_write_bytes(void *context, const uint8_t *data, size_t length);
static void pandora_display_delay_ms(void *context, uint32_t delay_ms);

static display_st7789_context_t pandora_display_context = {
  .bus_context = NULL,
  .bus =
    {
      .set_power = pandora_display_set_power,
      .set_reset = pandora_display_set_reset,
      .set_chip_select = pandora_display_set_chip_select,
      .set_data_command = pandora_display_set_data_command,
      .write_bytes = pandora_display_write_bytes,
      .delay_ms = pandora_display_delay_ms,
    },
  .size = {BSP_DISPLAY_WIDTH, BSP_DISPLAY_HEIGHT},
  .colors = {0xFFFFU, 0x0000U},
  .cursor = {1U, 1U},
};

ErrorStatus pandora_display_init(void) {
  const board_resource_t *display = bsp_board_display_resource();
  if (display == NULL || display->display_kind != BOARD_DISPLAY_KIND_TFT_LCD) {
    return ERROR;
  }

  if (display_api_bind(display_st7789_driver(), &pandora_display_context) != SUCCESS) {
    return ERROR;
  }

  LOG_INFO("Pandora display bound: %s controller=%s size=%ux%u", display->name, display->display_controller,
           (unsigned int)display->display_width, (unsigned int)display->display_height);
  return SUCCESS;
}

static ErrorStatus pandora_display_set_power(void *context, bool enabled) {
  (void)context;
  (void)enabled;
  return ERROR;
}

static ErrorStatus pandora_display_set_reset(void *context, bool asserted) {
  (void)context;
  (void)asserted;
  return ERROR;
}

static ErrorStatus pandora_display_set_chip_select(void *context, bool selected) {
  (void)context;
  (void)selected;
  return ERROR;
}

static ErrorStatus pandora_display_set_data_command(void *context, bool data_mode) {
  (void)context;
  (void)data_mode;
  return ERROR;
}

static ErrorStatus pandora_display_write_bytes(void *context, const uint8_t *data, size_t length) {
  (void)context;
  (void)data;
  (void)length;
  return ERROR;
}

static void pandora_display_delay_ms(void *context, uint32_t delay_ms) {
  (void)context;
  bsp_delay_ms(delay_ms);
}
