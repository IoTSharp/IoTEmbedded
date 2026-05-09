#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"

void bsp_board_init(void) {
  HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_Port, BSP_RS485_RE_Pin, GPIO_PIN_RESET);
  // CH395Q 现在只走 UART4/CN2，启动时仍要把 SEL 拉高，避免芯片误判成别的接口模式。
  bsp_ch395_prepare_boot_pins();
  // 最新 SVG 原理图确认 PA4/RST1 是低有效外部复位输入；PA5/RST 是 CH395Q 复位输出，只能读取不能由 MCU 驱动。
  HAL_GPIO_WritePin(BSP_CH395_RSTI_GPIO_Port, BSP_CH395_RSTI_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_BUZZER_GPIO_Port, BSP_BUZZER_Pin, GPIO_PIN_RESET);
}

void bsp_delay_ms(uint32_t ms) {
  HAL_Delay(ms);
}

void bsp_delay_us(uint32_t us) {
  if (us == 0U) {
    return;
  }

  // CH395Q 命令字后要求微秒级保持时间；DWT 周期计数比空循环更不依赖编译优化。
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = (SystemCoreClock / 1000000U) * us;
  while ((DWT->CYCCNT - start) < cycles) {
  }
}

uint32_t bsp_get_tick_ms(void) {
  return HAL_GetTick();
}
