#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Bus/Uart/Inc/bsp_uart.h"

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  bsp_rs485_rx_complete_callback(huart);
  bsp_uart_rx_complete_callback(huart);
}
