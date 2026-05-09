#ifndef BSP_UART_H
#define BSP_UART_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef bsp_uart_write(UART_HandleTypeDef *uart, const uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_uart_read(UART_HandleTypeDef *uart, uint8_t *data, uint16_t length, uint32_t timeout_ms);
// 运行期切换共享 UART 的 8N1 波特率；用于 UART4 在 Air724UG 与 CH395Q/CN2 跳线模式之间复用。
HAL_StatusTypeDef bsp_uart_configure_8n1(UART_HandleTypeDef *uart, uint32_t baudrate);
// 清掉 UART 接收寄存器里的残留字节，避免上一条 AT/CH395 命令污染下一次同步帧。
void bsp_uart_flush_rx(UART_HandleTypeDef *uart);
HAL_StatusTypeDef bsp_debug_write(const char *text);
HAL_StatusTypeDef bsp_debug_write_bytes(const uint8_t *data, uint16_t length);
HAL_StatusTypeDef bsp_debug_start_receive_it(void);
void bsp_uart_rx_complete_callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif
