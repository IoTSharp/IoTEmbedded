# BASIC MQTT Examples

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
