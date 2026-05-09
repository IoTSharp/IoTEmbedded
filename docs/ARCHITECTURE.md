# Architecture

This repository is organized like a small embedded stack, following the same kind of layered split used by luaos and rt-thread:

```text
src/
  Core/
    Basic/      BASIC lexer, parser, AST, evaluator
    Runtime/    execution loop, errors, built-ins, runtime services
    Common/     shared types, utilities, memory helpers
    Public/     headers exposed to drivers and projects
  Drives/
    Chip/       MCU-specific clock, flash, irq, peripheral glue
    Board/      board resources, IOC, pin mapping, LEDs, buttons
    Device/     reusable device drivers
    Bus/        UART, I2C, SPI, CAN, etc.
    Port/       platform adaptation layer
projects/
  <vendor>/<series>/<board>/
    app/        product entry points
    board/      board-specific resources
    linker/     linker script and memory layout
    visualgdb/  VisualGDB project files
```

## Design rules

- `Core` must not depend on a specific chip.
- `Drives/Chip` may depend on chip headers, but not on a product workflow.
- `Drives/Board` owns board-level wiring and IOC mapping.
- `projects` contains one build/debug entry per target board or product.
- Any external system that wants to extend the runtime should talk to `Core/Public` first.

## Split by responsibility

- Interpreter team: `src/Core/Basic`
- Runtime team: `src/Core/Runtime`
- MCU port team: `src/Drives/Chip`
- Board bring-up team: `src/Drives/Board`
- Common driver team: `src/Drives/Device` and `src/Drives/Bus`
- Target integration team: `projects`

## Future extension

Add a new chip by creating a new chip folder under `src/Drives/Chip` and a matching entry under `projects`.
Add a new product by creating a new board folder under `projects` without changing `Core`.
