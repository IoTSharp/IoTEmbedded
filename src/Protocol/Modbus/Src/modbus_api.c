#include <stdio.h>
#include <string.h>
#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Protocol/Modbus/Inc/modbus_api.h"
#include "Protocol/Modbus/Inc/modbus_core.h"
modbus_data modbus;

static uint32_t modbus_enter_critical(void);
static void modbus_exit_critical(uint32_t primask);

/*获取缓冲数据*/
modbus_data *get_modbus(void) {
  return &modbus;
}

/*清空缓冲数据*/
void clear_modbus_buf(void) {
  uint32_t primask = modbus_enter_critical();
  memset(&modbus, 0x00, sizeof(modbus_data));
  modbus_exit_critical(primask);
}

// 发送数据回调函数指针
void modbus_send_func(void *data, uint16_t len) {
  (void)bsp_rs485_write((const uint8_t *)data, len, 1000U);
}
// 接收数据回调函数指针
uint16_t modbus_recv_func(void *recDataBuf) {
  uint16_t ret_len = 0;
  uint32_t primask = modbus_enter_critical();
  /*
   * USART1 接收在中断里逐字节追加。复制和清空必须是一个短临界区，
   * 否则主站取帧时刚好进来新字节，会把下一次事务的开头误清掉。
   */
  ret_len = modbus.len;
  memcpy(recDataBuf, modbus.buff, ret_len);
  memset(&modbus, 0x00, sizeof(modbus_data));
  modbus_exit_critical(primask);
  return ret_len;
}
/*注册回调函数*/
void init_modbus(void) {
  Modbus_RegistrySendCallBack(&modbus_send_func);

  Modbus_RegistryRecCallBack(&modbus_recv_func);
  clear_modbus_buf();
}

void modbus_rx_byte(uint8_t byte) {
  if (modbus.len < sizeof(modbus.buff)) {
    modbus.buff[modbus.len++] = byte;
  }
}

void modbus_rx_bytes(const uint8_t *data, uint16_t len) {
  if (data == NULL) {
    return;
  }
  for (uint16_t i = 0; i < len; i++) {
    modbus_rx_byte(data[i]);
  }
}

uint16_t modbus_rx_length(void) {
  uint32_t primask = modbus_enter_critical();
  uint16_t len = modbus.len;
  modbus_exit_critical(primask);
  return len;
}

static uint32_t modbus_enter_critical(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void modbus_exit_critical(uint32_t primask) {
  if (primask == 0U) {
    __enable_irq();
  }
}
