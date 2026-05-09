#ifndef DISCRETE_SENSOR_H
#define DISCRETE_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "device_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // serviceId 沿用平台枚举值，便于 registerInfo 直接映射到设备适配层。
  DISCRETE_SENSOR_SMOKE = DEVICE_SERVICE_SMOKE,
  DISCRETE_SENSOR_IMMERSION = DEVICE_SERVICE_IMMERSION,
  DISCRETE_SENSOR_INFRARED = DEVICE_SERVICE_INFRARED,
  DISCRETE_SENSOR_POWER_OUTAGE = DEVICE_SERVICE_POWER_OUTAGE,
} discrete_sensor_type_t;

typedef struct {
  discrete_sensor_type_t type;
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  uint8_t alarm;
  uint16_t alarm_delay;
  uint16_t sensitivity;
  uint8_t last_error;
} discrete_sensor_t;

void discrete_sensor_init(discrete_sensor_t *sensor, discrete_sensor_type_t type, uint16_t manufacture_model,
                          uint8_t slave_addr);
// 读取状态量设备的报警状态；不同设备寄存器地址不同，由实现层按 type 分发。
bool discrete_sensor_poll(discrete_sensor_t *sensor);
bool discrete_sensor_set_alarm_delay(discrete_sensor_t *sensor, uint16_t value);
bool discrete_sensor_set_sensitivity(discrete_sensor_t *sensor, uint16_t value);
const char *discrete_sensor_type_name(discrete_sensor_type_t type);

#ifdef __cplusplus
}
#endif

#endif
