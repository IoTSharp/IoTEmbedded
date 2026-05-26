#ifndef BSP_HAL_H
#define BSP_HAL_H

#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
#include "stm32l4xx_hal.h"
#elif defined(STM32F103xE) || defined(STM32F103VETx) || defined(STM32F103VETX) || defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "Unsupported STM32 HAL family. Define an STM32F1 or STM32L4 device macro in the BSP."
#endif

#endif
