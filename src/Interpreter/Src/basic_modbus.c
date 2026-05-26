#include "Interpreter/Inc/basic_modbus.h"

#include "Board/Inc/bsp_board.h"
#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Interpreter/Inc/basic.h"
#include "Interpreter/Inc/basic_serial.h"
#include "Protocol/Modbus/Inc/modbus_core_crc.h"
#include "Protocol/Modbus/Inc/modbus_core_define.h"
#include "Protocol/Modbus/Inc/modbus_core_master.h"

#include "Board/Inc/bsp_hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BASIC_MODBUS_FRAME_MAX       110U
#define BASIC_MODBUS_READ_DATA_MAX   (BASIC_MODBUS_FRAME_MAX - 5U)
#define BASIC_MODBUS_WRITE_DATA_MAX  (BASIC_MODBUS_FRAME_MAX - 9U)
#define BASIC_MODBUS_DEFAULT_WAIT_MS 180U
#define BASIC_MODBUS_FRAME_GAP_MS    5U
#define BASIC_MODBUS_RTU_BITS_CHAR   11U

static uint8_t basic_modbus_last_error;

static int basic_modbus_read_coils(struct mb_interpreter_t *s, void **l);
static int basic_modbus_read_discrete_inputs(struct mb_interpreter_t *s, void **l);
static int basic_modbus_read_holding_registers(struct mb_interpreter_t *s, void **l);
static int basic_modbus_read_input_registers(struct mb_interpreter_t *s, void **l);
static int basic_modbus_write_coil(struct mb_interpreter_t *s, void **l);
static int basic_modbus_write_register(struct mb_interpreter_t *s, void **l);
static int basic_modbus_write_coils(struct mb_interpreter_t *s, void **l);
static int basic_modbus_write_registers(struct mb_interpreter_t *s, void **l);
static int basic_modbus_error(struct mb_interpreter_t *s, void **l);

static int basic_modbus_read_bits_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code);
static int basic_modbus_read_registers_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code);
static int basic_modbus_write_single_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code);
static int basic_modbus_write_multiple_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code);
static int basic_modbus_pop_port(struct mb_interpreter_t *s, void **l, basic_serial_port_t *port);
static bool basic_modbus_pop_u8(struct mb_interpreter_t *s, void **l, uint8_t min_value, uint8_t max_value,
                                uint8_t *value);
static bool basic_modbus_pop_u16(struct mb_interpreter_t *s, void **l, uint16_t min_value, uint16_t max_value,
                                 uint16_t *value);
static bool basic_modbus_array_capacity(struct mb_interpreter_t *s, void **l, mb_value_t array_value,
                                        uint16_t *capacity);
static int basic_modbus_unpack_bits_to_array(struct mb_interpreter_t *s, void **l, void *array,
                                             const uint8_t *buffer, uint16_t bit_count);
static int basic_modbus_pack_bits_from_array(struct mb_interpreter_t *s, void **l, void *array, uint16_t bit_count,
                                             uint8_t *buffer, uint16_t buffer_capacity, uint16_t *byte_count);
static int basic_modbus_copy_array_to_buffer(struct mb_interpreter_t *s, void **l, void *array, uint16_t length,
                                             uint8_t *buffer);
static int basic_modbus_copy_buffer_to_array(struct mb_interpreter_t *s, void **l, void *array,
                                             const uint8_t *buffer, uint16_t length);
static int basic_modbus_get_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint16_t index,
                                       uint8_t *byte);
static int basic_modbus_set_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint16_t index,
                                       uint8_t byte);
static bool basic_modbus_value_to_byte(mb_value_t value, uint8_t *byte);
static bool basic_modbus_value_to_bool(mb_value_t value, bool *bit);
static void basic_modbus_release_value(struct mb_interpreter_t *s, mb_value_t value);
static uint8_t basic_modbus_build_simple_request(uint8_t *request, uint8_t slave_addr, uint8_t func_code,
                                                 uint16_t reg_addr, uint16_t value);
static void basic_modbus_append_crc(uint8_t *buffer, uint8_t *length);
static bool basic_modbus_exchange(basic_serial_port_t port, const uint8_t *request, uint8_t request_len,
                                  uint16_t expected_len, uint8_t *response, uint16_t *response_len,
                                  uint32_t wait_ms);
static uint32_t basic_modbus_response_timeout_ms(basic_serial_port_t port, uint32_t wait_ms,
                                                 uint16_t expected_len);
static void basic_modbus_prepare_port(basic_serial_port_t port, bool *restart_rs485_rx);
static void basic_modbus_finish_port(bool restart_rs485_rx);
static bool basic_modbus_validate_response(const uint8_t *response, uint16_t response_len, uint8_t slave_addr,
                                           uint8_t func_code);
static bool basic_modbus_finish_write_response(const uint8_t *request, const uint8_t *response,
                                               uint16_t response_len, uint8_t slave_addr, uint8_t func_code);
static void basic_modbus_set_error(uint8_t error);

ErrorStatus basic_modbus_register(struct mb_interpreter_t *interpreter) {
  if (interpreter == NULL) {
    return ERROR;
  }

  basic_modbus_last_error = 0U;
  int result = MB_FUNC_OK;
  result |= mb_register_func(interpreter, "MODBUS_READ_COILS", basic_modbus_read_coils);
  result |= mb_register_func(interpreter, "MODBUS_READ_DISCRETE_INPUTS", basic_modbus_read_discrete_inputs);
  result |= mb_register_func(interpreter, "MODBUS_READ_HOLD_REGS", basic_modbus_read_holding_registers);
  result |= mb_register_func(interpreter, "MODBUS_READ_HOLDING_REGISTERS", basic_modbus_read_holding_registers);
  result |= mb_register_func(interpreter, "MODBUS_READ_INPUT_REGS", basic_modbus_read_input_registers);
  result |= mb_register_func(interpreter, "MODBUS_READ_INPUT_REGISTERS", basic_modbus_read_input_registers);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_COIL", basic_modbus_write_coil);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_REG", basic_modbus_write_register);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_REGISTER", basic_modbus_write_register);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_COILS", basic_modbus_write_coils);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_REGS", basic_modbus_write_registers);
  result |= mb_register_func(interpreter, "MODBUS_WRITE_REGISTERS", basic_modbus_write_registers);
  result |= mb_register_func(interpreter, "MODBUS_ERROR", basic_modbus_error);
  result |= mb_register_func(interpreter, "MODBUS_LAST_ERROR", basic_modbus_error);
  return result == MB_FUNC_OK ? SUCCESS : ERROR;
}

static int basic_modbus_read_coils(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_read_bits_after_func(s, l, ReadCoilState);
}

static int basic_modbus_read_discrete_inputs(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_read_bits_after_func(s, l, ReadInputState);
}

static int basic_modbus_read_holding_registers(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_read_registers_after_func(s, l, ReadHoldReg);
}

static int basic_modbus_read_input_registers(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_read_registers_after_func(s, l, ReadInputReg);
}

static int basic_modbus_write_coil(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_write_single_after_func(s, l, WriteOneCoil);
}

static int basic_modbus_write_register(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_write_single_after_func(s, l, WriteOneReg);
}

static int basic_modbus_write_coils(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_write_multiple_after_func(s, l, WriteMulCoilsReg);
}

static int basic_modbus_write_registers(struct mb_interpreter_t *s, void **l) {
  return basic_modbus_write_multiple_after_func(s, l, WriteMulReg);
}

static int basic_modbus_error(struct mb_interpreter_t *s, void **l) {
  basic_serial_feed_heartbeat();
  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));
  return mb_push_int(s, l, (int_t)basic_modbus_last_error);
}

static int basic_modbus_read_bits_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code) {
  basic_serial_feed_heartbeat();
  basic_serial_port_t port = {0};
  uint8_t slave_addr = 0U;
  uint16_t reg_addr = 0U;
  uint16_t bit_count = 0U;
  mb_value_t array_value;
  mb_make_nil(array_value);
  int_t wait_arg = (int_t)BASIC_MODBUS_DEFAULT_WAIT_MS;
  uint8_t request[8U] = {0};
  uint8_t response[BASIC_MODBUS_FRAME_MAX] = {0};
  uint16_t response_len = 0U;
  int result = MB_FUNC_OK;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_modbus_pop_port(s, l, &port));
  if (!basic_modbus_pop_u8(s, l, 1U, 247U, &slave_addr) ||
      !basic_modbus_pop_u16(s, l, 0U, UINT16_MAX, &reg_addr) ||
      !basic_modbus_pop_u16(s, l, 1U, (uint16_t)(BASIC_MODBUS_READ_DATA_MAX * 8U), &bit_count)) {
    return MB_FUNC_ERR;
  }
  result = mb_pop_value(s, l, &array_value);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  if (mb_has_arg(s, l)) {
    result = mb_pop_int(s, l, &wait_arg);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  uint16_t capacity = 0U;
  if (!basic_serial_port_is_modbus_capable(port) || !basic_modbus_array_capacity(s, l, array_value, &capacity) ||
      capacity < bit_count) {
    basic_modbus_set_error(InvalidDataErrorCode);
    result = mb_push_int(s, l, 0);
    goto exit;
  }

  uint16_t data_len = (uint16_t)((bit_count + 7U) / 8U);
  uint8_t request_len = basic_modbus_build_simple_request(request, slave_addr, func_code, reg_addr, bit_count);
  if (!basic_modbus_exchange(port, request, request_len, (uint16_t)(data_len + 5U), response, &response_len,
                             basic_serial_timeout_from_int(wait_arg)) ||
      !basic_modbus_validate_response(response, response_len, slave_addr, func_code)) {
    result = mb_push_int(s, l, 0);
    goto exit;
  }
  if (response[2] != data_len || response_len != (uint16_t)(data_len + 5U)) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    result = mb_push_int(s, l, 0);
    goto exit;
  }

  result = basic_modbus_unpack_bits_to_array(s, l, array_value.value.array, response + 3U, bit_count);
  if (result == MB_FUNC_OK) {
    basic_modbus_set_error(0U);
    result = mb_push_int(s, l, (int_t)bit_count);
  }

exit:
  basic_modbus_release_value(s, array_value);
  return result;
}

static int basic_modbus_read_registers_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code) {
  basic_serial_feed_heartbeat();
  basic_serial_port_t port = {0};
  uint8_t slave_addr = 0U;
  uint16_t reg_addr = 0U;
  uint16_t reg_count = 0U;
  mb_value_t array_value;
  mb_make_nil(array_value);
  int_t wait_arg = (int_t)BASIC_MODBUS_DEFAULT_WAIT_MS;
  uint8_t request[8U] = {0};
  uint8_t response[BASIC_MODBUS_FRAME_MAX] = {0};
  uint16_t response_len = 0U;
  int result = MB_FUNC_OK;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_modbus_pop_port(s, l, &port));
  if (!basic_modbus_pop_u8(s, l, 1U, 247U, &slave_addr) ||
      !basic_modbus_pop_u16(s, l, 0U, UINT16_MAX, &reg_addr) ||
      !basic_modbus_pop_u16(s, l, 1U, (uint16_t)(BASIC_MODBUS_READ_DATA_MAX / 2U), &reg_count)) {
    return MB_FUNC_ERR;
  }
  result = mb_pop_value(s, l, &array_value);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  if (mb_has_arg(s, l)) {
    result = mb_pop_int(s, l, &wait_arg);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  uint16_t data_len = (uint16_t)(reg_count * 2U);
  uint16_t capacity = 0U;
  if (!basic_serial_port_is_modbus_capable(port) || !basic_modbus_array_capacity(s, l, array_value, &capacity) ||
      capacity < data_len) {
    basic_modbus_set_error(InvalidDataErrorCode);
    result = mb_push_int(s, l, 0);
    goto exit;
  }

  uint8_t request_len = basic_modbus_build_simple_request(request, slave_addr, func_code, reg_addr, reg_count);
  if (!basic_modbus_exchange(port, request, request_len, (uint16_t)(data_len + 5U), response, &response_len,
                             basic_serial_timeout_from_int(wait_arg)) ||
      !basic_modbus_validate_response(response, response_len, slave_addr, func_code)) {
    result = mb_push_int(s, l, 0);
    goto exit;
  }
  if (response[2] != data_len || response_len != (uint16_t)(data_len + 5U)) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    result = mb_push_int(s, l, 0);
    goto exit;
  }

  result = basic_modbus_copy_buffer_to_array(s, l, array_value.value.array, response + 3U, data_len);
  if (result == MB_FUNC_OK) {
    basic_modbus_set_error(0U);
    result = mb_push_int(s, l, (int_t)reg_count);
  }

exit:
  basic_modbus_release_value(s, array_value);
  return result;
}

static int basic_modbus_write_single_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code) {
  basic_serial_feed_heartbeat();
  basic_serial_port_t port = {0};
  uint8_t slave_addr = 0U;
  uint16_t reg_addr = 0U;
  uint16_t value = 0U;
  int_t raw_value = 0;
  int_t wait_arg = (int_t)BASIC_MODBUS_DEFAULT_WAIT_MS;
  uint8_t request[8U] = {0};
  uint8_t response[BASIC_MODBUS_FRAME_MAX] = {0};
  uint16_t response_len = 0U;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_modbus_pop_port(s, l, &port));
  if (!basic_modbus_pop_u8(s, l, 1U, 247U, &slave_addr) ||
      !basic_modbus_pop_u16(s, l, 0U, UINT16_MAX, &reg_addr)) {
    return MB_FUNC_ERR;
  }
  mb_check(mb_pop_int(s, l, &raw_value));
  if (mb_has_arg(s, l)) {
    mb_check(mb_pop_int(s, l, &wait_arg));
  }
  mb_check(mb_attempt_close_bracket(s, l));

  if (!basic_serial_port_is_modbus_capable(port)) {
    basic_modbus_set_error(InvalidDataErrorCode);
    return mb_push_int(s, l, 0);
  }
  if (func_code == WriteOneCoil) {
    value = raw_value != 0 ? 0xFF00U : 0x0000U;
  } else {
    if (raw_value < 0 || raw_value > (int_t)UINT16_MAX) {
      basic_modbus_set_error(InvalidDataErrorCode);
      return mb_push_int(s, l, 0);
    }
    value = (uint16_t)raw_value;
  }

  uint8_t request_len = basic_modbus_build_simple_request(request, slave_addr, func_code, reg_addr, value);
  bool ok = basic_modbus_exchange(port, request, request_len, 8U, response, &response_len,
                                  basic_serial_timeout_from_int(wait_arg)) &&
            basic_modbus_validate_response(response, response_len, slave_addr, func_code) &&
            basic_modbus_finish_write_response(request, response, response_len, slave_addr, func_code);
  return mb_push_int(s, l, ok ? 1 : 0);
}

static int basic_modbus_write_multiple_after_func(struct mb_interpreter_t *s, void **l, uint8_t func_code) {
  basic_serial_feed_heartbeat();
  basic_serial_port_t port = {0};
  uint8_t slave_addr = 0U;
  uint16_t reg_addr = 0U;
  uint16_t item_count = 0U;
  mb_value_t array_value;
  mb_make_nil(array_value);
  int_t wait_arg = (int_t)BASIC_MODBUS_DEFAULT_WAIT_MS;
  uint8_t request[BASIC_MODBUS_FRAME_MAX] = {0};
  uint8_t response[BASIC_MODBUS_FRAME_MAX] = {0};
  uint16_t response_len = 0U;
  int result = MB_FUNC_OK;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(basic_modbus_pop_port(s, l, &port));
  if (!basic_modbus_pop_u8(s, l, 1U, 247U, &slave_addr) ||
      !basic_modbus_pop_u16(s, l, 0U, UINT16_MAX, &reg_addr)) {
    return MB_FUNC_ERR;
  }
  if (func_code == WriteMulCoilsReg) {
    if (!basic_modbus_pop_u16(s, l, 1U, (uint16_t)(BASIC_MODBUS_WRITE_DATA_MAX * 8U), &item_count)) {
      return MB_FUNC_ERR;
    }
  } else if (!basic_modbus_pop_u16(s, l, 1U, (uint16_t)(BASIC_MODBUS_WRITE_DATA_MAX / 2U), &item_count)) {
    return MB_FUNC_ERR;
  }
  result = mb_pop_value(s, l, &array_value);
  if (result != MB_FUNC_OK) {
    goto exit;
  }
  if (mb_has_arg(s, l)) {
    result = mb_pop_int(s, l, &wait_arg);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
  }
  result = mb_attempt_close_bracket(s, l);
  if (result != MB_FUNC_OK) {
    goto exit;
  }

  uint16_t capacity = 0U;
  uint8_t request_len = 0U;
  uint16_t data_len = 0U;

  if (func_code == WriteMulCoilsReg) {
    uint8_t packed[BASIC_MODBUS_WRITE_DATA_MAX] = {0};
    if (!basic_serial_port_is_modbus_capable(port) ||
        !basic_modbus_array_capacity(s, l, array_value, &capacity) || capacity < item_count) {
      basic_modbus_set_error(InvalidDataErrorCode);
      result = mb_push_int(s, l, 0);
      goto exit;
    }

    result = basic_modbus_pack_bits_from_array(s, l, array_value.value.array, item_count, packed,
                                               (uint16_t)sizeof(packed), &data_len);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
    if (data_len > BASIC_MODBUS_WRITE_DATA_MAX) {
      basic_modbus_set_error(InvalidDataErrorCode);
      result = mb_push_int(s, l, 0);
      goto exit;
    }

    request[request_len++] = slave_addr;
    request[request_len++] = func_code;
    request[request_len++] = (uint8_t)(reg_addr >> 8);
    request[request_len++] = (uint8_t)reg_addr;
    request[request_len++] = (uint8_t)(item_count >> 8);
    request[request_len++] = (uint8_t)item_count;
    request[request_len++] = (uint8_t)data_len;
    memcpy(&request[request_len], packed, data_len);
    request_len = (uint8_t)(request_len + data_len);
    basic_modbus_append_crc(request, &request_len);
  } else {
    if (!basic_serial_port_is_modbus_capable(port) || !basic_modbus_array_capacity(s, l, array_value, &capacity) ||
        capacity < (uint16_t)(item_count * 2U)) {
      basic_modbus_set_error(InvalidDataErrorCode);
      result = mb_push_int(s, l, 0);
      goto exit;
    }

    data_len = (uint16_t)(item_count * 2U);
    if (data_len > BASIC_MODBUS_WRITE_DATA_MAX) {
      basic_modbus_set_error(InvalidDataErrorCode);
      result = mb_push_int(s, l, 0);
      goto exit;
    }

    request[request_len++] = slave_addr;
    request[request_len++] = func_code;
    request[request_len++] = (uint8_t)(reg_addr >> 8);
    request[request_len++] = (uint8_t)reg_addr;
    request[request_len++] = (uint8_t)(item_count >> 8);
    request[request_len++] = (uint8_t)item_count;
    request[request_len++] = (uint8_t)data_len;
    result = basic_modbus_copy_array_to_buffer(s, l, array_value.value.array, data_len, &request[request_len]);
    if (result != MB_FUNC_OK) {
      goto exit;
    }
    request_len = (uint8_t)(request_len + data_len);
    basic_modbus_append_crc(request, &request_len);
  }

  bool ok = basic_modbus_exchange(port, request, request_len, 8U, response, &response_len,
                                  basic_serial_timeout_from_int(wait_arg)) &&
            basic_modbus_validate_response(response, response_len, slave_addr, func_code) &&
            basic_modbus_finish_write_response(request, response, response_len, slave_addr, func_code);
  result = mb_push_int(s, l, ok ? 1 : 0);

exit:
  basic_modbus_release_value(s, array_value);
  return result;
}

static int basic_modbus_pop_port(struct mb_interpreter_t *s, void **l, basic_serial_port_t *port) {
  mb_value_t port_value;
  mb_make_nil(port_value);
  int result = mb_pop_value(s, l, &port_value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  result = basic_serial_port_from_value(port_value, port) ? MB_FUNC_OK : MB_FUNC_ERR;
  basic_modbus_release_value(s, port_value);
  return result;
}

static bool basic_modbus_pop_u8(struct mb_interpreter_t *s, void **l, uint8_t min_value, uint8_t max_value,
                                uint8_t *value) {
  int_t int_value = 0;
  if (value == NULL || mb_pop_int(s, l, &int_value) != MB_FUNC_OK || int_value < (int_t)min_value ||
      int_value > (int_t)max_value) {
    return false;
  }
  *value = (uint8_t)int_value;
  return true;
}

static bool basic_modbus_pop_u16(struct mb_interpreter_t *s, void **l, uint16_t min_value, uint16_t max_value,
                                 uint16_t *value) {
  int_t int_value = 0;
  if (value == NULL || mb_pop_int(s, l, &int_value) != MB_FUNC_OK || int_value < (int_t)min_value ||
      int_value > (int_t)max_value) {
    return false;
  }
  *value = (uint16_t)int_value;
  return true;
}

static bool basic_modbus_array_capacity(struct mb_interpreter_t *s, void **l, mb_value_t array_value,
                                        uint16_t *capacity) {
  if (capacity == NULL || array_value.type != MB_DT_ARRAY || array_value.value.array == NULL) {
    return false;
  }

  int length = 0;
  if (mb_get_array_len(s, l, array_value.value.array, 0, &length) != MB_FUNC_OK || length <= 0 ||
      length > (int)UINT16_MAX) {
    return false;
  }
  *capacity = (uint16_t)length;
  return true;
}

static int basic_modbus_unpack_bits_to_array(struct mb_interpreter_t *s, void **l, void *array,
                                             const uint8_t *buffer, uint16_t bit_count) {
  if (array == NULL || buffer == NULL) {
    return MB_FUNC_ERR;
  }

  for (uint16_t i = 0U; i < bit_count; i++) {
    uint8_t byte = buffer[i / 8U];
    uint8_t bit = (uint8_t)((byte >> (i % 8U)) & 0x01U);
    int result = basic_modbus_set_array_byte(s, l, array, i, bit);
    if (result != MB_FUNC_OK) {
      return result;
    }
  }
  return MB_FUNC_OK;
}

static int basic_modbus_pack_bits_from_array(struct mb_interpreter_t *s, void **l, void *array, uint16_t bit_count,
                                             uint8_t *buffer, uint16_t buffer_capacity, uint16_t *byte_count) {
  bool bits[BASIC_MODBUS_WRITE_DATA_MAX * 8U];
  uint16_t bytes_needed = (uint16_t)((bit_count + 7U) / 8U);
  if (byte_count != NULL) {
    *byte_count = 0U;
  }
  if (array == NULL || buffer == NULL || byte_count == NULL || bytes_needed > buffer_capacity) {
    return MB_FUNC_ERR;
  }

  if (bit_count > (uint16_t)(sizeof(bits) / sizeof(bits[0]))) {
    return MB_FUNC_ERR;
  }

  for (uint16_t i = 0U; i < bit_count; i++) {
    int array_index = (int)i;
    mb_value_t value;
    bool bit = false;
    mb_make_nil(value);
    int result = mb_get_array_elem(s, l, array, &array_index, 1, &value);
    if (result != MB_FUNC_OK) {
      return result;
    }
    if (!basic_modbus_value_to_bool(value, &bit)) {
      return MB_FUNC_ERR;
    }
    bits[i] = bit;
  }

  for (uint16_t i = 0U; i < bytes_needed; i++) {
    uint8_t packed = 0U;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      uint16_t idx = (uint16_t)(i * 8U + bit);
      if (idx >= bit_count) {
        break;
      }
      if (bits[idx]) {
        packed |= (uint8_t)(1U << bit);
      }
    }
    buffer[i] = packed;
  }

  *byte_count = bytes_needed;
  return MB_FUNC_OK;
}

static int basic_modbus_copy_array_to_buffer(struct mb_interpreter_t *s, void **l, void *array, uint16_t length,
                                             uint8_t *buffer) {
  if (array == NULL || buffer == NULL) {
    return MB_FUNC_ERR;
  }

  for (uint16_t i = 0U; i < length; i++) {
    int result = basic_modbus_get_array_byte(s, l, array, i, &buffer[i]);
    if (result != MB_FUNC_OK) {
      return result;
    }
  }
  return MB_FUNC_OK;
}

static int basic_modbus_copy_buffer_to_array(struct mb_interpreter_t *s, void **l, void *array,
                                             const uint8_t *buffer, uint16_t length) {
  if (array == NULL || buffer == NULL) {
    return MB_FUNC_ERR;
  }

  for (uint16_t i = 0U; i < length; i++) {
    int result = basic_modbus_set_array_byte(s, l, array, i, buffer[i]);
    if (result != MB_FUNC_OK) {
      return result;
    }
  }
  return MB_FUNC_OK;
}

static int basic_modbus_get_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint16_t index,
                                       uint8_t *byte) {
  int array_index = (int)index;
  mb_value_t value;
  mb_make_nil(value);
  int result = mb_get_array_elem(s, l, array, &array_index, 1, &value);
  if (result != MB_FUNC_OK) {
    return result;
  }
  return basic_modbus_value_to_byte(value, byte) ? MB_FUNC_OK : MB_FUNC_ERR;
}

static int basic_modbus_set_array_byte(struct mb_interpreter_t *s, void **l, void *array, uint16_t index,
                                       uint8_t byte) {
  int array_index = (int)index;
  mb_value_t value;
  mb_make_int(value, (int_t)byte);
  return mb_set_array_elem(s, l, array, &array_index, 1, value);
}

static bool basic_modbus_value_to_byte(mb_value_t value, uint8_t *byte) {
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

static bool basic_modbus_value_to_bool(mb_value_t value, bool *bit) {
  int_t integer = 0;
  if (bit == NULL) {
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

  *bit = integer != 0;
  return true;
}

static void basic_modbus_release_value(struct mb_interpreter_t *s, mb_value_t value) {
  if (value.type == MB_DT_ARRAY) {
    (void)mb_dispose_value(s, value);
  }
}

static uint8_t basic_modbus_build_simple_request(uint8_t *request, uint8_t slave_addr, uint8_t func_code,
                                                 uint16_t reg_addr, uint16_t value) {
  uint8_t length = 0U;
  request[length++] = slave_addr;
  request[length++] = func_code;
  request[length++] = (uint8_t)(reg_addr >> 8);
  request[length++] = (uint8_t)reg_addr;
  request[length++] = (uint8_t)(value >> 8);
  request[length++] = (uint8_t)value;
  basic_modbus_append_crc(request, &length);
  return length;
}

static void basic_modbus_append_crc(uint8_t *buffer, uint8_t *length) {
  uint16_t crc = GetCRCData(buffer, *length);
  buffer[(*length)++] = (uint8_t)(crc >> 8);
  buffer[(*length)++] = (uint8_t)crc;
}

static bool basic_modbus_exchange(basic_serial_port_t port, const uint8_t *request, uint8_t request_len,
                                  uint16_t expected_len, uint8_t *response, uint16_t *response_len,
                                  uint32_t wait_ms) {
  bool ok = false;
  bool restart_rs485_rx = false;
  uint8_t lock_error = 0U;
  if (request == NULL || request_len == 0U || response == NULL || response_len == NULL ||
      expected_len > BASIC_MODBUS_FRAME_MAX) {
    basic_modbus_set_error(InvalidDataErrorCode);
    return false;
  }

  *response_len = 0U;
  if (!Modbus_MasterLock(&lock_error)) {
    basic_modbus_set_error(lock_error);
    return false;
  }

  basic_modbus_prepare_port(port, &restart_rs485_rx);
  bsp_delay_ms(BASIC_MODBUS_FRAME_GAP_MS);
  size_t written = basic_serial_write_data(port, request, request_len, wait_ms);
  if (written != request_len) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    goto exit;
  }

  uint32_t response_timeout = basic_modbus_response_timeout_ms(port, wait_ms, expected_len);
  *response_len = basic_serial_read_data(port, response, expected_len, response_timeout);
  if (*response_len == 0U) {
    basic_modbus_set_error(ModbusNoResponseErrorCode);
    goto exit;
  }

  ok = true;

exit:
  basic_modbus_finish_port(restart_rs485_rx);
  Modbus_MasterUnlock();
  return ok;
}

static uint32_t basic_modbus_response_timeout_ms(basic_serial_port_t port, uint32_t wait_ms,
                                                 uint16_t expected_len) {
  uint32_t baud_rate = basic_serial_get_baud_rate(port);
  if (baud_rate == 0U) {
    baud_rate = 9600U;
  }
  uint32_t frame_ms =
    (((uint32_t)expected_len * BASIC_MODBUS_RTU_BITS_CHAR * 1000U) + baud_rate - 1U) / baud_rate;
  return wait_ms + frame_ms + BASIC_MODBUS_FRAME_GAP_MS;
}

static void basic_modbus_prepare_port(basic_serial_port_t port, bool *restart_rs485_rx) {
  if (restart_rs485_rx == NULL) {
    return;
  }
  *restart_rs485_rx = false;
  if (port.interface == BASIC_SERIAL_INTERFACE_RS485 && port.handle == BSP_RS485_UART_HANDLE) {
    (void)HAL_UART_AbortReceive_IT(port.handle);
    *restart_rs485_rx = true;
  }
  basic_serial_flush_port(port);
}

static void basic_modbus_finish_port(bool restart_rs485_rx) {
  if (restart_rs485_rx) {
    (void)bsp_rs485_start_receive_it();
  }
}

static bool basic_modbus_validate_response(const uint8_t *response, uint16_t response_len, uint8_t slave_addr,
                                           uint8_t func_code) {
  if (response == NULL || response_len < 5U || response_len > BASIC_MODBUS_FRAME_MAX) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    return false;
  }

  uint16_t response_crc = (uint16_t)(((uint16_t)response[response_len - 2U] << 8) | response[response_len - 1U]);
  if (GetCRCData((uint8_t *)response, (uint16_t)(response_len - 2U)) != response_crc) {
    basic_modbus_set_error(ModbusCrcErrorCode);
    return false;
  }
  if (response[0] != slave_addr) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    return false;
  }
  if (response[1] == (uint8_t)(func_code | 0x80U) && response_len == 5U) {
    basic_modbus_set_error(response[2]);
    return false;
  }
  if (response[1] != func_code) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    return false;
  }
  return true;
}

static bool basic_modbus_finish_write_response(const uint8_t *request, const uint8_t *response,
                                               uint16_t response_len, uint8_t slave_addr, uint8_t func_code) {
  if (request == NULL || response == NULL || response_len != 8U) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    return false;
  }
  if (response[0] != slave_addr || response[1] != func_code || memcmp(response, request, 6U) != 0) {
    basic_modbus_set_error(ModbusFrameErrorCode);
    return false;
  }
  basic_modbus_set_error(0U);
  return true;
}

static void basic_modbus_set_error(uint8_t error) {
  basic_modbus_last_error = error;
}
