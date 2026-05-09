# IoTEmBASIC

MCU 上的 BASIC 解释器与边缘运行时，面向 STM32 和其他资源受限设备。

## 定位

本仓库参考 luaos 和 rt-thread 的分层思路，把系统拆成几层：

- `src/Core`：BASIC 解释器内核和公共运行时
- `src/Drives`：芯片、板级、设备、总线等驱动层
- `projects`：不同芯片 / 不同新品的独立工程入口

## 目录原则

详细结构见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 路线图

详见 [ROADMAP.md](ROADMAP.md)。

## 许可证

详见 [LICENSE](LICENSE)。
