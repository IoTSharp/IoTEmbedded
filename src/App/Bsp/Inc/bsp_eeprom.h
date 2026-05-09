#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef bsp_eeprom_read(uint16_t memory_address, uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_eeprom_write(uint16_t memory_address, const uint8_t *data, uint16_t length, uint32_t timeout_ms);
HAL_StatusTypeDef bsp_eeprom_is_ready(uint32_t trials, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
