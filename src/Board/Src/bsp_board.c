#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
#include "Board/Pandora/Inc/pandora_display.h"
#include "Board/Pandora/Inc/pandora_board_resources.h"
#else
#include "Board/PowerEnvDaq/Inc/power_env_daq_board_resources.h"
#endif

void bsp_board_init(void) {
#if BSP_ENABLE_FULL_IO_MAP
  HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_Port, BSP_RS485_RE_Pin, GPIO_PIN_RESET);
  // CH395Q 现在只走 UART4/CN2，启动时仍要把 SEL 拉高，避免芯片误判成别的接口模式。
  bsp_ch395_prepare_boot_pins();
  // 最新 SVG 原理图确认 PA4/RST1 是低有效外部复位输入；PA5/RST 是 CH395Q 复位输出，只能读取不能由 MCU 驱动。
  HAL_GPIO_WritePin(BSP_CH395_RSTI_GPIO_Port, BSP_CH395_RSTI_Pin, GPIO_PIN_SET);
#endif
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

const board_resource_t *bsp_board_resources(size_t *count) {
#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
  return pandora_board_resources(count);
#else
  return power_env_daq_board_resources(count);
#endif
}

const board_resource_t *bsp_board_display_resource(void) {
  size_t count = 0U;
  const board_resource_t *resources = bsp_board_resources(&count);
  if (resources == NULL) {
    return NULL;
  }

  for (size_t i = 0U; i < count; i++) {
    if (resources[i].category == BOARD_RESOURCE_DISPLAY) {
      return &resources[i];
    }
  }

  return NULL;
}

ErrorStatus bsp_board_display_init(void) {
#if BSP_HAS_DISPLAY
  return pandora_display_init();
#else
  return ERROR;
#endif
}
