#ifndef AC_CONTROLLER_H
#define AC_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "device_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

// 空调控制器沿用平台 serviceId=150；当前实板未接入时默认不参与轮询。
#define AC_CONTROLLER_SERVICE_ID DEVICE_SERVICE_AC_CONTROLLER
#define AC_CONTROLLER_DEFAULT_MODEL 6U
#define AC_CONTROLLER_DEFAULT_SLAVE_ADDR 6U
#define AC_CONTROLLER_MAX_DIY_INDEX 20U

typedef enum {
  AC_CONTROLLER_CMD_COOL_ON = 1U,
  AC_CONTROLLER_CMD_HEAT_ON = 2U,
  AC_CONTROLLER_CMD_SHUTDOWN = 3U,
  AC_CONTROLLER_CMD_DIY_SEND = 4U,
  AC_CONTROLLER_CMD_LEGACY_OPEN = 5U,
  AC_CONTROLLER_CMD_LEGACY_CLOSE = 6U,
  AC_CONTROLLER_CMD_COOL_LEARN = 11U,
  AC_CONTROLLER_CMD_HEAT_LEARN = 12U,
  AC_CONTROLLER_CMD_SHUTDOWN_LEARN = 13U,
  AC_CONTROLLER_CMD_DIY_LEARN = 14U,
} ac_controller_command_t;

typedef struct {
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  uint8_t last_error;
  uint16_t humidity_x10_rh;
  int16_t temperature_x10_c;
  uint16_t current_ch1;
  uint16_t current_ch2;
  uint8_t running_state_ch1;
  uint8_t running_state_ch2;
} ac_controller_t;

// 初始化空调控制器缓存；slave_addr=0 表示未注册，不参与轮询和上报。
void ac_controller_init(ac_controller_t *device, uint16_t manufacture_model, uint8_t slave_addr);
// 读取环境温湿度、两路真实电流和两路运行状态，寄存器地址来自当前空调控制器协议。
bool ac_controller_poll(ac_controller_t *device);
// 执行平台/串口命令；1/2/3 为制冷开机、制热开机、关机，4/14 使用 param 作为 1..20 自定义序号。
bool ac_controller_execute(ac_controller_t *device, uint16_t command, uint16_t param);
// 平台状态约定：0 停止，1 制冷，2 制热，3 未知；两路状态均为 0 时用真实电流降级判断是否运行。
uint8_t ac_controller_get_status(const ac_controller_t *device);

#ifdef __cplusplus
}
#endif

#endif
