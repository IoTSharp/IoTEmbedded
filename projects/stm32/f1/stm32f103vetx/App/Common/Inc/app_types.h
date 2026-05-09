#ifndef APP_TYPES_H
#define APP_TYPES_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#ifndef __IO
#define __IO volatile
#endif

#ifdef __cplusplus
}
#endif

#endif
