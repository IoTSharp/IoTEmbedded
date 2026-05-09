#include "Board/Inc/bsp_watchdog.h"

HAL_StatusTypeDef bsp_watchdog_refresh(void) {
  return HAL_IWDG_Refresh(BSP_IWDG_HANDLE);
}
