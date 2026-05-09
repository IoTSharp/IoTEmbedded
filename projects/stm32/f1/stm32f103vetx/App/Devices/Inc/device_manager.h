#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化当前实板联调设备表；启动时优先恢复 EEPROM 里保存的现场设备表。
void device_manager_init(void);
// 恢复当前保存的现场设备表到运行态；旧版本里这里曾经是温湿度基线。
void device_manager_apply_current_defaults(void);
// 清空当前运行态设备表；配置命令会同时清空内存中的待保存设备表。
void device_manager_clear_registered_devices(void);
// 按平台字段 serviceId/brand/serialPort 配置子设备，返回 false 表示当前固件还不支持该类型。
bool device_manager_configure_registered_device(uint16_t service_id, uint16_t manufacture_model, uint8_t slave_addr);
// 按配置周期轮询设备，并在 MQTT 就绪时按“变化幅度 + 最小间隔”策略自动上报 `/datas/`。
void device_manager_poll(uint32_t now_ms, uint32_t report_interval_ms);
bool device_manager_poll_all(void);
bool device_manager_poll_temperature_humidity(void);
// 构造 `/datas/` 上报 JSON，供 MQTT 发布和串口诊断共用。
bool device_manager_build_data_payload(char *buffer, uint16_t buffer_len);
// 强制发布当前完整 `/datas/` 快照，供手工诊断和平台命令执行后回读使用。
bool device_manager_publish_all(void);
bool device_manager_publish_temperature_humidity(void);
bool device_manager_execute_command(uint16_t service_id, uint8_t slave_addr, uint16_t command, uint16_t param);
void device_manager_print_status(void);

#ifdef __cplusplus
}
#endif

#endif
