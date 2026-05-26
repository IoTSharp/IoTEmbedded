#include "Bus/Uart/Inc/bsp_uart.h"

#include "Config/Inc/config.h"
#include <string.h>

static uint8_t debug_rx_byte;

HAL_StatusTypeDef bsp_uart_write(UART_HandleTypeDef *uart, const uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  if (uart == NULL || data == NULL || length == 0U) {
    return HAL_ERROR;
  }
  return HAL_UART_Transmit(uart, (uint8_t *)data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_uart_read(UART_HandleTypeDef *uart, uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  if (uart == NULL || data == NULL || length == 0U) {
    return HAL_ERROR;
  }
  return HAL_UART_Receive(uart, data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_uart_configure_8n1(UART_HandleTypeDef *uart, uint32_t baudrate) {
  if (uart == NULL || baudrate == 0U) {
    return HAL_ERROR;
  }

  (void)HAL_UART_DeInit(uart);
  uart->Init.BaudRate = baudrate;
  uart->Init.WordLength = UART_WORDLENGTH_8B;
  uart->Init.StopBits = UART_STOPBITS_1;
  uart->Init.Parity = UART_PARITY_NONE;
  uart->Init.Mode = UART_MODE_TX_RX;
  uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart->Init.OverSampling = UART_OVERSAMPLING_16;
  return HAL_UART_Init(uart);
}

void bsp_uart_flush_rx(UART_HandleTypeDef *uart) {
  if (uart == NULL) {
    return;
  }

  /* STM32F1 UART 没有硬件 FIFO，但 RXNE/ORE 可能残留一字节或错误状态。
   * CH395Q UART 帧没有长度头，下一条命令前必须清干净，否则会把旧字节误当成返回值。 */
  for (uint8_t guard = 0U; guard < 32U && __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET; guard++) {
    __HAL_UART_FLUSH_DRREGISTER(uart);
  }
  __HAL_UART_CLEAR_OREFLAG(uart);
}

HAL_StatusTypeDef bsp_debug_write(const char *text) {
  if (text == NULL) {
    return HAL_ERROR;
  }
  return bsp_debug_write_bytes((const uint8_t *)text, (uint16_t)strlen(text));
}

HAL_StatusTypeDef bsp_debug_write_bytes(const uint8_t *data, uint16_t length) {
  return bsp_uart_write(BSP_DEBUG_UART_HANDLE, data, length, HAL_MAX_DELAY);
}

HAL_StatusTypeDef bsp_debug_start_receive_it(void) {
  return HAL_UART_Receive_IT(BSP_DEBUG_UART_HANDLE, &debug_rx_byte, 1U);
}

void bsp_uart_rx_complete_callback(UART_HandleTypeDef *huart) {
  if (huart == BSP_DEBUG_UART_HANDLE) {
    config_receive_cmd_byte(debug_rx_byte);
    (void)bsp_debug_start_receive_it();
  }
}

int __io_putchar(int ch) {
  uint8_t byte = (uint8_t)ch;
  (void)bsp_debug_write_bytes(&byte, 1U);
  return ch;
}
