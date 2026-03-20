# Tech Stack

## Target Hardware
- **MCU**: STM32F777ZIT6 (ARM Cortex-M7, Thumb2-only, no ARM mode)
- **FPU**: FPv5-D16 (VFP hard float)
- **Flash**: 64MB QSPI (assemblies + firmware)
- **SDRAM**: 32MB (Mono runtime + managed heap loaded here at 0xC0000000)
- **Boards**: F7FeatherV2, CoreComputeModuleV2 (CCMv2)

## RTOS
- **NuttX**: Real-time operating system, POSIX-compatible
- **Kernel**: Flat build, user-space ELF for apps (including Mono)

## Runtime
- **Current**: Mono 6.9.0 (Wilderness Labs fork with Thumb2 JIT + NuttX patches)
- **Target**: .NET 10 Mono (`dotnet/runtime/src/mono`, commit TBD)
- **Core library**: System.Private.CoreLib.dll (replaces mscorlib.dll)
- **Assembly loading**: TPA (Trusted Platform Assemblies) via monovm_initialize

## Managed Framework
- **Meadow.Core**: Hardware abstraction (GPIO, SPI, I2C, PWM, ADC) via P/Invoke to `"nuttx"` library
- **Meadow.Foundation**: Higher-level peripheral drivers (LEDs, sensors, displays)
- **Meadow.Contracts**: Interface definitions (IPin, IDigitalOutputPort, etc.)
- **TFM**: `net10.0` (currently `netstandard2.1`, will be retargeted)

## P/Invoke Architecture
```
C# App → Meadow.Core (DllImport("nuttx")) → ioctl(/dev/upd) → NuttX HAL → STM32 HW
```

## Build Tools
- **Mono build**: CMake (replacing autoconf) with `arm-none-eabi-gcc` toolchain
- **NuttX build**: Make/Kconfig
- **Managed build**: `dotnet build` targeting `net10.0`
- **Deployment**: Meadow.CLI via HCOM protocol (USB or socket://localhost:4242 for emulator)

## Emulator
- **Renode**: v1.16.1, STM32F7 platform emulation
- **HCOM**: Exposed on TCP port 4242 (UART4 redirect)
- **Hooks**: C# extension methods + IronPython for address-specific patches
- **Address extraction**: `tools/extract-hook-addresses.py` (nm + objdump → hooks.resc)

## TLS
- **Current**: Mbed TLS 3.2.1 (mono/mbedtls/mono-mbedtls.c)
- **Target**: Port to .NET 10 mono's TLS provider interface

## Key Repos
- `Meadow` (this repo): NuttX + Mono + mbedtls + build scripts
- `Meadow.OS.Emulator`: Renode platform, hooks, build scripts
- `Meadow.Core`: Managed hardware abstraction (P/Invoke layer)
- `Meadow.Foundation`: Peripheral drivers
- `Meadow.CLI`: Deployment tool (socket branch for emulator)
- `Meadow.Samples`: Sample apps (Blinky, etc.)
