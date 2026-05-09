#ifndef PROCOTOCOLBUILDPROJ_MODBUS_CORE_MASTER_H
#define PROCOTOCOLBUILDPROJ_MODBUS_CORE_MASTER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// 初始化主站事务锁；RTOS 内核初始化后调用，用于串行化所有 RS485/Modbus 访问。
void Modbus_MasterInit(void);

/**
 * @brief 06
 * @param slaveAddr
 * @param regAddr
 * @param val
 * @param errorCode 错误码，根据返回状态，如果正确,错误码不生效
 * @return
 */
bool Master_WriteOneRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t val, uint8_t *errorCode);

/**
 * @brief 10
 * @param slaveAddr
 * @param regAddr
 * @param data
 * @param dataLen
 * @param errorCode
 * @return
 */
bool Master_WriteMulRegister(uint8_t slaveAddr, uint16_t regAddr, uint8_t *data, uint16_t dataLen, uint8_t *errorCode);

/**
 * @brief 05
 * @param slaveAddr
 * @param regAddr
 * @param val
 * @param errorCode
 * @return
 */
bool Master_WriteOneCoil(uint8_t slaveAddr, uint16_t regAddr, bool val, uint8_t *errorCode);

/**
 * @brief 0F
 * @param slaveAddr
 * @param regAddr
 * @param val
 * @param dataLen
 * @param errorCode
 * @return
 */
bool Master_WriteMulCoil(uint8_t slaveAddr, uint16_t regAddr, bool *val, uint16_t dataLen, uint8_t *errorCode);

/**
 * @brief 读保持寄存器
 * @param slaveAddr
 * @param regAddr
 * @param regNum
 * @param retData 返回的数据（数据长度为寄存器个数*2）
 * @param errorCode
 * @return
 */
bool Master_ReadHoldRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData,
                              uint8_t *errorCode);

/**
 * @brief 读输入寄存器
 * @param slaveAddr
 * @param regAddr
 * @param regNum
 * @param retData 返回的数据（数据长度为寄存器个数*2）
 * @param errorCode
 * @return
 */
bool Master_ReadInputRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData,
                               uint8_t *errorCode);

/**
 * @brief 读线圈状态
 * @param slaveAddr
 * @param regAddr
 * @param regNum 寄存器个数
 * @param retData 返回的数据（数据长度为寄存器个数）
 * @param errorCode
 * @return
 */
bool Master_ReadCoilStatus(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData, uint8_t *errorCode);

/**
 * @brief 读输入状态
 * @param slaveAddr
 * @param regAddr
 * @param regNum 寄存器个数
 * @param retData 返回的数据（数据长度为寄存器个数）
 * @param errorCode
 * @return
 */
bool Master_ReadInputStatus(uint8_t slaveAddr, uint16_t regAddr, uint16_t regNum, uint8_t *retData, uint8_t *errorCode);

#endif // PROCOTOCOLBUILDPROJ_MODBUS_CORE_MASTER_H
