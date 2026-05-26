cmake_minimum_required(VERSION 3.15)

project(STM32L475VETX LANGUAGES C CXX ASM)

# Single source root. Cross-module headers are referenced as "<Module>/Inc/<header>.h".
set(SRC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../src)
set(PLATFORM_ROOT ${SRC_ROOT}/Platform/stm32/l4/stm32l475vetx)

find_bsp(
	ID com.sysprogs.arm.stm32l4
	MCU STM32L475VE
	FRAMEWORKS
		com.sysprogs.arm.stm32.STM32L4_hal
		com.sysprogs.arm.stm32.STM32L4_systeminit
		com.sysprogs.arm.fixed_stack_and_heap
	CONFIGURATION
		"com.sysprogs.bspoptions.primary_memory=flash"
		"com.sysprogs.bspoptions.stackheap.stacksize=4096"
		"com.sysprogs.bspoptions.stackheap.heapsize=32768"
		"com.sysprogs.toolchainoptions.arm.libctype=--specs=nano.specs -u _printf_float -u _scanf_float"
		"com.sysprogs.toolchainoptions.arm.syscallspecs=--specs=nosys.specs")

bsp_include_directories(
	${SRC_ROOT}
	${SRC_ROOT}/ThirdParty/Parson
	${PLATFORM_ROOT}/Core/Inc)

bsp_compile_definitions(
	APP_ENABLE_CMSIS_RTOS=0
	APP_ENABLE_FULL_STACK=0
	CONFIG_ENABLE_STORAGE_LOAD=0)

target_sources(BSP PRIVATE
	${PLATFORM_ROOT}/Core/Src/main.c
	${PLATFORM_ROOT}/Core/Src/retarget.c
	${PLATFORM_ROOT}/Core/Src/stm32l4xx_hal_msp.c
	${PLATFORM_ROOT}/Core/Src/stm32l4xx_it.c)

add_bsp_based_executable(
	NAME STM32L475VETX
	GENERATE_BIN
	GENERATE_MAP)

target_sources(STM32L475VETX PRIVATE
	# Application
	${SRC_ROOT}/Application/Src/app.c
	${SRC_ROOT}/Application/Src/app_rtos.c
	${SRC_ROOT}/Application/Src/modbus_test.c
	# Board
	${SRC_ROOT}/Board/Src/bsp_board.c
	${SRC_ROOT}/Board/Src/bsp_watchdog.c
	# Bus
	${SRC_ROOT}/Bus/Rs485/Src/bsp_rs485.c
	${SRC_ROOT}/Bus/Uart/Src/bsp_uart.c
	${SRC_ROOT}/Bus/Uart/Src/bsp_uart_callbacks.c
	# Common
	${SRC_ROOT}/Common/Src/log.c
	${SRC_ROOT}/Common/Src/md5.c
	${SRC_ROOT}/Common/Src/util.c
	# Config
	${SRC_ROOT}/Config/Src/config.c
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
	# Interpreter
	${SRC_ROOT}/Interpreter/Src/app_basic.c
	${SRC_ROOT}/Interpreter/Src/basic.c
	${SRC_ROOT}/Interpreter/Src/basic_config_api.c
	${SRC_ROOT}/Interpreter/Src/basic_format.c
	${SRC_ROOT}/Interpreter/Src/basic_json.c
	${SRC_ROOT}/Interpreter/Src/basic_modbus.c
	${SRC_ROOT}/Interpreter/Src/basic_mqtt.c
	${SRC_ROOT}/Interpreter/Src/basic_serial.c
	# Modem
	${SRC_ROOT}/Modem/Src/bsp_air724.c
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
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_api.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core_crc.c
	${SRC_ROOT}/Protocol/Modbus/Src/modbus_core_master.c
	${SRC_ROOT}/Protocol/Mqtt/Src/mqtt_client.c
	${SRC_ROOT}/Protocol/Platform/Src/platform_messages.c
	# Storage
	${SRC_ROOT}/Storage/Src/app_eeprom.c
	${SRC_ROOT}/Storage/Src/bsp_eeprom.c
	# ThirdParty
	${SRC_ROOT}/ThirdParty/Parson/parson.c
	${SRC_ROOT}/ThirdParty/Parson/parson_ex.c)

target_include_directories(STM32L475VETX PRIVATE
	${SRC_ROOT}
	${SRC_ROOT}/ThirdParty/Parson
	${PLATFORM_ROOT}/Core/Inc)

target_compile_definitions(STM32L475VETX PRIVATE
	APP_ENABLE_CMSIS_RTOS=0
	APP_ENABLE_FULL_STACK=0
	CONFIG_ENABLE_STORAGE_LOAD=0)

target_link_libraries(STM32L475VETX PRIVATE m)
