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
| AP6181 WiFi | SDMMC1 + GPIO | PC8/PC9/PC10/PC11/PC12/PD2, IRQ PC5, EN PD1 | board pins forced to SDMMC1, socket adapter wired to WiFi backend |
| W25Q128 flash | QUADSPI | PE10/PE11/PE12/PE13/PE14/PE15 | IOC mapped, storage driver pending |
| TF card | SPI1 + GPIO | PA5/PA6/PA7, CS PC3 | IOC mapped, FatFs driver pending |
| 1.3 inch TFT LCD | SPI3 + GPIO | PB3/PB5, CS PD7, DC PB4, RES PB6, PWR PB7 | ST7789 driver ready; SPI3 is initialized in `Board/Pandora` because this CubeMX import does not generate `hspi3` |
| ICM-20608 IMU | I2C3 | PC0/PC1 | protocol driver pending |
| AHT10 | software I2C candidate | SCL PD6, SDA PC1 | schematic/timing review needed |
| ES8388 audio | SAI1 + I2C3 | PE2/PE3/PE4/PE5/PE6, PC0/PC1 | amplifier-enable pin review needed |
| TC214B motor | TIM1 PWM | PE9 TIM1_CH1 | IOC mapped, driver pending |
| RGB LED | GPIO/PWM candidate | PB10/PB11/PB8 | IOC mapped |
| Keys | GPIO input | PD10/PD9/PD8/PC13 | IOC mapped, debounce pending |
| RS485 expansion | USART2 + GPIO | PA2/PA3, RE/DE PA12 | IOC mapped, external transceiver/header review needed |

Pandora's onboard network path is AP6181 over SDMMC1. The firmware forces
`network_mode=wifi` for this board and binds MQTT only through the
`AP6181 WiFi/SDMMC1` socket adapter. The adapter now owns the full
`is_ready/open/send/recv/close` chain used by MQTT and reports explicit AP6181
status values such as `config_missing`, `backend_missing`, `ip_down`, and
`ready`; it must not fall back to CH395Q Ethernet, Air724UG 4G, or any
UART-to-Ethernet path.

The AP6181 implementation follows the old Pandora reference flow from the
board资料: initialize the AP6181 WICED/SDIO layer, set `wlan0` to station mode,
connect with the configured SSID/password, then wait for the WiFi-ready/IP-ready
state before opening sockets. In this repository that backend is exposed through
`Network/Ap6181`; the default backend binds to the old RT-WLAN/SAL symbols when
they are linked. If the old AP6181 RT-WLAN/SAL/lwIP backend is not present, MQTT
now stops with `backend_missing` instead of a hard-coded, ambiguous WiFi-not-ready
failure.

The older UART4/UART5 CH395Q/Air724UG path conflicts with SDMMC1 pins
PC10/PC11/PC12/PD2 and is not enabled for this board profile. `bsp_board_init()`
reclaims those pins for AP6181 SDMMC1 after platform init so the active wiring is
PC8 D0, PC9 D1, PC10 D2, PC11 D3, PC12 CLK, PD2 CMD, PC5 IRQ, and PD1 EN. The
legacy AT24C EEPROM storage path is also not confirmed on Pandora; use W25Q128
QSPI flash and TF card as the planned storage targets.

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
4. Extend display support with text rendering or a framebuffer if `PRINT`/`PAINT` semantics are needed.
5. Bring up I2C sensors: ICM-20608 first, then AHT10 after shared SDA review.
6. Port or link the AP6181 RT-WLAN/SAL/lwIP backend behind `Network/Ap6181` so
   the socket adapter can move from `backend_missing` to `ready` on hardware.
7. Bring up ES8388 audio after confirming the amplifier-enable pin assignment.

The CMake project follows the same structure as the F103 reference platform:
`find_bsp()` uses `com.sysprogs.project_importers.stm32.cubemx`, the generated
`Core/`, `Drivers/`, and `Middlewares/` sources are owned by the BSP target, and
the executable target lists only repository business sources.
