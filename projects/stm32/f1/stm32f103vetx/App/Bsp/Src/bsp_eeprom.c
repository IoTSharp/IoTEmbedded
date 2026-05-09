#include "bsp_eeprom.h"

#define BSP_AT24C_HAL_ADDRESS (BSP_AT24C_7BIT_ADDRESS << 1U)

HAL_StatusTypeDef bsp_eeprom_read(uint16_t memory_address, uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  if (data == NULL || length == 0U) {
    return HAL_ERROR;
  }
  return HAL_I2C_Mem_Read(BSP_AT24C_I2C_HANDLE, BSP_AT24C_HAL_ADDRESS, memory_address, I2C_MEMADD_SIZE_16BIT, data,
                          length, timeout_ms);
}

HAL_StatusTypeDef bsp_eeprom_write(uint16_t memory_address, const uint8_t *data, uint16_t length, uint32_t timeout_ms) {
  if (data == NULL || length == 0U) {
    return HAL_ERROR;
  }
  return HAL_I2C_Mem_Write(BSP_AT24C_I2C_HANDLE, BSP_AT24C_HAL_ADDRESS, memory_address, I2C_MEMADD_SIZE_16BIT,
                           (uint8_t *)data, length, timeout_ms);
}

HAL_StatusTypeDef bsp_eeprom_is_ready(uint32_t trials, uint32_t timeout_ms) {
  return HAL_I2C_IsDeviceReady(BSP_AT24C_I2C_HANDLE, BSP_AT24C_HAL_ADDRESS, trials, timeout_ms);
}
