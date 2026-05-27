# STM32L475VETX Platform

This platform describes the ALIENTEK Pandora STM32L4 IoT development board used
for the bring-up firmware.

- MCU: STM32L475VET6 / STM32L475VETx, LQFP100, 512 KiB Flash
- Cube firmware package: `STM32Cube_FW_L4_V1.18.2`
- VisualGDB BSP mode: CubeMX project importer
- Current clock: HSI + PLL at 80 MHz, RTC from LSI
- Debug/console: USART1 on PA9/PA10 at 115200 baud via ST-LINK VCP
- RTOS: FreeRTOS through CMSIS-RTOS2, GCC `ARM_CM4F` port
- HAL tick timebase: TIM2, not SysTick
- Bring-up note: BOOT jumper must short BOOT0 to GND for boot-from-Flash

The `STM32L475VETX.ioc` file is the CubeMX/VisualGDB platform description for
the board pinout and enabled peripherals. The `.project`, `.cproject`,
`.mxproject`, `.settings/`, `.gpdsc`, and linker script are the CubeIDE/CubeMX
metadata VisualGDB uses when importing the IOC as a generated BSP-style source
project.

The CMake project follows the same structure as the F103 reference platform:
`find_bsp()` uses `com.sysprogs.project_importers.stm32.cubemx`, the generated
`Core/`, `Drivers/`, and `Middlewares/` sources are owned by the BSP target, and
the executable target lists only repository business sources.
