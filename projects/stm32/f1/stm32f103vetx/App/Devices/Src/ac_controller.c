#include "ac_controller.h"

#include "log.h"
#include "modbus_core_master.h"
#include <string.h>

// 以下寄存器地址来自当前空调控制器协议，平台命令号暂按新迁移层收口。
#define AC_CTL_RH_ADDR 0x0000U
#define AC_CTL_CLOD_OPEN_LEARN_ADDR 0x0007U
#define AC_CTL_HOT_OPEN_LEARN_ADDR 0x0008U
#define AC_CTL_SHUTDOWN_LEARN_ADDR 0x0009U
#define AC_CTL_DIY_LEARN_BASE_ADDR 0x0009U
#define AC_CTL_CLOD_OPEN_ADDR 0x00B9U
#define AC_CTL_HOT_OPEN_ADDR 0x00BAU
#define AC_CTL_SHUTDOWN_ADDR 0x00BBU
#define AC_CTL_DIY_BASE_ADDR 0x00BBU
#define AC_CTL_CURRENT_ADDR 0x0071U
#define AC_CTL_STATUS_ADDR 0x00D7U

#define AC_CONTROLLER_RUNNING_STOP 0U
#define AC_CONTROLLER_RUNNING_COOL 1U

static uint16_t read_be_u16(const uint8_t *data);
static int16_t read_be_i16(const uint8_t *data);
static bool read_u16_pair(ac_controller_t *device, uint16_t register_addr, uint16_t *first, uint16_t *second);
static bool write_command_register(ac_controller_t *device, uint16_t register_addr);

void ac_controller_init(ac_controller_t *device, uint16_t manufacture_model, uint8_t slave_addr) {
  if (device == NULL) {
    return;
  }
  memset(device, 0, sizeof(*device));
  device->manufacture_model = manufacture_model;
  device->slave_addr = slave_addr;
}

bool ac_controller_poll(ac_controller_t *device) {
  uint16_t humidity = 0U;
  uint16_t temperature = 0U;
  uint16_t running_state_ch1 = 0U;
  uint16_t running_state_ch2 = 0U;

  if (device == NULL || device->slave_addr == 0U) {
    return false;
  }

  bool ok = read_u16_pair(device, AC_CTL_RH_ADDR, &humidity, &temperature) &&
            read_u16_pair(device, AC_CTL_CURRENT_ADDR, &device->current_ch1, &device->current_ch2) &&
            read_u16_pair(device, AC_CTL_STATUS_ADDR, &running_state_ch1, &running_state_ch2);
  device->online = ok;
  if (ok) {
    device->humidity_x10_rh = humidity;
    device->temperature_x10_c = (int16_t)temperature;
    device->running_state_ch1 = (uint8_t)(running_state_ch1 & 0x00FFU);
    device->running_state_ch2 = (uint8_t)(running_state_ch2 & 0x00FFU);
  } else {
    LOG_ERROR("AC controller poll failed, slave=%u err=0x%02X", device->slave_addr, device->last_error);
  }
  return ok;
}

bool ac_controller_execute(ac_controller_t *device, uint16_t command, uint16_t param) {
  if (device == NULL || device->slave_addr == 0U) {
    return false;
  }

  switch (command) {
  case AC_CONTROLLER_CMD_COOL_ON:
    return write_command_register(device, AC_CTL_CLOD_OPEN_ADDR);
  case AC_CONTROLLER_CMD_HEAT_ON:
    return write_command_register(device, AC_CTL_HOT_OPEN_ADDR);
  case AC_CONTROLLER_CMD_SHUTDOWN:
    return write_command_register(device, AC_CTL_SHUTDOWN_ADDR);
  case AC_CONTROLLER_CMD_DIY_SEND:
    if (param == 0U || param > AC_CONTROLLER_MAX_DIY_INDEX) {
      return false;
    }
    return write_command_register(device, (uint16_t)(AC_CTL_DIY_BASE_ADDR + param));
  case AC_CONTROLLER_CMD_LEGACY_OPEN:
    // 兼容旧 excv_cmd：cmd=5 且 params=1 表示制热开机，params=0 表示制冷开机。
    if (param == 1U) {
      return write_command_register(device, AC_CTL_HOT_OPEN_ADDR);
    }
    if (param == 0U) {
      return write_command_register(device, AC_CTL_CLOD_OPEN_ADDR);
    }
    return false;
  case AC_CONTROLLER_CMD_LEGACY_CLOSE:
    return write_command_register(device, AC_CTL_SHUTDOWN_ADDR);
  case AC_CONTROLLER_CMD_COOL_LEARN:
    return write_command_register(device, AC_CTL_CLOD_OPEN_LEARN_ADDR);
  case AC_CONTROLLER_CMD_HEAT_LEARN:
    return write_command_register(device, AC_CTL_HOT_OPEN_LEARN_ADDR);
  case AC_CONTROLLER_CMD_SHUTDOWN_LEARN:
    return write_command_register(device, AC_CTL_SHUTDOWN_LEARN_ADDR);
  case AC_CONTROLLER_CMD_DIY_LEARN:
    if (param == 0U || param > AC_CONTROLLER_MAX_DIY_INDEX) {
      return false;
    }
    return write_command_register(device, (uint16_t)(AC_CTL_DIY_LEARN_BASE_ADDR + param));
  default:
    return false;
  }
}

uint8_t ac_controller_get_status(const ac_controller_t *device) {
  if (device == NULL) {
    return AC_CONTROLLER_RUNNING_STOP;
  }
  if (device->running_state_ch1 != 0U) {
    return device->running_state_ch1;
  }
  if (device->running_state_ch2 != 0U) {
    return device->running_state_ch2;
  }
  // 运行状态寄存器全为 0 时，用真实电流兜底判断是否至少有一路空调在运行。
  return (device->current_ch1 != 0U || device->current_ch2 != 0U) ? AC_CONTROLLER_RUNNING_COOL : AC_CONTROLLER_RUNNING_STOP;
}

static uint16_t read_be_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static int16_t read_be_i16(const uint8_t *data) {
  return (int16_t)read_be_u16(data);
}

static bool read_u16_pair(ac_controller_t *device, uint16_t register_addr, uint16_t *first, uint16_t *second) {
  uint8_t data[4] = {0};
  uint8_t error_code = 0U;
  bool ok = Master_ReadCoilStatus(device->slave_addr, register_addr, 2U, data, &error_code);
  device->last_error = ok ? 0U : error_code;
  if (ok) {
    if (first != NULL) {
      *first = read_be_u16(&data[0]);
    }
    if (second != NULL) {
      *second = (uint16_t)read_be_i16(&data[2]);
    }
  }
  return ok;
}

static bool write_command_register(ac_controller_t *device, uint16_t register_addr) {
  uint8_t error_code = 0U;
  bool ok = Master_WriteOneRegister(device->slave_addr, register_addr, 0x0001U, &error_code);
  device->last_error = ok ? 0U : error_code;
  if (!ok) {
    LOG_ERROR("AC controller command failed, slave=%u reg=0x%04X err=0x%02X", device->slave_addr, register_addr,
              error_code);
  }
  return ok;
}
