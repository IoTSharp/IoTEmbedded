#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include "app_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_MSG_BASE_ADDR 0x0000U

ErrorStatus eeprom_write_config_data(const void *data, size_t data_size);
ErrorStatus eeprom_read_config_data(void *data, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif
