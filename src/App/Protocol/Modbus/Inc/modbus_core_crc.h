#ifndef PROCOTOCOLBUILDPROJ_MODBUS_CORE_CRC_H
#define PROCOTOCOLBUILDPROJ_MODBUS_CORE_CRC_H

#include "modbus_core_define.h"

uint16_t Modbus_Crc_Compute(const uint8_t *buf, uint16_t bufLen);

uint16_t GetCRCData(uint8_t *buffer, uint16_t len);

#endif // PROCOTOCOLBUILDPROJ_MODBUS_CORE_CRC_H
