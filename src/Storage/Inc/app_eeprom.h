#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include "Common/Inc/app_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_MSG_BASE_ADDR 0x0000U
#define EEPROM_CONFIG_BASE_ADDR STORE_MSG_BASE_ADDR
#define EEPROM_BASIC_SCRIPT_SLOT_COUNT 2U
#define EEPROM_BASIC_SCRIPT_NAME_SIZE 16U
#define EEPROM_BASIC_SCRIPT_MAX_SIZE 4064U

typedef enum {
  EEPROM_BASIC_SCRIPT_SLOT_APP01 = 0,
  EEPROM_BASIC_SCRIPT_SLOT_APP02 = 1,
} eeprom_basic_script_slot_t;

typedef struct {
  char name[EEPROM_BASIC_SCRIPT_NAME_SIZE];
  uint32_t data_size;
  uint16_t crc;
  bool valid;
} eeprom_basic_script_info_t;

ErrorStatus eeprom_write_config_data(const void *data, size_t data_size);
ErrorStatus eeprom_read_config_data(void *data, size_t data_size);
ErrorStatus eeprom_write_basic_script(eeprom_basic_script_slot_t slot, const char *name, const char *script,
                                      size_t script_size);
ErrorStatus eeprom_read_basic_script(eeprom_basic_script_slot_t slot, char *script, size_t script_size,
                                     size_t *actual_size);
ErrorStatus eeprom_get_basic_script_info(eeprom_basic_script_slot_t slot, eeprom_basic_script_info_t *info);
bool eeprom_basic_script_slot_from_package_name(const char *name, eeprom_basic_script_slot_t *slot);
const char *eeprom_basic_script_slot_name(eeprom_basic_script_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif
