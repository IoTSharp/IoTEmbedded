# STM32 Platform Standard

STM32 platform packages live under `src/Platform/stm32/<family>/<board>`.
New STM32 boards must follow the F103 project shape unless there is a documented
hardware reason to differ.

## Required Files

Each board platform directory keeps the CubeMX/CubeIDE layout intact:

- `.ioc`, `.project`, `.cproject`, `.mxproject`, `.settings/`
- `<PROJECT>.gpdsc`
- `<PROJECT>_FLASH.ld`
- `Core/Inc`, `Core/Src`, `Core/Startup`
- `Drivers/CMSIS`, `Drivers/STM32<family>xx_HAL_Driver`
- `Middlewares/Third_Party/FreeRTOS` when RTOS is enabled

Do not rearrange generated `Core/`, `Drivers/`, or `Middlewares/` files into the
repository `Inc/Src` module layout. Platform code is generated/vendor code and is
the explicit exception.

## VisualGDB/CMake

Board projects live at `projects/stm32/<family>/<board>/CMakeLists.txt`.
They must use the CubeMX importer:

```cmake
find_bsp(
  ID com.sysprogs.project_importers.stm32.cubemx
  SOURCE_PROJECT ${PLATFORM_ROOT}/<PROJECT>.ioc
  CONFIGURATION "com.sysprogs.toolchainoptions.arm.libctype=--specs=nano.specs -u _printf_float -u _scanf_float")
```

The executable target must not list any `${PLATFORM_ROOT}/**` source file.
Generated `Core/`, HAL/CMSIS `Drivers/`, and FreeRTOS `Middlewares/` belong to
the BSP target produced by `find_bsp()` and `add_bsp_based_executable()`.

Include paths are limited to:

- `${SRC_ROOT}`
- `${SRC_ROOT}/ThirdParty/Parson`
- platform-private `Core/Inc`, HAL, CMSIS, and FreeRTOS include directories

Do not add per-module include paths for repository business modules.

## FreeRTOS

RTOS boards use FreeRTOS through CMSIS-RTOS2:

- `APP_ENABLE_CMSIS_RTOS=1`
- application sources include `Application/Src/app_freertos_hooks.c`
- `Core/Inc/FreeRTOSConfig.h` is present
- `Core/Src/freertos.c` is present
- `Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c` is in the
  imported BSP source list
- HAL tick uses a hardware timer such as TIM2; SysTick is reserved for the RTOS

Use the correct FreeRTOS GCC port for the CPU:

- STM32F1 Cortex-M3: `portable/GCC/ARM_CM3`
- STM32L4 Cortex-M4F: `portable/GCC/ARM_CM4F` plus
  `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`

## Device Macros

Always define the official ST CMSIS device macro, for example `STM32F103xE` or
`STM32L475xx`. A board may also define a local compatibility macro when existing
repository code uses one, such as `STM32L475VE`.

## Validation

After adding or changing a platform:

- run the VisualGDB CubeMX importer or configure the CMake project from a clean
  build directory
- build once with VisualGDB/CMake
- confirm the generated BSP contains `Core`, `Drivers`, and `Middlewares` when
  RTOS is enabled
- confirm no executable `target_sources()` entry points into `${PLATFORM_ROOT}`
- confirm module includes do not omit `/Inc/`
