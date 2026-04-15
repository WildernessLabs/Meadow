# Meadow.OS — .NET 10 Mono Runtime Upgrade

## Project Overview

Upgrading Meadow.OS firmware from Mono 6.9.0 to .NET 10 Mono (dotnet/runtime).
Target: STM32F777 (Cortex-M7, Thumb2-only, 64MB QSPI flash, 32MB SDRAM, NuttX RTOS).

## Repository Layout

| Directory | Purpose |
|-----------|---------|
| `Meadow/` (this repo) | NuttX firmware + legacy Mono + mbedtls |
| `Meadow.OS.Emulator/` (sibling) | Renode emulator platform, hooks, build script |
| `runtime/` (sibling) | dotnet/runtime fork with .NET 10 Mono + NuttX patches |
| `conductor/` | Project management: tracks, plans, specs, workflow |

## Getting Started

Read these files first:
1. `conductor/tracks.md` — Track registry (what's done, what's next)
2. `conductor/workflow.md` — Build process, prerequisites, emulator usage
3. The current track's `handoff.md` or `plan.md` — Where the previous session left off

## Critical Rules

- **Build firmware**: Always use `Meadow.OS.Emulator/build-meadow.os-emulated.sh`. Never run `make` directly in `nuttx/`.
- **After rebuild**: The build script generates `mono_bss_zero.bin` and `addresses.resc` automatically. If `.mono_bss` address/size changes, update `meadow-dotnet10-headless.resc` (see workflow.md).
- **Renode**: Lives at `/Users/lexas/renode/renode` (not on PATH). Use `--port <N>` mode, not `--console`, for background runs. Pick a NEW port each run to avoid "Address already in use".
- **Managed assemblies**: Deploy to LFS image using `tools/build_lfs_v1_image` (C tool). **NEVER use the Python `build_littlefs_image.py`** — it creates LFS v2 which NuttX silently formats over, losing all files.
- **P/Invoke**: All native function resolution goes through `meadow_pinvoke_override` in `mono_main.c`. No dlopen on NuttX.
- **NuttX errno**: Use `set_errno()`/`get_errno()`, not direct `errno` assignment (doesn't compile on NuttX).

## Current State (as of Track 12 completion)

- **Tracks 01-12: COMPLETE** (build, platform, emulator, monovm, trampolines, PAL, Blinky, HW, JIT fixes, Thumb2 JIT, TLS, networking)
- **Execution: ARM Thumb2 JIT** (not interpreter — interpreter compiled in as fallback only)
- All 6 HW tests pass: NetworkInterface, DNS, GC, HTTP, HTTPS×2
- ~120 System.Native P/Invoke functions mapped in `mono_main.c` (zero unmapped gaps)
- 80 framework assemblies deployed (~41MB on flash), selective SDRAM caching
- Invariant globalization enabled, UseSystemResourceKeys=true
- .NET thread pool working (gate thread, workers, hill climbing)
- Meadow.Core + Meadow.F7 multi-targeted for netstandard2.1 + net10.0

### Memory Configuration

| Resource | Value | Notes |
|----------|-------|-------|
| GC heap | max=8MB, nursery=512KB, soft=4MB | marksweep, no concurrent |
| Interp fallback stack | 1MB | `INTERP_STACK_SIZE` — used when JIT falls back to interpreter |
| Task stack | 1MB | `CONFIG_EXAMPLES_MONO_STACKSIZE` |
| SDRAM caching | Selective | Skip assemblies 100KB–1MB to save SDRAM |
| Total SDRAM | ~32MB | **Tight** — adding assemblies may OOM |

### monovm Properties

```c
TRUSTED_PLATFORM_ASSEMBLIES         // colon-separated list of all /meadow0/*.dll
APP_PATHS                           // /meadow0
NATIVE_DLL_SEARCH_DIRECTORIES       // /meadow0
PINVOKE_OVERRIDE                    // address of meadow_pinvoke_override
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT  // "1"
System.Resources.UseSystemResourceKeys // "true"
```

## System.Native PAL Port (Cross-Cutting)

The .NET BCL routes ALL POSIX operations through `libSystem.Native` — a C shim library with
**265 exported functions** across 41 source files. On normal Linux/macOS it's a shared .so;
on NuttX it must be statically linked via the P/Invoke override.

**Upstream source**: `runtime/src/native/libs/System.Native/pal_*.c`

**Current state**: ~120 functions mapped in `mono_main.c`. Most are wired to upstream
`libSystem.Native.a` implementations. Unknown functions return NULL → `EntryPointNotFoundException`.

**What triggers them**: Not Meadow.Core directly, but the BCL assemblies it depends on.
`System.Threading`, `System.IO`, `System.Net`, `System.Collections`, etc. all call System.Native
internally. Every new BCL namespace pulled in will surface new required System.Native functions.

**Key NuttX gaps** (functions that can't be ported directly):
- `fork`/`exec` — NuttX uses `task_create`/`posix_spawn` instead
- `mmap` — NuttX has limited mmap support
- `signals` — NuttX signal support is partial
- `sockets` — NuttX has BSD sockets but with some differences
- `symlinks` — NuttX typically doesn't support them

**Strategy**: Each track should expand System.Native coverage for its needs. Long-term, consider
building the upstream `pal_*.c` files directly for NuttX with `#ifdef __NuttX__` blocks, rather
than maintaining parallel stubs in mono_main.c.

## Build Environment

Requires: `arm-none-eabi-gcc` 10.3, `gmake` 4.x+, `ld.lld`, Python 3 + `pyyaml` + `kconfiglib`, `dotnet` SDK 9+.
See `conductor/workflow.md` for install commands.

## Building the LFS Image

```bash
# Copy assemblies to staging
cp <assemblies> Meadow.OS.Emulator/build/dotnet10/assemblies/

# Build LFS v1 image with files at BOTH root and /mono/4.5/
Meadow.OS.Emulator/tools/build_lfs_v1_image \
    build/dotnet10/assemblies \
    build/dotnet10/littlefs.bin \
    build/dotnet10/assemblies
```

The third arg (app_dir) puts .dll files at root `/` — needed because firmware checks `/meadow0/System.Private.CoreLib.dll` at startup.

Framework assemblies source: `runtime/.dotnet/shared/Microsoft.NETCore.App/11.0.0-preview.3.26161.119/`

## Key Firmware Files

| File | What it does |
|------|-------------|
| `apps/examples/mono/mono_main.c` | Mono startup: TPA, monovm_initialize, 26 System.Native P/Invoke stubs, selective SDRAM caching |
| `apps/examples/mono/mono_nuttx_stubs.c` | POSIX stubs for NuttX (dlopen, pthread, etc.) |
| `apps/examples/mono/mappings-meadow.h` | P/Invoke mapping table for NuttX driver calls (DllImport("nuttx")) |

## Key Runtime Files

| File | What it does |
|------|-------------|
| `runtime/src/mono/mono/mini/mini-arm.h` | ARM JIT trampoline macro configuration for NuttX |
| `runtime/src/mono/mono/mini/interp/interp.c` | Interpreter fallback paths (used when JIT can't handle a method) |
| `runtime/src/mono/mono/mini/interp/interp-internals.h` | INTERP_STACK_SIZE (1MB on NuttX) for interpreter fallback |
| `runtime/src/mono/mono/mini/interp/transform.c` | Monitor.TryEnterFast/TryExitChecked → return false (interpreter fallback fix) |
| `runtime/src/mono/mono/metadata/sgen-mono.c` | Non-fatal GC stack_end assertion on NuttX |

## Known Issues / Gotchas

1. **Console.Write throws IOException in emulator** — CDCACM returns EBADF without USB host. Output still appears via syslog mirror. App needs try/catch.
2. **SslGetPeerCertificate returns NULL** — mbedTLS shim doesn't extract the peer cert for managed code. Requires `ServerCertificateCustomValidationCallback` for HTTPS. mbedTLS verifies certs natively during handshake.
3. **Diagnostic logging is verbose** — P/Invoke resolve + runtime_invoke depth logging should be removed/gated for production.
4. **NuttX struct stat is minimal** — Missing st_uid, st_gid, st_ino, st_dev, st_rdev. FileStatus fields set to 0.
