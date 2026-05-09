#ifndef TELEMETRY_REPORTER_H
#define TELEMETRY_REPORTER_H

#include "Devices/Inc/ac_controller.h"
#include "Devices/Inc/discrete_sensor.h"
#include "Devices/Inc/electricity_meter.h"
#include "Devices/Inc/smart_switch.h"
#include "Devices/Inc/temperature_humidity_sensor.h"
#include "Devices/Inc/ups_detector_interface.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 清空采样窗口、已判定值和待上传队列；设备注册表变化时必须调用，避免旧地址数据串到新设备。
void telemetry_reporter_reset(void);
// 返回遥测固定采样周期。当前按现场决策：6 秒采样一次，10 组组成 1 分钟窗口。
uint32_t telemetry_reporter_sample_interval_ms(void);
// 把本轮设备缓存纳入 1 分钟窗口，窗口满后按中值/稳定状态判定是否入队。
void telemetry_reporter_ingest(uint32_t now_ms, const temp_humidity_sensor_t *temp_humidity,
                               const discrete_sensor_t *smoke, const discrete_sensor_t *immersion,
                               const discrete_sensor_t *infrared, const discrete_sensor_t *power_outage,
                               const smart_switch_t *smart_switch, const ac_controller_t *ac_controller,
                               const electricity_meter_t *electricity_meter, const ups_detector_t *ups);
// MQTT 就绪且满足最小发送间隔时，从待上传队列头部发送一条；发送失败时保留队头。
bool telemetry_reporter_flush_one(uint32_t now_ms);
uint8_t telemetry_reporter_queue_count(void);

#ifdef __cplusplus
}
#endif

#endif
