cmake_minimum_required(VERSION 3.15)

project(STM32F103VETX LANGUAGES C CXX ASM)

# Single source root. Cross-module headers are referenced as "<Module>/<header>.h"
set(SRC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../src)
set(PLATFORM_ROOT ${SRC_ROOT}/Platform/stm32/f1/stm32f103vetx)

find_bsp(
	ID com.sysprogs.project_importers.stm32.cubemx
	SOURCE_PROJECT ${PLATFORM_ROOT}/STM32F103VETX.ioc
	CONFIGURATION "com.sysprogs.toolchainoptions.arm.libctype=--specs=nano.specs -u _printf_float -u _scanf_float")

bsp_include_directories(
	${SRC_ROOT}
	${SRC_ROOT}/ThirdParty/Parson
	${PLATFORM_ROOT}/Core/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
	${PLATFORM_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
	${PLATFORM_ROOT}/Drivers/CMSIS/Include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3)

add_bsp_based_executable(
	NAME STM32F103VETX
	GENERATE_BIN
	GENERATE_MAP)

target_sources(STM32F103VETX PRIVATE
	# Application
	${SRC_ROOT}/Application/Src/app.c
	${SRC_ROOT}/Application/Src/app_freertos_hooks.c
	${SRC_ROOT}/Application/Src/app_rtos.c
	${SRC_ROOT}/Application/Src/modbus_test.c
	# Board
	${SRC_ROOT}/Board/Src/bsp_board.c
	${SRC_ROOT}/Board/Src/bsp_watchdog.c
	# Bus
	${SRC_ROOT}/Bus/Uart/Src/bsp_uart.c
	${SRC_ROOT}/Bus/Uart/Src/bsp_uart_callbacks.c
	${SRC_ROOT}/Bus/Rs485/Src/bsp_rs485.c
	# Modem
	${SRC_ROOT}/Modem/Src/bsp_air724.c
	# Common
	${SRC_ROOT}/Common/Src/log.c
	${SRC_ROOT}/Common/Src/md5.c
	${SRC_ROOT}/Common/Src/util.c
	# Config
	${SRC_ROOT}/Config/Src/config.c
	# Storage
	${SRC_ROOT}/Storage/Src/app_eeprom.c
	${SRC_ROOT}/Storage/Src/bsp_eeprom.c
	# Devices
	${SRC_ROOT}/Devices/Src/ac_controller.c
	${SRC_ROOT}/Devices/Src/device_catalog.c
	${SRC_ROOT}/Devices/Src/device_manager.c
	${SRC_ROOT}/Devices/Src/discrete_sensor.c
	${SRC_ROOT}/Devices/Src/electricity_meter.c
	${SRC_ROOT}/Devices/Src/smart_switch.c
	${SRC_ROOT}/Devices/Src/telemetry_reporter.c
	${SRC_ROOT}/Devices/Src/temperature_humidity_sensor.c
	${SRC_ROOT}/Devices/Src/ups_detector_interface.c
	# Network
	${SRC_ROOT}/Network/Src/network_manager.c
	${SRC_ROOT}/Network/Src/network_socket.c
	${SRC_ROOT}/Network/Src/network_socket_air724.c
	${SRC_ROOT}/Network/Src/network_socket_ch395.c
	${SRC_ROOT}/Network/Src/network_time.c
	# Network / CH395
	${SRC_ROOT}/Network/Ch395/Src/bsp_ch395.c
	${SRC_ROOT}/Network/Ch395/Src/ch395_board.c
	${SRC_ROOT}/Network/Ch395/Src/ch395_driver.c
	${SRC_ROOT}/Network/Ch395/Src/ch395_probe.c
	${SRC_ROOT}/Network/Ch395/Src/ch395_socket.c
	# Protocol
	${SRC_ROOT}/Protocol/Mqtt/Src/mqtt_client.c
	${SRC_ROOT}/Protocol/Platform/Src/platform_messages.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_api.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core_crc.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core_master.c
	# ThirdParty
	${SRC_ROOT}/ThirdParty/Parson/parson.c
	# Platform (CubeMX-generated, do not modify)
	${PLATFORM_ROOT}/Core/Src/freertos.c
	${PLATFORM_ROOT}/Core/Src/main.c
	${PLATFORM_ROOT}/Core/Src/stm32f1xx_hal_msp.c
	${PLATFORM_ROOT}/Core/Src/stm32f1xx_hal_timebase_tim.c
	${PLATFORM_ROOT}/Core/Src/stm32f1xx_it.c
	${PLATFORM_ROOT}/Core/Src/syscalls.c
	${PLATFORM_ROOT}/Core/Src/sysmem.c
	${PLATFORM_ROOT}/Core/Src/system_stm32f1xx.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc_ex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/croutine.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/list.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/queue.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/timers.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c)

target_include_directories(STM32F103VETX PRIVATE
	${SRC_ROOT}
	${SRC_ROOT}/ThirdParty/Parson
	${PLATFORM_ROOT}/Core/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
	${PLATFORM_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
	${PLATFORM_ROOT}/Drivers/CMSIS/Include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3)

target_compile_definitions(STM32F103VETX PRIVATE APP_ENABLE_CMSIS_RTOS=1)
