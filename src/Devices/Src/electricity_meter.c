#include "Devices/Inc/electricity_meter.h"

#include "Common/Inc/log.h"
#include "Protocol/Modbus/Inc/modbus_core_master.h"
#include <string.h>

// 当前电力仪表协议使用 0x0021 起 0x2D 个运行信息项。
// 实测数据区按 Modbus 寄存器返回的大端字节解析，每个寄存器 2 字节。
#define ELECTRICITY_METER_OPERATION_INFO_ADDR 0x0021U
#define ELECTRICITY_METER_OPERATION_INFO_CNT 0x002DU
#define ELECTRICITY_METER_OPERATION_INFO_BYTES (ELECTRICITY_METER_OPERATION_INFO_CNT * 2U)

#define ELECTRICITY_METER_SWITCH_STATE_OFFSET 0U
#define ELECTRICITY_METER_VOLT_POINT_OFFSET 4U
#define ELECTRICITY_METER_CURRENT_POINT_OFFSET 5U
#define ELECTRICITY_METER_PHASE_A_VOLT_OFFSET 8U
#define ELECTRICITY_METER_PHASE_B_VOLT_OFFSET 10U
#define ELECTRICITY_METER_PHASE_C_VOLT_OFFSET 12U
#define ELECTRICITY_METER_PHASE_A_CURRENT_OFFSET 20U
#define ELECTRICITY_METER_PHASE_B_CURRENT_OFFSET 22U
#define ELECTRICITY_METER_PHASE_C_CURRENT_OFFSET 24U

static uint16_t read_be_u16(const uint8_t *data);
static int16_t read_be_i16(const uint8_t *data);
static uint16_t decimal_divisor_for_x10(uint8_t point);

void electricity_meter_init(electricity_meter_t *meter, uint16_t manufacture_model, uint8_t slave_addr) {
  if (meter == NULL) {
    return;
  }
  memset(meter, 0, sizeof(*meter));
  meter->manufacture_model = manufacture_model;
  meter->slave_addr = slave_addr;
}

bool electricity_meter_poll(electricity_meter_t *meter) {
  uint8_t error_code = 0U;
  uint8_t data[ELECTRICITY_METER_OPERATION_INFO_BYTES] = {0};

  if (meter == NULL || meter->slave_addr == 0U) {
    return false;
  }

  if (!Master_ReadHoldRegisters(meter->slave_addr, ELECTRICITY_METER_OPERATION_INFO_ADDR,
                                ELECTRICITY_METER_OPERATION_INFO_CNT, data, &error_code)) {
    meter->online = false;
    meter->last_error = error_code;
    LOG_ERROR("Electricity meter poll failed, slave=%u err=0x%02X", meter->slave_addr, error_code);
    return false;
  }

  meter->switch_state = read_be_u16(&data[ELECTRICITY_METER_SWITCH_STATE_OFFSET]);
  meter->voltage_decimal_point = data[ELECTRICITY_METER_VOLT_POINT_OFFSET];
  meter->current_decimal_point = data[ELECTRICITY_METER_CURRENT_POINT_OFFSET];

  /*
   * 平台上报约定使用 0.1V/0.1A 单位。仪表原始值的小数点位置由运行信息
   * 第 4/5 字节给出：point=3 表示原始值已是 x10，point=2/1 分别需再除 10/100。
   * 这里保留整数 x10，避免在 STM32F103 上引入不必要的浮点格式化开销。
   */
  uint16_t voltage_div = decimal_divisor_for_x10(meter->voltage_decimal_point);
  uint16_t current_div = decimal_divisor_for_x10(meter->current_decimal_point);

  meter->phase_voltage_x10[0] = (int16_t)(read_be_i16(&data[ELECTRICITY_METER_PHASE_A_VOLT_OFFSET]) / voltage_div);
  meter->phase_voltage_x10[1] = (int16_t)(read_be_i16(&data[ELECTRICITY_METER_PHASE_B_VOLT_OFFSET]) / voltage_div);
  meter->phase_voltage_x10[2] = (int16_t)(read_be_i16(&data[ELECTRICITY_METER_PHASE_C_VOLT_OFFSET]) / voltage_div);
  meter->phase_current_x10[0] = (uint16_t)(read_be_u16(&data[ELECTRICITY_METER_PHASE_A_CURRENT_OFFSET]) / current_div);
  meter->phase_current_x10[1] = (uint16_t)(read_be_u16(&data[ELECTRICITY_METER_PHASE_B_CURRENT_OFFSET]) / current_div);
  meter->phase_current_x10[2] = (uint16_t)(read_be_u16(&data[ELECTRICITY_METER_PHASE_C_CURRENT_OFFSET]) / current_div);

  meter->online = true;
  meter->last_error = 0U;
  return true;
}

static uint16_t read_be_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static int16_t read_be_i16(const uint8_t *data) {
  return (int16_t)read_be_u16(data);
}

static uint16_t decimal_divisor_for_x10(uint8_t point) {
  if (point == 3U) {
    return 1U;
  }
  if (point == 2U) {
    return 10U;
  }
  // 协议说明电压/电流量程不考虑 point=4；异常值按最保守 /100 处理。
  return 100U;
}
