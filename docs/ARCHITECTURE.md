# Architecture

This repository is organized like a small embedded stack, following the same kind of layered split used by luaos and rt-thread:

```text
src/
  Core/
    Basic/      BASIC lexer, parser, AST, evaluator
    Runtime/    execution loop, errors, built-ins, runtime services
    Common/     shared types, utilities, memory helpers
    Public/     headers exposed to drivers and projects
  App/
    app.c/h     product entry points and runtime glue
    app_rtos.c/h RTOS helpers and hooks
    modbus_test.c/h  Modbus test harness
    Devices/    product/device logic
    Network/    network stack adapters and transport glue
    Protocol/   MQTT, Modbus, and other app protocols
  Bsp/          board services, pin and peripheral wrappers
  Common/       logging, hashing, utilities, shared helpers
  Config/       runtime configuration and product settings
  Storage/      EEPROM / Flash persistence helpers
  ThirdParty/   vendored libraries used by App
  Drives/
    Device/     reusable device drivers
  Platform/
    Chip/       MCU-specific clock, flash, irq, peripheral glue
    Board/      board resources, IOC, pin mapping, LEDs, buttons
    Bus/        UART, I2C, SPI, CAN, etc.
    Port/       platform adaptation layer
projects/
  <vendor>/<series>/<target>/
    project files and build/debug wiring only
```

## Design rules

- `Core` must not depend on a specific chip.
- `App` is the business layer and may depend on `Core`, `Bsp`, `Common`, `Config`, `Storage`, `ThirdParty`, `Drives`, and `Platform`.
- `Bsp` owns board services and peripheral wrappers.
- `Common` owns shared helpers used across the application layer.
- `Config` owns runtime configuration and product settings.
- `Storage` owns EEPROM / Flash persistence helpers.
- `ThirdParty` owns vendored libraries used by `App`.
- `Drives` only holds reusable device drivers.
- `Platform` owns chip, board, bus, and port glue.
- `projects` contains only project-specific wiring, not reusable platform code.
- Any external system that wants to extend the runtime should talk to `Core/Public` first.

## Split by responsibility

- Interpreter team: `src/Core/Basic`
- Runtime team: `src/Core/Runtime`
- Application team: `src/App`
- Board services team: `src/Bsp`
- Common helpers team: `src/Common`
- Config team: `src/Config`
- Storage team: `src/Storage`
- Third-party team: `src/ThirdParty`
- Common driver team: `src/Drives/Device`
- Platform team: `src/Platform`
- Target integration team: `projects`

## Future extension

Add a new chip by creating a new chip folder under `src/Platform/Chip`.
Add a new product by creating a new target folder under `projects` that wires into `src/App`, `src/Bsp`, `src/Common`, `src/Config`, `src/Storage`, `src/ThirdParty`, `src/Core`, `src/Drives`, and `src/Platform`.
