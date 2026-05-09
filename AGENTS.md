# 仓库约束 (IoTEmBASIC / IoTSharp.Edge.Stm32)

本仓库面向 STM32 + STM32 HAL + FreeRTOS + CMSIS_RTOS_V2 的纯 C 嵌入式工程。所有
新增代码、目录调整、构建脚本变更都必须遵守以下硬性约束。

## 1. `src/` 目录布局

`src/` 按"职责"分模块，每个模块一律采用 **`Inc/` + `Src/` 并列**的二级目录结构：

```
src/<Module>[/<SubModule>]/
    Inc/   只放 .h（公共/对外暴露的头文件）
    Src/   只放 .c
```

- 一个模块要么 **同时**含 `Inc/` 与 `Src/`，要么 **只**含 `Inc/`（纯头文件模块）。
- 禁止把 `.h` 与 `.c` 平铺在模块根目录；禁止 `.h` 出现在 `Src/` 内。
- 子模块（如 `Bus/Uart`、`Network/Ch395`、`Protocol/Modbus`）同样遵守上述结构。

当前模块清单（请保持最新）：

| 模块 | 说明 |
| --- | --- |
| `Application/` | 业务入口、应用层 RTOS 任务、测试入口 |
| `Board/` | 板级初始化、看门狗等板级抽象 |
| `Bus/Uart/`, `Bus/Rs485/` | 总线驱动（按物理总线再分子模块） |
| `Common/` | 公共工具（log、md5、util、通用类型） |
| `Config/` | 配置 / 网络配置 |
| `Devices/` | 业务设备抽象（空调、电表、开关、传感器等） |
| `Modem/` | 模组（Air724 等） |
| `Network/` | 网络管理、Socket、时间同步；`Network/Ch395/` 为 CH395 子模块 |
| `Protocol/Modbus/`, `Protocol/Mqtt/`, `Protocol/Platform/` | 协议层 |
| `Storage/` | EEPROM 等存储抽象 |
| `Platform/stm32/...` | **CubeMX 生成代码，禁止重排目录结构**（main.c 的 USER CODE 区允许编辑） |
| `ThirdParty/Parson/` | 第三方库，**保持原样**，不引入 Inc/Src 拆分 |

## 2. `#include` 风格

- **跨模块引用**统一使用模块前缀 + `Inc` 段，相对 `src/` 根：

  ```c
  #include "Application/Inc/app.h"
  #include "Network/Ch395/Inc/bsp_ch395.h"
  #include "Protocol/Modbus/Inc/modbus_api.h"
  ```

- **同模块内**（同一 `Inc/` 或 `Src/` → 自家 `Inc/`）也使用同样的完整模块前缀路径，
  禁止 `#include "../Inc/foo.h"` 之类的相对路径。
- 禁止裸 `#include "foo.h"` 形式引用本仓库模块头文件。
- 例外（仅以下两类裸/特殊包含被允许）：
  - 第三方头：`#include "parson.h"`（仅在 `src/ThirdParty/Parson/` 内部）。
  - HAL / FreeRTOS / CMSIS / CubeMX 生成头：
    `stm32f1xx_hal*.h`, `stm32f1xx_hal_*.h`, `Legacy/stm32f1xx_hal_can_legacy.h`,
    `FreeRTOS.h`, `task.h`, `queue.h`, `semphr.h`, `cmsis_os2.h`, `main.h` 等。

## 3. 构建脚本约束（CMake / VisualGDB）

- 工程文件位于 `projects/stm32/f1/<board>/CMakeLists.txt`，由 `*.vgdbcmake` 引用。
- **include 路径**只暴露三处：
  1. `${SRC_ROOT}`（即 `src/`）—— 让所有 `<Module>/Inc/<header>.h` 正常解析；
  2. `${SRC_ROOT}/ThirdParty/Parson` —— Parson 自包含；
  3. CubeMX/HAL/CMSIS/FreeRTOS 自有的私有头路径（在 `src/Platform/stm32/...` 下）。
- 不要为每个模块单独追加 include path；新增模块只需把其 `Src/*.c` 加入源码列表。
- 新增 / 重命名 / 移动文件后，必须同步修改 `CMakeLists.txt` 的源码列表；保持
  按模块分组、字母顺序便于 review。

## 4. 文件操作规范

- 移动 / 重命名一律使用 `git mv`，保留文件历史。
- 文件保存使用 UTF-8（无 BOM），保留行尾换行符；批量改写脚本必须用
  `Get-Content -Raw -Encoding UTF8` + `Set-Content -Encoding UTF8 -NoNewline`
  以避免破坏原有字节与换行。
- 禁止改动 `src/Platform/stm32/**` 的 CubeMX 生成代码，例外仅限 `main.c` 的
  `USER CODE BEGIN/END` 段。
- 禁止改动 `src/ThirdParty/**` 第三方源码与目录结构。

## 5. 验证清单（每次结构性改动后）

1. `git status` 确认 rename 而非 add+delete。
2. grep 校验：
   ```
   ^#include "(Application|Board|Bus/...|Common|Config|Devices|Modem|Network|Network/Ch395|Protocol/Modbus|Protocol/Mqtt|Protocol/Platform|Storage)/[A-Za-z0-9_]+\.h"
   ```
   返回 0 命中（即不存在缺少 `/Inc/` 段的模块前缀包含）。
3. 在 VisualGDB / CMake 构建一次，确保所有 include 路径解析成功。

## 6. 编码风格

- 仅使用 C（C99/C11，跟随 STM32 工程默认）。
- 函数 / 变量遵循既有的 `module_action_*` 风格；HAL 回调命名沿用 CubeMX 约定。
- 中文注释保留 UTF-8；不要用脚本盲目转码（参考用户备忘：避免 `?` 不可逆替换）。
