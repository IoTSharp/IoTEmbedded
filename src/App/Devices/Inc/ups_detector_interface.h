#ifndef UPS_DETECTOR_INTERFACE_H
#define UPS_DETECTOR_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "device_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

// UPS 使用平台 serviceId=170；当前现场未接入，必须由 registerInfo 或 device add 注册后才轮询。
#define UPS_DETECTOR_SERVICE_ID DEVICE_SERVICE_UPS
#define UPS_DETECTOR_DEFAULT_SLAVE_ADDR 0U

// 平台 manufacture_model 映射值；同族型号在 .c 的描述表中复用协议实现。
#define UPS_DETECTOR_MODEL_NET_30KVA_FIRST 8U
#define UPS_DETECTOR_MODEL_YTG_B3120 12U
#define UPS_DETECTOR_MODEL_HUAWEI_UPS2000 14U
#define UPS_DETECTOR_MODEL_HUAWEI_UPS5000 15U
#define UPS_DETECTOR_MODEL_CHP_SERIES 16U
#define UPS_DETECTOR_MODEL_NET_30KVA_SECOND 17U
#define UPS_DETECTOR_MODEL_HTT_SERIES 19U
#define UPS_DETECTOR_MODEL_YTG_B3330_FIRST 20U
#define UPS_DETECTOR_MODEL_NET_S20 24U

typedef struct {
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  uint8_t last_error;
  uint16_t input_voltage_x10_v[3];
  uint16_t output_voltage_x10_v[3];
  uint16_t load_rate_x10_percent[3];
  uint16_t battery_positive_voltage_x10_v;
  uint16_t battery_negative_voltage_x10_v;
  uint8_t battery_state_of_charge_percent;
  uint16_t battery_residual_discharge_minutes;
  int16_t battery_temperature_x10_c;
} ups_detector_t;

// 初始化 UPS 统一缓存；slave_addr=0 表示未注册，不参与轮询和上报。
void ups_detector_init(ups_detector_t *ups, uint16_t manufacture_model, uint8_t slave_addr);
// 根据 manufacture_model 查表分发到具体 UPS 协议，新增型号只扩展描述表和必要的特殊解析函数。
bool ups_detector_poll(ups_detector_t *ups);
// 判断当前厂商/型号是否已完成迁移，供 registerInfo 接入时过滤未知 UPS。
bool ups_detector_is_supported_model(uint16_t manufacture_model);

#ifdef __cplusplus
}
#endif

#endif
