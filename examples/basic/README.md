# BASIC Examples

本目录放可以写入 EEPROM 脚本槽的 `.bas` 示例。
当前固件默认从 EEPROM 里的 `app01.bas` / `app02.bas` 读取脚本；这里的示例文件用于编写和演示脚本内容。

## MQTT 函数

- `MQTT_CONNECT()`：连接服务器。
- `MQTT_DISCONNECT()`：断开连接。
- `MQTT_CONNECTED()`：返回是否已建立 MQTT 会话。
- `MQTT_MAINTAIN(reconnect_ms)`：轮询 MQTT 并按需重连/保活，返回 1 表示已连接；业务 topic 仍由脚本订阅。
- `MQTT_READY()`：返回 MQTT 是否已经完成连接和订阅。
- `MQTT_PING()`：主动发送 ping。
- `MQTT_BUILD_TOPIC(prefix$)`：返回 `prefix$ + mqtt.user_name`，适合平台按采集器 ID 结尾的 topic。
- `MQTT_SUBSCRIBE(topic$)`：订阅完整 topic。
- `MQTT_PUBLISH(topic$, payload$)`：向完整 topic 发布 payload。
- `MQTT_POLL()`：接收一次 MQTT 数据并返回待处理消息数量。
- `MQTT_RECV()`：弹出一条下行消息，返回 1 表示成功。
- `MQTT_RECV_TOPIC()` / `MQTT_RECV_PAYLOAD()`：读取最近一次 `MQTT_RECV()` 弹出的 topic 和 payload。
- `MQTT_MESSAGE_COUNT()`：返回当前收件箱待处理消息数量。
- `MQTT_OVERFLOW()`：返回因收件箱满而丢弃的消息数量。
- `BASIC_DELAY(ms)` / `BASIC_TICKS()`：脚本延时和系统 tick 辅助函数。

## UART / RS485 函数

- `UART_OPEN(port, type$)`：按 CubeMX 生成的 UART 句柄打开端口，`port` 取 `1`/`2`/`4`/`5`，`type$` 取 `"UART"`、`"RS485"` 或 `"RS232"`，返回可传递的句柄。
- `UART_BAUD(port_handle)` / `UART_SET_BAUD(port_handle, baud)`：读取或按 8N1 重新配置 UART 波特率。
- `UART_WRITE(port_handle, text$, timeout_ms)` / `UART_READ(port_handle, len, timeout_ms)`：文本收发；`UART_READ` 单次最多返回 256 字节。
- `UART_WRITE_BYTES(port_handle, buf, len, timeout_ms)` / `UART_READ_BYTES(port_handle, buf, len, timeout_ms)`：一维数值数组收发原始字节，数组元素范围为 0-255，返回实际字节数。
- `UART_FLUSH(port_handle)`：清掉对应 UART 的接收残留。

## MODBUS 函数

- `MODBUS_READ_COILS(port_handle, slave, reg, count, buf[, timeout_ms])`：`buf` 按 `count` 个 0/1 元素展开，返回位数。
- `MODBUS_READ_DISCRETE_INPUTS(port_handle, slave, reg, count, buf[, timeout_ms])`：`buf` 按 `count` 个 0/1 元素展开，返回位数。
- `MODBUS_READ_HOLD_REGS(port_handle, slave, reg, count, buf[, timeout_ms])`：`buf` 每个寄存器占 2 字节，返回寄存器个数。
- `MODBUS_READ_INPUT_REGS(port_handle, slave, reg, count, buf[, timeout_ms])`：`buf` 每个寄存器占 2 字节，返回寄存器个数。
- `MODBUS_WRITE_COIL(port_handle, slave, reg, value[, timeout_ms])`
- `MODBUS_WRITE_REG(port_handle, slave, reg, value[, timeout_ms])`
- `MODBUS_WRITE_COILS(port_handle, slave, reg, count, buf[, timeout_ms])`：`buf` 传 `count` 个 0/1 元素。
- `MODBUS_WRITE_REGS(port_handle, slave, reg, count, buf[, timeout_ms])`
- `MODBUS_ERROR()` / `MODBUS_LAST_ERROR()`：读取最近一次 Modbus 错误码。

`AIR724` 只是挂在 `UART4` 上的外设名称，不是 BASIC 端口类型；如果脚本要通过它做串口收发，也还是先 `UART_OPEN(4, "UART")` 再使用句柄。
当前板级可用组合是 `1/RS485`、`2/UART`、`4/UART`、`5/RS232`。

示例：

```basic
LET rs485 = UART_OPEN(1, "RS485")
DIM regs(3)
IF MODBUS_READ_HOLD_REGS(rs485, 1, 0, 2, regs) THEN
  PRINT regs(0); ","; regs(1); ","; regs(2); ","; regs(3)
END IF
```
