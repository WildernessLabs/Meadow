# Track 07: Blinky on Interpreter — Handoff

## Status: COMPLETE

Full Meadow.Core Blinky running on .NET 10 Mono interpreter in Renode emulator.
`MeadowOS.Main()` → `FindAppType()` → `App<F7FeatherV2>` → DigitalOutputPort → 10 LED blink cycles.

### What Works

1. **Full MeadowOS.Main → App<F7FeatherV2>** — Platform detection, app type discovery, F7 device initialization, GPIO via UPD driver, managed Console.WriteLine via HCOM.

2. **Threading** — .NET thread pool (gate thread, worker threads, hill climbing), `Thread.Sleep`, `LowLevelMonitor_*` (pthread condition variables), `CreateThread`.

3. **Assembly reflection** — `Assembly.GetTypes()` works on all deployed assemblies (Meadow.Contracts: 454 types, Meadow.F7: 208 types, App.dll: 2 types). Fixed by scalar vtype interpreter fix.

4. **80 framework assemblies deployed** (~41MB on flash), including System.Net.*, System.Security.Cryptography.*, System.Text.*, System.Data.Common, System.Private.Xml.

5. **Console.WriteLine via HCOM** — stdout/stderr redirected to `/dev/monostdout` FIFO → HCOM TCP:4242.

### Key Fixes This Track

| Fix | Repo | File(s) | Description |
|-----|------|---------|-------------|
| Scalar vtype interpreter fix | runtime | `interp/interp.c` | `mini_interp_is_scalar_vtype()` detects single-field value type structs (e.g. ObjectHandleOnStack) and classifies as PINVOKE_ARG_SCALAR_VTYPE. Gated on `__NuttX__ && DISABLE_JIT`. |
| NuttX platform detection | Meadow.Core | `MeadowOS.cs` | Check `RuntimeInformation.OSDescription.StartsWith("NuttX")` before Linux check. .NET 10 SPCL reports NuttX as Linux. |
| Thread stack size | runtime | `CMakeLists.txt` | Reduce MONO_DEFAULT_STACKSIZE from 1MB to 256KB on NuttX (32MB SDRAM is tight). |
| net9.0 multi-targeting | Meadow.Core | `.csproj` files | Meadow.Core + Meadow.F7 build for both netstandard2.1 and net9.0. Cloud/sqlite/MQTTnet conditionally excluded. |
| Exception handling guards | runtime | `mini-exceptions.c`, `reflection.c` | NULL guards for captured_traces walk and mono_method_get_object_handle. |
| System.Native PAL expansion | Meadow | `mono_main.c` | ReadDir, ChDir, RmDir, FChMod, PRead, PWrite, signal stubs, Stat/LStat/FStat aliases. |
| LILI trampoline | runtime | `nuttx_m2n_invoke.g.h` | For `lseek(int, int64, int) -> int64`. |

### Assembly Layout

```
/meadow0/  (80 assemblies, ~41MB on QSPI flash)
├── App.dll                 (BlinkyCS: MeadowApp : App<F7FeatherV2>)
├── Meadow.dll              (Meadow.Core — entry assembly)
├── Meadow.Contracts.dll
├── Meadow.F7.dll
├── Meadow.Logging.dll
├── Meadow.Units.dll
├── MicroJson.dll
├── System.Private.CoreLib.dll
├── System.Console.dll
├── System.Net.NetworkInformation.dll
├── System.Net.Primitives.dll
├── System.Security.Cryptography.dll
├── ... (68 more framework assemblies)
└── meadow.config.yaml     (MonoControl: --interp)
```

### Memory Budget

| Resource | Value | Notes |
|----------|-------|-------|
| GC heap | max=8MB | marksweep, no concurrent |
| Thread stacks | 256KB each | Main=1MB, workers=256KB |
| Assemblies on flash | ~41MB | Only loaded to SDRAM on demand |
| SDRAM selective caching | Skip 100KB-1MB | Saves SDRAM for GC/threads |
| Total SDRAM | 32MB | Tight but functional |

### Known Issues

1. **Cloud services throw** — `NotSupportedException("Cloud services not available on this target")` — expected, gated behind `#if NETSTANDARD2_1`.

2. **sysconf() warning** — "Your operating system's sysconf (3) function doesn't correctly report physical memory size!" — cosmetic, doesn't affect operation.

3. **`app.config.yaml` missing** — Falls back to defaults. No impact.

4. **LFS image stores assemblies at root only** — Use `mkdir -p /tmp/empty_bcl && tools/build_lfs_v1_image /tmp/empty_bcl <output> <app_dir>` to avoid doubling.

### Build & Test Commands

```bash
# Rebuild Mono library (after runtime changes)
cd runtime/src/mono/build-nuttx-debug && cmake --build . -- -j$(sysctl -n hw.ncpu)

# Rebuild firmware
cd Meadow.OS.Emulator && bash build-meadow.os-emulated.sh --clean

# Rebuild Meadow.Core (after MeadowOS.cs changes)
cd Meadow.Core && dotnet build source/Meadow.Core/Meadow.Core.csproj -c Release

# Build LFS (assemblies at root only)
mkdir -p /tmp/empty_bcl
tools/build_lfs_v1_image /tmp/empty_bcl build/dotnet10/littlefs.bin build/dotnet10/assemblies

# Run in Renode
cd Meadow.OS.Emulator
/Users/lexas/renode/renode --disable-gui --port 43399 --plain \
    -e "i @scripts/meadow-dotnet10-headless.resc" > /tmp/renode-console.log 2>&1 &

# Check output
tail -f /tmp/renode-uart-dotnet10.txt            # syslog
cat /tmp/renode-uart4-raw.txt | strings          # HCOM managed output
```
