#include "Interpreter/Inc/basic_uart.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_board.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Bus/Uart/Inc/bsp_uart.h"
#include "Interpreter/Inc/basic.h"

#include "stm32f1xx_hal.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BASIC_UART_DEFAULT_TIMEOUT_MS 1000U
#define BASIC_UART_READ_SLICE_MS      20U
#define BASIC_UART_TEXT_READ_MAX      256U
#define BASIC_UART_TX_CHUNK_SIZE      64U

typedef enum {
  BASIC_UART_KIND_NONE = 0,
  BASIC_UART_KIND_RS485,
  BASIC_UART_KIND_DEBUG,
  BASIC_UART_KIND_AIR724,
  BASIC_UART_KIND_RS232,
} basic_uart_kind_t;

typedef struct {
  basic_uart_kind_t kind;
  UART_HandleTypeDef *handle;
} basic_uart_target_t;

static int basic_uart_write(struct mb_interpreter_t *s, void **l);
static int basic_uart_write_bytes(struct mb_interpreter_t *s, void **l);
static int basic_uart_read(struct mb_interpreter_t *s, void **l);
static int basic_uart_read_bytes(struct mb_interpreter_t *s, void **l);
static int basic_uart_baud(struct mb_interpreter_t *s, void **l);
static int basic_uart_set_baud(struct mb_interpreter_t *s, void **l);
static int basic_uart_flush_func(struct mb_interpreter_t *s, void **l);
static int basic_rs485_write(struct mb_interpreter_t *s, void **l);
static int basic_rs485_write_bytes(struct mb_interpreter_t *s, void **l);
static int basic_rs485_read(struct mb_interpreter_t *s, void **l);
static int basic_rs485_read_bytes(struct mb_interpreter_t *s, void **l);
static int basic_rs485_baud(struct mb_interpreter_t *s, void **l);
static int basic_rs485_set_baud(struct mb_interpreter_t *s, void **l);
static int basic_rs485_flush(struct mb_interpreter_t *s, void **l);

static int basic_uart_pop_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t *target);
static int basic_uart_write_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_write_bytes_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_read_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_read_bytes_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_baud_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_set_baud_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static int basic_uart_flush_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target);
static bool basic_uart_target_from_value(mb_value_t value, basic_uart_target_t *target);
static bool basic_uart_kind_from_port_id(int_t port_id, basic_uart_kind_t *kind);
static bool basic_uart_kind_from_name(const char *name, basic_uart_kind_t *kind);
static bool basic_uart_name_equals(const char *left, const char *right);
static basic_uart_target_t basic_uart_target_from_kind(basic_uart_kind_t kind);
static basic_uart_target_t basic_uart_rs485_target(void);
static uint32_t basic_uart_get_baud_rate(basic_uart_target_t target);
static HAL_StatusTypeDef basic_uart_configure_baud_rate(basic_uart_target_t target, uint32_t baud_rate);
static void basic_uart_begin_write(basic_uart_target_t target);
static HAL_StatusTypeDef basic_uart_end_write(basic_uart_target_t target, HAL_StatusTypeDef status);
static HAL_StatusTypeDef basic_uart_write_chunk(basic_uart_target_t target, const uint8_t *data, uint16_t length,
                                                uint32_t timeout_ms);
static uint32_t basic_uart_remaining_timeout(uint32_t start_ms, uint32_t timeout_ms);
static size_t basic_uart_write_contiguous(basic_uart_target_t target, const uint8_t *data, size_t length,
                                          uint32_t timeout_ms);
static int basic_uart_write_array(struct mb_interpreter_t *s, void **l, basic_uart_target_t target, void *array,
                                  uint32_t length, uint32_t timeout_ms, uint32_t *written);
static int basic_uart_validate_array(struct mb_interpreter_t *s, void **l, void *array, uint32_t length, bool *valid);
static int basic_uart_array_to_chunk(struct mb_interpreter_t *s, void **l, void *array, uint32_t offset,
                                     uint8_t *chunk, uint16_t length);
static bool basic_uart_value_to_byte(mb_value_t value, uint8_t *byte);
static bool basic_uart_array_capacity(struct mb_interpreter_t *s, void **l, mb_value_t array_value,
                                      uint32_t *capacity);
static HAL_StatusTypeDef basic_uart_read_one(basic_uart_target_t target, uint8_t *byte, uint32_t timeout_ms);
static bool basic_uart_next_read_slice(uint32_t start_ms, uint32_t timeout_ms, uint32_t *slice_ms);
static uint16_t basic_uart_read_to_buffer(basic_uart_target_t target, uint8_t *buffer, uint16_t length,
                                          uint32_t timeout_ms);
static int basic_uart_read_to_array(struct mb_interpreter_t *s, void **l, basic_uart_target_t target, void *array,
                                    uint32_t length, uint32_t timeout_ms, uint32_t *read_count);
static int basic_uart_set_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint32_t index,
                                     uint8_t byte);
static void basic_uart_flush_target(basic_uart_target_t target);
static void basic_uart_release_array_value(struct mb_interpreter_t *s, mb_value_t value);
static void basic_uart_feed_heartbeat(void);
static int basic_uart_push_sized_string(struct mb_interpreter_t *s, void **l, const char *value, uint16_t length);
static int_t basic_uart_size_to_int(size_t value);
static int_t basic_uart_u32_to_int(uint32_t value);
static uint32_t basic_uart_timeout_from_int(int_t timeout_ms);

ErrorStatus basic_uart_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "UART_WRITE", basic_uart_write);
  result |= mb_register_func(interpreter, "UART_WRITE_BYTES", basic_uart_write_bytes);
  result |= mb_register_func(interpreter, "UART_READ", basic_uart_read);
  result |= mb_register_func(interpreter, "UART_READ_BYTES", basic_uart_read_bytes);
  result |= mb_register_func(interpreter, "UART_BAUD", basic_uart_baud);
  result |= mb_register_func(interpreter, "UART_SET_BAUD", basic_uart_set_baud);
  result |= mb_register_func(interpreter, "UART_FLUSH", basic_uart_flush_func);
  result |= mb_register_func(interpreter, "RS485_WRITE", basic_rs485_write);
  result |= mb_register_func(interpreter, "RS485_WRITE_BYTES", basic_rs485_write_bytes);
  result |= mb_register_func(interpreter, "RS485_READ", basic_rs485_read);
  result |= mb_register_func(interpreter, "RS485_READ_BYTES", basic_rs485_read_bytes);
  result |= mb_register_func(interpreter, "RS485_BAUD", basic_rs485_baud);
  result |= mb_register_func(interpreter, "RS485_SET_BAUD", basic_rs485_set_baud);
  result |= mb_register_func(interpreter, "RS485_FLUSH", basic_rs485_flush);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_uart_write(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_write_after_target(s, l, target);
}

static int basic_uart_write_bytes(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_write_bytes_after_target(s, l, target);
}

static int basic_uart_read(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_read_after_target(s, l, target);
}

static int basic_uart_read_bytes(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_read_bytes_after_target(s, l, target);
}

static int basic_uart_baud(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_baud_after_target(s, l, target);
}

static int basic_uart_set_baud(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_set_baud_after_target(s, l, target);
}

static int basic_uart_flush_func(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  basic_uart_target_t target = {0};
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_uart_pop_target(s, l, &target));
  return basic_uart_flush_after_target(s, l, target);
}

static int basic_rs485_write(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_write_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_write_bytes(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_write_bytes_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_read(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_read_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_read_bytes(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_read_bytes_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_baud(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_baud_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_set_baud(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_set_baud_after_target(s, l, basic_uart_rs485_target());
}

static int basic_rs485_flush(struct mb_interpreter_t *s, void **l) {
  basic_uart_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  return basic_uart_flush_after_target(s, l, basic_uart_rs485_target());
}

static int basic_uart_pop_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t *target) {
  mb_value_t port_value;
  mb_make_nil(port_value);

  int result = mb_pop_value(s, l, &port_value);
  if (result != MB_FUNC_OK) {
    return result;
  }

  bool ok = basic_uart_target_from_value(port_value, target);
  basic_uart_release_array_value(s, port_value);
  return ok ? MB_FUNC_OK : MB_FUNC_ERR;
}

static int basic_uart_write_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  char *text = NULL;
  int_t timeout_arg = (int_t)BASIC_UART_DEFAULT_TIMEOUT_MS;

  mb_check(mb_pop_string(s, l, &text));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &timeout_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  uint32_t timeout_ms = basic_uart_timeout_from_int(timeout_arg);
  size_t written = basic_uart_write_contiguous(target, (const uint8_t *)text, strlen(text), timeout_ms);
  basic_uart_feed_heartbeat();
  return mb_push_int(s, l, basic_uart_size_to_int(written));
}

static int basic_uart_write_bytes_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  mb_value_t array_value;
  mb_make_nil(array_value);
  int_t length_arg = 0;
  int_t timeout_arg = (int_t)BASIC_UART_DEFAULT_TIMEOUT_MS;
  int result = MB_FUNC_OK;
  uint32_t written = 0U;

  result = mb_pop_value(s, l, &array_value);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  result = mb_pop_int(s, l, &length_arg);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  if (mb_has_arg(s, l)) {
    result = mb_pop_int(s, l, &timeout_arg);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  if (array_value.type == MB_DT_ARRAY && array_value.value.array != NULL && length_arg > 0) {
    uint32_t capacity = 0U;
    if (basic_uart_array_capacity(s, l, array_value, &capacity)) {
      uint32_t length = (uint32_t)length_arg;
      if (length > capacity) {
        length = capacity;
      }
      result = basic_uart_write_array(s, l, target, array_value.value.array, length,
                                      basic_uart_timeout_from_int(timeout_arg), &written);
      if (result != MB_FUNC_OK) {
        goto exit;
      }
    }
  }

  result = mb_push_int(s, l, basic_uart_u32_to_int(written));

exit:
  basic_uart_release_array_value(s, array_value);
  return result;
}

static int basic_uart_read_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  int_t length_arg = 0;
  int_t timeout_arg = (int_t)BASIC_UART_DEFAULT_TIMEOUT_MS;
  uint8_t buffer[BASIC_UART_TEXT_READ_MAX + 1U] = {0};

  mb_check(mb_pop_int(s, l, &length_arg));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &timeout_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (length_arg <= 0) {
    return basic_uart_push_sized_string(s, l, "", 0U);
  }

  uint16_t length =
    length_arg > (int_t)BASIC_UART_TEXT_READ_MAX ? BASIC_UART_TEXT_READ_MAX : (uint16_t)length_arg;

  uint16_t read_count =
    basic_uart_read_to_buffer(target, buffer, length, basic_uart_timeout_from_int(timeout_arg));
  buffer[read_count] = '\0';
  basic_uart_feed_heartbeat();
  return basic_uart_push_sized_string(s, l, (const char *)buffer, read_count);
}

static int basic_uart_read_bytes_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  mb_value_t array_value;
  mb_make_nil(array_value);
  int_t length_arg = 0;
  int_t timeout_arg = (int_t)BASIC_UART_DEFAULT_TIMEOUT_MS;
  int result = MB_FUNC_OK;
  uint32_t read_count = 0U;

  result = mb_pop_value(s, l, &array_value);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  result = mb_pop_int(s, l, &length_arg);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  if (mb_has_arg(s, l)) {
    result = mb_pop_int(s, l, &timeout_arg);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  if (array_value.type == MB_DT_ARRAY && array_value.value.array != NULL && length_arg > 0) {
    uint32_t capacity = 0U;
    if (basic_uart_array_capacity(s, l, array_value, &capacity)) {
      uint32_t length = (uint32_t)length_arg;
      if (length > capacity) {
        length = capacity;
      }
      result = basic_uart_read_to_array(s, l, target, array_value.value.array, length,
                                        basic_uart_timeout_from_int(timeout_arg), &read_count);
      if (result != MB_FUNC_OK) {
        goto exit;
      }
    }
  }

  result = mb_push_int(s, l, basic_uart_u32_to_int(read_count));

exit:
  basic_uart_release_array_value(s, array_value);
  return result;
}

static int basic_uart_baud_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, basic_uart_u32_to_int(basic_uart_get_baud_rate(target)));
}

static int basic_uart_set_baud_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  int_t baud_arg = 0;

  mb_check(mb_pop_int(s, l, &baud_arg));
  mb_check(mb_attempt_close_bracket(s, l));

  if (baud_arg <= 0) {
    return mb_push_int(s, l, 0);
  }

  HAL_StatusTypeDef status = basic_uart_configure_baud_rate(target, (uint32_t)baud_arg);
  basic_uart_feed_heartbeat();
  return mb_push_int(s, l, status == HAL_OK ? 1 : 0);
}

static int basic_uart_flush_after_target(struct mb_interpreter_t *s, void **l, basic_uart_target_t target) {
  mb_check(mb_attempt_close_bracket(s, l));
  basic_uart_flush_target(target);
  basic_uart_feed_heartbeat();
  return mb_push_int(s, l, 1);
}

static bool basic_uart_target_from_value(mb_value_t value, basic_uart_target_t *target) {
  if (target == NULL) {
    return false;
  }

  basic_uart_kind_t kind = BASIC_UART_KIND_NONE;
  switch (value.type) {
  case MB_DT_INT:
    if (!basic_uart_kind_from_port_id(value.value.integer, &kind)) {
      return false;
    }
    break;
  case MB_DT_REAL: {
    int_t port_id = (int_t)value.value.float_point;
    if ((real_t)port_id != value.value.float_point || !basic_uart_kind_from_port_id(port_id, &kind)) {
      return false;
    }
    break;
  }
  case MB_DT_STRING:
    if (!basic_uart_kind_from_name(value.value.string, &kind)) {
      return false;
    }
    break;
  default:
    return false;
  }

  *target = basic_uart_target_from_kind(kind);
  return target->handle != NULL;
}

static bool basic_uart_kind_from_port_id(int_t port_id, basic_uart_kind_t *kind) {
  if (kind == NULL) {
    return false;
  }

  switch (port_id) {
  case 1:
    *kind = BASIC_UART_KIND_RS485;
    return true;
  case 2:
    *kind = BASIC_UART_KIND_DEBUG;
    return true;
  case 4:
    *kind = BASIC_UART_KIND_AIR724;
    return true;
  case 5:
    *kind = BASIC_UART_KIND_RS232;
    return true;
  default:
    return false;
  }
}

static bool basic_uart_kind_from_name(const char *name, basic_uart_kind_t *kind) {
  if (name == NULL || kind == NULL) {
    return false;
  }

  char *end = NULL;
  long numeric_port = strtol(name, &end, 10);
  if (end != name && *end == '\0') {
    return basic_uart_kind_from_port_id((int_t)numeric_port, kind);
  }

  if (basic_uart_name_equals(name, "RS485") || basic_uart_name_equals(name, "USART1") ||
      basic_uart_name_equals(name, "UART1")) {
    *kind = BASIC_UART_KIND_RS485;
    return true;
  }
  if (basic_uart_name_equals(name, "DEBUG") || basic_uart_name_equals(name, "USART2") ||
      basic_uart_name_equals(name, "UART2")) {
    *kind = BASIC_UART_KIND_DEBUG;
    return true;
  }
  if (basic_uart_name_equals(name, "AIR724") || basic_uart_name_equals(name, "UART4") ||
      basic_uart_name_equals(name, "USART4")) {
    *kind = BASIC_UART_KIND_AIR724;
    return true;
  }
  if (basic_uart_name_equals(name, "RS232") || basic_uart_name_equals(name, "UART5") ||
      basic_uart_name_equals(name, "USART5")) {
    *kind = BASIC_UART_KIND_RS232;
    return true;
  }

  return false;
}

static bool basic_uart_name_equals(const char *left, const char *right) {
  if (left == NULL || right == NULL) {
    return false;
  }

  while (*left != '\0' && *right != '\0') {
    if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
      return false;
    }
    left++;
    right++;
  }
  return *left == '\0' && *right == '\0';
}

static basic_uart_target_t basic_uart_target_from_kind(basic_uart_kind_t kind) {
  basic_uart_target_t target = {
    .kind = kind,
    .handle = NULL,
  };

  switch (kind) {
  case BASIC_UART_KIND_RS485:
    target.handle = BSP_RS485_UART_HANDLE;
    break;
  case BASIC_UART_KIND_DEBUG:
    target.handle = BSP_DEBUG_UART_HANDLE;
    break;
  case BASIC_UART_KIND_AIR724:
    target.handle = BSP_AIR724_UART_HANDLE;
    break;
  case BASIC_UART_KIND_RS232:
    target.handle = BSP_RS232_UART_HANDLE;
    break;
  default:
    break;
  }
  return target;
}

static basic_uart_target_t basic_uart_rs485_target(void) {
  return basic_uart_target_from_kind(BASIC_UART_KIND_RS485);
}

static uint32_t basic_uart_get_baud_rate(basic_uart_target_t target) {
  if (target.kind == BASIC_UART_KIND_RS485) {
    return bsp_rs485_get_baud_rate();
  }
  if (target.handle == NULL) {
    return 0U;
  }
  return target.handle->Init.BaudRate;
}

static HAL_StatusTypeDef basic_uart_configure_baud_rate(basic_uart_target_t target, uint32_t baud_rate) {
  if (baud_rate == 0U || target.handle == NULL) {
    return HAL_ERROR;
  }
  if (target.kind == BASIC_UART_KIND_RS485) {
    return bsp_rs485_set_baud_rate(baud_rate);
  }
  return bsp_uart_configure_8n1(target.handle, baud_rate);
}

static void basic_uart_begin_write(basic_uart_target_t target) {
  if (target.kind == BASIC_UART_KIND_RS485) {
    bsp_rs485_set_transmit();
    bsp_delay_us(50U);
  }
}

static HAL_StatusTypeDef basic_uart_end_write(basic_uart_target_t target, HAL_StatusTypeDef status) {
  if (target.kind == BASIC_UART_KIND_RS485) {
    if (status == HAL_OK) {
      uint32_t start_ms = HAL_GetTick();
      while (__HAL_UART_GET_FLAG(target.handle, UART_FLAG_TC) == RESET) {
        basic_uart_feed_heartbeat();
        if ((HAL_GetTick() - start_ms) >= BASIC_UART_READ_SLICE_MS) {
          status = HAL_TIMEOUT;
          break;
        }
      }
    }
    bsp_rs485_set_receive();
  }
  return status;
}

static HAL_StatusTypeDef basic_uart_write_chunk(basic_uart_target_t target, const uint8_t *data, uint16_t length,
                                                uint32_t timeout_ms) {
  if (target.handle == NULL) {
    return HAL_ERROR;
  }
  return bsp_uart_write(target.handle, data, length, timeout_ms);
}

static uint32_t basic_uart_remaining_timeout(uint32_t start_ms, uint32_t timeout_ms) {
  if (timeout_ms == HAL_MAX_DELAY) {
    return HAL_MAX_DELAY;
  }

  uint32_t elapsed_ms = HAL_GetTick() - start_ms;
  if (elapsed_ms >= timeout_ms) {
    return 0U;
  }
  return timeout_ms - elapsed_ms;
}

static size_t basic_uart_write_contiguous(basic_uart_target_t target, const uint8_t *data, size_t length,
                                          uint32_t timeout_ms) {
  if (target.handle == NULL || data == NULL || length == 0U) {
    return 0U;
  }

  size_t written = 0U;
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t start_ms = HAL_GetTick();
  basic_uart_begin_write(target);
  while (written < length) {
    uint32_t chunk_timeout = basic_uart_remaining_timeout(start_ms, timeout_ms);
    if (chunk_timeout == 0U && timeout_ms != 0U && timeout_ms != HAL_MAX_DELAY) {
      status = HAL_TIMEOUT;
      break;
    }

    size_t remaining = length - written;
    uint16_t chunk_length =
      remaining > BASIC_UART_TX_CHUNK_SIZE ? BASIC_UART_TX_CHUNK_SIZE : (uint16_t)remaining;
    basic_uart_feed_heartbeat();
    status = basic_uart_write_chunk(target, &data[written], chunk_length, chunk_timeout);
    if (status != HAL_OK) {
      break;
    }
    written += chunk_length;
  }
  (void)basic_uart_end_write(target, status);
  return written;
}

static int basic_uart_write_array(struct mb_interpreter_t *s, void **l, basic_uart_target_t target, void *array,
                                  uint32_t length, uint32_t timeout_ms, uint32_t *written) {
  uint8_t chunk[BASIC_UART_TX_CHUNK_SIZE];
  bool valid = false;
  int result = MB_FUNC_OK;
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t start_ms = HAL_GetTick();
  uint32_t offset = 0U;

  if (written != NULL) {
    *written = 0U;
  }
  if (array == NULL || written == NULL || length == 0U) {
    return MB_FUNC_OK;
  }

  result = basic_uart_validate_array(s, l, array, length, &valid);
  if (result != MB_FUNC_OK || !valid) {
    return result;
  }

  basic_uart_begin_write(target);
  while (offset < length) {
    uint32_t chunk_timeout = basic_uart_remaining_timeout(start_ms, timeout_ms);
    if (chunk_timeout == 0U && timeout_ms != 0U && timeout_ms != HAL_MAX_DELAY) {
      status = HAL_TIMEOUT;
      break;
    }

    uint32_t remaining = length - offset;
    uint16_t chunk_length =
      remaining > BASIC_UART_TX_CHUNK_SIZE ? BASIC_UART_TX_CHUNK_SIZE : (uint16_t)remaining;
    result = basic_uart_array_to_chunk(s, l, array, offset, chunk, chunk_length);
    if (result != MB_FUNC_OK) {
      status = HAL_ERROR;
      break;
    }

    basic_uart_feed_heartbeat();
    status = basic_uart_write_chunk(target, chunk, chunk_length, chunk_timeout);
    if (status != HAL_OK) {
      break;
    }
    offset += chunk_length;
    *written = offset;
  }
  (void)basic_uart_end_write(target, status);
  return result;
}

static int basic_uart_validate_array(struct mb_interpreter_t *s, void **l, void *array, uint32_t length, bool *valid) {
  if (valid == NULL) {
    return MB_FUNC_ERR;
  }

  *valid = false;
  for (uint32_t i = 0U; i < length; i++) {
    int index = (int)i;
    mb_value_t value;
    uint8_t byte = 0U;
    mb_make_nil(value);
    int result = mb_get_array_elem(s, l, array, &index, 1, &value);
    if (result != MB_FUNC_OK) {
      return result;
    }
    if (!basic_uart_value_to_byte(value, &byte)) {
      return MB_FUNC_OK;
    }
  }

  *valid = true;
  return MB_FUNC_OK;
}

static int basic_uart_array_to_chunk(struct mb_interpreter_t *s, void **l, void *array, uint32_t offset,
                                     uint8_t *chunk, uint16_t length) {
  if (array == NULL || chunk == NULL) {
    return MB_FUNC_ERR;
  }

  for (uint16_t i = 0U; i < length; i++) {
    int index = (int)(offset + i);
    mb_value_t value;
    mb_make_nil(value);
    int result = mb_get_array_elem(s, l, array, &index, 1, &value);
    if (result != MB_FUNC_OK) {
      return result;
    }
    if (!basic_uart_value_to_byte(value, &chunk[i])) {
      return MB_FUNC_ERR;
    }
  }
  return MB_FUNC_OK;
}

static bool basic_uart_value_to_byte(mb_value_t value, uint8_t *byte) {
  int_t integer = 0;
  if (byte == NULL) {
    return false;
  }

  switch (value.type) {
  case MB_DT_INT:
    integer = value.value.integer;
    break;
  case MB_DT_REAL:
    integer = (int_t)value.value.float_point;
    if ((real_t)integer != value.value.float_point) {
      return false;
    }
    break;
  default:
    return false;
  }

  if (integer < 0 || integer > 255) {
    return false;
  }

  *byte = (uint8_t)integer;
  return true;
}

static bool basic_uart_array_capacity(struct mb_interpreter_t *s, void **l, mb_value_t array_value,
                                      uint32_t *capacity) {
  if (capacity == NULL || array_value.type != MB_DT_ARRAY || array_value.value.array == NULL) {
    return false;
  }

  int length = 0;
  if (mb_get_array_len(s, l, array_value.value.array, 0, &length) != MB_FUNC_OK || length <= 0) {
    return false;
  }
  *capacity = (uint32_t)length;
  return true;
}

static HAL_StatusTypeDef basic_uart_read_one(basic_uart_target_t target, uint8_t *byte, uint32_t timeout_ms) {
  if (target.handle == NULL || byte == NULL) {
    return HAL_ERROR;
  }
  if (target.kind == BASIC_UART_KIND_RS485) {
    return bsp_rs485_read(byte, 1U, timeout_ms);
  }
  return bsp_uart_read(target.handle, byte, 1U, timeout_ms);
}

static bool basic_uart_next_read_slice(uint32_t start_ms, uint32_t timeout_ms, uint32_t *slice_ms) {
  if (slice_ms == NULL) {
    return false;
  }

  if (timeout_ms == 0U) {
    *slice_ms = 0U;
    return true;
  }

  if (timeout_ms == HAL_MAX_DELAY) {
    *slice_ms = BASIC_UART_READ_SLICE_MS;
    return true;
  }

  uint32_t elapsed_ms = HAL_GetTick() - start_ms;
  if (elapsed_ms >= timeout_ms) {
    return false;
  }

  uint32_t remaining_ms = timeout_ms - elapsed_ms;
  *slice_ms = remaining_ms > BASIC_UART_READ_SLICE_MS ? BASIC_UART_READ_SLICE_MS : remaining_ms;
  return true;
}

static uint16_t basic_uart_read_to_buffer(basic_uart_target_t target, uint8_t *buffer, uint16_t length,
                                          uint32_t timeout_ms) {
  if (target.handle == NULL || buffer == NULL || length == 0U) {
    return 0U;
  }

  uint16_t read_count = 0U;
  uint32_t start_ms = HAL_GetTick();
  while (read_count < length) {
    uint32_t slice_ms = 0U;
    if (!basic_uart_next_read_slice(start_ms, timeout_ms, &slice_ms)) {
      break;
    }

    basic_uart_feed_heartbeat();
    HAL_StatusTypeDef status = basic_uart_read_one(target, &buffer[read_count], slice_ms);
    if (status == HAL_OK) {
      read_count++;
      continue;
    }
    if (status == HAL_TIMEOUT && timeout_ms != 0U) {
      continue;
    }
    break;
  }

  return read_count;
}

static int basic_uart_read_to_array(struct mb_interpreter_t *s, void **l, basic_uart_target_t target, void *array,
                                    uint32_t length, uint32_t timeout_ms, uint32_t *read_count) {
  if (read_count == NULL) {
    return MB_FUNC_ERR;
  }
  *read_count = 0U;
  if (target.handle == NULL || array == NULL || length == 0U) {
    return MB_FUNC_OK;
  }

  uint32_t start_ms = HAL_GetTick();
  while (*read_count < length) {
    uint32_t slice_ms = 0U;
    uint8_t byte = 0U;
    if (!basic_uart_next_read_slice(start_ms, timeout_ms, &slice_ms)) {
      break;
    }

    basic_uart_feed_heartbeat();
    HAL_StatusTypeDef status = basic_uart_read_one(target, &byte, slice_ms);
    if (status == HAL_OK) {
      int result = basic_uart_set_array_byte(s, l, array, *read_count, byte);
      if (result != MB_FUNC_OK) {
        return result;
      }
      (*read_count)++;
      continue;
    }
    if (status == HAL_TIMEOUT && timeout_ms != 0U) {
      continue;
    }
    break;
  }

  return MB_FUNC_OK;
}

static int basic_uart_set_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint32_t index,
                                     uint8_t byte) {
  int array_index = (int)index;
  mb_value_t value;
  mb_make_int(value, (int_t)byte);
  return mb_set_array_elem(s, l, array, &array_index, 1, value);
}

static void basic_uart_flush_target(basic_uart_target_t target) {
  if (target.handle == NULL) {
    return;
  }
  if (target.kind == BASIC_UART_KIND_RS485) {
    bsp_rs485_set_receive();
  }
  bsp_uart_flush_rx(target.handle);
}

static void basic_uart_release_array_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_ARRAY) {
    (void)mb_dispose_value(s, value);
  }
}

static void basic_uart_feed_heartbeat(void) {
  (void)bsp_watchdog_refresh();
  app_rtos_mark_heartbeat(APP_RTOS_HEARTBEAT_BASIC, HAL_GetTick());
}

static int basic_uart_push_sized_string(struct mb_interpreter_t *s, void **l, const char *value, uint16_t length) {
  const char empty[] = "";
  const char *safe_value = value == NULL ? empty : value;
  if (value == NULL) {
    length = 0U;
  }
  char *copy = mb_memdup(safe_value, (unsigned)length + 1U);
  if (copy == NULL) {
    return MB_FUNC_ERR;
  }
  copy[length] = '\0';
  return mb_push_string(s, l, copy);
}

static int_t basic_uart_size_to_int(size_t value) {
  return value > (size_t)INT_MAX ? (int_t)INT_MAX : (int_t)value;
}

static int_t basic_uart_u32_to_int(uint32_t value) {
  return value > (uint32_t)INT_MAX ? (int_t)INT_MAX : (int_t)value;
}

static uint32_t basic_uart_timeout_from_int(int_t timeout_ms) {
  return timeout_ms < 0 ? 0U : (uint32_t)timeout_ms;
}
