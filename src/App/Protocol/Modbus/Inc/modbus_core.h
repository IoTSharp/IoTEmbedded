#ifndef PROCOTOCOLBUILDPROJ_MODBUS_CORE_H
#define PROCOTOCOLBUILDPROJ_MODBUS_CORE_H

#include <string.h>
#include <stdbool.h>
#include "modbus_core_define.h"
#include "modbus_core_crc.h"
#include "modbus_core_master.h"
// #include "modbus_core_slave.h"

extern SwSendDataCallBack modbusSendCallBack;
extern SwRecDataCallBack modbusRecCallBack;

// 注册发送数据回调
void Modbus_RegistrySendCallBack(SwSendDataCallBack sendDataCallBack);
// 注册结束数据回调
void Modbus_RegistryRecCallBack(SwRecDataCallBack recDataCallBack);

/**
 * @brief 将bool型指针转字节数组
 * @param dst 存储转换的字节数据
 * @param data bool数据
 * @param dataLen 数据长度
 * @return
 */
uint16_t PackBoolArray(uint8_t *dst, const bool *data, uint16_t dataLen);

/**
 * @brief bool类型数组转字节数组
 * @param dst 存储
 * @param cnt 转换的个数
 * @param data 字节数据
 * @param dataLen 数据长度
 */
void UnPackBool(bool *dst, uint16_t cnt, const uint8_t *data, uint16_t dataLen);

#endif // PROCOTOCOLBUILDPROJ_MODBUS_CORE_H
