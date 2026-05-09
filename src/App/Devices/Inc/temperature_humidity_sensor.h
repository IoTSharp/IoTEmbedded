#ifndef TEMPERATURE_HUMIDITY_SENSOR_H
#define TEMPERATURE_HUMIDITY_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "device_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEMP_HUMIDITY_DEFAULT_SLAVE_ADDR 1U
#define TEMP_HUMIDITY_RH_REG_ADDR 0x0000U
#define TEMP_HUMIDITY_TEMP_REG_ADDR 0x0001U
#define TEMP_HUMIDITY_TEMP_CALI_REG_ADDR 0x0050U
#define TEMP_HUMIDITY_HUMI_CALI_REG_ADDR 0x0051U
#define TEMP_HUMIDITY_ADDR_REG_ADDR 0x07D0U
#define TEMP_HUMIDITY_BAUD_REG_ADDR 0x07D1U

// 温湿度设备 0x07D0/0x07D1 保存的 485 参数，用于现场确认真实从站配置。
typedef struct {
  uint8_t reported_slave_addr;
  uint16_t baud_code;
  uint32_t baud_rate;
} temp_humidity_rs485_params_t;

typedef struct {
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  int16_t temperature_x10_c;
  uint16_t humidity_x10_rh;
  int16_t temperature_cali_x10_c;
  uint16_t humidity_cali_x10_rh;
  uint8_t reported_slave_addr;
  uint32_t baud_rate;
  uint8_t last_error;
} temp_humidity_sensor_t;

void temp_humidity_sensor_init(uint16_t manufacture_model, uint8_t slave_addr);
bool temp_humidity_sensor_poll(void);
uint32_t temp_humidity_sensor_baud_code_to_rate(uint16_t code);
bool temp_humidity_sensor_rate_to_baud_code(uint32_t rate, uint16_t *code);
bool temp_humidity_sensor_read_rs485_params(uint8_t slave_addr, temp_humidity_rs485_params_t *params,
                                            uint8_t *error_code);
bool temp_humidity_sensor_write_slave_addr_at(uint8_t current_addr, uint8_t new_addr, uint8_t *error_code);
bool temp_humidity_sensor_write_baud_at(uint8_t slave_addr, uint32_t baud_rate, uint8_t *error_code);
bool temp_humidity_sensor_set_slave_addr(uint8_t slave_addr);
bool temp_humidity_sensor_set_temperature_cali(int16_t value_x10_c);
bool temp_humidity_sensor_set_humidity_cali(uint16_t value_x10_rh);
const temp_humidity_sensor_t *temp_humidity_sensor_get(void);

#ifdef __cplusplus
}
#endif

#endif
