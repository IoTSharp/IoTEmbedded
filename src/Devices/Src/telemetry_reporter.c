#include "Devices/Inc/telemetry_reporter.h"

#include "Devices/Inc/ac_controller.h"
#include "Config/Inc/config.h"
#include "Devices/Inc/device_catalog.h"
#include "Common/Inc/log.h"
#include "Protocol/Mqtt/Inc/mqtt_client.h"
#include "ThirdParty/Parson/parson.h"
#include <stdio.h>
#include <string.h>

#define TELEMETRY_SAMPLE_INTERVAL_MS 6000UL
#define TELEMETRY_WINDOW_SAMPLE_COUNT 10U
#define TELEMETRY_MIN_UPLOAD_INTERVAL_MS 60000UL
#define TELEMETRY_METRIC_CAPACITY 48U
#define TELEMETRY_QUEUE_CAPACITY 8U
#define TELEMETRY_PAYLOAD_MAX_LEN 768U

/*
 * 阈值先集中在这里，后续平台或现场确认精度后只改这一组常量。
 * 单位沿用设备缓存：x10 表示 0.1 单位，普通状态量按整数精确变化判定。
 */
#define TELEMETRY_TEMP_DELTA_X10 5
#define TELEMETRY_HUMIDITY_DELTA_X10 20
#define TELEMETRY_VOLTAGE_DELTA_X10 10
#define TELEMETRY_CURRENT_DELTA_X10 10
#define TELEMETRY_LOAD_DELTA_X10 10
#define TELEMETRY_UNIT_DELTA 1

typedef enum {
  TELEMETRY_KIND_ANALOG = 0,
  TELEMETRY_KIND_STATE,
} telemetry_metric_kind_t;

typedef enum {
  TM_TEMP_T = 1,
  TM_TEMP_H,
  TM_TEMP_TCALI,
  TM_TEMP_HCALI,
  TM_ALARM,
  TM_ALARM_DELAY,
  TM_SENSITIVITY,
  TM_SWITCH_INPUT,
  TM_SWITCH_OUTPUT,
  TM_AC_TEMP,
  TM_AC_HUMI,
  TM_AC_STATUS,
  TM_AC_CURRENT1,
  TM_AC_CURRENT2,
  TM_EM_DIO,
  TM_EM_UA,
  TM_EM_UB,
  TM_EM_UC,
  TM_EM_IA,
  TM_EM_IB,
  TM_EM_IC,
  TM_UPS_INPUT_UA,
  TM_UPS_INPUT_UB,
  TM_UPS_INPUT_UC,
  TM_UPS_OUTPUT_UA,
  TM_UPS_OUTPUT_UB,
  TM_UPS_OUTPUT_UC,
  TM_UPS_LOAD_A,
  TM_UPS_LOAD_B,
  TM_UPS_LOAD_C,
  TM_UPS_BVP,
  TM_UPS_BVN,
  TM_UPS_SOC,
  TM_UPS_RDT,
  TM_UPS_BTEMP,
} telemetry_metric_id_t;

typedef struct {
  bool used;
  uint16_t service_id;
  uint8_t addr;
  uint8_t metric_id;
  telemetry_metric_kind_t kind;
  int32_t threshold;
  int32_t samples[TELEMETRY_WINDOW_SAMPLE_COUNT];
  uint8_t count;
  uint8_t index;
  bool last_value_valid;
  int32_t last_value;
  uint32_t last_upload_tick;
} telemetry_metric_cache_t;

typedef struct {
  bool used;
  char payload[TELEMETRY_PAYLOAD_MAX_LEN];
} telemetry_queue_item_t;

static telemetry_metric_cache_t metric_caches[TELEMETRY_METRIC_CAPACITY];
static telemetry_queue_item_t upload_queue[TELEMETRY_QUEUE_CAPACITY];
static uint8_t queue_head;
static uint8_t queue_count;
static uint32_t last_sample_tick;
static uint32_t last_flush_tick;

static telemetry_metric_cache_t *find_metric_cache(uint16_t service_id, uint8_t addr, uint8_t metric_id,
                                                   telemetry_metric_kind_t kind, int32_t threshold);
static void track_metric(uint16_t service_id, uint8_t addr, uint8_t metric_id, telemetry_metric_kind_t kind,
                         int32_t value, int32_t threshold, uint32_t now_ms, bool *report_device);
static int32_t resolve_window_value(const telemetry_metric_cache_t *cache);
static int32_t median_i32(const int32_t *values, uint8_t count);
static bool value_changed(const telemetry_metric_cache_t *cache, int32_t value);
static bool enqueue_payload(JSON_Value *root_value);
static bool enqueue_temp_humidity(uint32_t now_ms, const temp_humidity_sensor_t *sensor);
static bool enqueue_discrete(uint32_t now_ms, const discrete_sensor_t *sensor);
static bool enqueue_smart_switch(uint32_t now_ms, const smart_switch_t *device);
static bool enqueue_ac_controller(uint32_t now_ms, const ac_controller_t *device);
static bool enqueue_electricity_meter(uint32_t now_ms, const electricity_meter_t *meter);
static bool enqueue_ups(uint32_t now_ms, const ups_detector_t *ups);
static JSON_Object *create_single_device_payload(JSON_Value **root_value, uint32_t now_ms, uint16_t service_id,
                                                 uint8_t addr);
static void append_bool(JSON_Object *object, const char *name, bool value);
static void append_number_array_u8(JSON_Object *object, const char *name, uint16_t bits);

void telemetry_reporter_reset(void) {
  memset(metric_caches, 0, sizeof(metric_caches));
  memset(upload_queue, 0, sizeof(upload_queue));
  queue_head = 0U;
  queue_count = 0U;
  last_sample_tick = 0U;
  last_flush_tick = 0U;
}

uint32_t telemetry_reporter_sample_interval_ms(void) {
  return TELEMETRY_SAMPLE_INTERVAL_MS;
}

void telemetry_reporter_ingest(uint32_t now_ms, const temp_humidity_sensor_t *temp_humidity,
                               const discrete_sensor_t *smoke, const discrete_sensor_t *immersion,
                               const discrete_sensor_t *infrared, const discrete_sensor_t *power_outage,
                               const smart_switch_t *smart_switch, const ac_controller_t *ac_controller,
                               const electricity_meter_t *electricity_meter, const ups_detector_t *ups) {
  if (last_sample_tick != 0U && (now_ms - last_sample_tick) < TELEMETRY_SAMPLE_INTERVAL_MS) {
    return;
  }
  last_sample_tick = now_ms;

  bool report = false;

  /*
   * 自动变化上报只采纳本轮仍在线的设备。离线状态仍可通过 device status/payload 诊断，
   * 但不要把缓存旧值封装成 online=true 的平台遥测。
   */
  if (temp_humidity != NULL && temp_humidity->slave_addr != 0U && temp_humidity->online) {
    report = false;
    track_metric(DEVICE_SERVICE_TEMP_HUMIDITY, temp_humidity->slave_addr, TM_TEMP_T, TELEMETRY_KIND_ANALOG,
                 temp_humidity->temperature_x10_c, TELEMETRY_TEMP_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_TEMP_HUMIDITY, temp_humidity->slave_addr, TM_TEMP_H, TELEMETRY_KIND_ANALOG,
                 temp_humidity->humidity_x10_rh, TELEMETRY_HUMIDITY_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_TEMP_HUMIDITY, temp_humidity->slave_addr, TM_TEMP_TCALI, TELEMETRY_KIND_ANALOG,
                 temp_humidity->temperature_cali_x10_c, TELEMETRY_TEMP_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_TEMP_HUMIDITY, temp_humidity->slave_addr, TM_TEMP_HCALI, TELEMETRY_KIND_ANALOG,
                 temp_humidity->humidity_cali_x10_rh, TELEMETRY_HUMIDITY_DELTA_X10, now_ms, &report);
    if (report) {
      (void)enqueue_temp_humidity(now_ms, temp_humidity);
    }
  }

  if (smoke != NULL && smoke->slave_addr != 0U && smoke->online) {
    report = false;
    track_metric((uint16_t)smoke->type, smoke->slave_addr, TM_ALARM, TELEMETRY_KIND_STATE, smoke->alarm,
                 TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric((uint16_t)smoke->type, smoke->slave_addr, TM_ALARM_DELAY, TELEMETRY_KIND_ANALOG,
                 smoke->alarm_delay, TELEMETRY_UNIT_DELTA, now_ms, &report);
    if (report) {
      (void)enqueue_discrete(now_ms, smoke);
    }
  }

  if (immersion != NULL && immersion->slave_addr != 0U && immersion->online) {
    report = false;
    track_metric((uint16_t)immersion->type, immersion->slave_addr, TM_ALARM, TELEMETRY_KIND_STATE, immersion->alarm,
                 TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric((uint16_t)immersion->type, immersion->slave_addr, TM_ALARM_DELAY, TELEMETRY_KIND_ANALOG,
                 immersion->alarm_delay, TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric((uint16_t)immersion->type, immersion->slave_addr, TM_SENSITIVITY, TELEMETRY_KIND_ANALOG,
                 immersion->sensitivity, TELEMETRY_UNIT_DELTA, now_ms, &report);
    if (report) {
      (void)enqueue_discrete(now_ms, immersion);
    }
  }

  if (infrared != NULL && infrared->slave_addr != 0U && infrared->online) {
    report = false;
    track_metric((uint16_t)infrared->type, infrared->slave_addr, TM_ALARM, TELEMETRY_KIND_STATE, infrared->alarm,
                 TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric((uint16_t)infrared->type, infrared->slave_addr, TM_ALARM_DELAY, TELEMETRY_KIND_ANALOG,
                 infrared->alarm_delay, TELEMETRY_UNIT_DELTA, now_ms, &report);
    if (report) {
      (void)enqueue_discrete(now_ms, infrared);
    }
  }

  if (power_outage != NULL && power_outage->slave_addr != 0U && power_outage->online) {
    report = false;
    track_metric((uint16_t)power_outage->type, power_outage->slave_addr, TM_ALARM, TELEMETRY_KIND_STATE,
                 power_outage->alarm, TELEMETRY_UNIT_DELTA, now_ms, &report);
    if (report) {
      (void)enqueue_discrete(now_ms, power_outage);
    }
  }

  if (smart_switch != NULL && smart_switch->slave_addr != 0U && smart_switch->online) {
    report = false;
    track_metric(DEVICE_SERVICE_SMART_SWITCH, smart_switch->slave_addr, TM_SWITCH_INPUT, TELEMETRY_KIND_STATE,
                 smart_switch->input_bits, TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric(DEVICE_SERVICE_SMART_SWITCH, smart_switch->slave_addr, TM_SWITCH_OUTPUT, TELEMETRY_KIND_STATE,
                 smart_switch->output_bits, TELEMETRY_UNIT_DELTA, now_ms, &report);
    if (report) {
      (void)enqueue_smart_switch(now_ms, smart_switch);
    }
  }

  if (ac_controller != NULL && ac_controller->slave_addr != 0U && ac_controller->online) {
    report = false;
    track_metric(DEVICE_SERVICE_AC_CONTROLLER, ac_controller->slave_addr, TM_AC_TEMP, TELEMETRY_KIND_ANALOG,
                 ac_controller->temperature_x10_c, TELEMETRY_TEMP_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_AC_CONTROLLER, ac_controller->slave_addr, TM_AC_HUMI, TELEMETRY_KIND_ANALOG,
                 ac_controller->humidity_x10_rh, TELEMETRY_HUMIDITY_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_AC_CONTROLLER, ac_controller->slave_addr, TM_AC_STATUS, TELEMETRY_KIND_STATE,
                 ac_controller_get_status(ac_controller), TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric(DEVICE_SERVICE_AC_CONTROLLER, ac_controller->slave_addr, TM_AC_CURRENT1, TELEMETRY_KIND_ANALOG,
                 ac_controller->current_ch1, TELEMETRY_CURRENT_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_AC_CONTROLLER, ac_controller->slave_addr, TM_AC_CURRENT2, TELEMETRY_KIND_ANALOG,
                 ac_controller->current_ch2, TELEMETRY_CURRENT_DELTA_X10, now_ms, &report);
    if (report) {
      (void)enqueue_ac_controller(now_ms, ac_controller);
    }
  }

  if (electricity_meter != NULL && electricity_meter->slave_addr != 0U && electricity_meter->online) {
    report = false;
    track_metric(DEVICE_SERVICE_ELECTRICITY_METER, electricity_meter->slave_addr, TM_EM_DIO, TELEMETRY_KIND_STATE,
                 electricity_meter->switch_state, TELEMETRY_UNIT_DELTA, now_ms, &report);
    for (uint8_t phase = 0U; phase < 3U; phase++) {
      track_metric(DEVICE_SERVICE_ELECTRICITY_METER, electricity_meter->slave_addr, (uint8_t)(TM_EM_UA + phase),
                   TELEMETRY_KIND_ANALOG, electricity_meter->phase_voltage_x10[phase], TELEMETRY_VOLTAGE_DELTA_X10,
                   now_ms, &report);
      track_metric(DEVICE_SERVICE_ELECTRICITY_METER, electricity_meter->slave_addr, (uint8_t)(TM_EM_IA + phase),
                   TELEMETRY_KIND_ANALOG, electricity_meter->phase_current_x10[phase], TELEMETRY_CURRENT_DELTA_X10,
                   now_ms, &report);
    }
    if (report) {
      (void)enqueue_electricity_meter(now_ms, electricity_meter);
    }
  }

  if (ups != NULL && ups->slave_addr != 0U && ups->online) {
    report = false;
    for (uint8_t phase = 0U; phase < 3U; phase++) {
      track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, (uint8_t)(TM_UPS_INPUT_UA + phase), TELEMETRY_KIND_ANALOG,
                   ups->input_voltage_x10_v[phase], TELEMETRY_VOLTAGE_DELTA_X10, now_ms, &report);
      track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, (uint8_t)(TM_UPS_OUTPUT_UA + phase), TELEMETRY_KIND_ANALOG,
                   ups->output_voltage_x10_v[phase], TELEMETRY_VOLTAGE_DELTA_X10, now_ms, &report);
      track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, (uint8_t)(TM_UPS_LOAD_A + phase), TELEMETRY_KIND_ANALOG,
                   ups->load_rate_x10_percent[phase], TELEMETRY_LOAD_DELTA_X10, now_ms, &report);
    }
    track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, TM_UPS_BVP, TELEMETRY_KIND_ANALOG,
                 ups->battery_positive_voltage_x10_v, TELEMETRY_VOLTAGE_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, TM_UPS_BVN, TELEMETRY_KIND_ANALOG,
                 ups->battery_negative_voltage_x10_v, TELEMETRY_VOLTAGE_DELTA_X10, now_ms, &report);
    track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, TM_UPS_SOC, TELEMETRY_KIND_ANALOG,
                 ups->battery_state_of_charge_percent, TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, TM_UPS_RDT, TELEMETRY_KIND_ANALOG,
                 ups->battery_residual_discharge_minutes, TELEMETRY_UNIT_DELTA, now_ms, &report);
    track_metric(DEVICE_SERVICE_UPS, ups->slave_addr, TM_UPS_BTEMP, TELEMETRY_KIND_ANALOG,
                 ups->battery_temperature_x10_c, TELEMETRY_TEMP_DELTA_X10, now_ms, &report);
    if (report) {
      (void)enqueue_ups(now_ms, ups);
    }
  }
}

bool telemetry_reporter_flush_one(uint32_t now_ms) {
  if (queue_count == 0U || !upload_queue[queue_head].used || !mqtt_client_is_ready()) {
    return false;
  }
  if (last_flush_tick != 0U && (now_ms - last_flush_tick) < TELEMETRY_MIN_UPLOAD_INTERVAL_MS) {
    return false;
  }
  if (!mqtt_client_publish_data(upload_queue[queue_head].payload)) {
    return false;
  }
  memset(&upload_queue[queue_head], 0, sizeof(upload_queue[queue_head]));
  queue_head = (uint8_t)((queue_head + 1U) % TELEMETRY_QUEUE_CAPACITY);
  queue_count--;
  last_flush_tick = now_ms;
  return true;
}

uint8_t telemetry_reporter_queue_count(void) {
  return queue_count;
}

static telemetry_metric_cache_t *find_metric_cache(uint16_t service_id, uint8_t addr, uint8_t metric_id,
                                                   telemetry_metric_kind_t kind, int32_t threshold) {
  telemetry_metric_cache_t *free_slot = NULL;
  for (uint8_t index = 0U; index < TELEMETRY_METRIC_CAPACITY; index++) {
    telemetry_metric_cache_t *cache = &metric_caches[index];
    if (cache->used && cache->service_id == service_id && cache->addr == addr && cache->metric_id == metric_id) {
      return cache;
    }
    if (!cache->used && free_slot == NULL) {
      free_slot = cache;
    }
  }
  if (free_slot == NULL) {
    LOG_WARNING("Telemetry metric cache full, serviceId=%u addr=%u metric=%u", service_id, addr, metric_id);
    return NULL;
  }
  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->used = true;
  free_slot->service_id = service_id;
  free_slot->addr = addr;
  free_slot->metric_id = metric_id;
  free_slot->kind = kind;
  free_slot->threshold = threshold;
  return free_slot;
}

static void track_metric(uint16_t service_id, uint8_t addr, uint8_t metric_id, telemetry_metric_kind_t kind,
                         int32_t value, int32_t threshold, uint32_t now_ms, bool *report_device) {
  if (addr == 0U || report_device == NULL) {
    return;
  }
  telemetry_metric_cache_t *cache = find_metric_cache(service_id, addr, metric_id, kind, threshold);
  if (cache == NULL) {
    return;
  }

  cache->samples[cache->index] = value;
  cache->index = (uint8_t)((cache->index + 1U) % TELEMETRY_WINDOW_SAMPLE_COUNT);
  if (cache->count < TELEMETRY_WINDOW_SAMPLE_COUNT) {
    cache->count++;
  }
  if (cache->count < TELEMETRY_WINDOW_SAMPLE_COUNT) {
    return;
  }

  int32_t window_value = resolve_window_value(cache);
  cache->count = 0U;
  cache->index = 0U;
  if (!value_changed(cache, window_value)) {
    return;
  }
  if (cache->last_upload_tick != 0U && (now_ms - cache->last_upload_tick) < TELEMETRY_MIN_UPLOAD_INTERVAL_MS) {
    return;
  }

  cache->last_value = window_value;
  cache->last_value_valid = true;
  cache->last_upload_tick = now_ms;
  *report_device = true;
}

static int32_t resolve_window_value(const telemetry_metric_cache_t *cache) {
  if (cache == NULL || cache->count == 0U) {
    return 0;
  }
  /*
   * 模拟量按 10 组样本取中值，削掉偶发 Modbus 抖动；状态/位图不做数学中值，
   * 以窗口最后一次稳定读取为准，否则 8 路开关位图取中值会失去协议含义。
   */
  if (cache->kind == TELEMETRY_KIND_STATE) {
    uint8_t last_index = cache->index == 0U ? (TELEMETRY_WINDOW_SAMPLE_COUNT - 1U) : (uint8_t)(cache->index - 1U);
    return cache->samples[last_index];
  }
  return median_i32(cache->samples, cache->count);
}

static int32_t median_i32(const int32_t *values, uint8_t count) {
  int32_t sorted[TELEMETRY_WINDOW_SAMPLE_COUNT] = {0};
  if (values == NULL || count == 0U) {
    return 0;
  }
  if (count > TELEMETRY_WINDOW_SAMPLE_COUNT) {
    count = TELEMETRY_WINDOW_SAMPLE_COUNT;
  }
  memcpy(sorted, values, (size_t)count * sizeof(sorted[0]));
  for (uint8_t i = 1U; i < count; i++) {
    int32_t key = sorted[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  if ((count % 2U) == 0U) {
    return (sorted[(count / 2U) - 1U] + sorted[count / 2U]) / 2;
  }
  return sorted[count / 2U];
}

static bool value_changed(const telemetry_metric_cache_t *cache, int32_t value) {
  if (cache == NULL) {
    return false;
  }
  if (!cache->last_value_valid) {
    return true;
  }
  if (cache->kind == TELEMETRY_KIND_STATE) {
    return value != cache->last_value;
  }
  int32_t diff = value - cache->last_value;
  if (diff < 0) {
    diff = -diff;
  }
  return diff >= cache->threshold;
}

static bool enqueue_payload(JSON_Value *root_value) {
  if (root_value == NULL) {
    return false;
  }
  if (queue_count >= TELEMETRY_QUEUE_CAPACITY) {
    LOG_WARNING("Telemetry upload queue full, drop newest payload");
    return false;
  }
  uint8_t tail = (uint8_t)((queue_head + queue_count) % TELEMETRY_QUEUE_CAPACITY);
  size_t needed = json_serialization_size(root_value);
  if (needed == 0U || needed > TELEMETRY_PAYLOAD_MAX_LEN) {
    LOG_WARNING("Telemetry payload too large, need=%u buffer=%u", (unsigned int)needed, TELEMETRY_PAYLOAD_MAX_LEN);
    return false;
  }
  if (json_serialize_to_buffer(root_value, upload_queue[tail].payload, sizeof(upload_queue[tail].payload)) !=
      JSONSuccess) {
    return false;
  }
  upload_queue[tail].used = true;
  queue_count++;
  LOG_INFO("Telemetry queued, count=%u", queue_count);
  return true;
}

static bool enqueue_temp_humidity(uint32_t now_ms, const temp_humidity_sensor_t *sensor) {
  JSON_Value *root_value = NULL;
  JSON_Object *data = create_single_device_payload(&root_value, now_ms, DEVICE_SERVICE_TEMP_HUMIDITY, sensor->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  json_object_set_number(data, "at", sensor->temperature_x10_c);
  json_object_set_number(data, "ah", sensor->humidity_x10_rh);
  json_object_set_number(data, "tcv", sensor->temperature_cali_x10_c);
  json_object_set_number(data, "hcv", sensor->humidity_cali_x10_rh);
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static bool enqueue_discrete(uint32_t now_ms, const discrete_sensor_t *sensor) {
  JSON_Value *root_value = NULL;
  JSON_Object *data = create_single_device_payload(&root_value, now_ms, (uint16_t)sensor->type, sensor->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  switch (sensor->type) {
  case DISCRETE_SENSOR_SMOKE:
    json_object_set_number(data, "sa", sensor->alarm);
    json_object_set_number(data, "sad", sensor->alarm_delay);
    break;
  case DISCRETE_SENSOR_IMMERSION:
    json_object_set_number(data, "rwis", sensor->alarm);
    json_object_set_number(data, "wad", sensor->alarm_delay);
    json_object_set_number(data, "wcs", sensor->sensitivity);
    break;
  case DISCRETE_SENSOR_INFRARED:
    json_object_set_number(data, "pea", sensor->alarm);
    json_object_set_number(data, "pad", sensor->alarm_delay);
    break;
  case DISCRETE_SENSOR_POWER_OUTAGE:
    json_object_set_number(data, "poa", sensor->alarm);
    break;
  default:
    break;
  }
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static bool enqueue_smart_switch(uint32_t now_ms, const smart_switch_t *device) {
  JSON_Value *root_value = NULL;
  JSON_Object *data = create_single_device_payload(&root_value, now_ms, DEVICE_SERVICE_SMART_SWITCH, device->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  append_number_array_u8(data, "iss", device->input_bits);
  append_number_array_u8(data, "oss", device->output_bits);
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static bool enqueue_ac_controller(uint32_t now_ms, const ac_controller_t *device) {
  JSON_Value *root_value = NULL;
  JSON_Object *data =
    create_single_device_payload(&root_value, now_ms, DEVICE_SERVICE_AC_CONTROLLER, device->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  json_object_set_number(data, "chval", device->temperature_x10_c);
  json_object_set_number(data, "ctval", device->humidity_x10_rh);
  json_object_set_number(data, "crs", ac_controller_get_status(device));
  json_object_set_number(data, "current1", device->current_ch1);
  json_object_set_number(data, "current2", device->current_ch2);
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static bool enqueue_electricity_meter(uint32_t now_ms, const electricity_meter_t *meter) {
  JSON_Value *root_value = NULL;
  JSON_Object *data =
    create_single_device_payload(&root_value, now_ms, DEVICE_SERVICE_ELECTRICITY_METER, meter->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  json_object_set_number(data, "dio", meter->switch_state);
  json_object_set_number(data, "ua", meter->phase_voltage_x10[0]);
  json_object_set_number(data, "ub", meter->phase_voltage_x10[1]);
  json_object_set_number(data, "uc", meter->phase_voltage_x10[2]);
  json_object_set_number(data, "ia", meter->phase_current_x10[0]);
  json_object_set_number(data, "ib", meter->phase_current_x10[1]);
  json_object_set_number(data, "ic", meter->phase_current_x10[2]);
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static bool enqueue_ups(uint32_t now_ms, const ups_detector_t *ups) {
  JSON_Value *root_value = NULL;
  JSON_Object *data = create_single_device_payload(&root_value, now_ms, DEVICE_SERVICE_UPS, ups->slave_addr);
  if (data == NULL) {
    json_value_free(root_value);
    return false;
  }
  json_object_set_number(data, "mpva", ups->input_voltage_x10_v[0]);
  json_object_set_number(data, "mpvb", ups->input_voltage_x10_v[1]);
  json_object_set_number(data, "mpvc", ups->input_voltage_x10_v[2]);
  json_object_set_number(data, "ova", ups->output_voltage_x10_v[0]);
  json_object_set_number(data, "ovb", ups->output_voltage_x10_v[1]);
  json_object_set_number(data, "ovc", ups->output_voltage_x10_v[2]);
  json_object_set_number(data, "lra", ups->load_rate_x10_percent[0]);
  json_object_set_number(data, "lrb", ups->load_rate_x10_percent[1]);
  json_object_set_number(data, "lrc", ups->load_rate_x10_percent[2]);
  json_object_set_number(data, "bvp", ups->battery_positive_voltage_x10_v);
  json_object_set_number(data, "bvn", ups->battery_negative_voltage_x10_v);
  json_object_set_number(data, "soc", ups->battery_state_of_charge_percent);
  json_object_set_number(data, "rdt", ups->battery_residual_discharge_minutes);
  bool ok = enqueue_payload(root_value);
  json_value_free(root_value);
  return ok;
}

static JSON_Object *create_single_device_payload(JSON_Value **root_value, uint32_t now_ms, uint16_t service_id,
                                                 uint8_t addr) {
  if (root_value == NULL || addr == 0U) {
    return NULL;
  }
  *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(*root_value);
  JSON_Value *devices_value = json_value_init_array();
  JSON_Array *devices = json_value_get_array(devices_value);
  JSON_Value *device_value = json_value_init_object();
  JSON_Object *device = json_value_get_object(device_value);
  JSON_Value *data_value = json_value_init_object();
  JSON_Object *data = json_value_get_object(data_value);
  if (root == NULL || devices == NULL || device == NULL || data == NULL) {
    return NULL;
  }

  json_object_set_string(root, "type", "datas");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  json_object_set_number(root, "eventTime", now_ms / 1000UL);
  json_object_set_value(root, "devices", devices_value);
  json_object_set_number(device, "serialPort", addr);
  json_object_set_number(device, "serviceId", service_id);
  append_bool(device, "online", true);
  json_object_set_value(device, "data", data_value);
  json_array_append_value(devices, device_value);
  return data;
}

static void append_bool(JSON_Object *object, const char *name, bool value) {
  (void)json_object_set_boolean(object, name, value ? 1 : 0);
}

static void append_number_array_u8(JSON_Object *object, const char *name, uint16_t bits) {
  JSON_Value *array_value = json_value_init_array();
  JSON_Array *array = json_value_get_array(array_value);
  if (array == NULL) {
    json_value_free(array_value);
    return;
  }
  for (uint8_t channel = 0U; channel < SMART_SWITCH_MAX_CHANNELS; channel++) {
    json_array_append_number(array, (bits & (1U << channel)) != 0U ? 1 : 0);
  }
  json_object_set_value(object, name, array_value);
}
