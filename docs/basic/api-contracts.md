# BASIC API Contracts

本文固化 IoTEmbedded BASIC 扩展函数的错误码、超时、句柄生命周期和内存所有权约定。机器可读清单见 `docs/basic/basic-api.v1.json`，其中 `functionSignatures` 是供 CodeGen 和 Profile 校验使用的逐函数 canonical 列表；运行时事实源见 `src/Interpreter/Src/app_basic_registry.c`。

## 统一规则

- 参数类型或数量不合法时，扩展函数返回 `MB_FUNC_ERR`，解释器按脚本错误处理。
- 可恢复的设备、链路、解析或配置失败不终止解释器，函数按各自返回类型返回 `0`、空字符串、`NIL`、默认值或部分字节数。
- 传入数组、字典、JSON handle、串口 handle 时，调用方仍保留脚本层变量；扩展函数只在函数执行期间读取或按契约原地写入。
- 返回的字符串、JSON handle、DICT、LIST、标量都由 MY-BASIC 管理，脚本不得也不能手工释放。
- 所有会等待外设的函数必须刷新 BASIC 任务心跳，避免看门狗误判。

## 错误码

### Modbus

`MODBUS_LAST_ERROR()` / `MODBUS_ERROR()` 返回最近一次 Modbus 错误码：

| 代码 | 名称 | 含义 |
| --- | --- | --- |
| `0x00` | OK | 最近一次 Modbus 操作成功 |
| `0x02` | InvalidDataErrorCode | 参数、数组容量、端口能力或从站返回非法数据 |
| `0xFF` | RecDataLenErrorCode | 接收长度不正确，保留兼容 |
| `0xF0` | ModbusCallbackMissingErrorCode | 总线回调缺失 |
| `0xF1` | ModbusNoResponseErrorCode | 超时未收到任何应答 |
| `0xF2` | ModbusFrameErrorCode | 帧结构、长度或回显不符合预期 |
| `0xF3` | ModbusCrcErrorCode | CRC 校验失败 |
| `0x01..0x0B` | Modbus exception | 从站返回的标准 Modbus 异常码 |

### MQTT

MQTT 不使用数字错误码。`MQTT_LAST_ERROR([handle])` 返回最近一次错误文本；成功操作会清空该文本。无效句柄、连接失败、QoS 超出 STM32 支持范围、配置写入失败都会返回 `0` 或 `NIL` 并写入错误文本。

### Serial / Config / JSON / Display

- SERIAL：设备失败返回 `0`、空字符串或部分字节数；当前没有 `SERIAL_LAST_ERROR`。
- CONFIG：未知 key、非法 value、apply/save 失败返回 `0` 或 `CONFIG_GET` 的默认字符串。
- JSON：解析失败返回 `NIL`；读不到路径时返回 `NIL`、默认值、`0`、空字符串或 `JSON_TYPE=-1`。
- DISPLAY：驱动不存在或操作不支持返回 `0`；Pandora 当前 `PAINT` 因无读像素/帧缓存返回 `0`。

## 超时

| 模块 | 约定 |
| --- | --- |
| SERIAL | `timeout_ms` 可选，默认 `1000 ms`；负数按 `0` 处理。读操作按短 slice 轮询直到读满或超时，写操作返回实际写出字节数。 |
| MODBUS | `wait_ms` 可选，默认 `180 ms`，实际响应等待时间会再叠加按波特率和预期帧长计算出的 RTU 传输时间。 |
| MQTT | `MQTT_RECEIVE(handle[, timeout_ms])` 默认 `0`，即非阻塞；等待时以 `50 ms` slice poll。连接、发布、订阅和断开沿底层网络配置与驱动时序。 |
| CONFIG | 无脚本层显式超时，落盘和网络切换由板级配置/网络驱动控制。 |
| DISPLAY | 无脚本层显式超时，由显示驱动同步执行。 |
| DELAY/SLEEP | 参数为等待毫秒数；负数按 `0` 处理，长等待会分片刷新心跳。 |

## 句柄生命周期

| 句柄 | 生命周期 |
| --- | --- |
| `serial_port` | `SERIAL_OPEN` 返回板级串口描述的 usertype 引用；它是借用的静态端口描述，不拥有硬件资源，解释器/profile reload 后脚本变量失效。 |
| Modbus | 当前 Modbus RTU API 不创建独立连接句柄，直接消费 `serial_port`。 |
| MQTT handle | `MQTT_CONNECT` 返回正整数句柄，`0` 表示失败。当前 STM32 profile 只有一个物理会话，再次 `MQTT_CONNECT` 会替换前一个会话并让旧句柄失效；`MQTT_DISCONNECT(handle)` 后句柄失效。 |
| JSON handle | JSON 构造、解析、`JSON_GET` 和 `JSON_AT` 返回 MY-BASIC 引用计数管理的 usertype。`JSON_GET` / `JSON_AT` 返回独立深拷贝。 |
| DISPLAY | `SCREEN(12)` 选择显示作为 PRINT 目标，`SCREEN(0)` 恢复默认输出；没有显式 display handle。 |

## 内存所有权

- `SERIAL_READ_BYTES`、Modbus 读函数会原地写入脚本传入的一维数组；数组容量不足时返回失败。
- `SERIAL_WRITE_BYTES`、Modbus 多写函数只读取脚本数组，元素必须是 `0..255` 的整数值。
- JSON 写入函数会把传入的源 JSON 深拷贝到目标树里；源 handle 仍归脚本/MY-BASIC 管理。
- `MQTT_RECEIVE` 返回的 DICT 至少包含 `topic`、`payload`、`qos`、`retain`，其中字符串为拷贝。
- 配置中的密码类 key 不应通过示例或 smoke 输出明文；脚本优先读 `*_password_set` 这类布尔状态。
