#ifndef BASIC_SERIAL_H
#define BASIC_SERIAL_H

#include "Common/Inc/app_types.h"
#include "Interpreter/Inc/basic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BASIC_SERIAL_DEFAULT_TIMEOUT_MS 1000U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BASIC_SERIAL_INTERFACE_NONE = 0,
  BASIC_SERIAL_INTERFACE_SERIAL,
  BASIC_SERIAL_INTERFACE_RS485,
  BASIC_SERIAL_INTERFACE_RS232,
} basic_serial_interface_t;

typedef struct {
  basic_serial_interface_t interface;
  UART_HandleTypeDef *handle;
  uint8_t port_id;
} basic_serial_port_t;

ErrorStatus basic_serial_register(struct mb_interpreter_t *interpreter);

bool basic_serial_port_from_value(mb_value_t value, basic_serial_port_t *port);
bool basic_serial_port_is_modbus_capable(basic_serial_port_t port);
basic_serial_port_t basic_serial_rs485_port(void);
uint32_t basic_serial_get_baud_rate(basic_serial_port_t port);
HAL_StatusTypeDef basic_serial_configure_baud_rate(basic_serial_port_t port, uint32_t baud_rate);
size_t basic_serial_write_data(basic_serial_port_t port, const uint8_t *data, size_t length, uint32_t timeout_ms);
uint16_t basic_serial_read_data(basic_serial_port_t port, uint8_t *buffer, uint16_t length, uint32_t timeout_ms);
void basic_serial_flush_port(basic_serial_port_t port);
void basic_serial_feed_heartbeat(void);
int_t basic_serial_u32_to_int(uint32_t value);
uint32_t basic_serial_timeout_from_int(int_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
