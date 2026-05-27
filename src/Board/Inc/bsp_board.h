#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "Board/Inc/bsp_hal.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
#if !defined(STM32L475xx) && !defined(STM32L475VE) && !defined(STM32L4)
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
#endif
#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
extern I2C_HandleTypeDef hi2c3;
#else
extern I2C_HandleTypeDef hi2c1;
#endif
extern IWDG_HandleTypeDef hiwdg;
extern RTC_HandleTypeDef hrtc;

#define BSP_UART1_HANDLE      (&huart1)
#define BSP_UART2_HANDLE      (&huart2)

#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
#define BSP_BOARD_NAME         "ALIENTEK Pandora STM32L475"
#define BSP_MCU_NAME           "STM32L475VET6"
#define BSP_DEBUG_UART_NAME    "USART1/ST-LINK"
#define BSP_ENABLE_FULL_IO_MAP 0
#define BSP_HAS_UART4          0
#define BSP_HAS_UART5          0
#define BSP_HAS_RS232          0
#define BSP_HAS_CH395Q         0
#define BSP_HAS_AIR724UG       0
#define BSP_HAS_AT24C          0
#define BSP_HAS_AP6181         1
#define BSP_DEBUG_UART_HANDLE  BSP_UART1_HANDLE
#define BSP_RS485_UART_HANDLE  BSP_UART2_HANDLE
#define BSP_RS232_UART_HANDLE  NULL
#define BSP_BUZZER_GPIO_Port   GPIOB
#define BSP_BUZZER_Pin         GPIO_PIN_2
#else
#define BSP_BOARD_NAME         "STM32F103VETX"
#define BSP_MCU_NAME           "STM32F103VET6"
#define BSP_DEBUG_UART_NAME    "USART2"
#define BSP_ENABLE_FULL_IO_MAP 1
#define BSP_HAS_UART4          1
#define BSP_HAS_UART5          1
#define BSP_HAS_RS232          1
#define BSP_HAS_CH395Q         1
#define BSP_HAS_AIR724UG       1
#define BSP_HAS_AT24C          1
#define BSP_HAS_AP6181         0
#define BSP_UART4_HANDLE       (&huart4)
#define BSP_UART5_HANDLE       (&huart5)
#define BSP_DEBUG_UART_HANDLE  BSP_UART2_HANDLE
#define BSP_RS485_UART_HANDLE  BSP_UART1_HANDLE
#define BSP_RS232_UART_HANDLE  BSP_UART5_HANDLE
#define BSP_BUZZER_GPIO_Port   GPIOE
#define BSP_BUZZER_Pin         GPIO_PIN_7
#endif

#if BSP_HAS_AT24C
#define BSP_AT24C_I2C_HANDLE  (&hi2c1)
#else
#define BSP_AT24C_I2C_HANDLE  NULL
#endif
#if defined(STM32L475xx) || defined(STM32L475VE) || defined(STM32L4)
#define BSP_SENSOR_I2C_HANDLE (&hi2c3)
#else
#define BSP_SENSOR_I2C_HANDLE (&hi2c1)
#endif
#define BSP_IWDG_HANDLE       (&hiwdg)
#define BSP_RTC_HANDLE        (&hrtc)

#define BSP_RS485_RE_GPIO_Port GPIOA
#define BSP_RS485_RE_Pin       GPIO_PIN_12

#define BSP_CH395_RSTI_GPIO_Port GPIOA
#define BSP_CH395_RSTI_Pin       GPIO_PIN_4
#define BSP_CH395_RST_GPIO_Port  GPIOA
#define BSP_CH395_RST_Pin        GPIO_PIN_5
#define BSP_CH395_SEL_GPIO_Port  GPIOB
#define BSP_CH395_SEL_Pin        GPIO_PIN_0
#define BSP_CH395_RDY_GPIO_Port  GPIOB
#define BSP_CH395_RDY_Pin        GPIO_PIN_1
#define BSP_CH395_SCS_GPIO_Port  GPIOB
#define BSP_CH395_SCS_Pin        GPIO_PIN_12
#define BSP_CH395_SCK_GPIO_Port  GPIOB
#define BSP_CH395_SCK_Pin        GPIO_PIN_13
#define BSP_CH395_MISO_GPIO_Port GPIOB
#define BSP_CH395_MISO_Pin       GPIO_PIN_14
#define BSP_CH395_MOSI_GPIO_Port GPIOB
#define BSP_CH395_MOSI_Pin       GPIO_PIN_15
#define BSP_CH395_INT_GPIO_Port  GPIOD
#define BSP_CH395_INT_Pin        GPIO_PIN_13

#define BSP_AIR4G_NETSTATE_GPIO_Port GPIOA
#define BSP_AIR4G_NETSTATE_Pin       GPIO_PIN_1
#define BSP_AIR4G_RST_GPIO_Port      GPIOE
#define BSP_AIR4G_RST_Pin            GPIO_PIN_2

#define BSP_AT24C_7BIT_ADDRESS 0x50U

void bsp_board_init(void);
void bsp_delay_ms(uint32_t ms);
void bsp_delay_us(uint32_t us);
uint32_t bsp_get_tick_ms(void);

#ifdef __cplusplus
}
#endif

#endif
