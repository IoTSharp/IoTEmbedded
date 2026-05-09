# IoTEmBASIC

MCU 上的 BASIC 解释器与边缘运行时，面向 STM32 和其他资源受限设备。

## 定位

本仓库参考 luaos 和 rt-thread 的分层思路，把系统拆成几层：

- `src/Core`：BASIC 解释器内核和公共运行时
- `src/App`：业务层，承接产品逻辑、设备编排、网络和协议流程
- `src/Bsp`：板级服务和外设封装
- `src/Common`：共享类型、日志、哈希和通用工具
- `src/Config`：运行配置和产品参数
- `src/Storage`：EEPROM / Flash 持久化
- `src/ThirdParty`：App 依赖的第三方库
- `src/Drives`：可复用设备驱动
- `src/Platform`：芯片、板级、总线、端口适配
- `projects`：项目级构建和调试入口

## 目录原则

详细结构见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 路线图

详见 [ROADMAP.md](ROADMAP.md)。

## 许可证

详见 [LICENSE](LICENSE)。
