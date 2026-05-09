#include "Modem/Inc/bsp_air724.h"

#include "Bus/Uart/Inc/bsp_uart.h"
#include <string.h>

void bsp_air724_assert_reset(void) {
  // Air724UG 的 RST 低电平有效；保持低电平可让模块退出 UART4 总线竞争，但不等同于切断 VBAT/VCC。
  HAL_GPIO_WritePin(BSP_AIR4G_RST_GPIO_Port, BSP_AIR4G_RST_Pin, GPIO_PIN_RESET);
}

void bsp_air724_release_reset(void) {
  HAL_GPIO_WritePin(BSP_AIR4G_RST_GPIO_Port, BSP_AIR4G_RST_Pin, GPIO_PIN_SET);
}

bool bsp_air724_is_reset_asserted(void) {
  return HAL_GPIO_ReadPin(BSP_AIR4G_RST_GPIO_Port, BSP_AIR4G_RST_Pin) == GPIO_PIN_RESET;
}

void bsp_air724_reset(void) {
  bsp_air724_assert_reset();
  bsp_delay_ms(200U);
  bsp_air724_release_reset();
  bsp_delay_ms(1000U);
}

GPIO_PinState bsp_air724_read_netstate(void) {
  return HAL_GPIO_ReadPin(BSP_AIR4G_NETSTATE_GPIO_Port, BSP_AIR4G_NETSTATE_Pin);
}

HAL_StatusTypeDef bsp_air724_uart_write(const uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  return bsp_uart_write(BSP_AIR724_UART_HANDLE, data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_air724_uart_read(uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  return bsp_uart_read(BSP_AIR724_UART_HANDLE, data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_air724_at_command(const char *command, char *response, uint16_t response_size, uint32_t timeout_ms) {
  if (command == NULL) {
    return HAL_ERROR;
  }

  if (response != NULL && response_size > 0U) {
    response[0] = '\0';
  }

  /* AT 查询前短暂清掉 UART4 残留字节，避免上一次超时或模块 URC 混进本次诊断输出。 */
  uint8_t discard = 0U;
  while (bsp_air724_uart_read(&discard, 1U, 2U) == HAL_OK) {
  }

  HAL_StatusTypeDef status = bsp_air724_uart_write((const uint8_t *)command, (uint16_t)strlen(command), timeout_ms);
  if (status != HAL_OK) {
    return status;
  }
  status = bsp_air724_uart_write((const uint8_t *)"\r\n", 2U, timeout_ms);
  if (status != HAL_OK || response == NULL || response_size == 0U) {
    return status;
  }

  uint16_t received = 0U;
  uint32_t start = bsp_get_tick_ms();
  while (received < (uint16_t)(response_size - 1U) && (bsp_get_tick_ms() - start) < timeout_ms) {
    uint8_t byte = 0U;
    if (bsp_air724_uart_read(&byte, 1U, 20U) == HAL_OK) {
      response[received++] = (char)byte;
      response[received] = '\0';
      if (strstr(response, "\r\nOK\r\n") != NULL || strstr(response, "\nOK\r\n") != NULL ||
          strstr(response, "\r\nERROR\r\n") != NULL || strstr(response, "\nERROR\r\n") != NULL ||
          strstr(response, "+CME ERROR") != NULL || strstr(response, "+CMS ERROR") != NULL) {
        break;
      }
    }
  }

  return received > 0U ? HAL_OK : HAL_TIMEOUT;
}
