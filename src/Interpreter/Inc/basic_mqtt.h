#ifndef BASIC_MQTT_H
#define BASIC_MQTT_H

#include "Common/Inc/app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mb_interpreter_t;

ErrorStatus basic_mqtt_register(struct mb_interpreter_t *interpreter);

#ifdef __cplusplus
}
#endif

#endif
