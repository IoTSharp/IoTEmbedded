#include "Protocol/Modbus/Inc/modbus_core.h"
#include "Board/Inc/bsp_board.h"
#include "Protocol/Modbus/Inc/modbus_api.h"

#if APP_ENABLE_CMSIS_RTOS
#include "cmsis_os2.h"
#endif

/* 从站响应等待的基准时间。实际等待会叠加当前波特率下的回包传输时间，
 * 避免电力仪等长帧在 9600bps 下被 100ms 固定窗口截断成 CRC 错。 */
#define MODBUS_WAIT_ECHO_TIME (180U)
#define MODBUS_INTER_FRAME_GAP_MS (5U)
#define MODBUS_POLL_STEP_MS (2U)
#define MODBUS_RTU_BITS_PER_CHAR (11U)

// 错误指令响应长度
#define ERROR_RESP_DATA_LEN   (5)
// 错误响应码下标位置
#define ERROR_RESP_CODE_IDX   (2)
#define MODBUS_PACKET_BUF_LEN (110)
#define MODBUS_WRITE_RESP_DATA_LEN (8U)

// modbus 包缓存数组
static uint8_t modbus_packet_buf[MODBUS_PACKET_BUF_LEN] = {0};
#if APP_ENABLE_CMSIS_RTOS
static osMutexId_t modbus_master_lock;
#endif

static uint8_t core_build_packet(uint8_t slaveAddr, uint16_t regAddr, uint16_t val, uint8_t funcCode, uint8_t *retBuf);
static void modbus_set_error(uint8_t *errorCode, uint8_t value);
static bool modbus_lock_transaction(uint8_t *errorCode);
static void modbus_unlock_transaction(void);
static bool modbus_prepare_transaction(uint8_t *errorCode);
static bool modbus_send_request(uint8_t packetLen, uint16_t wait_ms, uint16_t expectedLen, uint16_t *recLen,
                                uint8_t *errorCode);
static bool modbus_receive_response(uint16_t wait_ms, uint16_t expectedLen, uint16_t *recLen, uint8_t *errorCode);
static uint16_t modbus_response_timeout_ms(uint16_t wait_ms, uint16_t expectedLen);
static void modbus_wait_for_response(uint16_t wait_ms, uint16_t expectedLen);
static bool modbus_finish_write_response(uint16_t recLen, uint8_t *errorCode);
static bool modbus_copy_read_response(uint16_t recLen, uint8_t *retData, uint16_t expectedDataLen, uint8_t *errorCode);

void Modbus_MasterInit(void) {
#if APP_ENABLE_CMSIS_RTOS
  if (modbus_master_lock == NULL) {
    const osMutexAttr_t attr = {
      .name = "modbus_master",
      .attr_bits = osMutexRecursive | osMutexPrioInherit,
    };
    modbus_master_lock = osMutexNew(&attr);
  }
#endif
}

/**
 * 从机地址(1)-功能码(1)-寄存器地址(2)--数据(2)-CRC(2)
 * @brief 写一个寄存器
 * @param slaveAddr 从机地址
 * @param regAddr 寄存器地址
 * @param val 值
 * @param errorCode 错误码
 * @return
 */
bool Master_WriteOneRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t val, uint8_t *errorCode) {
  uint16_t recLen = 0U;
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  uint8_t idx = core_build_packet(slaveAddr, regAddr, val, WriteOneReg, modbus_packet_buf);
  ok = modbus_send_request(idx, MODBUS_WAIT_ECHO_TIME, MODBUS_WRITE_RESP_DATA_LEN, &recLen, errorCode) &&
       modbus_finish_write_response(recLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

/**
 * 从机地址(1)-功能码(1)-寄存器地址(2)-寄存器个数(2)-字节数(1)-数据(n)-CRC(2)
 * @brief  串口协议包(0x10) 写多个寄存器
 * @param slaveAddr 从机地址
 * @param regAddr 寄存器地址
 * @param data 数据（长度为2的倍数）
 * @param dataLen 数据长度()
 * @param errorCode 错误码
 * @return
 */
bool Master_WriteMulRegister(uint8_t slaveAddr, uint16_t regAddr, uint8_t *data, uint16_t dataLen, uint8_t *errorCode) {
  uint8_t idx = 0U;
  uint16_t recLen = 0U;
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (data == NULL || dataLen > (MODBUS_PACKET_BUF_LEN - 9U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  modbus_packet_buf[idx++] = slaveAddr;
  modbus_packet_buf[idx++] = WriteMulReg;
  modbus_packet_buf[idx++] = (uint8_t)(regAddr >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(regAddr);
  uint16_t regNum = (uint16_t)(dataLen / 2U);
  modbus_packet_buf[idx++] = (uint8_t)(regNum >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(regNum);
  modbus_packet_buf[idx++] = (uint8_t)(dataLen);
  for (uint16_t i = 0U; i < dataLen; ++i) {
    modbus_packet_buf[idx++] = data[i];
  }
  uint16_t crc = GetCRCData(modbus_packet_buf, idx);
  modbus_packet_buf[idx++] = (uint8_t)(crc >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(crc);
  ok = modbus_send_request(idx, MODBUS_WAIT_ECHO_TIME, MODBUS_WRITE_RESP_DATA_LEN, &recLen, errorCode) &&
       modbus_finish_write_response(recLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

/**
 * 从机地址(1)-功能码(1)-寄存器地址(2)--数据(2)-CRC(2)
 * @brief 写一个线圈
 * @param slaveAddr 从机地址
 * @param regAddr 寄存器地址
 * @param val 值
 * @param errorCode 错误码
 * @return
 */
bool Master_WriteOneCoil(uint8_t slaveAddr, uint16_t regAddr, bool val, uint8_t *errorCode) {
  uint16_t data = val ? 0xFF00 : 0x0000;
  uint16_t recLen = 0U;
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  uint8_t idx = core_build_packet(slaveAddr, regAddr, data, WriteOneCoil, modbus_packet_buf);
  ok = modbus_send_request(idx, MODBUS_WAIT_ECHO_TIME, MODBUS_WRITE_RESP_DATA_LEN, &recLen, errorCode) &&
       modbus_finish_write_response(recLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

/**
 * @brief 写多个线圈
 * @param slaveAddr
 * @param regAddr
 * @param val
 * @param dataLen
 * @param errorCode
 * @return
 */
bool Master_WriteMulCoil(uint8_t slaveAddr, uint16_t regAddr, bool *val, uint16_t dataLen, uint8_t *errorCode) {
  uint8_t idx = 0U;
  uint16_t recLen = 0U;
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (val == NULL || ((dataLen + 7U) / 8U) > (MODBUS_PACKET_BUF_LEN - 9U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  // 计算数据占几个字节
  uint16_t realDataLen = PackBoolArray(modbus_packet_buf + 7, val, dataLen);
  modbus_packet_buf[idx++] = slaveAddr;
  modbus_packet_buf[idx++] = WriteMulCoilsReg;
  modbus_packet_buf[idx++] = (uint8_t)(regAddr >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(regAddr);
  modbus_packet_buf[idx++] = (uint8_t)(dataLen >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(dataLen);
  modbus_packet_buf[idx++] = (uint8_t)(realDataLen);
  idx += (uint8_t)realDataLen;
  uint16_t crc = GetCRCData(modbus_packet_buf, idx);
  modbus_packet_buf[idx++] = (uint8_t)(crc >> 8);
  modbus_packet_buf[idx++] = (uint8_t)(crc);
  ok = modbus_send_request(idx, MODBUS_WAIT_ECHO_TIME, MODBUS_WRITE_RESP_DATA_LEN, &recLen, errorCode) &&
       modbus_finish_write_response(recLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

/**
 * @brief 读保持寄存器
 * @param slaveAddr
 * @param regAddr
 * @param regNum
 * @param retData
 * @param errorCode
 * @return
 */
bool Master_ReadHoldRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData,
                              uint8_t *errorCode) {
  uint16_t recLen = 0U;
  uint16_t expectedDataLen = (uint16_t)(regNum * 2U);
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (expectedDataLen > (MODBUS_PACKET_BUF_LEN - 5U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  uint8_t packetLen = core_build_packet(slaveAddr, regAddr, regNum, ReadHoldReg, modbus_packet_buf);
  ok = modbus_send_request(packetLen, MODBUS_WAIT_ECHO_TIME, (uint16_t)(expectedDataLen + 5U), &recLen, errorCode) &&
       modbus_copy_read_response(recLen, retData, expectedDataLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

/**
 * @brief 读输入寄存器
 * @param slaveAddr
 * @param regAddr
 * @param regNum
 * @param retData
 * @param errorCode
 * @return
 */
bool Master_ReadInputRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData,
                               uint8_t *errorCode) {
  uint16_t recLen = 0U;
  uint16_t expectedDataLen = (uint16_t)(regNum * 2U);
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (expectedDataLen > (MODBUS_PACKET_BUF_LEN - 5U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  uint8_t packetLen = core_build_packet(slaveAddr, regAddr, regNum, ReadInputReg, modbus_packet_buf);
  ok = modbus_send_request(packetLen, MODBUS_WAIT_ECHO_TIME, (uint16_t)(expectedDataLen + 5U), &recLen, errorCode) &&
       modbus_copy_read_response(recLen, retData, expectedDataLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

bool Master_ReadCoilStatus(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData, uint8_t *errorCode) {
  uint16_t recLen = 0U;
  uint16_t wait_ms = 0U;
  uint16_t expectedDataLen = (uint16_t)(regNum * 2U);
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (expectedDataLen > (MODBUS_PACKET_BUF_LEN - 5U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  uint8_t packetLen = core_build_packet(slaveAddr, regAddr, regNum, ReadHoldReg, modbus_packet_buf);
  /* 旧工程这里虽然命名为 ReadCoilStatus，但实际按 0x03 保持寄存器读取。
   * 目前空调、烟感、UPS 等适配器依赖这个兼容行为，不能直接改成功能码 0x01。 */
  wait_ms = MODBUS_WAIT_ECHO_TIME;
  if (0x0D < regNum) {
    wait_ms = (uint16_t)(wait_ms + (5U * regNum));
  }
  ok = modbus_send_request(packetLen, wait_ms, (uint16_t)(expectedDataLen + 5U), &recLen, errorCode) &&
       modbus_copy_read_response(recLen, retData, expectedDataLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

bool Master_ReadInputStatus(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData,
                            uint8_t *errorCode) {
  uint16_t recLen = 0U;
  uint16_t expectedDataLen = (uint16_t)((regNum + 7U) / 8U);
  bool ok = false;
  if (!modbus_lock_transaction(errorCode)) {
    return false;
  }
  if (expectedDataLen > (MODBUS_PACKET_BUF_LEN - 5U)) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    modbus_unlock_transaction();
    return false;
  }
  uint8_t packetLen = core_build_packet(slaveAddr, regAddr, regNum, ReadInputState, modbus_packet_buf);
  ok = modbus_send_request(packetLen, MODBUS_WAIT_ECHO_TIME, (uint16_t)(expectedDataLen + 5U), &recLen, errorCode) &&
       modbus_copy_read_response(recLen, retData, expectedDataLen, errorCode);
  modbus_unlock_transaction();
  return ok;
}

static uint8_t core_build_packet(uint8_t slaveAddr, uint16_t regAddr, uint16_t val, uint8_t funcCode, uint8_t *retBuf) {
  uint8_t idx = 0;
  retBuf[idx++] = slaveAddr;
  retBuf[idx++] = funcCode;
  retBuf[idx++] = (uint8_t)(regAddr >> 8);
  retBuf[idx++] = (uint8_t)(regAddr);
  retBuf[idx++] = (uint8_t)(val >> 8);
  retBuf[idx++] = (uint8_t)(val);
  uint16_t crc = GetCRCData(retBuf, idx);
  retBuf[idx++] = (uint8_t)(crc >> 8);
  retBuf[idx++] = (uint8_t)(crc);
  return idx;
}

static void modbus_set_error(uint8_t *errorCode, uint8_t value) {
  if (errorCode != NULL) {
    *errorCode = value;
  }
}

static bool modbus_lock_transaction(uint8_t *errorCode) {
#if APP_ENABLE_CMSIS_RTOS
  if (modbus_master_lock != NULL && osKernelGetState() == osKernelRunning) {
    if (osMutexAcquire(modbus_master_lock, osWaitForever) != osOK) {
      modbus_set_error(errorCode, ModbusFrameErrorCode);
      return false;
    }
  }
#endif
  return true;
}

static void modbus_unlock_transaction(void) {
#if APP_ENABLE_CMSIS_RTOS
  if (modbus_master_lock != NULL && osKernelGetState() == osKernelRunning) {
    (void)osMutexRelease(modbus_master_lock);
  }
#endif
}

static bool modbus_prepare_transaction(uint8_t *errorCode) {
  if (modbusSendCallBack == NULL || modbusRecCallBack == NULL) {
    modbus_set_error(errorCode, ModbusCallbackMissingErrorCode);
    return false;
  }
  modbus_set_error(errorCode, 0U);
  return true;
}

static bool modbus_send_request(uint8_t packetLen, uint16_t wait_ms, uint16_t expectedLen, uint16_t *recLen,
                                uint8_t *errorCode) {
  if (!modbus_prepare_transaction(errorCode)) {
    return false;
  }
  /*
   * 事务加锁后仍保留 RTU 帧间静默时间，并在发送前清掉旧缓存。
   * 平台命令线程、主轮询线程、串口命令都走这里，避免相互抢占同一条 485 总线。
   */
  bsp_delay_ms(MODBUS_INTER_FRAME_GAP_MS);
  clear_modbus_buf();
  modbusSendCallBack(modbus_packet_buf, packetLen);
  return modbus_receive_response(wait_ms, expectedLen, recLen, errorCode);
}

static bool modbus_receive_response(uint16_t wait_ms, uint16_t expectedLen, uint16_t *recLen, uint8_t *errorCode) {
  uint16_t local_rec_len = 0U;
  uint16_t rtn_crc = 0U;

  if (recLen == NULL) {
    modbus_set_error(errorCode, ModbusFrameErrorCode);
    return false;
  }

  /*
   * 先按期望长度/波特率等待完整回包，再清接收缓存并取出当前帧。失败路径必须写明错误码：
   * 0x00 在上层代表成功，不能再用来表示“没填错误码”。
   */
  modbus_wait_for_response(wait_ms, expectedLen);
  memset(modbus_packet_buf, 0x00, sizeof(modbus_packet_buf));
  local_rec_len = modbusRecCallBack(modbus_packet_buf);
  *recLen = local_rec_len;

  if (local_rec_len == 0U) {
    modbus_set_error(errorCode, ModbusNoResponseErrorCode);
    return false;
  }
  if (local_rec_len <= 2U || local_rec_len > MODBUS_PACKET_BUF_LEN) {
    modbus_set_error(errorCode, RecDataLenErrorCode);
    return false;
  }

  rtn_crc = (uint16_t)(((uint16_t)modbus_packet_buf[local_rec_len - 2U] << 8) | modbus_packet_buf[local_rec_len - 1U]);
  if (GetCRCData(modbus_packet_buf, (uint16_t)(local_rec_len - 2U)) != rtn_crc) {
    modbus_set_error(errorCode, ModbusCrcErrorCode);
    return false;
  }

  return true;
}

static uint16_t modbus_response_timeout_ms(uint16_t wait_ms, uint16_t expectedLen) {
  uint32_t baud_rate = BSP_RS485_UART_HANDLE->Init.BaudRate;
  uint32_t frame_time_ms = 0U;
  uint32_t timeout_ms = wait_ms;

  if (baud_rate == 0U) {
    baud_rate = 9600U;
  }
  if (expectedLen > 0U) {
    frame_time_ms = (((uint32_t)expectedLen * MODBUS_RTU_BITS_PER_CHAR * 1000U) + baud_rate - 1U) / baud_rate;
  }
  timeout_ms += frame_time_ms + MODBUS_INTER_FRAME_GAP_MS;
  return timeout_ms > 0xFFFFU ? 0xFFFFU : (uint16_t)timeout_ms;
}

static void modbus_wait_for_response(uint16_t wait_ms, uint16_t expectedLen) {
  uint32_t start_ms = bsp_get_tick_ms();
  uint16_t timeout_ms = modbus_response_timeout_ms(wait_ms, expectedLen);
  for (;;) {
    uint16_t rx_len = modbus_rx_length();
    if (expectedLen > 0U && rx_len >= expectedLen) {
      break;
    }
    if ((bsp_get_tick_ms() - start_ms) >= timeout_ms) {
      break;
    }
    bsp_delay_ms(MODBUS_POLL_STEP_MS);
  }
}

static bool modbus_finish_write_response(uint16_t recLen, uint8_t *errorCode) {
  if (recLen == ERROR_RESP_DATA_LEN) {
    modbus_set_error(errorCode, modbus_packet_buf[ERROR_RESP_CODE_IDX]);
    return false;
  }
  if (recLen != MODBUS_WRITE_RESP_DATA_LEN) {
    modbus_set_error(errorCode, ModbusFrameErrorCode);
    return false;
  }
  modbus_set_error(errorCode, 0U);
  return true;
}

static bool modbus_copy_read_response(uint16_t recLen, uint8_t *retData, uint16_t expectedDataLen, uint8_t *errorCode) {
  uint8_t cpyDataLen = 0U;
  if (recLen == ERROR_RESP_DATA_LEN) {
    modbus_set_error(errorCode, modbus_packet_buf[ERROR_RESP_CODE_IDX]);
    return false;
  }
  if (retData == NULL || recLen < 5U) {
    modbus_set_error(errorCode, ModbusFrameErrorCode);
    return false;
  }

  cpyDataLen = modbus_packet_buf[2];
  if (cpyDataLen != expectedDataLen || recLen != (uint16_t)(cpyDataLen + 5U)) {
    modbus_set_error(errorCode, ModbusFrameErrorCode);
    return false;
  }

  memcpy(retData, modbus_packet_buf + 3, cpyDataLen);
  modbus_set_error(errorCode, 0U);
  return true;
}
