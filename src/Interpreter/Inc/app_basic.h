#ifndef APP_BASIC_H
#define APP_BASIC_H

#include "Common/Inc/app_types.h"
#include "Storage/Inc/app_eeprom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  APP_BASIC_SLOT_PRIMARY = EEPROM_BASIC_SCRIPT_SLOT_APP01,
  APP_BASIC_SLOT_BACKUP = EEPROM_BASIC_SCRIPT_SLOT_APP02,
} app_basic_slot_t;

typedef enum {
  APP_BASIC_PRINT_TARGET_DEFAULT = 0,
  APP_BASIC_PRINT_TARGET_DISPLAY,
} app_basic_print_target_t;

typedef struct {
  app_basic_slot_t loaded_slot;
  char loaded_name[EEPROM_BASIC_SCRIPT_NAME_SIZE];
  size_t loaded_size;
} app_basic_status_t;

typedef uint32_t app_basic_capability_flags_t;
typedef uint32_t app_basic_target_flags_t;

typedef enum {
  APP_BASIC_CAPABILITY_RUNTIME = 1UL << 0,
  APP_BASIC_CAPABILITY_SERIAL = 1UL << 1,
  APP_BASIC_CAPABILITY_RS485 = 1UL << 2,
  APP_BASIC_CAPABILITY_RS232 = 1UL << 3,
  APP_BASIC_CAPABILITY_MODBUS_RTU = 1UL << 4,
  APP_BASIC_CAPABILITY_FORMAT = 1UL << 5,
  APP_BASIC_CAPABILITY_JSON = 1UL << 6,
  APP_BASIC_CAPABILITY_CONFIG = 1UL << 7,
  APP_BASIC_CAPABILITY_STORAGE = 1UL << 8,
  APP_BASIC_CAPABILITY_NETWORK = 1UL << 9,
  APP_BASIC_CAPABILITY_NETWORK_CH395Q = 1UL << 10,
  APP_BASIC_CAPABILITY_NETWORK_AIR724UG = 1UL << 11,
  APP_BASIC_CAPABILITY_NETWORK_AP6181 = 1UL << 12,
  APP_BASIC_CAPABILITY_MQTT = 1UL << 13,
  APP_BASIC_CAPABILITY_TIME = 1UL << 14,
  APP_BASIC_CAPABILITY_DISPLAY = 1UL << 15,
} app_basic_capability_t;

typedef enum {
  APP_BASIC_TARGET_STM32 = 1UL << 0,
  APP_BASIC_TARGET_RTOS = 1UL << 1,
  APP_BASIC_TARGET_BARE_METAL = 1UL << 2,
  APP_BASIC_TARGET_LOW_RESOURCE_LINUX = 1UL << 3,
} app_basic_target_t;

typedef struct {
  const char *name;
  app_basic_target_flags_t targets;
  app_basic_capability_flags_t capabilities;
} app_basic_profile_t;

typedef struct {
  const char *module;
  const char *name;
  const char *parameters;
  const char *returns;
  const char *error_model;
  const char *timeout_model;
  const char *handle_lifecycle;
  const char *memory_owner;
  app_basic_capability_flags_t dependencies;
  app_basic_target_flags_t targets;
} app_basic_function_descriptor_t;

void app_basic_init(void);
ErrorStatus app_basic_load(app_basic_slot_t preferred_slot);
ErrorStatus app_basic_run_once(void);
ErrorStatus app_basic_reload_and_run(app_basic_slot_t preferred_slot);
app_basic_status_t app_basic_get_status(void);
void app_basic_set_print_target(app_basic_print_target_t target);
app_basic_print_target_t app_basic_get_print_target(void);
const app_basic_profile_t *app_basic_get_profile(void);
const app_basic_function_descriptor_t *app_basic_get_function_registry(size_t *count);
bool app_basic_function_is_available(const app_basic_function_descriptor_t *function);
bool app_basic_capabilities_are_available(app_basic_capability_flags_t dependencies);
bool app_basic_targets_are_available(app_basic_target_flags_t targets);

#ifdef __cplusplus
}
#endif

#endif
