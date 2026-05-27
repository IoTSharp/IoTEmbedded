# STM32L475VETX Platform

This platform describes the ALIENTEK / RT-Thread Pandora STM32L4 IoT board.
The board inventory is based on the RT-Thread Studio BSP
`sdk-bsp-stm32l475-atk-pandora` YAML, its CubeMX IOC, and board port drivers.

- MCU: STM32L475VET6 / STM32L475VETx, LQFP100, 512 KiB Flash, 128 KiB SRAM
- Cube firmware package: `STM32Cube_FW_L4_V1.18.2`
- VisualGDB BSP mode: CubeMX project importer
- Clock: HSE 8 MHz + PLL to 80 MHz, RTC from LSE
- Debug/console: USART1 on PA9/PA10 at 115200 baud via ST-LINK VCP
- RTOS: FreeRTOS through CMSIS-RTOS2, GCC `ARM_CM4F` port
- HAL tick timebase: TIM2, not SysTick

## Board Resources

| Resource | Interface | Pins | Status |
| --- | --- | --- | --- |
| AP6181 WiFi | SDMMC1 + GPIO | PC8/PC9/PC10/PC11/PC12/PD2, IRQ PC5, EN PD1 | IOC mapped, driver pending |
| W25Q128 flash | QUADSPI | PE10/PE11/PE12/PE13/PE14/PE15 | IOC mapped, storage driver pending |
| TF card | SPI1 + GPIO | PA5/PA6/PA7, CS PC3 | IOC mapped, FatFs driver pending |
| 1.3 inch TFT LCD | SPI3 + GPIO | PB3/PB5, CS PD7, DC PB4, RES PB6, PWR PB7 | IOC mapped, panel driver pending |
| ICM-20608 IMU | I2C3 | PC0/PC1 | protocol driver pending |
| AHT10 | software I2C candidate | SCL PD6, SDA PC1 | schematic/timing review needed |
| ES8388 audio | SAI1 + I2C3 | PE2/PE3/PE4/PE5/PE6, PC0/PC1 | amplifier-enable pin review needed |
| TC214B motor | TIM1 PWM | PE9 TIM1_CH1 | IOC mapped, driver pending |
| RGB LED | GPIO/PWM candidate | PB10/PB11/PB8 | IOC mapped |
| Keys | GPIO input | PD10/PD9/PD8/PC13 | IOC mapped, debounce pending |
| RS485 expansion | USART2 + GPIO | PA2/PA3, RE/DE PA12 | IOC mapped, external transceiver/header review needed |

Pandora's onboard network path is AP6181 over SDMMC1. The older UART4/UART5
CH395Q/Air724UG path conflicts with SDMMC1 pins PC10/PC11/PC12/PD2 and is not
enabled for this board profile. The legacy AT24C EEPROM storage path is also not
confirmed on Pandora; use W25Q128 QSPI flash and TF card as the planned storage
targets.

USART2 is kept as an RS485-capable expansion path for common Modbus devices, but
it is not the default network path on Pandora.

Board-specific mappings live in `src/Board/Pandora/`. Reusable device protocols
should stay under the existing shared modules or a future common sensor/device
module, while board-only pin timing, reset, chip-select, and power sequencing
stays in `Board/Pandora`.

## Bring-up Order

1. Regenerate the CubeMX/VisualGDB BSP from `STM32L475VETX.ioc` and verify all
   HAL modules for SDMMC1, QSPI, SPI, SAI, TIM, and GPIO are generated.
2. Bring up low-risk GPIO first: keys, RGB LED, beep, motor PWM.
3. Bring up storage buses: W25Q128 QSPI, then TF card over SPI1.
4. Bring up display over SPI3.
5. Bring up I2C sensors: ICM-20608 first, then AHT10 after shared SDA review.
6. Bring up AP6181 SDIO WiFi and bind it into the network socket abstraction.
7. Bring up ES8388 audio after confirming the amplifier-enable pin assignment.

The CMake project follows the same structure as the F103 reference platform:
`find_bsp()` uses `com.sysprogs.project_importers.stm32.cubemx`, the generated
`Core/`, `Drivers/`, and `Middlewares/` sources are owned by the BSP target, and
the executable target lists only repository business sources.
