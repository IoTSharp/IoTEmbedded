#include "ups_detector_interface.h"

#include "log.h"
#include "modbus_core_master.h"
#include <stddef.h>
#include <string.h>

#define UPS_ERR_UNSUPPORTED 0xFFU
#define UPS_ERR_INVALID_ADDR 0xFEU
#define UPS2000_MODBUS_ADDR_BASE 0x10U
#define UPS2000_MAX_INDEX 4U
#define UPS2000_MAX_RETRY 5U

#define REG_UNUSED 0xFFFFU
#define UPS_YTG_B3120_INPUT_REGISTER_ADDR 0x1388U
#define UPS_YTG_B3120_INPUT_REGISTER_COUNT 0x0099U

typedef enum {
  UPS_READ_HOLD,
  UPS_READ_INPUT,
  UPS_READ_COIL_COMPAT
} ups_register_read_t;

typedef enum {
  UPS_SOC_LOW_BYTE,
  UPS_SOC_U16_BE
} ups_soc_format_t;

typedef enum {
  UPS_SPECIAL_NONE,
  UPS_SPECIAL_HUAWEI_UPS2000,
  UPS_SPECIAL_YTG_B3120,
  UPS_SPECIAL_YTG_B3330
} ups_special_parser_t;

typedef struct {
  uint16_t manufacture_model;
  const char *name;
  ups_register_read_t read_type;
  ups_special_parser_t special_parser;
  uint16_t input_voltage_addr;
  uint16_t output_voltage_addr;
  uint16_t load_rate_addr;
  uint16_t battery_voltage_addr;
  uint16_t battery_state_addr;
  uint16_t battery_temperature_addr;
  uint16_t battery_time_addr;
  uint8_t battery_voltage_regs;
  uint8_t battery_state_regs;
  uint8_t battery_temperature_offset;
  uint8_t battery_time_offset;
  uint8_t battery_soc_offset;
  uint8_t load_scale_x10;
  bool has_negative_battery;
  bool backup_seconds_to_minutes;
  ups_soc_format_t soc_format;
} ups_model_descriptor_t;

static const ups_model_descriptor_t ups_models[] = {
  /* 旧 NET 30kVA 同族型号：8/9/10/17/18 共用寄存器表，读输入寄存器。 */
  {8U, "NET 30kVA", UPS_READ_INPUT, UPS_SPECIAL_NONE, 30000U, 30010U, 30020U, 30030U, 30036U, 30038U, 30040U, 2U, 1U, 0U, 0U, 0U, 10U, true, false, UPS_SOC_LOW_BYTE},
  {9U, "NET 30kVA", UPS_READ_INPUT, UPS_SPECIAL_NONE, 30000U, 30010U, 30020U, 30030U, 30036U, 30038U, 30040U, 2U, 1U, 0U, 0U, 0U, 10U, true, false, UPS_SOC_LOW_BYTE},
  {10U, "NET 30kVA", UPS_READ_INPUT, UPS_SPECIAL_NONE, 30000U, 30010U, 30020U, 30030U, 30036U, 30038U, 30040U, 2U, 1U, 0U, 0U, 0U, 10U, true, false, UPS_SOC_LOW_BYTE},
  {17U, "NET 30kVA", UPS_READ_INPUT, UPS_SPECIAL_NONE, 30000U, 30010U, 30020U, 30030U, 30036U, 30038U, 30040U, 2U, 1U, 0U, 0U, 0U, 10U, true, false, UPS_SOC_LOW_BYTE},
  {18U, "NET 30kVA", UPS_READ_INPUT, UPS_SPECIAL_NONE, 30000U, 30010U, 30020U, 30030U, 30036U, 30038U, 30040U, 2U, 1U, 0U, 0U, 0U, 10U, true, false, UPS_SOC_LOW_BYTE},
  {UPS_DETECTOR_MODEL_YTG_B3120, "YTG B3120", UPS_READ_INPUT, UPS_SPECIAL_YTG_B3120, REG_UNUSED, REG_UNUSED, REG_UNUSED, REG_UNUSED, REG_UNUSED, REG_UNUSED, REG_UNUSED, 0U, 0U, 0U, 0U, 0U, 1U, false, false, UPS_SOC_U16_BE},
  {UPS_DETECTOR_MODEL_HUAWEI_UPS2000, "Huawei UPS2000", UPS_READ_COIL_COMPAT, UPS_SPECIAL_HUAWEI_UPS2000, 1000U, 1008U, 1021U, 2000U, 2003U, REG_UNUSED, REG_UNUSED, 1U, 3U, 4U, 2U, 0U, 1U, false, false, UPS_SOC_U16_BE},
  {UPS_DETECTOR_MODEL_HUAWEI_UPS5000, "Huawei UPS5000", UPS_READ_COIL_COMPAT, UPS_SPECIAL_NONE, 40001U, 40046U, 40068U, 40101U, 40108U, REG_UNUSED, REG_UNUSED, 2U, 3U, 0U, 2U, 4U, 1U, true, true, UPS_SOC_U16_BE},
  {UPS_DETECTOR_MODEL_CHP_SERIES, "CHP Series", UPS_READ_COIL_COMPAT, UPS_SPECIAL_NONE, 200U, 215U, 212U, 221U, 219U, 223U, 220U, 2U, 1U, 0U, 0U, 0U, 1U, true, false, UPS_SOC_LOW_BYTE},
  {UPS_DETECTOR_MODEL_HTT_SERIES, "HTT Series", UPS_READ_COIL_COMPAT, UPS_SPECIAL_NONE, 0U, 3U, 18U, 24U, 28U, REG_UNUSED, 29U, 2U, 1U, 0U, 0U, 0U, 1U, true, false, UPS_SOC_LOW_BYTE},
  {20U, "YTG B3330", UPS_READ_INPUT, UPS_SPECIAL_YTG_B3330, REG_UNUSED, 5022U, 5031U, 5004U, 5003U, 5006U, 5002U, 1U, 1U, 0U, 0U, 0U, 10U, false, false, UPS_SOC_LOW_BYTE},
  {21U, "YTG B3330", UPS_READ_INPUT, UPS_SPECIAL_YTG_B3330, REG_UNUSED, 5022U, 5031U, 5004U, 5003U, 5006U, 5002U, 1U, 1U, 0U, 0U, 0U, 10U, false, false, UPS_SOC_LOW_BYTE},
  {22U, "YTG B3330", UPS_READ_INPUT, UPS_SPECIAL_YTG_B3330, REG_UNUSED, 5022U, 5031U, 5004U, 5003U, 5006U, 5002U, 1U, 1U, 0U, 0U, 0U, 10U, false, false, UPS_SOC_LOW_BYTE},
  {23U, "YTG B3330", UPS_READ_INPUT, UPS_SPECIAL_YTG_B3330, REG_UNUSED, 5022U, 5031U, 5004U, 5003U, 5006U, 5002U, 1U, 1U, 0U, 0U, 0U, 10U, false, false, UPS_SOC_LOW_BYTE},
  {UPS_DETECTOR_MODEL_NET_S20, "NET S20", UPS_READ_INPUT, UPS_SPECIAL_NONE, 171U, 175U, 182U, 350U, 356U, 200U, 357U, 2U, 1U, 0U, 0U, 0U, 1U, true, false, UPS_SOC_LOW_BYTE},
};

static const ups_model_descriptor_t *find_model(uint16_t manufacture_model);
static bool poll_model(ups_detector_t *ups, const ups_model_descriptor_t *model);
static bool poll_ytg_b3120(ups_detector_t *ups, const ups_model_descriptor_t *model);
static bool read_registers(const ups_model_descriptor_t *model, uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_count,
                           uint8_t *buffer, uint8_t *error_code);
static bool read_required(const ups_model_descriptor_t *model, uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_count,
                          uint8_t *buffer, uint8_t *error_code);
static bool read_ups2000_registers(uint8_t modbus_addr, uint8_t ups_index, uint16_t reg_addr, uint16_t reg_count,
                                   uint8_t *buffer, uint8_t *error_code);
static uint16_t read_be_u16(const uint8_t *data);
static uint8_t read_soc(const uint8_t *data, const ups_model_descriptor_t *model);
static uint16_t maybe_seconds_to_minutes(uint16_t value, bool convert);

void ups_detector_init(ups_detector_t *ups, uint16_t manufacture_model, uint8_t slave_addr) {
  if (ups == NULL) {
    return;
  }
  memset(ups, 0, sizeof(*ups));
  ups->manufacture_model = manufacture_model;
  ups->slave_addr = slave_addr;
}

bool ups_detector_poll(ups_detector_t *ups) {
  if (ups == NULL || ups->slave_addr == 0U) {
    return false;
  }

  const ups_model_descriptor_t *model = find_model(ups->manufacture_model);
  if (model == NULL) {
    ups->online = false;
    ups->last_error = UPS_ERR_UNSUPPORTED;
    LOG_ERROR("UPS model unsupported, model=%u slave=%u", ups->manufacture_model, ups->slave_addr);
    return false;
  }

  return model->special_parser == UPS_SPECIAL_YTG_B3120 ? poll_ytg_b3120(ups, model) : poll_model(ups, model);
}

bool ups_detector_is_supported_model(uint16_t manufacture_model) {
  return find_model(manufacture_model) != NULL;
}

static const ups_model_descriptor_t *find_model(uint16_t manufacture_model) {
  for (uint16_t i = 0U; i < (uint16_t)(sizeof(ups_models) / sizeof(ups_models[0])); i++) {
    if (ups_models[i].manufacture_model == manufacture_model) {
      return &ups_models[i];
    }
  }
  return NULL;
}

static bool poll_model(ups_detector_t *ups, const ups_model_descriptor_t *model) {
  uint8_t input_voltages[6] = {0};
  uint8_t output_voltages[6] = {0};
  uint8_t load_rates[6] = {0};
  uint8_t battery_voltage[4] = {0};
  uint8_t battery_state[6] = {0};
  uint8_t battery_temperature[2] = {0};
  uint8_t battery_time[2] = {0};
  uint8_t error_code = 0U;
  uint8_t slave_addr = ups->slave_addr;

  if (model->special_parser == UPS_SPECIAL_HUAWEI_UPS2000) {
    if (ups->slave_addr > UPS2000_MAX_INDEX) {
      ups->online = false;
      ups->last_error = UPS_ERR_INVALID_ADDR;
      LOG_ERROR("UPS2000 index invalid, index=%u", ups->slave_addr);
      return false;
    }
    slave_addr = (uint8_t)(UPS2000_MODBUS_ADDR_BASE + ups->slave_addr);
  }

  bool ok = true;
  ok = read_required(model, slave_addr, model->input_voltage_addr, 3U, input_voltages, &error_code) && ok;
  ok = read_required(model, slave_addr, model->output_voltage_addr, 3U, output_voltages, &error_code) && ok;
  ok = read_required(model, slave_addr, model->load_rate_addr, 3U, load_rates, &error_code) && ok;
  ok = read_required(model, slave_addr, model->battery_voltage_addr, model->battery_voltage_regs, battery_voltage, &error_code) && ok;

  if (model->battery_state_addr != REG_UNUSED) {
    ok = read_required(model, slave_addr, model->battery_state_addr, model->battery_state_regs, battery_state, &error_code) && ok;
  }
  if (model->battery_temperature_addr != REG_UNUSED) {
    ok = read_required(model, slave_addr, model->battery_temperature_addr, 1U, battery_temperature, &error_code) && ok;
  }
  if (model->battery_time_addr != REG_UNUSED) {
    ok = read_required(model, slave_addr, model->battery_time_addr, 1U, battery_time, &error_code) && ok;
  }

  if (!ok) {
    ups->online = false;
    ups->last_error = error_code;
    LOG_ERROR("UPS poll failed, model=%u name=%s slave=%u err=0x%02X", model->manufacture_model, model->name,
              ups->slave_addr, error_code);
    return false;
  }

  for (uint8_t phase = 0U; phase < 3U; phase++) {
    ups->input_voltage_x10_v[phase] = read_be_u16(&input_voltages[phase * 2U]);
    ups->output_voltage_x10_v[phase] = read_be_u16(&output_voltages[phase * 2U]);
    ups->load_rate_x10_percent[phase] = (uint16_t)(read_be_u16(&load_rates[phase * 2U]) * model->load_scale_x10);
  }

  ups->battery_positive_voltage_x10_v = read_be_u16(battery_voltage);
  ups->battery_negative_voltage_x10_v = model->has_negative_battery ? read_be_u16(&battery_voltage[2]) : 0U;
  ups->battery_state_of_charge_percent = read_soc(&battery_state[model->battery_soc_offset], model);
  ups->battery_residual_discharge_minutes =
    maybe_seconds_to_minutes(read_be_u16(model->battery_time_addr != REG_UNUSED ? battery_time : &battery_state[model->battery_time_offset]),
                             model->backup_seconds_to_minutes);
  ups->battery_temperature_x10_c =
    (int16_t)read_be_u16(model->battery_temperature_addr != REG_UNUSED ? battery_temperature : &battery_state[model->battery_temperature_offset]);

  if (model->special_parser == UPS_SPECIAL_YTG_B3330 && ups->battery_residual_discharge_minutes == 0xFFFFU) {
    /* B3330 文档约定 0xFFFF 表示剩余时间正在计算，旧工程也上报为 0。 */
    ups->battery_residual_discharge_minutes = 0U;
  }

  ups->online = true;
  ups->last_error = 0U;
  return true;
}

static bool poll_ytg_b3120(ups_detector_t *ups, const ups_model_descriptor_t *model) {
  uint8_t input_register[UPS_YTG_B3120_INPUT_REGISTER_COUNT * 2U] = {0};
  uint8_t error_code = 0U;

  (void)model;
  /* B3120 旧代码只用 0x1388 起的大块输入寄存器解析关键遥测，LCD/协议版本不参与平台上报。 */
  if (!Master_ReadInputRegisters(ups->slave_addr, UPS_YTG_B3120_INPUT_REGISTER_ADDR, UPS_YTG_B3120_INPUT_REGISTER_COUNT,
                                 input_register, &error_code)) {
    ups->online = false;
    ups->last_error = error_code;
    LOG_ERROR("UPS B3120 poll failed, slave=%u err=0x%02X", ups->slave_addr, error_code);
    return false;
  }

  ups->battery_residual_discharge_minutes = read_be_u16(&input_register[2]);
  ups->battery_state_of_charge_percent = input_register[5];
  ups->battery_positive_voltage_x10_v = read_be_u16(&input_register[6]);
  ups->battery_negative_voltage_x10_v = 0U;
  ups->battery_temperature_x10_c = (int16_t)read_be_u16(&input_register[10]);
  for (uint8_t phase = 0U; phase < 3U; phase++) {
    ups->input_voltage_x10_v[phase] = read_be_u16(&input_register[16U + phase * 2U]);
    ups->output_voltage_x10_v[phase] = read_be_u16(&input_register[40U + phase * 2U]);
    ups->load_rate_x10_percent[phase] = read_be_u16(&input_register[58U + phase * 2U]);
  }

  ups->online = true;
  ups->last_error = 0U;
  return true;
}

static bool read_required(const ups_model_descriptor_t *model, uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_count,
                          uint8_t *buffer, uint8_t *error_code) {
  if (reg_addr == REG_UNUSED || reg_count == 0U) {
    return true;
  }
  return model->special_parser == UPS_SPECIAL_HUAWEI_UPS2000
           ? read_ups2000_registers(slave_addr, (uint8_t)(slave_addr - UPS2000_MODBUS_ADDR_BASE), reg_addr, reg_count,
                                    buffer, error_code)
           : read_registers(model, slave_addr, reg_addr, reg_count, buffer, error_code);
}

static bool read_registers(const ups_model_descriptor_t *model, uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_count,
                           uint8_t *buffer, uint8_t *error_code) {
  switch (model->read_type) {
  case UPS_READ_INPUT:
    return Master_ReadInputRegisters(slave_addr, reg_addr, reg_count, buffer, error_code);
  case UPS_READ_COIL_COMPAT:
    /* 旧 Modbus 层函数名叫 ReadCoilStatus，但内部沿用 ReadHoldReg 构包，多个旧设备均依赖该兼容行为。 */
    return Master_ReadCoilStatus(slave_addr, reg_addr, reg_count, buffer, error_code);
  case UPS_READ_HOLD:
  default:
    return Master_ReadHoldRegisters(slave_addr, reg_addr, reg_count, buffer, error_code);
  }
}

static bool read_ups2000_registers(uint8_t modbus_addr, uint8_t ups_index, uint16_t reg_addr, uint16_t reg_count,
                                   uint8_t *buffer, uint8_t *error_code) {
  bool ok = false;
  uint16_t real_reg_addr = (uint16_t)(reg_addr + (10000U * ups_index));
  uint8_t retry = 0U;

  do {
    /* 旧工程记录 UPS2000 偶发 CRC/应答失败，最多重试 5 次以兼容现场监控卡。 */
    ok = Master_ReadCoilStatus(modbus_addr, real_reg_addr, reg_count, buffer, error_code);
    retry++;
  } while (!ok && retry < UPS2000_MAX_RETRY);

  return ok;
}

static uint16_t read_be_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint8_t read_soc(const uint8_t *data, const ups_model_descriptor_t *model) {
  return model->soc_format == UPS_SOC_LOW_BYTE ? data[1] : (uint8_t)read_be_u16(data);
}

static uint16_t maybe_seconds_to_minutes(uint16_t value, bool convert) {
  return convert ? (uint16_t)(value / 60U) : value;
}
