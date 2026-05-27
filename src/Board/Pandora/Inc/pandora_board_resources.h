#ifndef PANDORA_BOARD_RESOURCES_H
#define PANDORA_BOARD_RESOURCES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PANDORA_RESOURCE_NETWORK,
  PANDORA_RESOURCE_STORAGE,
  PANDORA_RESOURCE_DISPLAY,
  PANDORA_RESOURCE_SENSOR,
  PANDORA_RESOURCE_AUDIO,
  PANDORA_RESOURCE_ACTUATOR,
  PANDORA_RESOURCE_USER_IO,
  PANDORA_RESOURCE_EXPANSION,
  PANDORA_RESOURCE_DEBUG,
} pandora_resource_category_t;

typedef enum {
  PANDORA_RESOURCE_STATUS_PLANNED,
  PANDORA_RESOURCE_STATUS_IOC_MAPPED,
  PANDORA_RESOURCE_STATUS_DRIVER_PENDING,
  PANDORA_RESOURCE_STATUS_READY,
  PANDORA_RESOURCE_STATUS_CONFLICT_NEEDS_REVIEW,
} pandora_resource_status_t;

typedef enum {
  PANDORA_RESOURCE_SCOPE_COMMON,
  PANDORA_RESOURCE_SCOPE_BOARD_SPECIFIC,
} pandora_resource_scope_t;

typedef struct {
  const char *name;
  pandora_resource_category_t category;
  pandora_resource_scope_t scope;
  pandora_resource_status_t status;
  const char *bus;
  const char *pins;
  const char *protocol;
  const char *notes;
} pandora_resource_t;

const pandora_resource_t *pandora_board_resources(size_t *count);
const char *pandora_resource_category_name(pandora_resource_category_t category);
const char *pandora_resource_status_name(pandora_resource_status_t status);
const char *pandora_resource_scope_name(pandora_resource_scope_t scope);

#ifdef __cplusplus
}
#endif

#endif
