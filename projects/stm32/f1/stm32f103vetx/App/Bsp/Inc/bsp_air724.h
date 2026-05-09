#ifndef BSP_AIR724_H
#define BSP_AIR724_H

#include "bsp_board.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Air724UG 当前原理图只确认 PE2 复位脚，未确认独立电源 EN。
 * 这里的 assert/release 用于链路切换时做“复位隔离”，不是物理断电。 */
void bsp_air724_assert_reset(void);
void bsp_air724_release_reset(void);
bool bsp_air724_is_reset_asserted(void);
void bsp_air724_reset(void);
GPIO_PinState bsp_air724_read_netstate(void);
HAL_StatusTypeDef bsp_air724_uart_write(const uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_air724_uart_read(uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_air724_at_command(const char *command, char *response, uint16_t response_size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
