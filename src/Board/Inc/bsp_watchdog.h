#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

#include "Board/Inc/bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef bsp_watchdog_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
