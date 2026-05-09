#include "modbus_test.h"

#include "modbus_core_master.h"

#include <stdio.h>

bool modbus_test_read_hold_register(uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_num) {
  uint8_t error_code = 0U;
  uint8_t data[64] = {0};

  if ((uint16_t)(reg_num * 2U) > sizeof(data)) {
    printf("modbus test range too large\r\n");
    return false;
  }

  bool ok = Master_ReadHoldRegisters(slave_addr, reg_addr, reg_num, data, &error_code);
  if (!ok) {
    printf("modbus read failed, slave=%u reg=0x%04X err=0x%02X\r\n", slave_addr, reg_addr, error_code);
    return false;
  }

  printf("modbus read ok, slave=%u reg=0x%04X count=%u data=", slave_addr, reg_addr, reg_num);
  for (uint16_t i = 0; i < (uint16_t)(reg_num * 2U); i++) {
    printf("%02X", data[i]);
  }
  printf("\r\n");
  return true;
}
