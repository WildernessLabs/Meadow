# Implementation Plan: Build System

## Phase 1: Clone and Explore Upstream ✅
- [x] Clone or reference `dotnet/runtime` at a stable .NET 10 commit
  - Using WildernessLabs/runtime fork (side-by-side at `Wilderness_Labs/runtime/`)
  - Fork tracks `dotnet/runtime` upstream on `main` branch
- [x] Study `src/mono/CMakeLists.txt` structure — understand how OS targets are defined (look at Android, iOS, WASI as models)
  - HOST OS detection via `CLR_CMAKE_HOST_OS` (line ~186), TARGET OS (line ~307)
  - WASI is closest model: `CMAKE_SYSTEM_NAME=Generic`, static-only, no shared libs
- [x] Identify all CMake variables that need NuttX-specific values
  - HOST_NUTTX, TARGET_NUTTX, TARGET_UNIX, DISABLE_SHARED_LIBS, STATIC_COMPONENTS,
    DISABLE_EXECUTABLES, PTHREAD_POINTER_ID, plus ~30 feature overrides in configure.cmake
- [x] Document the mapping from old autoconf settings to CMake equivalents
  - `host_nuttx=yes` → `HOST_NUTTX=1` in CMakeLists.txt HOST OS block
  - `PTHREAD_POINTER_ID` → `set(PTHREAD_POINTER_ID 1)` (same name)
  - `HAVE_NUTTX` → `HOST_NUTTX` + `TARGET_NUTTX` + `__NuttX__` preprocessor define
  - Boehm GC disabled → default (upstream uses SGen exclusively now)
  - SIGPOSIX → `HAVE_SIGACTION 1` (auto-detected)

## Phase 2: Toolchain File ✅
- [x] Create `cmake/toolchains/arm-none-eabi-nuttx.cmake` toolchain file
  - At `src/mono/cmake/toolchains/arm-none-eabi-nuttx.cmake`
- [x] Set `CMAKE_SYSTEM_NAME` to `Generic`, `CMAKE_SYSTEM_VARIANT` to `nuttx`
  - Using variant detection pattern: `CLR_CMAKE_HOST_OS STREQUAL "generic" AND CMAKE_SYSTEM_VARIANT STREQUAL "nuttx"`
- [x] Set `CMAKE_C_COMPILER` to `arm-none-eabi-gcc`, `CMAKE_CXX_COMPILER` to `arm-none-eabi-g++`
- [x] Set architecture flags: `-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16`
- [x] Set `CMAKE_FIND_ROOT_PATH` for NuttX sysroot (headers for pthread, etc.)
  - Uses `NUTTX_INCLUDE_DIR` variable, passed via `-D` or auto-detected from sibling `Meadow.OS/nuttx/include`
- [x] Configure static library output (no shared libraries for embedded)
  - `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`, `BUILD_SHARED_LIBS=OFF`
- [x] Fix newlib `int32_t = long int` type mismatch
  - Key fix: `-D__INT32_TYPE__=int -D__UINT32_TYPE__="unsigned int"` in toolchain FLAGS_INIT
  - Without this, hundreds of "conflicting types" errors because mono mixes `int` and `int32_t`

## Phase 3: CMake NuttX Target ✅
- [x] Add `HOST_NUTTX` / `TARGET_NUTTX` detection in `src/mono/CMakeLists.txt`
- [x] Add NuttX to OS-specific conditionals (alongside linux, darwin, android, etc.)
  - Also added to `eng/native/configureplatform.cmake` and `eng/native/configuretools.cmake`
- [x] Set `TARGET_ARM=1`, `MONO_ARM_FPU=vfp-hard`, `HAVE_ARMV5=1`, `HAVE_ARMV6=1`, `HAVE_ARMV7=1`
  - HAVE_ARMV7 set conditionally for NuttX in the ARM arch block
- [x] Define `__NuttX__` as a compile definition
- [x] Disable features: `DISABLE_DEBUGGER_AGENT`, `DISABLE_PROFILER`, initial socket disable
  - AOT profiler excluded from NuttX build (uses sockets)
  - Debugger networking guarded with `HAVE_NETINET_IN_H`
- [x] Enable: `STATIC_COMPONENTS=1`, `ENABLE_SMALL_CONFIG` (optional)
- [x] Enable interpreter: ensure `MONO_ARCH_INTERPRETER_SUPPORTED` is set for ARM
  - Auto-detected (ARM is a supported interpreter arch in upstream)
- [x] Configure SGen GC for NuttX (no mmap, use malloc-based allocation)
  - `HAVE_SYS_MMAN_H=0`, `HAVE_MMAP=0` — SGen falls back to malloc

## Phase 4: Build and Fix ✅
- [x] Run CMake configure with toolchain file
- [x] Fix compile errors iteratively (missing headers, undefined symbols, type mismatches)
  - **Errors fixed (in order encountered):**
  1. `eng/native/configureplatform.cmake` — unknown arch: added NuttX host/target platform detection
  2. `eng/native/configuretools.cmake` — toolchain prefix detection: skip for NuttX (like WASI)
  3. `dlmalloc.h` — `struct mallinfo` redefined: added `__INCLUDE_MALLOC_H` guard
  4. `mono-os-semaphore.h` — fell through to Windows API: force `HAVE_SEMAPHORE_H=1`
  5. `mono-context.c` — `arm_ucontext` not available: added `HOST_NUTTX` to `MONO_CROSS_COMPILE` guard
  6. `minipal/thread.h` — unsupported platform: added `__NuttX__` with `pthread_self()`
  7. `atomic.h` / `atomic.c` — 64-bit atomics: added NuttX to ARMv7 native CAS path, use `__sync` builtins (no LDREXD on Cortex-M)
  8. `mini-arm.h` — `MONO_ARCH_USE_SIGACTION`: undef for NuttX (no `si_addr` in siginfo_t)
  9. `exceptions-arm.c` — `arm_ucontext`: skip for NuttX
  10. `mini-arm.c` — `si_addr`: return FALSE for NuttX
  11. `debugger-networking.h/.c` — socket types: guard with `HAVE_NETINET_IN_H`
  12. `profiler/CMakeLists.txt` — AOT profiler: exclude NuttX (uses sockets)
  13. `configure.cmake` — ~30 false-positive function/header overrides for newlib stubs
  14. `minipal/configure.cmake` — `HAVE_SYSCTLBYNAME=0` override
- [x] Stub out any NuttX-specific functions that will be implemented in Track 2
  - Signal context functions stubbed with `g_assert_not_reached()` / `return FALSE`
- [x] Verify `libmonosgen.a` is produced
  - `libmonosgen-2.0.a` — 19 MB, 4872 exported symbols
- [x] Verify compile flags in build output include Thumb2 flags
  - Confirmed: `-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16`

## Phase 5: Integration Script ✅
- [x] Create a build script (`build-nuttx.sh`) that wraps the CMake invocation
  - At `src/mono/build-nuttx.sh`, auto-detects NuttX headers from sibling Meadow.OS repo
- [x] Document required environment variables and prerequisites
  - `NUTTX_INCLUDE_DIR` (or auto-detected), `arm-none-eabi-gcc` on PATH, CMake >= 3.26
- [x] Verify clean build from scratch works
  - Full clean configure+build completes in ~35s (18s configure + 17s build)

## Build Output Summary
| Artifact | Size | Notes |
|----------|------|-------|
| `libmonosgen-2.0.a` | 19 MB | Main runtime library |
| `libsgen_objects.a` | 4.5 MB | SGen GC |
| `libminipal.a` | 102 KB | Platform abstraction |
| Component stubs | 64-290 KB each | Debugger, diagnostics, hot_reload, marshal-ilgen |

## Known Issues / Tech Debt for Later Tracks
- **int32_t type workaround**: Compiler-level override (`-D__INT32_TYPE__=int`) works but generates redefinition warnings on every compile unit. Could investigate patching newlib headers or using a wrapper header instead.
- **Signal context stubs**: `mono_sigctx_to_monoctx`, `mono_arch_ip_from_context`, `mono_arch_is_single_step_event`, `mono_arch_is_breakpoint_event` all stub to `g_assert_not_reached()` / `return FALSE`. Track 2 may need real implementations if cooperative suspend requires them.
- **`-Wno-error=incompatible-pointer-types`**: Still active for NuttX due to residual int/long pointer mismatches in atomic ops. May be cleanable after int32_t fix is validated more broadly.
- **Debugger networking**: Socket type definitions conditionally compiled out. If debugger is re-enabled (Track 2+), these will need NuttX socket implementations.
