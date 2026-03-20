# Track 12: AOT Compilation — Ahead-of-Time via LLVM

## Overview
Enable Ahead-of-Time (AOT) compilation for Meadow applications, eliminating JIT overhead at runtime. This uses LLVM's ARM backend to produce Thumb2 native code at build time.

## Background

### .NET 10 Mono AOT Modes
| Mode | Description |
|------|-------------|
| `MONO_AOT_MODE_NORMAL` | JIT + AOT hybrid (fallback to JIT for uncompiled methods) |
| `MONO_AOT_MODE_FULL` | Full AOT, no JIT at runtime |
| `MONO_AOT_MODE_LLVMONLY` | LLVM-generated AOT only |
| `MONO_AOT_MODE_LLVMONLY_INTERP` | LLVM AOT + interpreter fallback |

### LLVM and Thumb2
LLVM's ARM backend can emit Thumb2 natively — this is the path to AOT Thumb2 without needing the custom JIT backend. The AOT compiler cross-compiles on the host (macOS/Linux x86) targeting `armv7-none-linux-androideabi` (or a custom NuttX triple).

### Registration
For embedded targets, AOT modules are registered at startup via `mono_aot_register_module()` — they're statically linked into the firmware.

## Functional Requirements
1. Configure LLVM AOT cross-compilation for ARM Thumb2 target
2. AOT-compile `System.Private.CoreLib.dll` and app assemblies
3. Register AOT modules at startup
4. Execute AOT-compiled code on NuttX/Cortex-M7
5. Support hybrid mode (AOT + interpreter fallback) initially

## Acceptance Criteria
- [ ] AOT compiler produces valid Thumb2 object files
- [ ] AOT-compiled SPCL loads and works
- [ ] Hello World runs from AOT-compiled assembly
- [ ] Blinky runs from AOT-compiled assemblies
- [ ] Startup time improves vs. JIT/interpreter
- [ ] `MONO_AOT_MODE_LLVMONLY_INTERP` works (AOT with interpreter fallback for generics)
- [ ] Validated on emulator and hardware

## Out of Scope
- Full AOT without interpreter fallback (generics may need it)
- Integration into Meadow.CLI build pipeline (future enhancement)
- Trimming + AOT combined pipeline
