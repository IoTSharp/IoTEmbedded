#include "Network/Ap6181/Inc/bsp_ap6181.h"

#include "Board/Inc/bsp_board.h"

void bsp_ap6181_prepare_pins(void) {
#if BSP_HAS_AP6181
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

#if defined(UART4)
  __HAL_RCC_UART4_CLK_DISABLE();
#endif
#if defined(UART5)
  __HAL_RCC_UART5_CLK_DISABLE();
#endif
  HAL_GPIO_DeInit(BSP_AP6181_D2_GPIO_Port, BSP_AP6181_D2_Pin | BSP_AP6181_D3_Pin | BSP_AP6181_CLK_Pin);
  HAL_GPIO_DeInit(BSP_AP6181_CMD_GPIO_Port, BSP_AP6181_CMD_Pin);

  gpio.Pin = BSP_AP6181_D0_Pin | BSP_AP6181_D1_Pin | BSP_AP6181_D2_Pin | BSP_AP6181_D3_Pin | BSP_AP6181_CLK_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(BSP_AP6181_D0_GPIO_Port, &gpio);

  gpio.Pin = BSP_AP6181_CMD_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(BSP_AP6181_CMD_GPIO_Port, &gpio);

  gpio.Pin = BSP_AP6181_IRQ_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BSP_AP6181_IRQ_GPIO_Port, &gpio);

  HAL_GPIO_WritePin(BSP_AP6181_EN_GPIO_Port, BSP_AP6181_EN_Pin, GPIO_PIN_RESET);
  gpio.Pin = BSP_AP6181_EN_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BSP_AP6181_EN_GPIO_Port, &gpio);
#endif
}

void bsp_ap6181_prepare_irq_interrupt(void) {
#if BSP_HAS_AP6181
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
#if defined(__HAL_RCC_SYSCFG_CLK_ENABLE)
  __HAL_RCC_SYSCFG_CLK_ENABLE();
#endif

  gpio.Pin = BSP_AP6181_IRQ_Pin;
  gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BSP_AP6181_IRQ_GPIO_Port, &gpio);

#if defined(EXTI9_5_IRQn)
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
#endif
#endif
}

void bsp_ap6181_enable(void) {
#if BSP_HAS_AP6181
  HAL_GPIO_WritePin(BSP_AP6181_EN_GPIO_Port, BSP_AP6181_EN_Pin, GPIO_PIN_SET);
#endif
}

void bsp_ap6181_disable(void) {
#if BSP_HAS_AP6181
  HAL_GPIO_WritePin(BSP_AP6181_EN_GPIO_Port, BSP_AP6181_EN_Pin, GPIO_PIN_RESET);
#endif
}

bool bsp_ap6181_is_enabled(void) {
#if BSP_HAS_AP6181
  return HAL_GPIO_ReadPin(BSP_AP6181_EN_GPIO_Port, BSP_AP6181_EN_Pin) == GPIO_PIN_SET;
#else
  return false;
#endif
}

GPIO_PinState bsp_ap6181_read_irq(void) {
#if BSP_HAS_AP6181
  return HAL_GPIO_ReadPin(BSP_AP6181_IRQ_GPIO_Port, BSP_AP6181_IRQ_Pin);
#else
  return GPIO_PIN_RESET;
#endif
}

const char *bsp_ap6181_pin_map(void) {
  return "SDMMC1 PC8-D0 PC9-D1 PC10-D2 PC11-D3 PC12-CLK PD2-CMD, IRQ PC5, EN PD1";
}
