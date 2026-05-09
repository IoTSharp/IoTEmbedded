cmake_minimum_required(VERSION 3.15)

project(STM32F103VETX LANGUAGES C CXX ASM)

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../src/App)
set(PLATFORM_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../src/Platform/stm32/f1/stm32f103vetx)

find_bsp(
	ID com.sysprogs.project_importers.stm32.cubemx
	SOURCE_PROJECT ${PLATFORM_ROOT}/STM32F103VETX.ioc
	CONFIGURATION "com.sysprogs.toolchainoptions.arm.libctype=--specs=nano.specs -u _printf_float -u _scanf_float")

bsp_include_directories(
	${APP_ROOT}/Core/Inc
	${APP_ROOT}/Bsp/Inc
	${APP_ROOT}/Common/Inc
	${APP_ROOT}/Config/Inc
	${APP_ROOT}/Devices/Inc
	${APP_ROOT}/Network/Ch395/Inc
	${APP_ROOT}/Network/Inc
	${APP_ROOT}/Protocol/Mqtt/Inc
	${APP_ROOT}/Protocol/Platform/Inc
	${APP_ROOT}/Storage/Inc
	${APP_ROOT}/ThirdParty/Parson
	${APP_ROOT}/Protocol/Modbus/Inc
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
	${APP_ROOT}/Core/Src/app.c
	${APP_ROOT}/Core/Src/app_freertos_hooks.c
	${APP_ROOT}/Core/Src/app_rtos.c
	${APP_ROOT}/Core/Src/modbus_test.c
	${APP_ROOT}/Bsp/Src/bsp_air724.c
	${APP_ROOT}/Bsp/Src/bsp_board.c
	${APP_ROOT}/Bsp/Src/bsp_ch395.c
	${APP_ROOT}/Bsp/Src/bsp_eeprom.c
	${APP_ROOT}/Bsp/Src/bsp_rs485.c
	${APP_ROOT}/Bsp/Src/bsp_uart.c
	${APP_ROOT}/Bsp/Src/bsp_uart_callbacks.c
	${APP_ROOT}/Bsp/Src/bsp_watchdog.c
	${APP_ROOT}/Common/Src/log.c
	${APP_ROOT}/Common/Src/md5.c
	${APP_ROOT}/Common/Src/util.c
	${APP_ROOT}/Config/Src/config.c
	${APP_ROOT}/Devices/Src/ac_controller.c
	${APP_ROOT}/Devices/Src/device_catalog.c
	${APP_ROOT}/Devices/Src/discrete_sensor.c
	${APP_ROOT}/Devices/Src/device_manager.c
	${APP_ROOT}/Devices/Src/electricity_meter.c
	${APP_ROOT}/Devices/Src/telemetry_reporter.c
	${APP_ROOT}/Devices/Src/smart_switch.c
	${APP_ROOT}/Devices/Src/temperature_humidity_sensor.c
	${APP_ROOT}/Devices/Src/ups_detector_interface.c
	${APP_ROOT}/Network/Ch395/Src/ch395_board.c
	${APP_ROOT}/Network/Ch395/Src/ch395_driver.c
	${APP_ROOT}/Network/Ch395/Src/ch395_probe.c
	${APP_ROOT}/Network/Ch395/Src/ch395_socket.c
	${APP_ROOT}/Network/Src/network_manager.c
	${APP_ROOT}/Network/Src/network_socket.c
	${APP_ROOT}/Network/Src/network_socket_air724.c
	${APP_ROOT}/Network/Src/network_socket_ch395.c
	${APP_ROOT}/Protocol/Mqtt/Src/mqtt_client.c
	${APP_ROOT}/Protocol/Platform/Src/platform_messages.c
	${APP_ROOT}/Storage/Src/app_eeprom.c
	${APP_ROOT}/ThirdParty/Parson/parson.c
	${APP_ROOT}/Protocol/Modbus/Src/modbus_api.c
	${APP_ROOT}/Protocol/Modbus/Src/modbus_core.c
	${APP_ROOT}/Protocol/Modbus/Src/modbus_core_crc.c
	${APP_ROOT}/Protocol/Modbus/Src/modbus_core_master.c
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
	${APP_ROOT}/Core/Inc
	${APP_ROOT}/Bsp/Inc
	${APP_ROOT}/Common/Inc
	${APP_ROOT}/Config/Inc
	${APP_ROOT}/Devices/Inc
	${APP_ROOT}/Network/Ch395/Inc
	${APP_ROOT}/Network/Inc
	${APP_ROOT}/Protocol/Mqtt/Inc
	${APP_ROOT}/Protocol/Platform/Inc
	${APP_ROOT}/Storage/Inc
	${APP_ROOT}/ThirdParty/Parson
	${APP_ROOT}/Protocol/Modbus/Inc
	${PLATFORM_ROOT}/Core/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc
	${PLATFORM_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
	${PLATFORM_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
	${PLATFORM_ROOT}/Drivers/CMSIS/Include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
	${PLATFORM_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3)

target_compile_definitions(STM32F103VETX PRIVATE APP_ENABLE_CMSIS_RTOS=1)
