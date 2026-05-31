#include "Interpreter/Inc/app_basic.h"

#include "Common/Inc/log.h"
#include "Board/Inc/bsp_board.h"
#include "Interpreter/Inc/basic.h"
#include "Interpreter/Inc/basic_config_api.h"
#include "Interpreter/Inc/basic_format.h"
#include "Interpreter/Inc/basic_json.h"
#include "Interpreter/Inc/basic_modbus.h"
#include "Interpreter/Inc/basic_mqtt.h"
#include "Interpreter/Inc/basic_display.h"
#include "Interpreter/Inc/basic_serial.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define APP_BASIC_SCRIPT_BUFFER_SIZE (EEPROM_BASIC_SCRIPT_MAX_SIZE + 1U)
#define APP_BASIC_IMPORT_BUFFER_SIZE (EEPROM_BASIC_SCRIPT_MAX_SIZE + 1U)
#define APP_BASIC_PRINT_BUFFER_SIZE  160U
#define APP_BASIC_DIRECT_STRING_FMT  "%s"
#define APP_BASIC_ARRAY_LEN(value)   (sizeof(value) / sizeof((value)[0]))

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

static struct mb_interpreter_t *app_basic_interpreter;
static bool app_basic_system_initialized;
static bool app_basic_import_in_progress;
static app_basic_print_target_t app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
static app_basic_status_t app_basic_status = {
  .loaded_slot = APP_BASIC_SLOT_PRIMARY,
};
static char app_basic_script_buffer[APP_BASIC_SCRIPT_BUFFER_SIZE];
static char app_basic_import_buffer[APP_BASIC_IMPORT_BUFFER_SIZE];

typedef ErrorStatus (*app_basic_module_register_t)(struct mb_interpreter_t *interpreter);

typedef struct {
  const char *name;
  app_basic_module_register_t register_module;
  app_basic_capability_flags_t dependencies;
  app_basic_target_flags_t targets;
} app_basic_module_descriptor_t;

static const app_basic_profile_t app_basic_current_profile = {
  .name = APP_BASIC_CURRENT_PROFILE_NAME,
  .targets = APP_BASIC_CURRENT_TARGETS,
  .capabilities = APP_BASIC_CURRENT_CAPABILITIES,
};

static const app_basic_module_descriptor_t app_basic_module_registry[] = {
  {"serial", basic_serial_register, APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"modbus", basic_modbus_register,
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"format", basic_format_register, APP_BASIC_CAPABILITY_FORMAT, APP_BASIC_TARGET_STM32},
  {"json", basic_json_register, APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"config", basic_config_register, APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"mqtt", basic_mqtt_register, APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT,
   APP_BASIC_TARGET_STM32},
#if BSP_HAS_DISPLAY
  {"display", basic_display_register, APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
#endif
};

static const app_basic_function_descriptor_t app_basic_function_registry[] = {
  {"serial", "SERIAL_OPEN", "(port:int, interface:string)", "serial_port",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_WRITE", "(port:serial_port, text:string, [timeout_ms:int])", "int bytes_written",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_WRITE_BYTES", "(port:serial_port, bytes:array, length:int, [timeout_ms:int])",
   "int bytes_written", APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_READ", "(port:serial_port, length:int, [timeout_ms:int])", "string",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_READ_BYTES", "(port:serial_port, bytes:array, length:int, [timeout_ms:int])",
   "int bytes_read", APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_BAUD", "(port:serial_port)", "int baud_rate",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_SET_BAUD", "(port:serial_port, baud_rate:int)", "int ok",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},
  {"serial", "SERIAL_FLUSH", "(port:serial_port)", "int ok",
   APP_BASIC_CAPABILITY_SERIAL, APP_BASIC_TARGET_STM32},

  {"modbus", "MODBUS_READ_COILS", "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])",
   "int bits_read", APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_DISCRETE_INPUTS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int bits_read",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_HOLD_REGS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_HOLDING_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_INPUT_REGS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_READ_INPUT_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, out:array, [wait_ms:int])", "int registers_read",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_COIL", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REG", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGISTER", "(port:serial_port, slave:int, address:int, value:int, [wait_ms:int])",
   "int ok", APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_COILS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_WRITE_REGISTERS",
   "(port:serial_port, slave:int, address:int, count:int, values:array, [wait_ms:int])", "int ok",
   APP_BASIC_CAPABILITY_SERIAL | APP_BASIC_CAPABILITY_RS485 | APP_BASIC_CAPABILITY_MODBUS_RTU,
   APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_ERROR", "()", "int error_code",
   APP_BASIC_CAPABILITY_MODBUS_RTU, APP_BASIC_TARGET_STM32},
  {"modbus", "MODBUS_LAST_ERROR", "()", "int error_code",
   APP_BASIC_CAPABILITY_MODBUS_RTU, APP_BASIC_TARGET_STM32},

  {"format", "FORMAT", "(template:string, ...values:any)", "string",
   APP_BASIC_CAPABILITY_FORMAT, APP_BASIC_TARGET_STM32},
  {"format", "FMT", "(template:string, ...values:any)", "string",
   APP_BASIC_CAPABILITY_FORMAT, APP_BASIC_TARGET_STM32},

  {"json", "JSON_PARSE", "(text:string)", "json|nil",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_VALID", "(text:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_OBJECT", "()", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_ARRAY", "()", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_STRING", "(text:string)", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_NUMBER", "(value:real)", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_BOOL", "(value:int)", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_NULL", "()", "json",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_TYPE", "(json:json, [path:string])", "int type",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_STRINGIFY", "(json:json, [path:string])", "string",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_HAS", "(json:json, path:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_COUNT", "(json:json, [path:string])", "int count",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_KEY", "(json:json, path:string, index:int)", "string",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET", "(json:json, path:string)", "json|nil",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_STRING", "(json:json, path:string, [default:string])", "string",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_NUMBER", "(json:json, path:string, [default:real])", "real",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_INT", "(json:json, path:string, [default:int])", "int",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_GET_BOOL", "(json:json, path:string, [default:int])", "int",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT", "(json:json, path:string, index:int)", "json|nil",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_STRING", "(json:json, path:string, index:int, [default:string])", "string",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_NUMBER", "(json:json, path:string, index:int, [default:real])", "real",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_INT", "(json:json, path:string, index:int, [default:int])", "int",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_AT_BOOL", "(json:json, path:string, index:int, [default:int])", "int",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_STRING", "(json:json, path:string, text:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_NUMBER", "(json:json, path:string, value:real)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_BOOL", "(json:json, path:string, value:int)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_NULL", "(json:json, path:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_JSON", "(target:json, path:string, source:json)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_STRING", "(json:json, path:string, index:int, text:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_NUMBER", "(json:json, path:string, index:int, value:real)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_BOOL", "(json:json, path:string, index:int, value:int)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_NULL", "(json:json, path:string, index:int)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_SET_AT_JSON", "(target:json, path:string, index:int, source:json)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_STRING", "(json:json, path:string, text:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_NUMBER", "(json:json, path:string, value:real)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_BOOL", "(json:json, path:string, value:int)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_NULL", "(json:json, path:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_APPEND_JSON", "(target:json, path:string, source:json)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_REMOVE", "(json:json, path:string)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_REMOVE_AT", "(json:json, path:string, index:int)", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},
  {"json", "JSON_CLEAR", "(json:json, [path:string])", "int ok",
   APP_BASIC_CAPABILITY_JSON, APP_BASIC_TARGET_STM32},

  {"config", "CONFIG_GET", "(key:string, [default:string])", "string",
   APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_SET", "(key:string, value:any)", "int ok",
   APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_APPLY", "()", "int ok",
   APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_SAVE", "()", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_STORAGE, APP_BASIC_TARGET_STM32},
  {"config", "CONFIG_RESET", "()", "int ok",
   APP_BASIC_CAPABILITY_CONFIG, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_AUTO", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_CH395", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_CH395Q, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_4G", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_AIR724UG, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_USE", "(mode:string, [persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_MODE", "()", "string",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_LINK", "()", "string",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "NETWORK_READY", "()", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_AUTO", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_CH395", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_CH395Q, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_USE_4G", "([persist:int])", "int ok",
   APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_NETWORK_AIR724UG, APP_BASIC_TARGET_STM32},
  {"config", "MQTT_SETUP_CH395",
   "(host:string, port:int, user:string, password:string, local_ip:string, gateway:string, mask:string, [persist:int])",
   "int ok", APP_BASIC_CAPABILITY_CONFIG | APP_BASIC_CAPABILITY_MQTT | APP_BASIC_CAPABILITY_NETWORK_CH395Q,
   APP_BASIC_TARGET_STM32},

  {"mqtt", "MQTT_CONNECT",
   "(endpoint:string, [port:int], [client_id:string], [username:string], [password:string], [keepalive:int])",
   "int handle", APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_DISCONNECT", "(handle:int)", "int ok",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_CONNECTED", "(handle:int)", "int connected",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_PING", "(handle:int)", "int ok",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_PUBLISH", "(handle:int, topic:string, payload:string, [qos:int], [retain:int])", "int ok",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_SUBSCRIBE", "(handle:int, topic:string, [qos:int])", "int ok",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_UNSUBSCRIBE", "(handle:int, topic:string)", "int ok",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_RECEIVE", "(handle:int, [timeout_ms:int])", "dict|nil",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"mqtt", "MQTT_LAST_ERROR", "([handle:int])", "string",
   APP_BASIC_CAPABILITY_NETWORK | APP_BASIC_CAPABILITY_MQTT, APP_BASIC_TARGET_STM32},
  {"runtime", "DELAY", "(milliseconds:int)", "int ok",
   APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},
  {"runtime", "SLEEP", "(milliseconds:int)", "int ok",
   APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},
  {"runtime", "TICKS", "()", "int milliseconds",
   APP_BASIC_CAPABILITY_TIME, APP_BASIC_TARGET_STM32},

  {"display", BASIC_DISPLAY_FUNC_SCREEN, "([mode:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_CLS, "([color:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_COLOR, "([foreground:int], [background:int], [border:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_LOCATE, "([row:int], [column:int], [cursor:int], [start:int], [stop:int])",
   "int ok", APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PSET, "(x:int, y:int, [color:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PRESET, "(x:int, y:int, [color:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_LINE, "(x1:int, y1:int, x2:int, y2:int, [color:int], [style:string])",
   "int ok", APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_CIRCLE, "(x:int, y:int, radius:int, [color:int], [fill:int])", "int ok",
   APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
  {"display", BASIC_DISPLAY_FUNC_PAINT, "(x:int, y:int, [fill_color:int], [border_color:int], [background:int])",
   "int ok", APP_BASIC_CAPABILITY_DISPLAY, APP_BASIC_TARGET_STM32},
};

static ErrorStatus app_basic_load_from_slot(app_basic_slot_t slot);
static ErrorStatus app_basic_register_profile(struct mb_interpreter_t *interpreter);
static bool app_basic_capabilities_are_available(app_basic_capability_flags_t dependencies);
static bool app_basic_targets_are_available(app_basic_target_flags_t targets);
static int app_basic_import_handler(struct mb_interpreter_t *s, const char *name);
static int app_basic_printer(struct mb_interpreter_t *s, const char *fmt, ...);
#if BSP_HAS_DISPLAY
static int app_basic_write_to_display(const char *fmt, va_list args);
static int app_basic_write_formatted_to_display(const char *fmt, va_list args);
#endif
static void app_basic_close_current(void);

void app_basic_init(void) {
  app_basic_close_current();
  if (!app_basic_system_initialized && mb_init() == MB_FUNC_OK) {
    app_basic_system_initialized = true;
  }
  memset(&app_basic_status, 0, sizeof(app_basic_status));
  app_basic_status.loaded_slot = APP_BASIC_SLOT_PRIMARY;
  app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
}

ErrorStatus app_basic_load(app_basic_slot_t preferred_slot) {
  if (app_basic_load_from_slot(preferred_slot) == SUCCESS) {
    return SUCCESS;
  }

  app_basic_slot_t fallback_slot =
    preferred_slot == APP_BASIC_SLOT_PRIMARY ? APP_BASIC_SLOT_BACKUP : APP_BASIC_SLOT_PRIMARY;
  if (app_basic_load_from_slot(fallback_slot) == SUCCESS) {
    LOG_WARNING("BASIC loaded fallback script %s", app_basic_status.loaded_name);
    return SUCCESS;
  }

  LOG_WARNING("BASIC script not loaded from EEPROM");
  return ERROR;
}

ErrorStatus app_basic_run_once(void) {
  if (app_basic_interpreter == NULL) {
    return ERROR;
  }

  int result = mb_run(app_basic_interpreter, true);
  if (result == MB_FUNC_OK || result == MB_FUNC_END) {
    return SUCCESS;
  }

  const char *file = NULL;
  int pos = 0;
  unsigned short row = 0U;
  unsigned short col = 0U;
  mb_error_e err = mb_get_last_error(app_basic_interpreter, &file, &pos, &row, &col);
  LOG_ERROR("BASIC run failed err=%d %s pos=%d row=%u col=%u", err, mb_get_error_desc(err), pos, row, col);
  return ERROR;
}

ErrorStatus app_basic_reload_and_run(app_basic_slot_t preferred_slot) {
  if (app_basic_load(preferred_slot) != SUCCESS) {
    return ERROR;
  }
  return app_basic_run_once();
}

app_basic_status_t app_basic_get_status(void) {
  return app_basic_status;
}

void app_basic_set_print_target(app_basic_print_target_t target) {
  app_basic_print_target = target;
}

app_basic_print_target_t app_basic_get_print_target(void) {
  return app_basic_print_target;
}

const app_basic_profile_t *app_basic_get_profile(void) {
  return &app_basic_current_profile;
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

static ErrorStatus app_basic_load_from_slot(app_basic_slot_t slot) {
  eeprom_basic_script_slot_t eeprom_slot = (eeprom_basic_script_slot_t)slot;
  size_t script_size = 0U;
  if (!app_basic_system_initialized) {
    app_basic_init();
  }
  if (!app_basic_system_initialized) {
    return ERROR;
  }
  if (eeprom_read_basic_script(eeprom_slot, app_basic_script_buffer, sizeof(app_basic_script_buffer), &script_size) !=
      SUCCESS) {
    return ERROR;
  }

  app_basic_close_current();
  app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
  if (mb_open(&app_basic_interpreter) != MB_FUNC_OK || app_basic_interpreter == NULL) {
    app_basic_close_current();
    return ERROR;
  }

  (void)mb_set_printer(app_basic_interpreter, app_basic_printer);
  (void)mb_set_import_handler(app_basic_interpreter, app_basic_import_handler);
  if (app_basic_register_profile(app_basic_interpreter) != SUCCESS) {
    app_basic_close_current();
    return ERROR;
  }

  if (mb_load_string(app_basic_interpreter, app_basic_script_buffer, true) != MB_FUNC_OK) {
    app_basic_close_current();
    return ERROR;
  }

  memset(&app_basic_status, 0, sizeof(app_basic_status));
  app_basic_status.loaded_slot = slot;
  (void)strncpy(app_basic_status.loaded_name, eeprom_basic_script_slot_name(eeprom_slot),
                sizeof(app_basic_status.loaded_name) - 1U);
  app_basic_status.loaded_size = script_size;
  LOG_INFO("BASIC loaded %s size=%lu", app_basic_status.loaded_name, (uint32_t)script_size);
  return SUCCESS;
}

static ErrorStatus app_basic_register_profile(struct mb_interpreter_t *interpreter) {
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

static bool app_basic_capabilities_are_available(app_basic_capability_flags_t dependencies) {
  return (dependencies & ~app_basic_current_profile.capabilities) == 0U;
}

static bool app_basic_targets_are_available(app_basic_target_flags_t targets) {
  return (targets & app_basic_current_profile.targets) != 0U;
}

static int app_basic_import_handler(struct mb_interpreter_t *s, const char *name) {
  eeprom_basic_script_slot_t slot;
  size_t script_size = 0U;
  int result = MB_FUNC_ERR;
  if (app_basic_import_in_progress) {
    return MB_FUNC_ERR;
  }
  if (!eeprom_basic_script_slot_from_package_name(name, &slot)) {
    return MB_FUNC_ERR;
  }

  app_basic_import_in_progress = true;
  if (eeprom_read_basic_script(slot, app_basic_import_buffer, sizeof(app_basic_import_buffer), &script_size) != SUCCESS) {
    goto exit;
  }

  (void)script_size;
  result = mb_load_string(s, app_basic_import_buffer, true);

exit:
  app_basic_import_in_progress = false;
  return result;
}

static int app_basic_printer(struct mb_interpreter_t *s, const char *fmt, ...) {
  (void)s;
  if (fmt == NULL) {
    return 0;
  }

#if BSP_HAS_DISPLAY
  if (app_basic_print_target == APP_BASIC_PRINT_TARGET_DISPLAY) {
    va_list args;
    va_start(args, fmt);
    int result = app_basic_write_to_display(fmt, args);
    va_end(args);
    return result;
  }
#endif

  va_list args;
  va_start(args, fmt);
  int result = vprintf(fmt, args);
  va_end(args);
  return result;
}

#if BSP_HAS_DISPLAY
static int app_basic_write_to_display(const char *fmt, va_list args) {
  if (strcmp(fmt, APP_BASIC_DIRECT_STRING_FMT) == 0) {
    const char *text = va_arg(args, const char *);
    if (text == NULL) {
      text = "";
    }
    return basic_display_write_text(text) == SUCCESS ? (int)strlen(text) : -1;
  }
  return app_basic_write_formatted_to_display(fmt, args);
}

static int app_basic_write_formatted_to_display(const char *fmt, va_list args) {
  char buffer[APP_BASIC_PRINT_BUFFER_SIZE];
  int length = vsnprintf(buffer, sizeof(buffer), fmt, args);
  if (length < 0) {
    return length;
  }

  buffer[sizeof(buffer) - 1U] = '\0';
  if (basic_display_write_text(buffer) != SUCCESS) {
    return -1;
  }
  return length;
}
#endif

static void app_basic_close_current(void) {
  if (app_basic_interpreter != NULL) {
    (void)mb_close(&app_basic_interpreter);
    app_basic_interpreter = NULL;
  }
}
