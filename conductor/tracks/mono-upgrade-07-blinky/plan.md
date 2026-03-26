# Implementation Plan: Blinky on Interpreter

## System.Native PAL — Cross-Cutting Concern

The .NET BCL routes all POSIX operations through `libSystem.Native` (265 functions across 41 source
files). Track 06 implemented 26 of these for Console.WriteLine. Every subsequent track will need
more. The full upstream source is at `runtime/src/native/libs/System.Native/pal_*.c`.

**Strategy**: Build a real NuttX port of libSystem.Native as a static library, rather than adding
one-off stubs to `mono_main.c` indefinitely. This is the same approach used for WASM/WASI —
a platform-specific PAL source file per module.

### System.Native Functions Needed for Blinky (estimated)

Meadow.Core uses: Threading, Timers, File I/O, Collections, Reflection, Net (for Meadow.Cloud —
can be stubbed for now). The BCL types behind these will trigger these System.Native modules:

| Module | Functions | Why |
|--------|-----------|-----|
| `pal_threading` | CreateThread, LowLevelMonitor_* | System.Threading used extensively |
| `pal_time` | GetTimestamp, GetBootTimeTicks, GetSystemTimeAsTicks | Timers, Task.Delay |
| `pal_io` | Read, Write, Open, Close, FStat, Stat, Pipe, Poll, Dup, ReadDir, OpenDir, CloseDir, MkDir, Unlink, Rename, RealPath, Access, PathConf, FSync | System.IO file operations |
| `pal_memory` | Malloc, Free, Calloc, Realloc, AlignedAlloc, AlignedFree | Internal allocators |
| `pal_process` | GetPid, ForkAndExecProcess (stub), GetProcessPath | Diagnostics |
| `pal_signal` | SetPosixSignalHandler, EnablePosixSignalHandling | Signal handling (stub on NuttX) |
| `pal_errno` | SetErrNo, GetErrNo, ConvertError*, StrErrorR | Already done in Track 06 |
| `pal_random` | GetCryptographicallySecureRandomBytes, GetNonCrypto* | System.Random |
| `pal_runtimeinformation` | GetOSArchitecture, GetUnixVersion, GetUnixRelease | Platform detection |

### Approach Options

**Option A: Continue stub-by-stub in mono_main.c** (current approach)
- Pro: Simple, no build system changes
- Con: mono_main.c grows unboundedly, hard to maintain, NuttX-specific quirks duplicated

**Option B: Build upstream pal_*.c files for NuttX** (recommended)
- Fork the upstream `pal_*.c` files, add `#ifdef __NuttX__` blocks for NuttX differences
- Compile into the static Mono library or link separately
- Pro: Upstream-aligned, maintainable, covers 90% of functions with real POSIX implementations
- Con: Initial setup work, need to handle NuttX POSIX gaps (no fork, no mmap, limited signals)

**Option C: Hybrid** — build upstream PAL for what works, keep stubs in mono_main.c for NuttX-specific gaps
- Most practical for Track 07 timeline

## Phase 1: System.Native PAL Expansion

- [ ] Audit which System.Native functions Meadow.Core's BCL dependencies will trigger
- [ ] Decide on approach (A/B/C above) — discuss with user
- [ ] Implement threading PAL: `CreateThread`, `LowLevelMonitor_*` (Meadow.Core uses threads heavily)
- [ ] Implement time PAL: `GetTimestamp`, `GetBootTimeTicks`, `GetSystemTimeAsTicks`
- [ ] Implement memory PAL: `Malloc`, `Free`, `Calloc`, `Realloc` (may just pass through to NuttX)
- [ ] Implement random PAL: `GetCryptographicallySecureRandomBytes` (NuttX has `/dev/urandom`)
- [ ] Implement runtime info: `GetOSArchitecture`, `GetUnixVersion`
- [ ] Test: simple app using `Task.Delay(1000)` + `Thread.Sleep(100)` works

## Phase 2: Retarget Managed Libraries

- [ ] Retarget Meadow.Contracts to `net10.0` (or multi-target `netstandard2.1;net10.0`)
- [ ] Retarget Meadow.Core to `net10.0`
- [ ] Retarget Meadow.F7 to `net10.0`
- [ ] Fix any API incompatibilities from netstandard2.1 → net10.0
- [ ] Verify all projects build successfully

## Phase 3: Minimal GPIO Test (Before Full Blinky)

- [ ] Write a minimal test app that directly P/Invokes the UPD driver:
  ```csharp
  // open("/dev/upd"), ioctl(fd, UPD_SET_GPIO, ...), close(fd)
  ```
- [ ] Verify DllImport("nuttx") → mappings-meadow.h resolution works
- [ ] Verify UPD ioctl struct layouts match between managed and native
- [ ] Add any new trampoline signatures to nuttx_m2n_invoke.g.h if needed

## Phase 4: Retarget Meadow.Foundation + Build Blinky

- [ ] Retarget Meadow.Foundation core to `net10.0`
- [ ] Retarget Meadow.Foundation.Leds (RgbPwmLed) to `net10.0`
- [ ] Retarget BlinkyCS to `net10.0`
- [ ] Ensure correct `App<F7FeatherV2>` definition
- [ ] Build BlinkyCS, collect all assemblies

## Phase 5: Deploy and Debug

- [ ] Stage all assemblies in `build/dotnet10/assemblies/`
- [ ] Build LFS v1 image: `tools/build_lfs_v1_image <asm_dir> <output> <asm_dir>`
- [ ] Monitor SDRAM usage — may need to tune selective caching thresholds
- [ ] Boot emulator, debug App<T> initialization (will likely hit new unmapped P/Invoke functions)
- [ ] Iterate: add missing System.Native functions, add missing assemblies, add missing trampolines
- [ ] Verify GPIO state changes visible in Renode
- [ ] Run for 60+ seconds, check stability

## Phase 6: Document

- [ ] Document all newly implemented System.Native functions
- [ ] Document total assembly list and sizes
- [ ] Document SDRAM budget with Blinky workload
- [ ] Write Track 07 handoff
