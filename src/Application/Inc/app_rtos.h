#ifndef APP_RTOS_H
#define APP_RTOS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APP_ENABLE_CMSIS_RTOS
#define APP_ENABLE_CMSIS_RTOS 0
#endif

/** RTOS 任务心跳位：看门狗线程只在关键任务都按期更新后才喂狗。 */
typedef enum {
  APP_RTOS_HEARTBEAT_MAIN = 0,
  APP_RTOS_HEARTBEAT_MQTT_RX,
  APP_RTOS_HEARTBEAT_BASIC,
  APP_RTOS_HEARTBEAT_DEBUG,
  APP_RTOS_HEARTBEAT_COUNT,
} app_rtos_heartbeat_t;

/** RTOS 运行快照：仅暴露诊断所需字段，避免业务层依赖 FreeRTOS 私有句柄。 */
typedef struct {
  bool enabled;
  bool started;
  uint32_t main_stack_size;
  uint32_t mqtt_rx_stack_size;
  uint32_t basic_stack_size;
  uint32_t debug_stack_size;
  uint32_t watchdog_stack_size;
  uint32_t main_stack_free;
  uint32_t mqtt_rx_stack_free;
  uint32_t basic_stack_free;
  uint32_t debug_stack_free;
  uint32_t watchdog_stack_free;
  uint32_t main_last_heartbeat_ms;
  uint32_t mqtt_rx_last_heartbeat_ms;
  uint32_t basic_last_heartbeat_ms;
  uint32_t debug_last_heartbeat_ms;
  uint32_t heartbeat_timeout_ms;
  bool heartbeats_alive;
} app_rtos_status_t;

bool app_rtos_start(void);
void app_rtos_mark_heartbeat(app_rtos_heartbeat_t heartbeat, uint32_t now_ms);
bool app_rtos_all_heartbeats_alive(uint32_t now_ms, uint32_t timeout_ms);
uint32_t app_rtos_get_heartbeat_timeout_ms(void);
app_rtos_status_t app_rtos_get_status(uint32_t now_ms, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
