cmake_minimum_required(VERSION 3.15)

project(STM32F103VETX LANGUAGES C CXX ASM)

find_bsp(
	ID com.sysprogs.project_importers.stm32.cubemx
	SOURCE_PROJECT STM32F103VETX.ioc
	CONFIGURATION "com.sysprogs.toolchainoptions.arm.libctype=--specs=nano.specs -u _printf_float -u _scanf_float")

bsp_include_directories(App/Core/Inc)
add_bsp_based_executable(
	NAME STM32F103VETX
	GENERATE_BIN
	GENERATE_MAP)

target_sources(STM32F103VETX PRIVATE
	App/Core/Src/app.c
	App/Core/Src/app_freertos_hooks.c
	App/Core/Src/app_rtos.c
	App/Core/Src/modbus_test.c
	App/Bsp/Src/bsp_air724.c
	App/Bsp/Src/bsp_board.c
	App/Bsp/Src/bsp_ch395.c
	App/Bsp/Src/bsp_eeprom.c
	App/Bsp/Src/bsp_rs485.c
	App/Bsp/Src/bsp_uart.c
	App/Bsp/Src/bsp_uart_callbacks.c
	App/Bsp/Src/bsp_watchdog.c
	App/Common/Src/log.c
	App/Common/Src/md5.c
	App/Common/Src/util.c
	App/Config/Src/config.c
	App/Devices/Src/ac_controller.c
	App/Devices/Src/device_catalog.c
	App/Devices/Src/discrete_sensor.c
	App/Devices/Src/device_manager.c
	App/Devices/Src/electricity_meter.c
	App/Devices/Src/telemetry_reporter.c
	App/Devices/Src/smart_switch.c
	App/Devices/Src/temperature_humidity_sensor.c
	App/Devices/Src/ups_detector_interface.c
	App/Network/Ch395/Src/ch395_board.c
	App/Network/Ch395/Src/ch395_driver.c
	App/Network/Ch395/Src/ch395_probe.c
	App/Network/Ch395/Src/ch395_socket.c
	App/Network/Src/network_manager.c
	App/Network/Src/network_socket.c
	App/Network/Src/network_socket_air724.c
	App/Network/Src/network_socket_ch395.c
	App/Protocol/Mqtt/Src/mqtt_client.c
	App/Protocol/Platform/Src/platform_messages.c
	App/Storage/Src/app_eeprom.c
	App/ThirdParty/Parson/parson.c
	App/Protocol/Modbus/Src/modbus_api.c
	App/Protocol/Modbus/Src/modbus_core.c
	App/Protocol/Modbus/Src/modbus_core_crc.c
	App/Protocol/Modbus/Src/modbus_core_master.c)

target_include_directories(STM32F103VETX PRIVATE
	App/Core/Inc
	App/Bsp/Inc
	App/Common/Inc
	App/Config/Inc
	App/Devices/Inc
	App/Network/Ch395/Inc
	App/Network/Inc
	App/Protocol/Mqtt/Inc
	App/Protocol/Platform/Inc
	App/Storage/Inc
	App/ThirdParty/Parson
	App/Protocol/Modbus/Inc
	Middlewares/Third_Party/FreeRTOS/Source/include
	Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
	Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3)

target_compile_definitions(STM32F103VETX PRIVATE APP_ENABLE_CMSIS_RTOS=1)
