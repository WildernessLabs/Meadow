# Track 1: Build System — CMake + NuttX Cross-Compilation Target

## Overview
Get the .NET 10 Mono runtime (`dotnet/runtime/src/mono`) cross-compiling for ARM Cortex-M7 running NuttX. This requires adding NuttX as a target OS in the CMake build system and configuring the `arm-none-eabi-gcc` toolchain for Thumb2 compilation.

## Background
The current Mono 6.9.0 fork uses autoconf (`configure.ac`) with a custom `arm-none-eabi*` target block. The new .NET 10 mono uses CMake exclusively. There is no existing NuttX/RTOS target in the upstream CMake — it must be added from scratch.

### Key Files in Upstream
- `src/mono/CMakeLists.txt` — Main build file, defines OS targets, ARM options
- `src/mono/mono/mini/CMakeLists.txt` — JIT/interpreter build
- `src/mono/mono/metadata/CMakeLists.txt` — Metadata/class system build
- `src/mono/mono/utils/CMakeLists.txt` — Platform utilities build
- `src/mono/mono/sgen/CMakeLists.txt` — GC build

### Key autoconf Settings to Port
From the current `configure.ac` `arm-none-eabi*` block:
- `host_nuttx=yes`
- `PTHREAD_POINTER_ID` defined
- TLS: pthread-based
- Boehm GC disabled
- Signal handling: SIGPOSIX enabled

## Functional Requirements
1. Add NuttX as a recognized host OS in `src/mono/CMakeLists.txt`
2. Create a CMake toolchain file for `arm-none-eabi-gcc` targeting Cortex-M7
3. Configure compile flags: `-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16`
4. Set `TARGET_ARM=1`, `MONO_ARM_FPU=vfp-hard`
5. Enable: `STATIC_COMPONENTS`, interpreter
6. Disable initial features: debugger agent, profiler, sockets (re-enable later)
7. Output: static `libmonosgen.a` suitable for linking into NuttX user-space ELF
8. Define `__NuttX__` preprocessor macro for platform-conditional code

## Non-Functional Requirements
- Build should complete without errors or unresolved symbols (linking may have unresolved NuttX symbols — that's expected until Track 2)
- Maintain ability to build for other targets (don't break Linux/macOS/Android builds)

## Acceptance Criteria
- [x] `cmake` configure step completes for NuttX ARM target
- [x] `make` / `ninja` build produces `libmonosgen.a` — `libmonosgen-2.0.a` (19 MB, 4872 symbols)
- [x] All compile units build without errors (339/339 targets)
- [x] Build flags include `-mthumb -mcpu=cortex-m7 -mfloat-abi=hard` — confirmed
- [x] `__NuttX__` is defined during compilation — confirmed in build.ninja + config.h

## Out of Scope
- NuttX platform implementation (Track 2)
- Linking into actual NuttX firmware (Track 3)
- Managed code compilation (Track 5)
