#include "discrete_sensor.h"

#include "log.h"
#include "modbus_core_master.h"
#include <string.h>

#define SMOKE_ALARM_ADDR 0x0003U
#define SMOKE_ALARM_DELAY_ADDR 0x0033U
#define IMMERSION_ALARM_ADDR_1 0x0000U
#define IMMERSION_ALARM_ADDR_2 0x0002U
#define IMMERSION_ALARM_DELAY_ADDR 0x0033U
#define IMMERSION_ALARM_SENST_ADDR 0x0033U
#define INFRARED_ALARM_ADDR 0x0003U
#define INFRARED_ALARM_DELAY_ADDR 0x0033U
#define POWER_OUTAGE_ALARM_ADDR 0x0003U

static uint16_t read_le_u16(const uint8_t *data);
static bool read_alarm(discrete_sensor_t *sensor, uint16_t register_addr, uint8_t *alarm);
static bool read_hold_u16(discrete_sensor_t *sensor, uint16_t register_addr, uint16_t *value);
static bool write_hold_u16(discrete_sensor_t *sensor, uint16_t register_addr, uint16_t value);

void discrete_sensor_init(discrete_sensor_t *sensor, discrete_sensor_type_t type, uint16_t manufacture_model,
                          uint8_t slave_addr) {
  if (sensor == NULL) {
    return;
  }
  memset(sensor, 0, sizeof(*sensor));
  sensor->type = type;
  sensor->manufacture_model = manufacture_model;
  sensor->slave_addr = slave_addr;
}

bool discrete_sensor_poll(discrete_sensor_t *sensor) {
  bool ok = false;
  uint8_t alarm = 0U;

  if (sensor == NULL || sensor->slave_addr == 0U) {
    return false;
  }

  switch (sensor->type) {
  case DISCRETE_SENSOR_SMOKE:
    ok = read_alarm(sensor, SMOKE_ALARM_ADDR, &alarm);
    break;
  case DISCRETE_SENSOR_IMMERSION: {
    uint16_t alarm_1 = 0U;
    uint16_t alarm_2 = 0U;
    ok = read_hold_u16(sensor, IMMERSION_ALARM_ADDR_1, &alarm_1) &&
         read_hold_u16(sensor, IMMERSION_ALARM_ADDR_2, &alarm_2);
    alarm = (uint8_t)((alarm_1 != 0U || alarm_2 == 2U) ? 1U : 0U);
    break;
  }
  case DISCRETE_SENSOR_INFRARED:
    ok = read_alarm(sensor, INFRARED_ALARM_ADDR, &alarm);
    break;
  case DISCRETE_SENSOR_POWER_OUTAGE:
    ok = read_alarm(sensor, POWER_OUTAGE_ALARM_ADDR, &alarm);
    break;
  default:
    sensor->last_error = 0xFFU;
    break;
  }

  sensor->online = ok;
  if (ok) {
    sensor->alarm = alarm;
    sensor->last_error = 0U;
  } else {
    LOG_ERROR("Discrete sensor poll failed, type=%s slave=%u err=0x%02X", discrete_sensor_type_name(sensor->type),
              sensor->slave_addr, sensor->last_error);
  }
  return ok;
}

bool discrete_sensor_set_alarm_delay(discrete_sensor_t *sensor, uint16_t value) {
  uint16_t register_addr = 0U;
  if (sensor == NULL) {
    return false;
  }
  switch (sensor->type) {
  case DISCRETE_SENSOR_SMOKE:
    register_addr = SMOKE_ALARM_DELAY_ADDR;
    break;
  case DISCRETE_SENSOR_IMMERSION:
    register_addr = IMMERSION_ALARM_DELAY_ADDR;
    break;
  case DISCRETE_SENSOR_INFRARED:
    register_addr = INFRARED_ALARM_DELAY_ADDR;
    break;
  default:
    return false;
  }
  if (!write_hold_u16(sensor, register_addr, value)) {
    return false;
  }
  sensor->alarm_delay = value;
  return true;
}

bool discrete_sensor_set_sensitivity(discrete_sensor_t *sensor, uint16_t value) {
  if (sensor == NULL || sensor->type != DISCRETE_SENSOR_IMMERSION) {
    return false;
  }
  if (!write_hold_u16(sensor, IMMERSION_ALARM_SENST_ADDR, value)) {
    return false;
  }
  sensor->sensitivity = value;
  return true;
}

const char *discrete_sensor_type_name(discrete_sensor_type_t type) {
  switch (type) {
  case DISCRETE_SENSOR_SMOKE:
    return "smoke";
  case DISCRETE_SENSOR_IMMERSION:
    return "immersion";
  case DISCRETE_SENSOR_INFRARED:
    return "infrared";
  case DISCRETE_SENSOR_POWER_OUTAGE:
    return "power_outage";
  default:
    return "unknown";
  }
}

static uint16_t read_le_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static bool read_alarm(discrete_sensor_t *sensor, uint16_t register_addr, uint8_t *alarm) {
  uint8_t data[2] = {0};
  uint8_t error_code = 0U;
  bool ok = Master_ReadCoilStatus(sensor->slave_addr, register_addr, 1U, data, &error_code);
  sensor->last_error = ok ? 0U : error_code;
  if (ok && alarm != NULL) {
    *alarm = read_le_u16(data) != 0U ? 1U : 0U;
  }
  return ok;
}

static bool read_hold_u16(discrete_sensor_t *sensor, uint16_t register_addr, uint16_t *value) {
  uint8_t data[2] = {0};
  uint8_t error_code = 0U;
  bool ok = Master_ReadHoldRegisters(sensor->slave_addr, register_addr, 1U, data, &error_code);
  sensor->last_error = ok ? 0U : error_code;
  if (ok && value != NULL) {
    *value = read_le_u16(data);
  }
  return ok;
}

static bool write_hold_u16(discrete_sensor_t *sensor, uint16_t register_addr, uint16_t value) {
  uint8_t error_code = 0U;
  bool ok = Master_WriteOneRegister(sensor->slave_addr, register_addr, value, &error_code);
  sensor->last_error = ok ? 0U : error_code;
  return ok;
}
