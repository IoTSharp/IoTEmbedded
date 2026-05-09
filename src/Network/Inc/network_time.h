#ifndef NETWORK_TIME_H
#define NETWORK_TIME_H

#include "Config/Inc/network_config.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool valid;
  uint32_t unix_time;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} network_time_status_t;

bool network_time_sync_ntp(const ntp_config_t *config);
network_time_status_t network_time_get_status(void);

#ifdef __cplusplus
}
#endif

#endif
