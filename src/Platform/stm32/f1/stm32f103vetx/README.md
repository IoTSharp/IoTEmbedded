# PowerEnvDaq STM32F103 Platform

This platform describes the STM32F103VETx-based power/environment data
acquisition board.

- MCU: STM32F103VET6 / STM32F103VETx, LQFP100
- VisualGDB BSP mode: CubeMX project importer
- Clock: HSE 8 MHz + PLL to 36 MHz, RTC from LSE
- Debug/console: USART2 on PA2/PA3 at 115200 baud
- RTOS: FreeRTOS through CMSIS-RTOS2, GCC `ARM_CM3` port
- HAL tick timebase: TIM2, not SysTick

## Board Resources

| Resource | Interface | Pins | Status |
| --- | --- | --- | --- |
| CH395Q Ethernet | UART4 + GPIO | PC10/PC11, PA4, PA5, PB0, PB1, PB12, PD13 | ready, primary network |
| Air724UG 4G modem | UART4 shared + GPIO | UART4 shared, PE2 RST, PA1 NETSTATE | ready, fallback network |
| AT24C EEPROM | I2C1 | PB6/PB7 | ready, config/script storage |
| RS485 field bus | USART1 + GPIO | PA9/PA10, RE/DE PA12 | ready, Modbus RTU acquisition bus |
| RS232 service port | UART5 | PC12/PD2 | IOC mapped, external device depends on deployment |
| Debug console | USART2 | PA2/PA3 | ready |
| Buzzer | GPIO | PE7 | IOC mapped |
| RTC | RTC + LSE | PC14/PC15 | IOC mapped |
| Independent watchdog | IWDG | internal LSI | ready |

Board-specific resource inventory lives in `src/Board/PowerEnvDaq/`. Reusable
protocols and drivers remain in shared modules such as `Network/Ch395/`,
`Modem/`, `Storage/`, `Bus/Rs485/`, `Protocol/Modbus/`, and `Devices/`.

CH395Q and Air724UG share UART4, so runtime network selection must keep using
reset isolation before switching the active link.
