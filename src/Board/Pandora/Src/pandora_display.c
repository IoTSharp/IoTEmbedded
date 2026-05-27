#include "Board/Pandora/Inc/pandora_display.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Display/Inc/display_api.h"
#include "Display/Inc/display_st7789.h"

#if BSP_HAS_DISPLAY
#define PANDORA_LCD_SPI                     SPI3
#define PANDORA_LCD_SPI_TIMEOUT_TICKS       1000000U
#define PANDORA_LCD_SPI_BAUD_PRESCALER_BITS 0U
#define PANDORA_LCD_SPI_MODE_BITS           (SPI_CR1_CPOL | SPI_CR1_CPHA)
#define PANDORA_LCD_DC_GPIO_Port            GPIOB
#define PANDORA_LCD_DC_Pin                  GPIO_PIN_4
#define PANDORA_LCD_RES_GPIO_Port           GPIOB
#define PANDORA_LCD_RES_Pin                 GPIO_PIN_6
#define PANDORA_LCD_PWR_GPIO_Port           GPIOB
#define PANDORA_LCD_PWR_Pin                 GPIO_PIN_7
#define PANDORA_LCD_CS_GPIO_Port            GPIOD
#define PANDORA_LCD_CS_Pin                  GPIO_PIN_7
#define PANDORA_LCD_SCK_GPIO_Port           GPIOB
#define PANDORA_LCD_SCK_Pin                 GPIO_PIN_3
#define PANDORA_LCD_MOSI_GPIO_Port          GPIOB
#define PANDORA_LCD_MOSI_Pin                GPIO_PIN_5
#endif

static ErrorStatus pandora_display_bus_init(void);
static ErrorStatus pandora_display_spi_write(const uint8_t *data, size_t length);
static bool pandora_display_spi_wait_flag(uint32_t flag, bool set, uint32_t timeout_ticks);
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
  .x_offset = 0U,
  .y_offset = 0U,
  .colors = {0xFFFFU, 0x0000U},
  .cursor = {1U, 1U},
  .initialized = false,
};

ErrorStatus pandora_display_init(void) {
  const board_resource_t *display = bsp_board_display_resource();
  if (display == NULL || display->display_kind != BOARD_DISPLAY_KIND_TFT_LCD) {
    return ERROR;
  }
  if (pandora_display_bus_init() != SUCCESS) {
    LOG_ERROR("Pandora display bus init failed");
    return ERROR;
  }

  if (display_api_bind(display_st7789_driver(), &pandora_display_context) != SUCCESS) {
    return ERROR;
  }
  if (display_api_screen(0) != SUCCESS) {
    LOG_ERROR("Pandora ST7789 init sequence failed");
    return ERROR;
  }

  LOG_INFO("Pandora display bound: %s controller=%s size=%ux%u", display->name, display->display_controller,
           (unsigned int)display->display_width, (unsigned int)display->display_height);
  return SUCCESS;
}

static ErrorStatus pandora_display_bus_init(void) {
#if BSP_HAS_DISPLAY
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_SPI3_CLK_ENABLE();

  HAL_GPIO_WritePin(PANDORA_LCD_CS_GPIO_Port, PANDORA_LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PANDORA_LCD_DC_GPIO_Port, PANDORA_LCD_DC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PANDORA_LCD_RES_GPIO_Port, PANDORA_LCD_RES_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PANDORA_LCD_PWR_GPIO_Port, PANDORA_LCD_PWR_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = PANDORA_LCD_SCK_Pin | PANDORA_LCD_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(PANDORA_LCD_SCK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PANDORA_LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(PANDORA_LCD_DC_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PANDORA_LCD_RES_Pin | PANDORA_LCD_PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PANDORA_LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(PANDORA_LCD_CS_GPIO_Port, &GPIO_InitStruct);

  PANDORA_LCD_SPI->CR1 = 0U;
  PANDORA_LCD_SPI->CR2 = 0U;
  PANDORA_LCD_SPI->CR1 = PANDORA_LCD_SPI_MODE_BITS | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                         SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | PANDORA_LCD_SPI_BAUD_PRESCALER_BITS;
  PANDORA_LCD_SPI->CR2 = (7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
  PANDORA_LCD_SPI->CR1 |= SPI_CR1_SPE;
  return SUCCESS;
#else
  return ERROR;
#endif
}

static ErrorStatus pandora_display_spi_write(const uint8_t *data, size_t length) {
#if BSP_HAS_DISPLAY
  if (data == NULL && length > 0U) {
    return ERROR;
  }

  for (size_t i = 0U; i < length; i++) {
    if (!pandora_display_spi_wait_flag(SPI_SR_TXE, true, PANDORA_LCD_SPI_TIMEOUT_TICKS)) {
      return ERROR;
    }
    *((__IO uint8_t *)&PANDORA_LCD_SPI->DR) = data[i];
  }

  if (!pandora_display_spi_wait_flag(SPI_SR_TXE, true, PANDORA_LCD_SPI_TIMEOUT_TICKS) ||
      !pandora_display_spi_wait_flag(SPI_SR_BSY, false, PANDORA_LCD_SPI_TIMEOUT_TICKS)) {
    return ERROR;
  }

  return SUCCESS;
#else
  (void)data;
  (void)length;
  return ERROR;
#endif
}

static bool pandora_display_spi_wait_flag(uint32_t flag, bool set, uint32_t timeout_ticks) {
#if BSP_HAS_DISPLAY
  while (((PANDORA_LCD_SPI->SR & flag) != 0U) != set) {
    if (timeout_ticks == 0U) {
      return false;
    }
    timeout_ticks--;
  }
  return true;
#else
  (void)flag;
  (void)set;
  (void)timeout_ticks;
  return false;
#endif
}

static ErrorStatus pandora_display_set_power(void *context, bool enabled) {
  (void)context;
#if BSP_HAS_DISPLAY
  HAL_GPIO_WritePin(PANDORA_LCD_PWR_GPIO_Port, PANDORA_LCD_PWR_Pin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return SUCCESS;
#else
  (void)enabled;
  return ERROR;
#endif
}

static ErrorStatus pandora_display_set_reset(void *context, bool asserted) {
  (void)context;
#if BSP_HAS_DISPLAY
  HAL_GPIO_WritePin(PANDORA_LCD_RES_GPIO_Port, PANDORA_LCD_RES_Pin, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
  return SUCCESS;
#else
  (void)asserted;
  return ERROR;
#endif
}

static ErrorStatus pandora_display_set_chip_select(void *context, bool selected) {
  (void)context;
#if BSP_HAS_DISPLAY
  (void)selected;
  HAL_GPIO_WritePin(PANDORA_LCD_CS_GPIO_Port, PANDORA_LCD_CS_Pin, GPIO_PIN_RESET);
  return SUCCESS;
#else
  (void)selected;
  return ERROR;
#endif
}

static ErrorStatus pandora_display_set_data_command(void *context, bool data_mode) {
  (void)context;
#if BSP_HAS_DISPLAY
  HAL_GPIO_WritePin(PANDORA_LCD_DC_GPIO_Port, PANDORA_LCD_DC_Pin, data_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return SUCCESS;
#else
  (void)data_mode;
  return ERROR;
#endif
}

static ErrorStatus pandora_display_write_bytes(void *context, const uint8_t *data, size_t length) {
  (void)context;
  return pandora_display_spi_write(data, length);
}

static void pandora_display_delay_ms(void *context, uint32_t delay_ms) {
  (void)context;
  bsp_delay_ms(delay_ms);
}
