#include "bsp_rs485.h"

#include "bsp_uart.h"
#include "modbus_api.h"

#define BSP_RS485_TX_ENABLE_SETTLE_US 50U

static uint8_t rs485_rx_byte;

void bsp_rs485_set_receive(void) {
  HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_Port, BSP_RS485_RE_Pin, GPIO_PIN_RESET);
}

void bsp_rs485_set_transmit(void) {
  HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_Port, BSP_RS485_RE_Pin, GPIO_PIN_SET);
}

uint32_t bsp_rs485_get_baud_rate(void) {
  return BSP_RS485_UART_HANDLE->Init.BaudRate;
}

HAL_StatusTypeDef bsp_rs485_set_baud_rate(uint32_t baud_rate) {
  if (baud_rate == 0U) {
    return HAL_ERROR;
  }

  /*
   * 485 总线波特率改变时，先停止当前中断接收，重新初始化 USART1 后再回到接收态。
   * 这里只改 MCU 侧串口参数；从站自己的波特率寄存器由设备层命令单独写入。
   */
  (void)HAL_UART_AbortReceive_IT(BSP_RS485_UART_HANDLE);
  bsp_rs485_set_receive();
  BSP_RS485_UART_HANDLE->Init.BaudRate = baud_rate;
  HAL_StatusTypeDef status = HAL_UART_Init(BSP_RS485_UART_HANDLE);
  if (status == HAL_OK) {
    (void)bsp_rs485_start_receive_it();
  }
  return status;
}

HAL_StatusTypeDef bsp_rs485_write(const uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  HAL_StatusTypeDef status;

  bsp_rs485_set_transmit();
  /*
   * PA12 同时控制 485 收发方向，拉高后给收发器一个很短的使能建立时间。
   * 这里不要改成毫秒级延时，否则会无谓拉长整条 Modbus 总线轮询周期。
   */
  bsp_delay_us(BSP_RS485_TX_ENABLE_SETTLE_US);
  status = bsp_uart_write(BSP_RS485_UART_HANDLE, data, length, timeout_ms);
  while (__HAL_UART_GET_FLAG(BSP_RS485_UART_HANDLE, UART_FLAG_TC) == RESET) {
  }
  bsp_rs485_set_receive();

  return status;
}

HAL_StatusTypeDef bsp_rs485_read(uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  bsp_rs485_set_receive();
  return bsp_uart_read(BSP_RS485_UART_HANDLE, data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_rs485_start_receive_it(void) {
  bsp_rs485_set_receive();
  return HAL_UART_Receive_IT(BSP_RS485_UART_HANDLE, &rs485_rx_byte, 1U);
}

void bsp_rs485_rx_complete_callback(UART_HandleTypeDef *huart) {
  if (huart == BSP_RS485_UART_HANDLE) {
    modbus_rx_byte(rs485_rx_byte);
    (void)bsp_rs485_start_receive_it();
  }
}
