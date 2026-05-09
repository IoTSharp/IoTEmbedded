#ifndef ELECTRICITY_METER_H
#define ELECTRICITY_METER_H

#include <stdbool.h>
#include <stdint.h>

#include "Devices/Inc/device_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

// 电力仪表使用平台 serviceId=160；当前现场未接入，必须由 registerInfo 或 device add 注册后才轮询。
#define ELECTRICITY_METER_SERVICE_ID DEVICE_SERVICE_ELECTRICITY_METER
#define ELECTRICITY_METER_DEFAULT_SLAVE_ADDR 0U

typedef struct {
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  uint8_t last_error;
  uint16_t switch_state;
  uint8_t voltage_decimal_point;
  uint8_t current_decimal_point;
  int16_t phase_voltage_x10[3];
  uint16_t phase_current_x10[3];
} electricity_meter_t;

// 初始化电表缓存；slave_addr=0 表示未注册，不参与轮询和上报。
void electricity_meter_init(electricity_meter_t *meter, uint16_t manufacture_model, uint8_t slave_addr);
// 批量读取运行信息寄存器，并解析三相电压/电流等多寄存器字段。
bool electricity_meter_poll(electricity_meter_t *meter);

#ifdef __cplusplus
}
#endif

#endif
