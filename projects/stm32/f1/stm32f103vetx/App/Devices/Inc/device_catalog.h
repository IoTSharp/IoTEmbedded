#ifndef DEVICE_CATALOG_H
#define DEVICE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// serviceId 与平台 registerInfo 枚举保持一致，不能随意改号。
#define DEVICE_SERVICE_TEMP_HUMIDITY 100U
#define DEVICE_SERVICE_SMOKE 110U
#define DEVICE_SERVICE_IMMERSION 120U
#define DEVICE_SERVICE_INFRARED 130U
#define DEVICE_SERVICE_SMART_SWITCH 140U
#define DEVICE_SERVICE_AC_CONTROLLER 150U
#define DEVICE_SERVICE_ELECTRICITY_METER 160U
#define DEVICE_SERVICE_UPS 170U
#define DEVICE_SERVICE_POWER_OUTAGE 180U

typedef struct {
  // 平台 serviceId，决定设备适配器类型。
  uint16_t service_id;
  // 当前实板配置型号；没有接入实物时为 0，平台下发后由 brand 覆盖。
  uint16_t default_model;
  // 当前实板配置 Modbus 从站地址；没有接入实物时为 0。
  uint8_t default_addr;
  // 历史默认启用标记；当前启动实际以 EEPROM 现场设备表为准，保留字段仅兼容诊断代码。
  bool enable_by_default;
  // 串口诊断和文档中使用的简短设备名。
  const char *name;
} device_catalog_entry_t;

// 返回当前实板联调配置；调用方只读，不要修改表内容。
const device_catalog_entry_t *device_catalog_entries(uint16_t *count);
// 按 serviceId 查找目录项；未知类型返回 NULL。
const device_catalog_entry_t *device_catalog_find(uint16_t service_id);
const char *device_catalog_name(uint16_t service_id);
uint16_t device_catalog_default_model(uint16_t service_id);
uint8_t device_catalog_default_addr(uint16_t service_id);
bool device_catalog_default_enabled(uint16_t service_id);

#ifdef __cplusplus
}
#endif

#endif
