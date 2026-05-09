#include "smart_switch.h"

#include "log.h"
#include "modbus_core_master.h"
#include <string.h>

#define SWITCH_INPUT_ADDR 0x0000U
#define SWITCH_OUTPUT_ADDR 0x0001U
#define SWITCH_CTL_BASE_ADDR 0x0007U

static uint16_t read_le_u16(const uint8_t *data);
static bool read_hold_u16(smart_switch_t *device, uint16_t register_addr, uint16_t *value);

void smart_switch_init(smart_switch_t *device, uint16_t manufacture_model, uint8_t slave_addr) {
  if (device == NULL) {
    return;
  }
  memset(device, 0, sizeof(*device));
  device->manufacture_model = manufacture_model;
  device->slave_addr = slave_addr;
}

bool smart_switch_poll(smart_switch_t *device) {
  bool ok;
  if (device == NULL || device->slave_addr == 0U) {
    return false;
  }
  ok = read_hold_u16(device, SWITCH_INPUT_ADDR, &device->input_bits) &&
       read_hold_u16(device, SWITCH_OUTPUT_ADDR, &device->output_bits);
  device->online = ok;
  if (!ok) {
    LOG_ERROR("Smart switch poll failed, slave=%u err=0x%02X", device->slave_addr, device->last_error);
  }
  return ok;
}

bool smart_switch_set_all(smart_switch_t *device, uint16_t relay_bits) {
  uint8_t error_code = 0U;
  bool ok;
  if (device == NULL || device->slave_addr == 0U) {
    return false;
  }
  ok = Master_WriteOneRegister(device->slave_addr, SWITCH_OUTPUT_ADDR, relay_bits, &error_code);
  device->last_error = ok ? 0U : error_code;
  if (ok) {
    device->output_bits = relay_bits;
  }
  return ok;
}

bool smart_switch_set_one(smart_switch_t *device, uint8_t channel, bool enabled) {
  uint8_t error_code = 0U;
  uint16_t register_addr;
  bool ok;
  if (device == NULL || device->slave_addr == 0U || channel == 0U || channel > SMART_SWITCH_MAX_CHANNELS) {
    return false;
  }
  register_addr = (uint16_t)(SWITCH_CTL_BASE_ADDR + channel);
  ok = Master_WriteOneRegister(device->slave_addr, register_addr, enabled ? 1U : 0U, &error_code);
  device->last_error = ok ? 0U : error_code;
  if (ok) {
    if (enabled) {
      device->output_bits |= (uint16_t)(1U << (channel - 1U));
    } else {
      device->output_bits &= (uint16_t)~(1U << (channel - 1U));
    }
  }
  return ok;
}

uint8_t smart_switch_get_input(const smart_switch_t *device, uint8_t channel) {
  if (device == NULL || channel == 0U || channel > SMART_SWITCH_MAX_CHANNELS) {
    return 0U;
  }
  return (device->input_bits & (uint16_t)(1U << (channel - 1U))) != 0U ? 1U : 0U;
}

uint8_t smart_switch_get_output(const smart_switch_t *device, uint8_t channel) {
  if (device == NULL || channel == 0U || channel > SMART_SWITCH_MAX_CHANNELS) {
    return 0U;
  }
  return (device->output_bits & (uint16_t)(1U << (channel - 1U))) != 0U ? 1U : 0U;
}

static uint16_t read_le_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static bool read_hold_u16(smart_switch_t *device, uint16_t register_addr, uint16_t *value) {
  uint8_t data[2] = {0};
  uint8_t error_code = 0U;
  bool ok = Master_ReadHoldRegisters(device->slave_addr, register_addr, 1U, data, &error_code);
  device->last_error = ok ? 0U : error_code;
  if (ok && value != NULL) {
    *value = read_le_u16(data);
  }
  return ok;
}
