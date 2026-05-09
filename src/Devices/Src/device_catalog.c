#include "Devices/Inc/device_catalog.h"

#include <stddef.h>

// 当前目录只保留型号、地址和名称参考，不再作为开机默认设备表。
// 真正参与轮询的设备由 EEPROM 中保存的现场设备表决定；未勾选设备不会被自动注册。
static const device_catalog_entry_t catalog[] = {
  {DEVICE_SERVICE_TEMP_HUMIDITY, 1U, 9U, false, "temp_humi"},
  {DEVICE_SERVICE_SMOKE, 0U, 2U, false, "smoke"},
  {DEVICE_SERVICE_IMMERSION, 0U, 3U, false, "immersion"},
  {DEVICE_SERVICE_INFRARED, 0U, 4U, false, "infrared"},
  {DEVICE_SERVICE_SMART_SWITCH, 0U, 0U, false, "smart_switch"},
  {DEVICE_SERVICE_AC_CONTROLLER, 6U, 6U, false, "ac"},
  {DEVICE_SERVICE_ELECTRICITY_METER, 0U, 7U, false, "electricity_meter"},
  {DEVICE_SERVICE_UPS, 0U, 1U, false, "ups"},
  {DEVICE_SERVICE_POWER_OUTAGE, 0U, 0U, false, "power_outage"},
};

const device_catalog_entry_t *device_catalog_entries(uint16_t *count) {
  if (count != NULL) {
    *count = (uint16_t)(sizeof(catalog) / sizeof(catalog[0]));
  }
  return catalog;
}

const device_catalog_entry_t *device_catalog_find(uint16_t service_id) {
  for (uint16_t index = 0U; index < (uint16_t)(sizeof(catalog) / sizeof(catalog[0])); index++) {
    if (catalog[index].service_id == service_id) {
      return &catalog[index];
    }
  }
  return NULL;
}

const char *device_catalog_name(uint16_t service_id) {
  const device_catalog_entry_t *entry = device_catalog_find(service_id);
  return entry == NULL ? "unknown" : entry->name;
}

uint16_t device_catalog_default_model(uint16_t service_id) {
  const device_catalog_entry_t *entry = device_catalog_find(service_id);
  return entry == NULL ? 0U : entry->default_model;
}

uint8_t device_catalog_default_addr(uint16_t service_id) {
  const device_catalog_entry_t *entry = device_catalog_find(service_id);
  return entry == NULL ? 0U : entry->default_addr;
}

bool device_catalog_default_enabled(uint16_t service_id) {
  const device_catalog_entry_t *entry = device_catalog_find(service_id);
  return entry != NULL && entry->enable_by_default;
}
