#ifndef POWER_ENV_DAQ_BOARD_RESOURCES_H
#define POWER_ENV_DAQ_BOARD_RESOURCES_H

#include "Board/Inc/board_resources.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const board_resource_t *power_env_daq_board_resources(size_t *count);

#ifdef __cplusplus
}
#endif

#endif
