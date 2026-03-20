# Product Definition: Meadow.OS Mono Runtime Upgrade

## Vision
Upgrade the Meadow.OS managed runtime from Mono 6.9.0 (standalone fork) to the .NET 10 Mono runtime from dotnet/runtime, enabling modern C# language features, improved performance, and long-term alignment with the .NET ecosystem.

## Users
- **Meadow developers**: Build IoT applications in C# targeting Meadow F7 hardware (STM32F777 Cortex-M7)
- **Wilderness Labs team**: Maintain the Meadow.OS platform, SDK, and tooling

## Goals
1. Replace Mono 6.9.0 fork with .NET 10 Mono runtime (`dotnet/runtime/src/mono`)
2. Maintain full hardware support: GPIO, SPI, I2C, PWM, ADC via P/Invoke to NuttX UPD driver
3. Achieve functional parity: Blinky sample running on F7FeatherV2 / CCMv2
4. Port Thumb2 JIT backend for Cortex-M7 native code generation
5. Port Mbed TLS integration for secure networking
6. Enable future AOT compilation for production deployment

## Constraints
- **No CoreCLR**: Cortex-M7 is Thumb2-only (no ARM mode), CoreCLR lacks Thumb2/M7 support
- **No MMU**: STM32F7 has no virtual memory; SGen GC must work without mmap
- **64MB flash**: Ample space for BCL assemblies, but trimming is desirable for production
- **NuttX RTOS**: All platform integration goes through NuttX syscalls and drivers
- **Emulator-first**: Validate in Renode emulator before hardware testing
