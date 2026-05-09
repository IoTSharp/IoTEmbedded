# IoTEmBASIC

> MCU 上的 BASIC 解释器与边缘运行时。IoTSharp 生态中"边缘执行"层的 STM32 / 资源受限设备目标。

## 项目定位

IoTEmBASIC 是面向 MCU（STM32 / 类 Cortex-M / 通用 32 位单片机）的 BASIC 解释器内核，作为 IoTSharp 生态中 `IoTSharp.Edge.Stm32` 的底层运行时使用。

它解决资源受限设备上"逻辑可下发、可热更"的需求：

- 云端通过 `IoTSharp.SaaS` 的代码生成器把业务逻辑编译/翻译为 BASIC 脚本
- 脚本经签名后下发到 MCU
- 设备上的 IoTEmBASIC 解释器加载并执行脚本，访问通过 BasicRuntime 接口暴露的设备能力

## 与 IoTSharp 生态的关系

```
                IoTSharp.SaaS (云端生成 + 签名)
                          │
                          ▼  BASIC 脚本 + 签名
┌─────────────────────────────────────────────────┐
│  IoTSharp.Edge       IoTSharp.Edge.Linux        │
│  (C# AOT 宿主)        (Linux C 宿主)            │
│  IoTSharp.Edge.Stm32 ←  本仓库提供的解释器内核  │  ← you are here
└─────────────────────────────────────────────────┘
```

三类边缘宿主共享同一套 **BasicRuntime 接口注册表**；本仓库负责其中 MCU 子集的实现。

## 路线图

详见 [ROADMAP.md](ROADMAP.md)。

## 许可证

详见 [LICENSE](LICENSE)。