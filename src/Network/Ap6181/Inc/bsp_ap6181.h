#ifndef BSP_AP6181_H
#define BSP_AP6181_H

#include "Board/Inc/bsp_hal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bsp_ap6181_prepare_pins(void);
void bsp_ap6181_enable(void);
void bsp_ap6181_disable(void);
bool bsp_ap6181_is_enabled(void);
GPIO_PinState bsp_ap6181_read_irq(void);
const char *bsp_ap6181_pin_map(void);

#ifdef __cplusplus
}
#endif

#endif
