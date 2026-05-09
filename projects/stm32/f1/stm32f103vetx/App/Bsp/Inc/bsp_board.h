#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "stm32f1xx_hal.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern I2C_HandleTypeDef hi2c1;
extern IWDG_HandleTypeDef hiwdg;
extern RTC_HandleTypeDef hrtc;

#define BSP_DEBUG_UART_HANDLE (&huart2)
#define BSP_RS485_UART_HANDLE (&huart1)
#define BSP_AIR724_UART_HANDLE (&huart4)
#define BSP_RS232_UART_HANDLE (&huart5)
#define BSP_AT24C_I2C_HANDLE  (&hi2c1)
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

#define BSP_BUZZER_GPIO_Port GPIOE
#define BSP_BUZZER_Pin       GPIO_PIN_7

#define BSP_AT24C_7BIT_ADDRESS 0x50U

void bsp_board_init(void);
void bsp_delay_ms(uint32_t ms);
void bsp_delay_us(uint32_t us);
uint32_t bsp_get_tick_ms(void);

#ifdef __cplusplus
}
#endif

#endif
