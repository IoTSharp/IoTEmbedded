#ifndef BSP_RS485_H
#define BSP_RS485_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_rs485_set_receive(void);
void bsp_rs485_set_transmit(void);
// 返回 USART1 当前运行波特率，便于现场确认主机侧 485 参数。
uint32_t bsp_rs485_get_baud_rate(void);
// 运行时调整 USART1 波特率；仅改本机 485 口，不会写入任何从站寄存器。
HAL_StatusTypeDef bsp_rs485_set_baud_rate(uint32_t baud_rate);
HAL_StatusTypeDef bsp_rs485_write(const uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_rs485_read(uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_rs485_start_receive_it(void);
void bsp_rs485_rx_complete_callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif
