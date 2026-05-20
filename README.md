# IoTEmbedded

IoTEmbedded 是 IoTSharp 生态的嵌入式 BASIC/C 运行时，覆盖 STM32、RTOS、bare-metal 和低资源 Linux。仓库早期名称为 `IoTEmBASIC`，现在统一使用 `IoTEmbedded`。

## 定位

本仓库参考 luaos 和 rt-thread 的分层思路，把系统拆成几层：

- `src/Core`：BASIC 解释器内核和公共运行时
- `src/App`：业务层，承接产品逻辑、设备编排、网络和协议流程
- `src/Bsp`：板级服务和外设封装
- `src/Common`：共享类型、日志、哈希和通用工具
- `src/Config`：运行配置和产品参数
- `src/Storage`：EEPROM / Flash / 受控本地存储持久化
- `src/ThirdParty`：App 依赖的第三方库
- `src/Drives`：可复用设备驱动
- `src/Platform`：芯片、RTOS、低资源 Linux、板级、总线、端口适配
- `projects`：项目级构建和调试入口
- `examples/basic`：BASIC 业务示例脚本

## 覆盖目标

- STM32 与其他 MCU
- RTOS 设备
- bare-metal 固件
- 低资源 Linux 设备

`external/IoTEdge.Linux` / Pixiu 中的 C 运行时、RS485、CRC、parson、轻量 HTTP 与控制面板等模块会作为迁移来源按需纳入本仓库，不再作为新的独立产品线扩展。

## 目录原则

详细结构见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 路线图

详见 [ROADMAP.md](ROADMAP.md)。

## 许可证

详见 [LICENSE](LICENSE)。
