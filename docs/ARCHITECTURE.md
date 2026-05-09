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
    Bsp/        board services, pin and peripheral wrappers
    Common/     logging, hashing, utilities, app-wide helpers
    Config/     runtime configuration and product settings
    Devices/    product/device logic
    Network/    network stack adapters and transport glue
    Protocol/   MQTT, Modbus, and other app protocols
    Storage/    EEPROM / Flash persistence helpers
    ThirdParty/ vendored libraries used by App
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
- `App` is the application layer and may depend on `Core`, `Drives`, and `Platform`.
- `Drives` only holds reusable device drivers.
- `Platform` owns chip, board, bus, and port glue.
- `projects` contains only project-specific wiring, not reusable platform code.
- Any external system that wants to extend the runtime should talk to `Core/Public` first.

## Split by responsibility

- Interpreter team: `src/Core/Basic`
- Runtime team: `src/Core/Runtime`
- Application team: `src/App`
- Common driver team: `src/Drives/Device`
- Platform team: `src/Platform`
- Target integration team: `projects`

## Future extension

Add a new chip by creating a new chip folder under `src/Platform/Chip`.
Add a new product by creating a new target folder under `projects` that wires into `src/App`, `src/Core`, `src/Drives`, and `src/Platform`.
