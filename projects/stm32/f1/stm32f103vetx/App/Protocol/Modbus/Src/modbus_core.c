#include "modbus_core.h"

// 发送数据回调指针
SwSendDataCallBack modbusSendCallBack = NULL;
// 接收数据回调指针
SwRecDataCallBack modbusRecCallBack = NULL;

/**
 * @brief 注册发送数据回调
 * @param sendDataCallBack
 */
void Modbus_RegistrySendCallBack(SwSendDataCallBack sendDataCallBack) {
  modbusSendCallBack = sendDataCallBack;
}

/**
 * @brief 注册接收数据回调
 * @param recDataCallBack
 */
void Modbus_RegistryRecCallBack(SwRecDataCallBack recDataCallBack) {
  modbusRecCallBack = recDataCallBack;
}

uint16_t PackBoolArray(uint8_t *dst, const bool *data, uint16_t dataLen) {
  uint8_t retLen = 0;
  uint8_t curDat = 0;
  uint8_t bitIdx = 0;
  for (int i = 0; i < dataLen; ++i) {
    if (data[i]) {
      curDat |= (uint8_t)(1 << bitIdx);
    }
    if (++bitIdx == 8) {
      dst[retLen++] = curDat;
      curDat = 0;
      bitIdx = 0;
    }
  }
  if (bitIdx != 8) {
    dst[retLen++] = curDat;
  }
  return retLen;
}

/**
 * @brief bool类型数组转字节数组
 * @param dst 存储
 * @param cnt 转换的个数
 * @param data 字节数据
 * @param dataLen 数据长度
 */
void UnPackBool(bool *dst, uint16_t cnt, const uint8_t *data, uint16_t dataLen) {
  uint8_t curDat = 0;
  for (int i = 0; i < dataLen; ++i) {
    curDat = data[i];

    for (int j = 0; j < 8; ++j) {
      if (cnt == 0) {
        return;
      };
      *dst++ = (curDat & (1 << j)) != 0;
      cnt--;
    }
  }
}
