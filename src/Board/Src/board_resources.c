#include "Board/Inc/board_resources.h"

const char *board_resource_category_name(board_resource_category_t category) {
  switch (category) {
  case BOARD_RESOURCE_NETWORK:
    return "network";
  case BOARD_RESOURCE_STORAGE:
    return "storage";
  case BOARD_RESOURCE_DISPLAY:
    return "display";
  case BOARD_RESOURCE_SENSOR:
    return "sensor";
  case BOARD_RESOURCE_AUDIO:
    return "audio";
  case BOARD_RESOURCE_ACTUATOR:
    return "actuator";
  case BOARD_RESOURCE_USER_IO:
    return "user-io";
  case BOARD_RESOURCE_EXPANSION:
    return "expansion";
  case BOARD_RESOURCE_DEBUG:
    return "debug";
  default:
    return "unknown";
  }
}

const char *board_resource_status_name(board_resource_status_t status) {
  switch (status) {
  case BOARD_RESOURCE_STATUS_PLANNED:
    return "planned";
  case BOARD_RESOURCE_STATUS_IOC_MAPPED:
    return "ioc-mapped";
  case BOARD_RESOURCE_STATUS_DRIVER_PENDING:
    return "driver-pending";
  case BOARD_RESOURCE_STATUS_READY:
    return "ready";
  case BOARD_RESOURCE_STATUS_CONFLICT_NEEDS_REVIEW:
    return "conflict-needs-review";
  default:
    return "unknown";
  }
}

const char *board_resource_scope_name(board_resource_scope_t scope) {
  switch (scope) {
  case BOARD_RESOURCE_SCOPE_COMMON:
    return "common";
  case BOARD_RESOURCE_SCOPE_BOARD_SPECIFIC:
    return "board-specific";
  default:
    return "unknown";
  }
}
