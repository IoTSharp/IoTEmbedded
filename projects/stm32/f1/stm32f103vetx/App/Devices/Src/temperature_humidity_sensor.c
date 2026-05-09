#include "temperature_humidity_sensor.h"

#include "log.h"
#include "modbus_core_master.h"
#include <string.h>

static temp_humidity_sensor_t sensor;

static uint16_t read_be_u16(const uint8_t *data);
static bool read_registers(uint16_t start_addr, uint16_t count, uint8_t *buffer, uint8_t *error_code);

void temp_humidity_sensor_init(uint16_t manufacture_model, uint8_t slave_addr) {
  memset(&sensor, 0, sizeof(sensor));
  sensor.manufacture_model = manufacture_model;
  // slave_addr=0 表示该类型未注册；当前实板地址由 device_manager_init() 显式传入，避免清表后误轮询未接设备。
  sensor.slave_addr = slave_addr;
}

bool temp_humidity_sensor_poll(void) {
  uint8_t error_code = 0U;
  uint8_t data[4] = {0};
  bool ok = true;

  if (sensor.slave_addr == 0U) {
    sensor.online = false;
    sensor.last_error = 0U;
    return false;
  }

  if (read_registers(TEMP_HUMIDITY_RH_REG_ADDR, 2U, data, &error_code)) {
    sensor.humidity_x10_rh = read_be_u16(&data[0]);
    sensor.temperature_x10_c = (int16_t)read_be_u16(&data[2]);
  } else {
    ok = false;
  }

  if (ok && read_registers(TEMP_HUMIDITY_TEMP_CALI_REG_ADDR, 2U, data, &error_code)) {
    sensor.temperature_cali_x10_c = (int16_t)read_be_u16(&data[0]);
    sensor.humidity_cali_x10_rh = read_be_u16(&data[2]);
  } else if (ok) {
    ok = false;
  }

  if (ok && read_registers(TEMP_HUMIDITY_ADDR_REG_ADDR, 2U, data, &error_code)) {
    sensor.reported_slave_addr = (uint8_t)read_be_u16(&data[0]);
    sensor.baud_rate = temp_humidity_sensor_baud_code_to_rate(read_be_u16(&data[2]));
  } else if (ok) {
    ok = false;
  }

  sensor.online = ok;
  sensor.last_error = ok ? 0U : error_code;
  if (!ok) {
    LOG_ERROR("Temp humidity sensor poll failed, slave=%u err=0x%02X", sensor.slave_addr, error_code);
  }
  return ok;
}

uint32_t temp_humidity_sensor_baud_code_to_rate(uint16_t code) {
  static const uint32_t rates[] = {2400U, 4800U, 9600U, 19200U, 38400U, 57600U, 115200U, 1200U};
  if (code >= (sizeof(rates) / sizeof(rates[0]))) {
    return 0U;
  }
  return rates[code];
}

bool temp_humidity_sensor_rate_to_baud_code(uint32_t rate, uint16_t *code) {
  static const uint32_t rates[] = {2400U, 4800U, 9600U, 19200U, 38400U, 57600U, 115200U, 1200U};
  if (code == NULL) {
    return false;
  }
  for (uint16_t index = 0U; index < (uint16_t)(sizeof(rates) / sizeof(rates[0])); index++) {
    if (rates[index] == rate) {
      *code = index;
      return true;
    }
  }
  return false;
}

bool temp_humidity_sensor_read_rs485_params(uint8_t slave_addr, temp_humidity_rs485_params_t *params,
                                            uint8_t *error_code) {
  uint8_t data[4] = {0};
  uint8_t local_error = 0U;
  if (slave_addr == 0U || params == NULL) {
    if (error_code != NULL) {
      *error_code = 0xFEU;
    }
    return false;
  }

  /*
   * 0x07D0/0x07D1 是当前温湿度设备用于地址和波特率的保持寄存器。
   * 这里按显式 slave_addr 读取，不依赖当前运行态注册表，便于确认现场从站真实参数。
   */
  bool ok = Master_ReadHoldRegisters(slave_addr, TEMP_HUMIDITY_ADDR_REG_ADDR, 2U, data, &local_error);
  if (!ok) {
    if (error_code != NULL) {
      *error_code = local_error;
    }
    return false;
  }

  params->reported_slave_addr = (uint8_t)read_be_u16(&data[0]);
  params->baud_code = read_be_u16(&data[2]);
  params->baud_rate = temp_humidity_sensor_baud_code_to_rate(params->baud_code);
  if (error_code != NULL) {
    *error_code = 0U;
  }
  return true;
}

bool temp_humidity_sensor_write_slave_addr_at(uint8_t current_addr, uint8_t new_addr, uint8_t *error_code) {
  uint8_t local_error = 0U;
  if (current_addr == 0U || new_addr == 0U || new_addr > 247U) {
    if (error_code != NULL) {
      *error_code = 0xFEU;
    }
    return false;
  }

  bool ok = Master_WriteOneRegister(current_addr, TEMP_HUMIDITY_ADDR_REG_ADDR, new_addr, &local_error);
  if (error_code != NULL) {
    *error_code = ok ? 0U : local_error;
  }
  return ok;
}

bool temp_humidity_sensor_write_baud_at(uint8_t slave_addr, uint32_t baud_rate, uint8_t *error_code) {
  uint8_t local_error = 0U;
  uint16_t baud_code = 0U;
  if (slave_addr == 0U || !temp_humidity_sensor_rate_to_baud_code(baud_rate, &baud_code)) {
    if (error_code != NULL) {
      *error_code = 0xFEU;
    }
    return false;
  }

  bool ok = Master_WriteOneRegister(slave_addr, TEMP_HUMIDITY_BAUD_REG_ADDR, baud_code, &local_error);
  if (error_code != NULL) {
    *error_code = ok ? 0U : local_error;
  }
  return ok;
}

bool temp_humidity_sensor_set_slave_addr(uint8_t slave_addr) {
  uint8_t error_code = 0U;
  if (slave_addr == 0U) {
    return false;
  }
  if (!Master_WriteOneRegister(sensor.slave_addr, TEMP_HUMIDITY_ADDR_REG_ADDR, slave_addr, &error_code)) {
    sensor.last_error = error_code;
    LOG_ERROR("Set temp humidity slave address failed, old=%u new=%u err=0x%02X", sensor.slave_addr, slave_addr,
              error_code);
    return false;
  }
  sensor.slave_addr = slave_addr;
  return true;
}

bool temp_humidity_sensor_set_temperature_cali(int16_t value_x10_c) {
  uint8_t error_code = 0U;
  if (!Master_WriteOneRegister(sensor.slave_addr, TEMP_HUMIDITY_TEMP_CALI_REG_ADDR, (uint16_t)value_x10_c, &error_code)) {
    sensor.last_error = error_code;
    LOG_ERROR("Set temp humidity temperature calibration failed, slave=%u err=0x%02X", sensor.slave_addr, error_code);
    return false;
  }
  sensor.temperature_cali_x10_c = value_x10_c;
  return true;
}

bool temp_humidity_sensor_set_humidity_cali(uint16_t value_x10_rh) {
  uint8_t error_code = 0U;
  if (!Master_WriteOneRegister(sensor.slave_addr, TEMP_HUMIDITY_HUMI_CALI_REG_ADDR, value_x10_rh, &error_code)) {
    sensor.last_error = error_code;
    LOG_ERROR("Set temp humidity humidity calibration failed, slave=%u err=0x%02X", sensor.slave_addr, error_code);
    return false;
  }
  sensor.humidity_cali_x10_rh = value_x10_rh;
  return true;
}

const temp_humidity_sensor_t *temp_humidity_sensor_get(void) {
  return &sensor;
}

static uint16_t read_be_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static bool read_registers(uint16_t start_addr, uint16_t count, uint8_t *buffer, uint8_t *error_code) {
  memset(buffer, 0, (size_t)count * 2U);
  return Master_ReadHoldRegisters(sensor.slave_addr, start_addr, count, buffer, error_code);
}
