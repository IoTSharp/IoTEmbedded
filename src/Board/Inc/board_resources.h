#ifndef BOARD_RESOURCES_H
#define BOARD_RESOURCES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BOARD_RESOURCE_NETWORK,
  BOARD_RESOURCE_STORAGE,
  BOARD_RESOURCE_DISPLAY,
  BOARD_RESOURCE_SENSOR,
  BOARD_RESOURCE_AUDIO,
  BOARD_RESOURCE_ACTUATOR,
  BOARD_RESOURCE_USER_IO,
  BOARD_RESOURCE_EXPANSION,
  BOARD_RESOURCE_DEBUG,
} board_resource_category_t;

typedef enum {
  BOARD_RESOURCE_STATUS_PLANNED,
  BOARD_RESOURCE_STATUS_IOC_MAPPED,
  BOARD_RESOURCE_STATUS_DRIVER_PENDING,
  BOARD_RESOURCE_STATUS_READY,
  BOARD_RESOURCE_STATUS_CONFLICT_NEEDS_REVIEW,
} board_resource_status_t;

typedef enum {
  BOARD_RESOURCE_SCOPE_COMMON,
  BOARD_RESOURCE_SCOPE_BOARD_SPECIFIC,
} board_resource_scope_t;

typedef struct {
  const char *name;
  board_resource_category_t category;
  board_resource_scope_t scope;
  board_resource_status_t status;
  const char *bus;
  const char *pins;
  const char *protocol;
  const char *notes;
} board_resource_t;

const char *board_resource_category_name(board_resource_category_t category);
const char *board_resource_status_name(board_resource_status_t status);
const char *board_resource_scope_name(board_resource_scope_t scope);

#ifdef __cplusplus
}
#endif

#endif
