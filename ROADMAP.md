# IoTEmbedded 路线图

> 状态：✅ 已完成 ｜ 🚧 进行中 ｜ ⏳ 计划中 ｜ ⬜ 未开始 ｜ 🔁 持续维护
>
> 顺序：`[串行]` ｜ `[并行]` ｜ `[依赖: X]`

## 0. 目录与分层原则

- `src/App` 只放产品业务逻辑、设备编排、网络和协议流程。
- `src/Bsp` 只放板级服务和外设封装。
- `src/Common` 只放共享类型、日志、哈希和通用工具。
- `src/Config` 只放运行配置和产品参数。
- `src/Storage` 只放 EEPROM / Flash / 受控本地存储持久化。
- `src/ThirdParty` 只放 App 依赖的第三方库。
- `src/Core` 只放 BASIC 解释器、运行时公共层、公共数据结构。
- `src/Drives` 只放芯片、板级、设备、总线、端口适配等可复用驱动。
- `src/Platform` 负责 MCU、RTOS、bare-metal、低资源 Linux、板级、总线和端口适配。
- `projects/<platform>/<board>` 放不同芯片、RTOS、Linux 目标和产品的独立工程入口。
- `docs` 放架构、移植、调试和约束说明。
- 让外部系统二次开发时，只需要依赖 `Core` 的公开接口，不直接碰芯片或 OS 细节。

## 1. 目标结构

| 层级 | 作用 |
| --- | --- |
| `Core` | BASIC 词法、语法、AST、解释执行、标准函数、错误模型 |
| `App` | 应用逻辑、设备编排、网络和协议流程 |
| `Bsp` | 板级服务、外设封装 |
| `Common` | 共享类型、日志、哈希、通用工具 |
| `Config` | 运行配置、产品参数 |
| `Storage` | EEPROM / Flash / 受控本地存储持久化 |
| `ThirdParty` | App 依赖的第三方库 |
| `Drives` | 可复用设备驱动 |
| `Platform` | MCU、RTOS、bare-metal、低资源 Linux、板级、总线、端口适配 |
| `projects` | 每个芯片 / RTOS / Linux 目标 / 板子 / 产品的独立工程与调试入口 |

## 2. 里程碑

| 里程碑 | 状态 | 描述 |
| --- | --- | --- |
| M0 | ✅ | 目录骨架与公共分层定稿 |
| M1 | ⬜ | 解释器最小可运行（Token / Lex / Parse / Eval） |
| M2 | ⬜ | BasicRuntime 接口注册表 v1 |
| M3 | ⬜ | 第一个 STM32 / RTOS / bare-metal 目标工程可编译可调试 |
| M4 | ⬜ | 脚本加载、签名校验与本地版本管理 |
| M5 | ⬜ | 至少两个 MCU/RTOS 目标和一个低资源 Linux Profile 验证通过 |
| M6 | ⬜ | 资源优化：体积、堆栈、ROM 占用、低资源 Linux 启动耗时 |
| M7 | ⬜ | Pixiu 可复用 C 资产迁移完成，`IoTEdge.Linux` 进入只读维护 |

## 3. Phase A — 解释器内核　⬜

| 编号 | 状态 | 顺序 | 任务 |
| --- | --- | --- | --- |
| A1 | ⬜ | [串行] | Token / Lex / Parse / AST 节点定义 |
| A2 | ⬜ | [依赖: A1] | 解释器主循环、变量作用域、控制流 |
| A3 | ⬜ | [并行 ‖ A2] | 错误处理、异常传播、行号定位 |
| A4 | ⬜ | [依赖: A2] | 内置数学、字符串、时间函数 |

## 4. Phase B — 驱动与适配层　⬜

| 编号 | 状态 | 顺序 | 任务 |
| --- | --- | --- | --- |
| B1 | ⬜ | [依赖: A2] | 目录定型：App / Bsp / Common / Config / Storage / ThirdParty / Drives / Platform / projects |
| B2 | ⬜ | [依赖: B1] | 板级资源模型：IOC、时钟、引脚、外设映射 |
| B3 | ⬜ | [并行 ‖ B2] | SERIAL / I2C / SPI 的统一适配接口 |
| B4 | ⬜ | [依赖: B2] | GPIO / Timer / Flash / storage 等基础能力封装 |
| B5 | ⬜ | [并行 ‖ B2] | RTOS 与低资源 Linux Platform 抽象 |
| B6 | ⬜ | [并行 ‖ B2] | 其他系统复用的驱动接口约定 |

## 5. Phase C — 工程化与多目标　⬜

| 编号 | 状态 | 顺序 | 任务 |
| --- | --- | --- | --- |
| C1 | ⬜ | [依赖: B1] | 第一个 `projects/<platform>/<board>` 工程 |
| C2 | ⬜ | [依赖: C1] | VisualGDB / OpenOCD / vendor 工具链调试与下载链路打通 |
| C3 | ⬜ | [并行 ‖ C2] | 第二个 MCU 或 RTOS 目标移植 |
| C4 | ⬜ | [并行 ‖ C2] | 低资源 Linux Profile 与交叉编译入口 |

## 6. Phase D — 脚本生命周期与运行保障　⬜

| 编号 | 状态 | 顺序 | 任务 |
| --- | --- | --- | --- |
| D1 | ⬜ | [依赖: A2] | 脚本加载（Flash / 外部存储 / 受控本地存储） |
| D2 | ⬜ | [依赖: D1] | 签名校验（跟随 SaaS 格式） |
| D3 | ⬜ | [依赖: D1] | 多脚本槽与回滚 |
| D4 | ⬜ | [并行 ‖ D2] | 看门狗、步数预算、时间预算 |

## 7. Phase E — Pixiu 资产迁移　⬜

| 编号 | 状态 | 顺序 | 任务 |
| --- | --- | --- | --- |
| E1 | ⬜ | [串行] | 盘点 `external/IoTEdge.Linux/app` 中可复用 C 模块 |
| E2 | ⬜ | [依赖: E1] | 迁移 RS485、CRC、parson、轻量 HTTP 等通用模块 |
| E3 | ⬜ | [依赖: E1] | 评估 `web` 控制面板是否保留为 IoTEmbedded 低资源 Linux 管理界面 |
| E4 | ⬜ | [依赖: E2, E3] | 标记 Pixiu / `IoTEdge.Linux` 为只读维护状态 |

## 8. 接口稳定性公约

- `Core` 的公开接口优先稳定，驱动层只能通过约定好的抽象进入解释器。
- `Drives` 按芯片、板级、RTOS 和低资源 Linux 拆分，避免把产品逻辑塞进底层。
- 脚本签名格式不在本仓库定义，只做校验。
- 不在本仓库内引入租户、计费、License 之类的上层概念。
