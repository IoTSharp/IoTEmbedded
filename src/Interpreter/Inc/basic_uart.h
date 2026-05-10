#ifndef BASIC_UART_H
#define BASIC_UART_H

#include "Common/Inc/app_types.h"
#include "Interpreter/Inc/basic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BASIC_UART_DEFAULT_TIMEOUT_MS 1000U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BASIC_UART_INTERFACE_NONE = 0,
  BASIC_UART_INTERFACE_UART,
  BASIC_UART_INTERFACE_RS485,
  BASIC_UART_INTERFACE_RS232,
} basic_uart_interface_t;

typedef struct {
  basic_uart_interface_t interface;
  UART_HandleTypeDef *handle;
  uint8_t port_id;
} basic_uart_port_t;

ErrorStatus basic_uart_register(struct mb_interpreter_t *interpreter);

bool basic_uart_port_from_value(mb_value_t value, basic_uart_port_t *port);
bool basic_uart_port_is_modbus_capable(basic_uart_port_t port);
basic_uart_port_t basic_uart_rs485_port(void);
uint32_t basic_uart_get_baud_rate(basic_uart_port_t port);
HAL_StatusTypeDef basic_uart_configure_baud_rate(basic_uart_port_t port, uint32_t baud_rate);
size_t basic_uart_write_data(basic_uart_port_t port, const uint8_t *data, size_t length, uint32_t timeout_ms);
uint16_t basic_uart_read_data(basic_uart_port_t port, uint8_t *buffer, uint16_t length, uint32_t timeout_ms);
void basic_uart_flush_port(basic_uart_port_t port);
void basic_uart_feed_heartbeat(void);
int_t basic_uart_u32_to_int(uint32_t value);
uint32_t basic_uart_timeout_from_int(int_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
