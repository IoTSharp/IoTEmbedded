#ifndef APP_BASIC_H
#define APP_BASIC_H

#include "Common/Inc/app_types.h"
#include "Storage/Inc/app_eeprom.h"

#include <stddef.h>

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

void app_basic_init(void);
ErrorStatus app_basic_load(app_basic_slot_t preferred_slot);
ErrorStatus app_basic_run_once(void);
ErrorStatus app_basic_reload_and_run(app_basic_slot_t preferred_slot);
app_basic_status_t app_basic_get_status(void);
void app_basic_set_print_target(app_basic_print_target_t target);
app_basic_print_target_t app_basic_get_print_target(void);

#ifdef __cplusplus
}
#endif

#endif
