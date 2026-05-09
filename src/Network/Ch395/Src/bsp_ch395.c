#include "Network/Ch395/Inc/bsp_ch395.h"

static void bsp_ch395_config_rst_pin_input(void);

void bsp_ch395_prepare_boot_pins(void) {
  /* 当前固件只通过 UART4/CN2 访问 CH395Q；PB12/PB0 仍在复位前保持空闲态，
   * 避免这些引脚浮动影响芯片接口模式采样。 */
  HAL_GPIO_WritePin(BSP_CH395_SCS_GPIO_Port, BSP_CH395_SCS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_CH395_SEL_GPIO_Port, BSP_CH395_SEL_Pin, GPIO_PIN_SET);
  bsp_ch395_config_rst_pin_input();
}

void bsp_ch395_assert_reset(void) {
  bsp_ch395_prepare_boot_pins();
  HAL_GPIO_WritePin(BSP_CH395_RSTI_GPIO_Port, BSP_CH395_RSTI_Pin, GPIO_PIN_RESET);
}

void bsp_ch395_release_reset(void) {
  bsp_ch395_prepare_boot_pins();
  HAL_GPIO_WritePin(BSP_CH395_RSTI_GPIO_Port, BSP_CH395_RSTI_Pin, GPIO_PIN_SET);
}

bool bsp_ch395_is_reset_asserted(void) {
  return HAL_GPIO_ReadPin(BSP_CH395_RSTI_GPIO_Port, BSP_CH395_RSTI_Pin) == GPIO_PIN_RESET;
}

void bsp_ch395_reset(void) {
  bsp_ch395_assert_reset();
  bsp_delay_ms(20U);
  bsp_ch395_release_reset();
  bsp_delay_ms(300U);
}

static void bsp_ch395_config_rst_pin_input(void) {
  GPIO_InitTypeDef gpio = {0};

  // CH395Q 的 RST 是复位状态输出脚，不是 MCU 复位控制脚；保持输入可避免与芯片输出互相驱动。
  gpio.Pin = BSP_CH395_RST_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BSP_CH395_RST_GPIO_Port, &gpio);
}
