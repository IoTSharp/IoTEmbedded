#include "app_eeprom.h"

#include "bsp_eeprom.h"

#define EEPROM_TIMEOUT_MS 1000U
#define EEPROM_READY_TRIALS 100U
#define EEPROM_PAGE_SIZE 128U

ErrorStatus eeprom_write_config_data(const void *data, size_t data_size) {
  if (data == NULL || data_size == 0U || data_size > UINT16_MAX) {
    return ERROR;
  }

  const uint8_t *bytes = (const uint8_t *)data;
  uint16_t written = 0U;
  while (written < data_size) {
    uint16_t memory_address = (uint16_t)(STORE_MSG_BASE_ADDR + written);
    uint16_t page_offset = (uint16_t)(memory_address % EEPROM_PAGE_SIZE);
    uint16_t page_remaining = (uint16_t)(EEPROM_PAGE_SIZE - page_offset);
    uint16_t remaining = (uint16_t)(data_size - written);
    uint16_t chunk = remaining < page_remaining ? remaining : page_remaining;

    if (bsp_eeprom_write(memory_address, bytes + written, chunk, EEPROM_TIMEOUT_MS) != HAL_OK) {
      return ERROR;
    }

    if (bsp_eeprom_is_ready(EEPROM_READY_TRIALS, EEPROM_TIMEOUT_MS) != HAL_OK) {
      return ERROR;
    }

    written = (uint16_t)(written + chunk);
  }

  return SUCCESS;
}

ErrorStatus eeprom_read_config_data(void *data, size_t data_size) {
  if (data == NULL || data_size == 0U || data_size > UINT16_MAX) {
    return ERROR;
  }

  if (bsp_eeprom_read(STORE_MSG_BASE_ADDR, (uint8_t *)data, (uint16_t)data_size, EEPROM_TIMEOUT_MS) != HAL_OK) {
    return ERROR;
  }

  return SUCCESS;
}
