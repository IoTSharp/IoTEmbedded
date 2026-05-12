#include "Storage/Inc/app_eeprom.h"

#include "Storage/Inc/bsp_eeprom.h"
#include "Protocol/Modbus/Inc/modbus_core_crc.h"

#include <string.h>

#define EEPROM_TIMEOUT_MS 1000U
#define EEPROM_READY_TRIALS 100U
#define EEPROM_PAGE_SIZE 128U
#define EEPROM_BASIC_SCRIPT_MAGIC 0x42415331UL
#define EEPROM_BASIC_SCRIPT_VERSION 1U
#define EEPROM_BASIC_SCRIPT_HEADER_SIZE 32U
#define EEPROM_BASIC_SCRIPT_SLOT_SIZE 4096U
#define EEPROM_BASIC_SCRIPT_BASE_ADDR 0x2000U
#define EEPROM_BASIC_SCRIPT_TOTAL_SIZE (EEPROM_BASIC_SCRIPT_SLOT_COUNT * EEPROM_BASIC_SCRIPT_SLOT_SIZE)

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  char name[EEPROM_BASIC_SCRIPT_NAME_SIZE];
  uint32_t data_size;
  uint16_t crc;
  uint16_t reserved;
} eeprom_basic_script_header_t;

static ErrorStatus eeprom_read_bytes(uint16_t memory_address, void *data, size_t data_size);
static ErrorStatus eeprom_write_bytes(uint16_t memory_address, const void *data, size_t data_size);
static bool eeprom_basic_script_slot_is_valid(eeprom_basic_script_slot_t slot);
static uint16_t eeprom_basic_script_slot_base(eeprom_basic_script_slot_t slot);
static ErrorStatus eeprom_read_basic_script_header(eeprom_basic_script_slot_t slot, eeprom_basic_script_header_t *header);
static void eeprom_fill_basic_script_header(eeprom_basic_script_header_t *header, const char *name, size_t script_size,
                                            uint16_t crc);
static const char *eeprom_basic_script_leaf_name(const char *name);
static bool eeprom_basic_script_name_equals(const char *left, const char *right);

typedef char eeprom_basic_script_header_size_check[(sizeof(eeprom_basic_script_header_t) == EEPROM_BASIC_SCRIPT_HEADER_SIZE)
                                                    ? 1
                                                    : -1];
typedef char eeprom_basic_script_payload_size_check
  [(EEPROM_BASIC_SCRIPT_MAX_SIZE == (EEPROM_BASIC_SCRIPT_SLOT_SIZE - EEPROM_BASIC_SCRIPT_HEADER_SIZE)) ? 1 : -1];
typedef char eeprom_basic_script_address_size_check
  [((EEPROM_BASIC_SCRIPT_BASE_ADDR + EEPROM_BASIC_SCRIPT_TOTAL_SIZE) <= 0x10000UL) ? 1 : -1];

ErrorStatus eeprom_write_config_data(const void *data, size_t data_size) {
  return eeprom_write_bytes(EEPROM_CONFIG_BASE_ADDR, data, data_size);
}

ErrorStatus eeprom_read_config_data(void *data, size_t data_size) {
  return eeprom_read_bytes(EEPROM_CONFIG_BASE_ADDR, data, data_size);
}

ErrorStatus eeprom_write_basic_script(eeprom_basic_script_slot_t slot, const char *name, const char *script,
                                      size_t script_size) {
  if (!eeprom_basic_script_slot_is_valid(slot) || name == NULL || script == NULL || script_size == 0U ||
      script_size > EEPROM_BASIC_SCRIPT_MAX_SIZE) {
    return ERROR;
  }

  uint16_t base = eeprom_basic_script_slot_base(slot);
  uint16_t crc = GetCRCData((uint8_t *)script, (uint16_t)script_size);
  eeprom_basic_script_header_t header;
  eeprom_fill_basic_script_header(&header, name, script_size, crc);

  if (eeprom_write_bytes((uint16_t)(base + EEPROM_BASIC_SCRIPT_HEADER_SIZE), script, script_size) != SUCCESS) {
    return ERROR;
  }
  return eeprom_write_bytes(base, &header, sizeof(header));
}

ErrorStatus eeprom_read_basic_script(eeprom_basic_script_slot_t slot, char *script, size_t script_size,
                                     size_t *actual_size) {
  if (!eeprom_basic_script_slot_is_valid(slot) || script == NULL || script_size == 0U) {
    return ERROR;
  }
  if (actual_size != NULL) {
    *actual_size = 0U;
  }

  eeprom_basic_script_header_t header;
  if (eeprom_read_basic_script_header(slot, &header) != SUCCESS) {
    return ERROR;
  }
  if (header.magic != EEPROM_BASIC_SCRIPT_MAGIC || header.version != EEPROM_BASIC_SCRIPT_VERSION ||
      header.header_size != EEPROM_BASIC_SCRIPT_HEADER_SIZE || header.data_size == 0U ||
      header.data_size > EEPROM_BASIC_SCRIPT_MAX_SIZE || header.data_size >= script_size) {
    return ERROR;
  }

  uint16_t base = eeprom_basic_script_slot_base(slot);
  if (eeprom_read_bytes((uint16_t)(base + EEPROM_BASIC_SCRIPT_HEADER_SIZE), script, header.data_size) != SUCCESS) {
    return ERROR;
  }
  if (GetCRCData((uint8_t *)script, (uint16_t)header.data_size) != header.crc) {
    return ERROR;
  }

  script[header.data_size] = '\0';
  if (actual_size != NULL) {
    *actual_size = header.data_size;
  }
  return SUCCESS;
}

ErrorStatus eeprom_get_basic_script_info(eeprom_basic_script_slot_t slot, eeprom_basic_script_info_t *info) {
  if (!eeprom_basic_script_slot_is_valid(slot) || info == NULL) {
    return ERROR;
  }

  memset(info, 0, sizeof(*info));
  eeprom_basic_script_header_t header;
  if (eeprom_read_basic_script_header(slot, &header) != SUCCESS) {
    return ERROR;
  }

  memcpy(info->name, header.name, sizeof(info->name));
  info->name[EEPROM_BASIC_SCRIPT_NAME_SIZE - 1U] = '\0';
  info->data_size = header.data_size;
  info->crc = header.crc;
  info->valid = header.magic == EEPROM_BASIC_SCRIPT_MAGIC && header.version == EEPROM_BASIC_SCRIPT_VERSION &&
                header.header_size == EEPROM_BASIC_SCRIPT_HEADER_SIZE && header.data_size > 0U &&
                header.data_size <= EEPROM_BASIC_SCRIPT_MAX_SIZE;

  return SUCCESS;
}

bool eeprom_basic_script_slot_from_package_name(const char *name, eeprom_basic_script_slot_t *slot) {
  if (name == NULL || slot == NULL) {
    return false;
  }
  name = eeprom_basic_script_leaf_name(name);

  for (uint8_t i = 0U; i < EEPROM_BASIC_SCRIPT_SLOT_COUNT; i++) {
    eeprom_basic_script_slot_t current_slot = (eeprom_basic_script_slot_t)i;
    eeprom_basic_script_info_t info;
    if (eeprom_get_basic_script_info(current_slot, &info) != SUCCESS || !info.valid) {
      continue;
    }
    if (eeprom_basic_script_name_equals(name, info.name)) {
      *slot = current_slot;
      return true;
    }
  }

  /* Compatibility only: legacy scripts may still import physical slot names. */
  if (eeprom_basic_script_name_equals(name, "app01.bas") || eeprom_basic_script_name_equals(name, "app01")) {
    *slot = EEPROM_BASIC_SCRIPT_SLOT_APP01;
    return true;
  }
  if (eeprom_basic_script_name_equals(name, "app02.bas") || eeprom_basic_script_name_equals(name, "app02")) {
    *slot = EEPROM_BASIC_SCRIPT_SLOT_APP02;
    return true;
  }
  return false;
}

const char *eeprom_basic_script_slot_name(eeprom_basic_script_slot_t slot) {
  switch (slot) {
  case EEPROM_BASIC_SCRIPT_SLOT_APP01:
    return "app01.bas";
  case EEPROM_BASIC_SCRIPT_SLOT_APP02:
    return "app02.bas";
  default:
    return "";
  }
}

static ErrorStatus eeprom_read_bytes(uint16_t memory_address, void *data, size_t data_size) {
  if (data == NULL || data_size == 0U || data_size > UINT16_MAX) {
    return ERROR;
  }
  if ((uint32_t)memory_address + data_size > 0x10000UL) {
    return ERROR;
  }

  if (bsp_eeprom_read(memory_address, (uint8_t *)data, (uint16_t)data_size, EEPROM_TIMEOUT_MS) != HAL_OK) {
    return ERROR;
  }

  return SUCCESS;
}

static ErrorStatus eeprom_write_bytes(uint16_t memory_address, const void *data, size_t data_size) {
  if (data == NULL || data_size == 0U || data_size > UINT16_MAX) {
    return ERROR;
  }
  if ((uint32_t)memory_address + data_size > 0x10000UL) {
    return ERROR;
  }

  const uint8_t *bytes = (const uint8_t *)data;
  uint16_t written = 0U;
  while (written < data_size) {
    uint16_t current_address = (uint16_t)(memory_address + written);
    uint16_t page_offset = (uint16_t)(current_address % EEPROM_PAGE_SIZE);
    uint16_t page_remaining = (uint16_t)(EEPROM_PAGE_SIZE - page_offset);
    uint16_t remaining = (uint16_t)(data_size - written);
    uint16_t chunk = remaining < page_remaining ? remaining : page_remaining;

    if (bsp_eeprom_write(current_address, bytes + written, chunk, EEPROM_TIMEOUT_MS) != HAL_OK) {
      return ERROR;
    }

    if (bsp_eeprom_is_ready(EEPROM_READY_TRIALS, EEPROM_TIMEOUT_MS) != HAL_OK) {
      return ERROR;
    }

    written = (uint16_t)(written + chunk);
  }

  return SUCCESS;
}

static bool eeprom_basic_script_slot_is_valid(eeprom_basic_script_slot_t slot) {
  return slot == EEPROM_BASIC_SCRIPT_SLOT_APP01 || slot == EEPROM_BASIC_SCRIPT_SLOT_APP02;
}

static uint16_t eeprom_basic_script_slot_base(eeprom_basic_script_slot_t slot) {
  return (uint16_t)(EEPROM_BASIC_SCRIPT_BASE_ADDR + ((uint16_t)slot * EEPROM_BASIC_SCRIPT_SLOT_SIZE));
}

static ErrorStatus eeprom_read_basic_script_header(eeprom_basic_script_slot_t slot, eeprom_basic_script_header_t *header) {
  if (header == NULL) {
    return ERROR;
  }
  return eeprom_read_bytes(eeprom_basic_script_slot_base(slot), header, sizeof(*header));
}

static void eeprom_fill_basic_script_header(eeprom_basic_script_header_t *header, const char *name, size_t script_size,
                                            uint16_t crc) {
  const char *leaf_name = eeprom_basic_script_leaf_name(name);
  memset(header, 0, sizeof(*header));
  header->magic = EEPROM_BASIC_SCRIPT_MAGIC;
  header->version = EEPROM_BASIC_SCRIPT_VERSION;
  header->header_size = EEPROM_BASIC_SCRIPT_HEADER_SIZE;
  (void)strncpy(header->name, leaf_name, sizeof(header->name) - 1U);
  header->data_size = (uint32_t)script_size;
  header->crc = crc;
}

static const char *eeprom_basic_script_leaf_name(const char *name) {
  const char *leaf = name;
  while (*name != '\0') {
    if (*name == '/' || *name == '\\') {
      leaf = name + 1;
    }
    name++;
  }
  return leaf;
}

static bool eeprom_basic_script_name_equals(const char *left, const char *right) {
  while (*left != '\0' && *right != '\0') {
    char l = *left;
    char r = *right;
    if (l >= 'A' && l <= 'Z') {
      l = (char)(l - 'A' + 'a');
    }
    if (r >= 'A' && r <= 'Z') {
      r = (char)(r - 'A' + 'a');
    }
    if (l != r) {
      return false;
    }
    left++;
    right++;
  }
  return *left == '\0' && *right == '\0';
}
