#ifndef MODBUS_TEST_H
#define MODBUS_TEST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool modbus_test_read_hold_register(uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_num);

#ifdef __cplusplus
}
#endif

#endif
