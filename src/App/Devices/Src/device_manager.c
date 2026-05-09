#include "device_manager.h"

#include "ac_controller.h"
#include "bsp_board.h"
#include "config.h"
#include "discrete_sensor.h"
#include "device_catalog.h"
#include "electricity_meter.h"
#include "log.h"
#include "mqtt_client.h"
#include "parson.h"
#include "smart_switch.h"
#include "telemetry_reporter.h"
#include "temperature_humidity_sensor.h"
#include "ups_detector_interface.h"
#include <stdio.h>
#include <string.h>

#define DEVICE_MANAGER_DEFAULT_REPORT_INTERVAL_MS 10000UL

#define CMD_SWITCH_OPEN_ONE 1U
#define CMD_SWITCH_CLOSE_ONE 2U
#define CMD_SWITCH_OPEN_ALL 3U
#define CMD_SWITCH_CLOSE_ALL 4U
static discrete_sensor_t smoke_sensor;
static discrete_sensor_t immersion_sensor;
static discrete_sensor_t infrared_sensor;
static discrete_sensor_t power_outage_alarm;
static electricity_meter_t electricity_meter;
static smart_switch_t smart_switch;
static ac_controller_t ac_controller;
static ups_detector_t ups_detector;
static uint32_t last_background_poll_tick;
static void format_x10_signed(char *buffer, uint16_t buffer_len, int16_t value);
static void format_x10_unsigned(char *buffer, uint16_t buffer_len, uint16_t value);
static void print_x10_signed(const char *name, int16_t value);
static void print_x10_unsigned(const char *name, uint16_t value);
static bool serialize_json_to_buffer(JSON_Value *root_value, char *buffer, uint16_t buffer_len);
static void append_temperature_humidity(JSON_Array *devices);
static void append_discrete_sensor(JSON_Array *devices, const discrete_sensor_t *sensor);
static void append_electricity_meter(JSON_Array *devices);
static void append_smart_switch(JSON_Array *devices);
static void append_ac_controller(JSON_Array *devices);
static void append_ups_detector(JSON_Array *devices);

void device_manager_init(void) {
  device_manager_apply_current_defaults();
}

void device_manager_apply_current_defaults(void) {
  /*
   * 当前“默认”不再是温湿度基线，而是 EEPROM 里保存的现场设备表。
   * 启动时只恢复已保存且明确勾选的设备，避免把未接线设备硬塞进轮询表。
   */
  device_manager_clear_registered_devices();
  if (active_config.devices.count == 0U) {
    return;
  }

  for (uint8_t index = 0U; index < active_config.devices.count; index++) {
    const config_device_entry_t *entry = &active_config.devices.entries[index];
    if (entry->enabled == 0U || entry->slave_addr == 0U) {
      continue;
    }
    (void)device_manager_configure_registered_device(entry->service_id, entry->manufacture_model, entry->slave_addr);
  }
}

void device_manager_clear_registered_devices(void) {
  temp_humidity_sensor_init(0U, 0U);
  discrete_sensor_init(&smoke_sensor, DISCRETE_SENSOR_SMOKE, 0U, 0U);
  discrete_sensor_init(&immersion_sensor, DISCRETE_SENSOR_IMMERSION, 0U, 0U);
  discrete_sensor_init(&infrared_sensor, DISCRETE_SENSOR_INFRARED, 0U, 0U);
  discrete_sensor_init(&power_outage_alarm, DISCRETE_SENSOR_POWER_OUTAGE, 0U, 0U);
  smart_switch_init(&smart_switch, 0U, 0U);
  electricity_meter_init(&electricity_meter, 0U, 0U);
  ac_controller_init(&ac_controller, 0U, 0U);
  ups_detector_init(&ups_detector, 0U, 0U);
  last_background_poll_tick = 0U;
  telemetry_reporter_reset();
}

bool device_manager_configure_registered_device(uint16_t service_id, uint16_t manufacture_model, uint8_t slave_addr) {
  if (slave_addr == 0U) {
    return false;
  }
  // serviceId 与平台枚举一致，serialPort 在当前 RS485 场景中等价于 Modbus 从站地址。
  switch (service_id) {
  case DEVICE_SERVICE_TEMP_HUMIDITY:
    temp_humidity_sensor_init(manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case DISCRETE_SENSOR_SMOKE:
    discrete_sensor_init(&smoke_sensor, DISCRETE_SENSOR_SMOKE, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case DISCRETE_SENSOR_IMMERSION:
    discrete_sensor_init(&immersion_sensor, DISCRETE_SENSOR_IMMERSION, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case DISCRETE_SENSOR_INFRARED:
    discrete_sensor_init(&infrared_sensor, DISCRETE_SENSOR_INFRARED, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case DEVICE_SERVICE_SMART_SWITCH:
    smart_switch_init(&smart_switch, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case AC_CONTROLLER_SERVICE_ID:
    ac_controller_init(&ac_controller, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case ELECTRICITY_METER_SERVICE_ID:
    electricity_meter_init(&electricity_meter, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case UPS_DETECTOR_SERVICE_ID:
    if (!ups_detector_is_supported_model(manufacture_model)) {
      LOG_ERROR("Unsupported UPS registerInfo, model=%u slave=%u", manufacture_model, slave_addr);
      return false;
    }
    ups_detector_init(&ups_detector, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  case DISCRETE_SENSOR_POWER_OUTAGE:
    discrete_sensor_init(&power_outage_alarm, DISCRETE_SENSOR_POWER_OUTAGE, manufacture_model, slave_addr);
    telemetry_reporter_reset();
    return true;
  default:
    return false;
  }
}

void device_manager_poll(uint32_t now_ms, uint32_t report_interval_ms) {
  if (report_interval_ms == 0U) {
    report_interval_ms = DEVICE_MANAGER_DEFAULT_REPORT_INTERVAL_MS;
  }
  if (mqtt_client_is_ready() && telemetry_reporter_flush_one(now_ms)) {
    return;
  }
  if (last_background_poll_tick != 0U && (now_ms - last_background_poll_tick) < report_interval_ms) {
    return;
  }

  /*
   * 后台采集必须按配置周期跑，不能跟随主循环每秒抢 RS485。
   * 手工 device poll 仍直接调用 device_manager_poll_all()，用于现场立即确认从站响应。
   */
  last_background_poll_tick = now_ms;
  (void)device_manager_poll_all();
  telemetry_reporter_ingest(now_ms, temp_humidity_sensor_get(), &smoke_sensor, &immersion_sensor, &infrared_sensor,
                            &power_outage_alarm, &smart_switch, &ac_controller, &electricity_meter, &ups_detector);
  if (mqtt_client_is_ready()) {
    (void)telemetry_reporter_flush_one(now_ms);
  }
}

bool device_manager_poll_all(void) {
  bool ok = true;
  const temp_humidity_sensor_t *sensor = temp_humidity_sensor_get();
  if (sensor->slave_addr != 0U) {
    ok = device_manager_poll_temperature_humidity() && ok;
  }
  if (smoke_sensor.slave_addr != 0U) {
    ok = discrete_sensor_poll(&smoke_sensor) && ok;
  }
  if (immersion_sensor.slave_addr != 0U) {
    ok = discrete_sensor_poll(&immersion_sensor) && ok;
  }
  if (infrared_sensor.slave_addr != 0U) {
    ok = discrete_sensor_poll(&infrared_sensor) && ok;
  }
  if (power_outage_alarm.slave_addr != 0U) {
    ok = discrete_sensor_poll(&power_outage_alarm) && ok;
  }
  if (smart_switch.slave_addr != 0U) {
    ok = smart_switch_poll(&smart_switch) && ok;
  }
  if (ac_controller.slave_addr != 0U) {
    ok = ac_controller_poll(&ac_controller) && ok;
  }
  if (electricity_meter.slave_addr != 0U) {
    ok = electricity_meter_poll(&electricity_meter) && ok;
  }
  if (ups_detector.slave_addr != 0U) {
    ok = ups_detector_poll(&ups_detector) && ok;
  }
  return ok;
}

bool device_manager_poll_temperature_humidity(void) {
  return temp_humidity_sensor_poll();
}

bool device_manager_build_data_payload(char *buffer, uint16_t buffer_len) {
  if (buffer == NULL || buffer_len == 0U) {
    return false;
  }
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(root_value);
  JSON_Value *devices_value = json_value_init_array();
  JSON_Array *devices = json_value_get_array(devices_value);

  json_object_set_string(root, "type", "datas");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  json_object_set_value(root, "devices", devices_value);
  // 按当前注册表动态添加设备，避免 registerInfo 清表后继续上报 addr=0 的无效设备。
  append_temperature_humidity(devices);
  append_discrete_sensor(devices, &smoke_sensor);
  append_discrete_sensor(devices, &immersion_sensor);
  append_discrete_sensor(devices, &infrared_sensor);
  append_discrete_sensor(devices, &power_outage_alarm);
  append_smart_switch(devices);
  append_ac_controller(devices);
  append_electricity_meter(devices);
  append_ups_detector(devices);
  bool ok = serialize_json_to_buffer(root_value, buffer, buffer_len);
  json_value_free(root_value);
  return ok;
}

bool device_manager_publish_all(void) {
  char payload[MQTT_PAYLOAD_MAX_LEN] = {0};
  bool ok = device_manager_build_data_payload(payload, sizeof(payload)) && mqtt_client_publish_data(payload);
  return ok;
}

bool device_manager_publish_temperature_humidity(void) {
  char payload[MQTT_PAYLOAD_MAX_LEN] = {0};
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(root_value);
  JSON_Value *devices_value = json_value_init_array();
  JSON_Array *devices = json_value_get_array(devices_value);

  json_object_set_string(root, "type", "datas");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  json_object_set_value(root, "devices", devices_value);
  append_temperature_humidity(devices);
  bool ok = serialize_json_to_buffer(root_value, payload, sizeof(payload)) && mqtt_client_publish_data(payload);
  json_value_free(root_value);
  return ok;
}

bool device_manager_execute_command(uint16_t service_id, uint8_t slave_addr, uint16_t command, uint16_t param) {
  if (service_id == DEVICE_SERVICE_SMART_SWITCH) {
    if (slave_addr != 0U && smart_switch.slave_addr != 0U && slave_addr != smart_switch.slave_addr) {
      return false;
    }
    // 平台命令号保持兼容：1/2 控制单路继电器，3/4 控制全部继电器。
    switch (command) {
    case CMD_SWITCH_OPEN_ONE:
      return smart_switch_set_one(&smart_switch, (uint8_t)param, true);
    case CMD_SWITCH_CLOSE_ONE:
      return smart_switch_set_one(&smart_switch, (uint8_t)param, false);
    case CMD_SWITCH_OPEN_ALL:
      return smart_switch_set_all(&smart_switch, 0x00FFU);
    case CMD_SWITCH_CLOSE_ALL:
      return smart_switch_set_all(&smart_switch, 0x0000U);
    default:
      return false;
    }
  }
  if (service_id == AC_CONTROLLER_SERVICE_ID) {
    if (slave_addr != 0U && ac_controller.slave_addr != 0U && slave_addr != ac_controller.slave_addr) {
      return false;
    }
    return ac_controller_execute(&ac_controller, command, param);
  }
  return false;
}

void device_manager_print_status(void) {
  const temp_humidity_sensor_t *sensor = temp_humidity_sensor_get();
  LOG_CMD_RESP("temp_humi.addr=%u reported=%u baud=%lu", sensor->slave_addr, sensor->reported_slave_addr,
               sensor->baud_rate);
  LOG_CMD_RESP("temp_humi.online=%u last_error=0x%02X", sensor->online ? 1U : 0U, sensor->last_error);
  print_x10_signed("temp_humi.temperature", sensor->temperature_x10_c);
  print_x10_unsigned("temp_humi.humidity", sensor->humidity_x10_rh);
  print_x10_signed("temp_humi.temperature_cali", sensor->temperature_cali_x10_c);
  print_x10_unsigned("temp_humi.humidity_cali", sensor->humidity_cali_x10_rh);
  LOG_CMD_RESP("smoke.addr=%u online=%u alarm=%u err=0x%02X", smoke_sensor.slave_addr,
               smoke_sensor.online ? 1U : 0U, smoke_sensor.alarm, smoke_sensor.last_error);
  LOG_CMD_RESP("immersion.addr=%u online=%u alarm=%u err=0x%02X", immersion_sensor.slave_addr,
               immersion_sensor.online ? 1U : 0U, immersion_sensor.alarm, immersion_sensor.last_error);
  LOG_CMD_RESP("infrared.addr=%u online=%u alarm=%u err=0x%02X", infrared_sensor.slave_addr,
               infrared_sensor.online ? 1U : 0U, infrared_sensor.alarm, infrared_sensor.last_error);
  LOG_CMD_RESP("power_outage.addr=%u online=%u alarm=%u err=0x%02X", power_outage_alarm.slave_addr,
               power_outage_alarm.online ? 1U : 0U, power_outage_alarm.alarm, power_outage_alarm.last_error);
  LOG_CMD_RESP("smart_switch.addr=%u online=%u input=0x%04X output=0x%04X err=0x%02X", smart_switch.slave_addr,
               smart_switch.online ? 1U : 0U, smart_switch.input_bits, smart_switch.output_bits,
               smart_switch.last_error);
  LOG_CMD_RESP("electricity_meter.addr=%u online=%u dio=0x%04X err=0x%02X", electricity_meter.slave_addr,
                electricity_meter.online ? 1U : 0U, electricity_meter.switch_state, electricity_meter.last_error);
  print_x10_signed("electricity_meter.ua", electricity_meter.phase_voltage_x10[0]);
  print_x10_signed("electricity_meter.ub", electricity_meter.phase_voltage_x10[1]);
  print_x10_signed("electricity_meter.uc", electricity_meter.phase_voltage_x10[2]);
  print_x10_unsigned("electricity_meter.ia", electricity_meter.phase_current_x10[0]);
  print_x10_unsigned("electricity_meter.ib", electricity_meter.phase_current_x10[1]);
  print_x10_unsigned("electricity_meter.ic", electricity_meter.phase_current_x10[2]);
  LOG_CMD_RESP("ac.addr=%u online=%u status=%u current1=%u current2=%u err=0x%02X", ac_controller.slave_addr,
               ac_controller.online ? 1U : 0U, ac_controller_get_status(&ac_controller), ac_controller.current_ch1,
               ac_controller.current_ch2, ac_controller.last_error);
  print_x10_signed("ac.temperature", ac_controller.temperature_x10_c);
  print_x10_unsigned("ac.humidity", ac_controller.humidity_x10_rh);
  LOG_CMD_RESP("ups.addr=%u model=%u online=%u soc=%u backup_minutes=%u err=0x%02X", ups_detector.slave_addr,
               ups_detector.manufacture_model, ups_detector.online ? 1U : 0U,
               ups_detector.battery_state_of_charge_percent, ups_detector.battery_residual_discharge_minutes,
               ups_detector.last_error);
  print_x10_unsigned("ups.input.ua", ups_detector.input_voltage_x10_v[0]);
  print_x10_unsigned("ups.input.ub", ups_detector.input_voltage_x10_v[1]);
  print_x10_unsigned("ups.input.uc", ups_detector.input_voltage_x10_v[2]);
  print_x10_unsigned("ups.output.ua", ups_detector.output_voltage_x10_v[0]);
  print_x10_unsigned("ups.output.ub", ups_detector.output_voltage_x10_v[1]);
  print_x10_unsigned("ups.output.uc", ups_detector.output_voltage_x10_v[2]);
  print_x10_unsigned("ups.load.a", ups_detector.load_rate_x10_percent[0]);
  print_x10_unsigned("ups.load.b", ups_detector.load_rate_x10_percent[1]);
  print_x10_unsigned("ups.load.c", ups_detector.load_rate_x10_percent[2]);
  print_x10_unsigned("ups.battery.positive_voltage", ups_detector.battery_positive_voltage_x10_v);
  print_x10_signed("ups.battery.temperature", ups_detector.battery_temperature_x10_c);
}

static void format_x10_signed(char *buffer, uint16_t buffer_len, int16_t value) {
  uint16_t abs_value = (uint16_t)(value < 0 ? -value : value);
  (void)snprintf(buffer, buffer_len, "%s%u.%u", value < 0 ? "-" : "", abs_value / 10U, abs_value % 10U);
}

static void format_x10_unsigned(char *buffer, uint16_t buffer_len, uint16_t value) {
  (void)snprintf(buffer, buffer_len, "%u.%u", value / 10U, value % 10U);
}

static void print_x10_signed(const char *name, int16_t value) {
  char formatted[12] = {0};
  format_x10_signed(formatted, sizeof(formatted), value);
  LOG_CMD_RESP("%s=%s", name, formatted);
}

static void print_x10_unsigned(const char *name, uint16_t value) {
  char formatted[12] = {0};
  format_x10_unsigned(formatted, sizeof(formatted), value);
  LOG_CMD_RESP("%s=%s", name, formatted);
}

static bool serialize_json_to_buffer(JSON_Value *root_value, char *buffer, uint16_t buffer_len) {
  if (root_value == NULL || buffer == NULL || buffer_len == 0U) {
    return false;
  }
  size_t needed = json_serialization_size(root_value);
  if (needed == 0U || needed > buffer_len) {
    LOG_WARNING("Device payload too large, need=%u buffer=%u", (unsigned int)needed, buffer_len);
    return false;
  }
  return json_serialize_to_buffer(root_value, buffer, buffer_len) == JSONSuccess;
}

static void append_temperature_humidity(JSON_Array *devices) {
  const temp_humidity_sensor_t *sensor = temp_humidity_sensor_get();
  if (devices == NULL || sensor->slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", "temperatureHumidity");
  json_object_set_number(device, "serviceId", DEVICE_SERVICE_TEMP_HUMIDITY);
  json_object_set_number(device, "addr", sensor->slave_addr);
  json_object_set_boolean(device, "online", sensor->online ? 1 : 0);
  json_object_set_number(device, "temperature", (double)sensor->temperature_x10_c / 10.0);
  json_object_set_number(device, "humidity", (double)sensor->humidity_x10_rh / 10.0);
  json_object_set_number(device, "baud", sensor->baud_rate);
  json_array_append_value(devices, device_value);
}

static void append_discrete_sensor(JSON_Array *devices, const discrete_sensor_t *sensor) {
  if (devices == NULL || sensor == NULL || sensor->slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", discrete_sensor_type_name(sensor->type));
  json_object_set_number(device, "serviceId", (uint16_t)sensor->type);
  json_object_set_number(device, "addr", sensor->slave_addr);
  json_object_set_boolean(device, "online", sensor->online ? 1 : 0);
  json_object_set_number(device, "alarm", sensor->alarm);
  json_array_append_value(devices, device_value);
}

static void append_smart_switch(JSON_Array *devices) {
  if (devices == NULL || smart_switch.slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", "smartSwitch");
  json_object_set_number(device, "serviceId", DEVICE_SERVICE_SMART_SWITCH);
  json_object_set_number(device, "addr", smart_switch.slave_addr);
  json_object_set_boolean(device, "online", smart_switch.online ? 1 : 0);
  json_object_set_number(device, "input", smart_switch.input_bits);
  json_object_set_number(device, "output", smart_switch.output_bits);
  json_array_append_value(devices, device_value);
}

static void append_ac_controller(JSON_Array *devices) {
  if (devices == NULL || ac_controller.slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", "acController");
  json_object_set_number(device, "serviceId", AC_CONTROLLER_SERVICE_ID);
  json_object_set_number(device, "addr", ac_controller.slave_addr);
  json_object_set_boolean(device, "online", ac_controller.online ? 1 : 0);
  json_object_set_number(device, "temperature", (double)ac_controller.temperature_x10_c / 10.0);
  json_object_set_number(device, "humidity", (double)ac_controller.humidity_x10_rh / 10.0);
  json_object_set_number(device, "status", ac_controller_get_status(&ac_controller));
  json_object_set_number(device, "current1", ac_controller.current_ch1);
  json_object_set_number(device, "current2", ac_controller.current_ch2);
  json_array_append_value(devices, device_value);
}

static void append_electricity_meter(JSON_Array *devices) {
  if (devices == NULL || electricity_meter.slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", "electricityMeter");
  json_object_set_number(device, "serviceId", ELECTRICITY_METER_SERVICE_ID);
  json_object_set_number(device, "addr", electricity_meter.slave_addr);
  json_object_set_boolean(device, "online", electricity_meter.online ? 1 : 0);
  json_object_set_number(device, "dio", electricity_meter.switch_state);
  json_object_set_number(device, "ua", (double)electricity_meter.phase_voltage_x10[0] / 10.0);
  json_object_set_number(device, "ub", (double)electricity_meter.phase_voltage_x10[1] / 10.0);
  json_object_set_number(device, "uc", (double)electricity_meter.phase_voltage_x10[2] / 10.0);
  json_object_set_number(device, "ia", (double)electricity_meter.phase_current_x10[0] / 10.0);
  json_object_set_number(device, "ib", (double)electricity_meter.phase_current_x10[1] / 10.0);
  json_object_set_number(device, "ic", (double)electricity_meter.phase_current_x10[2] / 10.0);
  json_array_append_value(devices, device_value);
}

static void append_ups_detector(JSON_Array *devices) {
  if (devices == NULL || ups_detector.slave_addr == 0U) {
    return;
  }
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  json_object_set_string(device, "type", "ups");
  json_object_set_number(device, "serviceId", UPS_DETECTOR_SERVICE_ID);
  json_object_set_number(device, "brand", ups_detector.manufacture_model);
  json_object_set_number(device, "addr", ups_detector.slave_addr);
  json_object_set_boolean(device, "online", ups_detector.online ? 1 : 0);
  json_object_set_number(device, "inputUa", (double)ups_detector.input_voltage_x10_v[0] / 10.0);
  json_object_set_number(device, "inputUb", (double)ups_detector.input_voltage_x10_v[1] / 10.0);
  json_object_set_number(device, "inputUc", (double)ups_detector.input_voltage_x10_v[2] / 10.0);
  json_object_set_number(device, "outputUa", (double)ups_detector.output_voltage_x10_v[0] / 10.0);
  json_object_set_number(device, "outputUb", (double)ups_detector.output_voltage_x10_v[1] / 10.0);
  json_object_set_number(device, "outputUc", (double)ups_detector.output_voltage_x10_v[2] / 10.0);
  json_object_set_number(device, "loadA", (double)ups_detector.load_rate_x10_percent[0] / 10.0);
  json_object_set_number(device, "loadB", (double)ups_detector.load_rate_x10_percent[1] / 10.0);
  json_object_set_number(device, "loadC", (double)ups_detector.load_rate_x10_percent[2] / 10.0);
  json_object_set_number(device, "batteryVoltage", (double)ups_detector.battery_positive_voltage_x10_v / 10.0);
  json_object_set_number(device, "batterySoc", ups_detector.battery_state_of_charge_percent);
  json_object_set_number(device, "backupMinutes", ups_detector.battery_residual_discharge_minutes);
  json_object_set_number(device, "batteryTemperature", (double)ups_detector.battery_temperature_x10_c / 10.0);
  json_array_append_value(devices, device_value);
}
