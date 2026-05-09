#include "Protocol/Modbus/Inc/modbus_core_crc.h"

uint16_t GetCRCData(uint8_t *buffer, uint16_t len) {
  uint16_t wcrc = 0xFFFF; // 16位CRC寄存器预置
  uint8_t temp;
  uint16_t i = 0;
  uint8_t j = 0;            // 计数
  for (i = 0; i < len; i++) // 循环计算每个数据
  {
    temp = *buffer & 0x00FF; // 将八位数据与crc寄存器亦或
    buffer++;                // 指针地址增加，指向下个数据
    wcrc ^= temp;            // 将数据存入crc寄存器
    for (j = 0; j < 8; j++)  // 循环计算数据的
    {
      if (wcrc & 0x0001) // 判断右移出的是不是1，如果是1则与多项式进行异或。
      {
        wcrc >>= 1;     // 先将数据右移一位
        wcrc ^= 0xA001; // 与上面的多项式进行异或
      } else            // 如果不是1，则直接移除
      {
        wcrc >>= 1; // 直接移除
      }
    }
  }
  uint8_t CRC_L, CRC_H; // 定义数据高低位
  CRC_L = wcrc & 0xFF;  // CRC的低8位
  CRC_H = wcrc >> 8;    // CRC的高8位

  return ((CRC_L << 8) | CRC_H);
}
