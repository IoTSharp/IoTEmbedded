#include "Interpreter/Inc/app_basic_registry.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Interpreter/Inc/basic_config_api.h"
#include "Interpreter/Inc/basic_display.h"
#include "Interpreter/Inc/basic_format.h"
#include "Interpreter/Inc/basic_json.h"
#include "Interpreter/Inc/basic_modbus.h"
#include "Interpreter/Inc/basic_mqtt.h"
#include "Interpreter/Inc/basic_serial.h"

#define APP_BASIC_ARRAY_LEN(value) (sizeof(value) / sizeof((value)[0]))

#if BSP_HAS_AP6181 || BSP_HAS_DISPLAY
#define APP_BASIC_CURRENT_PROFILE_NAME "stm32l475vetx"
#elif BSP_HAS_CH395Q || BSP_HAS_AIR724UG || BSP_HAS_RS232
#define APP_BASIC_CURRENT_PROFILE_NAME "stm32f103vetx"
#else
#define APP_BASIC_CURRENT_PROFILE_NAME "generic-stm32"
#endif

#if defined(APP_ENABLE_CMSIS_RTOS) && APP_ENABLE_CMSIS_RTOS
#define APP_BASIC_CURRENT_TARGETS (APP_BASIC_TARGET_STM32 | APP_BASIC_TARGET_RTOS)
#else
#define APP_BASIC_CURRENT_TARGETS APP_BASIC_TARGET_STM32
#endif

#define APP_BASIC_CAP_IF(enabled, capability) ((enabled) ? (capability) : 0U)
#define APP_BASIC_CURRENT_CAPABILITIES                                                        \
  (APP_BASIC_CAPABILITY_RUNTIME | APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 |   \
   APP_BASIC_CAPABILITY_MODBUS_RTU | APP_BASIC_CAPABILITY_FORMAT | APP_BASIC_CAPABILITY_JSON | \
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT |   \
   APP_BASIC_CAPABILITY_TIME |                                                               \
   APP_BASIC_CAP_IF(BSP_HAS_AT24C, APP_BASIC_CAPABILITY_STORAGE) |                            \
   APP_BASIC_CAP_IF(BSP_HAS_RS232, APP_BASIC_CAPABILITY_RS232) |                               \
   APP_BASIC_CAP_IF(BSP_HAS_CH395Q, APP_BASIC_CAPABILITY_NETWORK_CH395Q) |                     \
   APP_BASIC_CAP_IF(BSP_HAS_AIR724UG, APP_BASIC_CAPABILITY_NETWORK_AIR724UG) |                 \
   APP_BASIC_CAP_IF(BSP_HAS_AP6181, APP_BASIC_CAPABILITY_NETWORK_AP6181) |                     \
   APP_BASIC_CAP_IF(BSP_HAS_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY))

#define APP_BASIC_ERROR_SERIAL                                                                \
  "Invalid arguments abort the call; device failures return 0, an empty string, or a partial " \
  "byte count."
#define APP_BASIC_ERROR_MODBUS                                                               \
  "Returns 0 on invalid data, no response, CRC, exception, or frame error; MODBUS_LAST_ERROR " \
  "returns the latest numeric code."
#define APP_BASIC_ERROR_MQTT                                                                  \
  "Returns 0 or NIL on failure; MQTT_LAST_ERROR returns the latest message."
#define APP_BASIC_ERROR_JSON                                                                  \
  "Invalid arguments abort the call; parse/get misses return NIL, default values, 0, or -1."
#define APP_BASIC_ERROR_CONFIG                                                                \
  "Returns 0 or the supplied default string on unknown key, invalid value, apply, or save failure."
#define APP_BASIC_ERROR_DISPLAY                                                              \
  "Returns 0 when the display operation is unavailable or unsupported."
#define APP_BASIC_ERROR_FORMAT "Invalid arguments abort the call."
#define APP_BASIC_ERROR_TIME   "Negative duration is treated as 0; returns 1 after the delay."

#define APP_BASIC_TIMEOUT_NONE    "No script-visible timeout."
#define APP_BASIC_TIMEOUT_SERIAL  "Optional timeout_ms; default 1000 ms; negative coerces to 0."
#define APP_BASIC_TIMEOUT_MODBUS  "Optional wait_ms; default 180 ms plus computed RTU frame time."
#define APP_BASIC_TIMEOUT_MQTT_RX "Optional timeout_ms; default 0 non-blocking; poll slice is 50 ms."
#define APP_BASIC_TIMEOUT_DELAY   "Duration is the first argument; negative coerces to 0."
#define APP_BASIC_TIMEOUT_CONFIG  "No script-visible timeout; operation follows board storage/network timing."
#define APP_BASIC_TIMEOUT_DISPLAY "No script-visible timeout; operation follows display driver timing."

#define APP_BASIC_HANDLE_NONE    "No handle."
#define APP_BASIC_HANDLE_SERIAL  "SERIAL_OPEN returns a borrowed board port usertype; valid until interpreter/profile reload."
#define APP_BASIC_HANDLE_MODBUS  "Uses SERIAL_OPEN port handles; no independent Modbus connection handle is created."
#define APP_BASIC_HANDLE_MQTT                                                                 \
  "MQTT_CONNECT returns an integer session handle; MQTT_DISCONNECT or another MQTT_CONNECT "    \
  "invalidates the previous physical session."
#define APP_BASIC_HANDLE_JSON                                                                 \
  "JSON constructors, parse, get, and at return interpreter-owned JSON usertype values while " \
  "referenced by the script."
#define APP_BASIC_HANDLE_DISPLAY "SCREEN selects the active display/print target; no explicit handle."

#define APP_BASIC_MEMORY_RUNTIME "Returned scalars, strings, lists, dictionaries, and usertypes are owned by MY-BASIC."
#define APP_BASIC_MEMORY_BUFFER  "Array/buffer arguments are owned by the script and may be read or mutated in place."
#define APP_BASIC_MEMORY_JSON    "JSON usertypes are owned by MY-BASIC; returned child values are independent copies."
#define APP_BASIC_MEMORY_MQTT    "MQTT_RECEIVE returns a MY-BASIC-owned DICT with copied topic and payload strings."
#define APP_BASIC_MEMORY_CONFIG  "Returned strings are MY-BASIC-owned copies; persisted config remains owned by Config."
#define APP_BASIC_MEMORY_DISPLAY "No script-owned memory beyond scalar return values."

#define APP_BASIC_DEP_MODBUS \
  (APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU)
#define APP_BASIC_DEP_MQTT (APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT)
#define APP_BASIC_DEP_CONFIG_NETWORK (APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK)
#define APP_BASIC_DEP_CONFIG_CH395 (APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_CH395Q)
#define APP_BASIC_DEP_CONFIG_4G (APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_AIR724UG)
#define APP_BASIC_DEP_MQTT_SETUP_CH395 \
  (APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_MQTT | APP_BASIC_CAPABILITY_NETWORK_CH395Q)

static const app_basic_profile_t app_basic_current_profile = {
  .name = APP_BASIC_CURRENT_PROFILE_NAME,
  .targets = APP_BASIC_CURRENT_TARGETS,
  .capabilities = APP_BASIC_CURRENT_CAPABILITIES,
};

static const app_basic_module_descriptor_t app_basic_module_registry[] = {
  {"serial", basic_serial_register, APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"modbus", basic_modbus_register, APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"format", basic_format_register, APP_BASIC_CAPABILITY_FORMAT, APP_BASIC_TARGET_STM32},
  {"json", basic_json_register, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"config", basic_config_register, APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"mqtt", basic_mqtt_register, APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},
#if BSP_HAS_DISPLAY
  {"display", basic_display_register, APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
#endif
};

static const app_basic_function_descriptor_t app_basic_function_registry[] = {
  {"serial", "SERIAL_OPEN", "(port:int, interface:string)", "serial_port", APP_BASIC_ERROR_SERIAL,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_SERIAL,
   APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_WRITE", "(port:serial_port, text:string, [timeout_ms:int])", "int bytes_written",
   APP_BASIC_ERROR_SERIAL, APP_BASIC_TIMEOUT_SERIAL, APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_WRITE_BYTES", "(port:serial_port, bytes:array, length:int, [timeout_ms:int])",
   "int bytes_written", APP_BASIC_ERROR_SERIAL, APP_BASIC_TIMEOUT_SERIAL, APP_BASIC_HANDLE_SERIAL,
   APP_BASIC_MEMORY_BUFFER, APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_READ", "(port:serial_port, length:int, [timeout_ms:int])", "string",
   APP_BASIC_ERROR_SERIAL, APP_BASIC_TIMEOUT_SERIAL, APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_READ_BYTES", "(port:serial_port, bytes:array, length:int, [timeout_ms:int])",
   "int bytes_read", APP_BASIC_ERROR_SERIAL, APP_BASIC_TIMEOUT_SERIAL, APP_BASIC_HANDLE_SERIAL,
   APP_BASIC_MEMORY_BUFFER, APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_BAUD", "(port:serial_port)", "int baud_rate", APP_BASIC_ERROR_SERIAL,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_SERIAL,
   APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_SET_BAUD", "(port:serial_port, baud_rate:int)", "int ok", APP_BASIC_ERROR_SERIAL,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_SERIAL,
   APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_FLUSH", "(port:serial_port)", "int ok", APP_BASIC_ERROR_SERIAL, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_SERIAL, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},

  {"modbus", "MODBUS_READ_COILS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int bits_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_DISCRETE_INPUTS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int bits_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_HOLD_REGS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_HOLDING_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_INPUT_REGS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_INPUT_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_COIL", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS,
   APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REG", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS,
   APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGISTER", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS,
   APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_COILS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_MODBUS, APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_BUFFER,
   APP_BASIC_DEP_MODBUS, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_ERROR", "()", "int error_code", APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_MODBUS_RTU, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_LAST_ERROR", "()", "int error_code", APP_BASIC_ERROR_MODBUS, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_MODBUS, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_MODBUS_RTU, APP_BASIC_TARGET_STM32},

  {"format", "FORMAT", "(template:string, ...values:any)", "string", APP_BASIC_ERROR_FORMAT,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_FORMAT,
   APP_BASIC_TARGET_STM32},
  {"format", "FMT", "(template:string, ...values:any)", "string", APP_BASIC_ERROR_FORMAT,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_FORMAT,
   APP_BASIC_TARGET_STM32},

  {"json", "JSON_PARSE", "(text:string)", "json|nil", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_VALID", "(text:string)", "int ok", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_OBJECT", "()", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON,
   APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_ARRAY", "()", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON,
   APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_STRING", "(text:string)", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_NUMBER", "(value:real)", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_BOOL", "(value:int)", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_NULL", "()", "json", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON,
   APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_TYPE", "(json:json, [path:string])", "int type", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_STRINGIFY", "(json:json, [path:string])", "string", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_HAS", "(json:json, path:string)", "int ok", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_COUNT", "(json:json, [path:string])", "int count", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_KEY", "(json:json, path:string, index:int)", "string", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET", "(json:json, path:string)", "json|nil", APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_STRING", "(json:json, path:string, [default:string])", "string", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_NUMBER", "(json:json, path:string, [default:real])", "real", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_INT", "(json:json, path:string, [default:int])", "int", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_BOOL", "(json:json, path:string, [default:int])", "int", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT", "(json:json, path:string, index:int)", "json|nil", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_STRING", "(json:json, path:string, index:int, [default:string])", "string",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_NUMBER", "(json:json, path:string, index:int, [default:real])", "real",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_INT", "(json:json, path:string, index:int, [default:int])", "int",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_BOOL", "(json:json, path:string, index:int, [default:int])", "int",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_STRING", "(json:json, path:string, text:string)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_NUMBER", "(json:json, path:string, value:real)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_BOOL", "(json:json, path:string, value:int)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_NULL", "(json:json, path:string)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_JSON", "(target:json, path:string, source:json)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_STRING", "(json:json, path:string, index:int, text:string)", "int ok",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_NUMBER", "(json:json, path:string, index:int, value:real)", "int ok",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_BOOL", "(json:json, path:string, index:int, value:int)", "int ok",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_NULL", "(json:json, path:string, index:int)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_JSON", "(target:json, path:string, index:int, source:json)", "int ok",
   APP_BASIC_ERROR_JSON, APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON,
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_STRING", "(json:json, path:string, text:string)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_NUMBER", "(json:json, path:string, value:real)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_BOOL", "(json:json, path:string, value:int)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_NULL", "(json:json, path:string)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_JSON", "(target:json, path:string, source:json)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_REMOVE", "(json:json, path:string)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_REMOVE_AT", "(json:json, path:string, index:int)", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},
  {"json", "JSON_CLEAR", "(json:json, [path:string])", "int ok", APP_BASIC_ERROR_JSON,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_JSON, APP_BASIC_MEMORY_JSON, APP_BASIC_CAPABILITY_JSON,
   APP_BASIC_TARGET_STM32},

  {"config", "CONFIG_GET", "(key:string, [default:string])", "string", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_CAPABILITY_CONFIG,
   APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_SET", "(key:string, value:any)", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_CAPABILITY_CONFIG,
   APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_APPLY", "()", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_SAVE", "()", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG,
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_STORAGE, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_RESET", "()", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_AUTO", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK,
   APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_CH395", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_CH395,
   APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_4G", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_4G, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_USE", "(mode:string, [persist:int])", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK,
   APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_MODE", "()", "string", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_LINK", "()", "string", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_READY", "()", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_AUTO", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_NETWORK,
   APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_CH395", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_CH395,
   APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_4G", "([persist:int])", "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG, APP_BASIC_DEP_CONFIG_4G, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_SETUP_CH395",
   "(host:string, port:int, user:string, password:string, local_ip:string, gateway:string, mask:string, [persist:int])",
   "int ok", APP_BASIC_ERROR_CONFIG, APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_CONFIG,
   APP_BASIC_DEP_MQTT_SETUP_CH395, APP_BASIC_TARGET_STM32},

  {"mqtt", "MQTT_CONNECT",
   "(endpoint:string, [port:int], [client_id:string], [username:string], [password:string], [keepalive:int])",
   "int handle", APP_BASIC_ERROR_MQTT, APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME,
   APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_DISCONNECT", "(handle:int)", "int ok", APP_BASIC_ERROR_MQTT, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_CONNECTED", "(handle:int)", "int connected", APP_BASIC_ERROR_MQTT,
   APP_BASIC_TIMEOUT_NONE, APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT,
   APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_PING", "(handle:int)", "int ok", APP_BASIC_ERROR_MQTT, APP_BASIC_TIMEOUT_CONFIG,
   APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_PUBLISH", "(handle:int, topic:string, payload:string, [qos:int], [retain:int])",
   "int ok", APP_BASIC_ERROR_MQTT, APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_MQTT,
   APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_SUBSCRIBE", "(handle:int, topic:string, [qos:int])", "int ok", APP_BASIC_ERROR_MQTT,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT,
   APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_UNSUBSCRIBE", "(handle:int, topic:string)", "int ok", APP_BASIC_ERROR_MQTT,
   APP_BASIC_TIMEOUT_CONFIG, APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT,
   APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_RECEIVE", "(handle:int, [timeout_ms:int])", "dict|nil", APP_BASIC_ERROR_MQTT,
   APP_BASIC_TIMEOUT_MQTT_RX, APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_MQTT, APP_BASIC_DEP_MQTT,
   APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_LAST_ERROR", "([handle:int])", "string", APP_BASIC_ERROR_MQTT, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_MQTT, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_DEP_MQTT, APP_BASIC_TARGET_STM32},

  {"runtime", "DELAY", "(milliseconds:int)", "int ok", APP_BASIC_ERROR_TIME, APP_BASIC_TIMEOUT_DELAY,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},
  {"runtime", "SLEEP", "(milliseconds:int)", "int ok", APP_BASIC_ERROR_TIME, APP_BASIC_TIMEOUT_DELAY,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},
  {"runtime", "TICKS", "()", "int milliseconds", APP_BASIC_ERROR_TIME, APP_BASIC_TIMEOUT_NONE,
   APP_BASIC_HANDLE_NONE, APP_BASIC_MEMORY_RUNTIME, APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},

  {"display", BASIC_DISPLAY_FUNC_SCREEN, "([mode:int])", "int ok", APP_BASIC_ERROR_DISPLAY,
   APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY,
   APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_CLS, "([color:int])", "int ok", APP_BASIC_ERROR_DISPLAY,
   APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY,
   APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_COLOR, "([foreground:int], [background:int], [border:int])", "int ok",
   APP_BASIC_ERROR_DISPLAY, APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY,
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_LOCATE,
   "([row:int], [column:int], [cursor:int], [start:int], [stop:int])", "int ok", APP_BASIC_ERROR_DISPLAY,
   APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY,
   APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PSET, "(x:int, y:int, [color:int])", "int ok", APP_BASIC_ERROR_DISPLAY,
   APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY,
   APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PRESET, "(x:int, y:int, [color:int])", "int ok", APP_BASIC_ERROR_DISPLAY,
   APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY,
   APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_LINE, "(x1:int, y1:int, x2:int, y2:int, [color:int], [style:string])",
   "int ok", APP_BASIC_ERROR_DISPLAY, APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY,
   APP_BASIC_MEMORY_DISPLAY, APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_CIRCLE, "(x:int, y:int, radius:int, [color:int], [fill:int])", "int ok",
   APP_BASIC_ERROR_DISPLAY, APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY,
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PAINT,
   "(x:int, y:int, [fill_color:int], [border_color:int], [background:int])", "int ok",
   APP_BASIC_ERROR_DISPLAY, APP_BASIC_TIMEOUT_DISPLAY, APP_BASIC_HANDLE_DISPLAY, APP_BASIC_MEMORY_DISPLAY,
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
};

const app_basic_profile_t *app_basic_get_profile(void) {
  return &app_basic_current_profile;
}

const app_basic_module_descriptor_t *app_basic_get_module_registry(size_t *count) {
  if (count != NULL) {
    *count = APP_BASIC_ARRAY_LEN(app_basic_module_registry);
  }
  return app_basic_module_registry;
}

const app_basic_function_descriptor_t *app_basic_get_function_registry(size_t *count) {
  if (count != NULL) {
    *count = APP_BASIC_ARRAY_LEN(app_basic_function_registry);
  }
  return app_basic_function_registry;
}

bool app_basic_function_is_available(const app_basic_function_descriptor_t *function) {
  return function != NULL && app_basic_targets_are_available(function->targets) &&
         app_basic_capabilities_are_available(function->dependencies);
}

ErrorStatus app_basic_register_profile(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  for (size_t i = 0U; i < APP_BASIC_ARRAY_LEN(app_basic_module_registry); i++) {
    const app_basic_module_descriptor_t *module = &app_basic_module_registry[i];
    if (!app_basic_targets_are_available(module->targets) ||
        !app_basic_capabilities_are_available(module->dependencies)) {
      continue;
    }
    if (module->register_module == NULL || module->register_module(interpreter) != SUCCESS) {
      LOG_ERROR("BASIC module register failed profile=%s module=%s", app_basic_current_profile.name,
                module->name == NULL ? "" : module->name);
      return ERROR;
    }
  }

  LOG_INFO("BASIC profile registered name=%s targets=0x%08lX capabilities=0x%08lX",
           app_basic_current_profile.name, (unsigned long)app_basic_current_profile.targets,
           (unsigned long)app_basic_current_profile.capabilities);
  return SUCCESS;
}

bool app_basic_capabilities_are_available(app_basic_capability_flags_t dependencies) {
  return (dependencies & ~app_basic_current_profile.capabilities) == 0U;
}

bool app_basic_targets_are_available(app_basic_target_flags_t targets) {
  return (targets & app_basic_current_profile.targets) != 0U;
}
