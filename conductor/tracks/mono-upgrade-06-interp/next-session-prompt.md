# Next Session Prompt — Track 07: Blinky on Interpreter

Copy everything below the line into a new Claude Code session.

---

This is a continuation of the Meadow.OS .NET 10 Mono runtime upgrade project. We are on the `Feature_NuttX_dotnet10` branch of the `Meadow` repository.

## Context

Read these files first to understand the project and current state:
1. `CLAUDE.md` — Project overview, critical rules, current state
2. `conductor/tracks.md` — Track registry
3. `conductor/tracks/mono-upgrade-06-interp/handoff.md` — What Track 06 accomplished (System.Native PAL + Hello World)
4. `conductor/tracks/mono-upgrade-07-blinky/spec.md` — Track 07 spec
5. `conductor/tracks/mono-upgrade-07-blinky/plan.md` — Track 07 plan (draft — may need revision)
6. `conductor/workflow.md` — Build process and emulator usage

## What's Done (Tracks 01–06)

The .NET 10 Mono interpreter runs on STM32F777 / NuttX in the Renode emulator. Console.WriteLine("Hello from Meadow!") works end-to-end with exit code 42. Key infrastructure:
- 26 System.Native P/Invoke functions in `apps/examples/mono/mono_main.c`
- 48 interp-to-native trampolines in `runtime/src/mono/mono/mini/nuttx_m2n_invoke.g.h`
- 128 native-to-interp thunks in `runtime/src/mono/mono/mini/aot-runtime-nuttx.c`
- 13 framework assemblies deployed (~8.3MB) in a 32MB SDRAM budget
- LittleFS v1 image at QSPI 0x90500000 (built with `tools/build_lfs_v1_image`, NEVER the Python tool)

## Your Task: Track 07 — Blinky on Interpreter

Get the Blinky sample app running on the Meadow emulator. This validates the full managed stack: .NET 10 Mono → Meadow.Core → NuttX UPD driver → GPIO (emulated).

### Source Locations

| Component | Path | Current TFM |
|-----------|------|------------|
| Meadow.Contracts | `/Users/lexas/Meadow.Contracts/Source/Meadow.Contracts/` | netstandard2.1 |
| Meadow.Core | `/Users/lexas/Meadow.Core/source/Meadow.Core/` | netstandard2.1 |
| Meadow.F7 | `/Users/lexas/Meadow.Core/source/implementations/f7/Meadow.F7/` | netstandard2.1 |
| Meadow.Foundation.Core | `/Users/lexas/Meadow.Foundation/Source/Meadow.Foundation.Core/` | netstandard2.1 |
| BlinkyCS (F7 sample) | `/Users/lexas/Meadow.Samples/Source/Meadow F7/Blinky/BlinkyCS/` | netstandard2.1 |
| BlinkyCS (Core sample) | `/Users/lexas/Meadow.Core.Samples/Source/Blinky/BlinkyCS/` | netstandard2.1 |
| Framework assemblies | `runtime/.dotnet/shared/Microsoft.NETCore.App/11.0.0-preview.3.26161.119/` | — |

### Key Challenge #1: System.Native PAL Port (CRITICAL)

The .NET BCL routes ALL POSIX operations through `libSystem.Native` — **265 exported functions**.
Track 06 implemented only 26 (for Console.WriteLine). Blinky will need significantly more because
Meadow.Core uses `System.Threading`, `System.IO`, `System.Timers`, `System.Collections.Concurrent`,
and `System.Net` — all of which call System.Native internally.

**Upstream source**: `runtime/src/native/libs/System.Native/pal_*.c` (41 files)

**Modules Blinky will likely need (beyond Track 06's 26 functions):**

| Module | Key Functions | Why |
|--------|---------------|-----|
| `pal_threading` | CreateThread, LowLevelMonitor_Create/Acquire/Release/Wait/Signal | Meadow.Core uses threads extensively |
| `pal_time` | GetTimestamp, GetBootTimeTicks, GetSystemTimeAsTicks | Task.Delay, Timers |
| `pal_memory` | Malloc, Free, Calloc, Realloc, AlignedAlloc | Internal BCL allocators |
| `pal_random` | GetCryptographicallySecureRandomBytes | System.Random |
| `pal_runtimeinformation` | GetOSArchitecture, GetUnixVersion, GetUnixRelease | Platform detection |
| `pal_signal` | SetPosixSignalHandler, EnablePosixSignalHandling | Stub — NuttX signals are limited |
| `pal_process` | GetProcessPath, ForkAndExecProcess (stub) | Diagnostics |
| `pal_io` (expanded) | Pipe, Poll, Dup, ReadDir, OpenDir, CloseDir, MkDir, RealPath, Access | File system operations |

**Approach decision needed**: The current strategy (adding stubs one-by-one to mono_main.c) won't
scale to 100+ functions. Consider:
- **Option A**: Continue stub-by-stub (simple but mono_main.c becomes unmaintainable)
- **Option B**: Build the upstream `pal_*.c` files for NuttX with `#ifdef __NuttX__` blocks (recommended — most functions are standard POSIX that NuttX supports)
- **Option C**: Hybrid — build upstream for what works, keep stubs for NuttX-specific gaps

Read `conductor/tracks/mono-upgrade-07-blinky/plan.md` for the full analysis.

### Key Challenge #2: TFM Retarget

All Meadow libraries are `netstandard2.1`. They need to build for `net10.0` (or at minimum `net9.0`
since our SDK is 9.x). This likely involves API compat fixes.

### Key Challenge #3: Memory Pressure

32MB SDRAM is already tight with 13 assemblies (8.3MB). Blinky adds Meadow.Core, Meadow.Contracts,
Meadow.Foundation.Core, Meadow.F7, plus additional framework dependencies. Each new assembly costs
SDRAM. Monitor carefully — may need to further tune GC heap or caching.

### Key Challenge #4: NuttX Driver P/Invoke

Meadow.Core uses `DllImport("nuttx")` for UPD driver calls (GPIO, PWM, I2C, SPI). The mapping table
in `mappings-meadow.h` already has these entries from legacy Mono. Verify they still work with the
new trampoline infrastructure. If new trampoline signatures are needed, the error is:
`"no trampoline for signature cookie 'XXXX'"` — add to `nuttx_m2n_invoke.g.h`.

### Key Challenge #5: App\<T\> Initialization

Meadow apps inherit from `App<F7FeatherV2>`. The `App<T>` base class does significant initialization
(platform detection, GPIO manager, network, storage). Many of these paths will hit unimplemented
P/Invoke functions. Start with the simplest possible approach (raw GPIO test) before attempting
full Blinky.

### Suggested Approach

1. **System.Native first**: Before Blinky, expand System.Native coverage for threading + timers.
   Write a test app: `Task.Delay(1000).Wait(); Console.WriteLine("Timer works!");`

2. **Start simple GPIO**: Before Meadow.Core, try a minimal test that directly P/Invokes the
   UPD driver — `open("/dev/upd")`, `ioctl(fd, UPD_SET_GPIO, ...)`. This validates the NuttX
   driver path without the full Meadow.Core stack.

3. **Incremental retarget**: Retarget Meadow.Contracts first (pure interfaces, should be trivial),
   then Meadow.Core, then Meadow.F7, then Meadow.Foundation.Core.

4. **Stub and iterate**: When a new P/Invoke function is needed, add it with a diagnostic stub
   first, then implement properly. The catch-all no-op stub logs unknown functions.

5. **Watch SDRAM**: After each new assembly is added, check that the emulator doesn't OOM.

### Phase 4 Cleanup (Optional, from Track 06)

If you have time, these Track 06 cleanup items are still pending:
- Remove P/Invoke resolve logging (currently every call is logged twice)
- Remove `interp_runtime_invoke` depth tracking diagnostic
- Replace catch-all no-op stub with abort for truly unknown functions

These are nice-to-have and can be deferred if Track 07 work is more pressing.
