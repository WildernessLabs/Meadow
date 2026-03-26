# Track 06 Handoff: System.Native PAL + Hello World Execution

## Status: COMPLETE

Track 06 implemented the System.Native PAL layer for NuttX and achieved Console.WriteLine
output on the STM32F777 target running the .NET 10 Mono interpreter in the Renode emulator.

## What Works

- `Console.WriteLine("Hello from Meadow!")` produces visible output on UART/syslog
- `return 42` executes to completion — `mono_exec returned: exit_code=42`
- `monovm_shutdown` completes cleanly
- 26 System.Native P/Invoke functions implemented in `mono_main.c`
- 13 framework assemblies deployed (~8.3MB total) fitting within 32MB SDRAM budget
- Invariant globalization enabled — eliminates System.Globalization.Native dependency
- UseSystemResourceKeys=true — bypasses ResourceManager null-assembly crash

## Verified Output

```
[   14.713000] [13] [managed fd=1] Hello from Meadow!
[   16.609000] [13] mono_exec returned: ret=0x00000000 exit_code=42
[   16.610000] [13] monovm_shutdown complete, latched exit code: 42
```

## Implemented System.Native Functions (26)

### Environment
- `SystemNative_GetEnv` → `getenv()` wrapper with syslog
- `SystemNative_LChflagsCanSetHiddenFlag` → returns 0 (no HFS+)
- `SystemNative_CanGetHiddenFlag` → returns 0

### Console I/O
- `SystemNative_Write` → NuttX `write()` + syslog mirror for fd 0-2
- `SystemNative_Read` → NuttX `read()`
- `SystemNative_IsATty` → returns 1 for fd 0-2

### Terminal Handling (all stubs — no real terminal on NuttX)
- `SystemNative_InitializeTerminalAndSignalHandling` → returns 1
- `SystemNative_SetKeypadXmit` → no-op
- `SystemNative_SetTerminalInvalidationHandler` → no-op
- `SystemNative_UninitializeTerminal` → no-op
- `SystemNative_GetControlCharacters` → zeros output
- `SystemNative_GetWindowSize` → returns 80×24

### File Operations
- `SystemNative_Open` → NuttX `open()`
- `SystemNative_Close` → NuttX `close()`
- `SystemNative_FStat2` → NuttX `fstat()` + struct conversion
- `SystemNative_Stat2` → NuttX `stat()` + struct conversion
- `SystemNative_LStat2` → falls through to Stat2
- `SystemNative_LSeek` → NuttX `lseek()`
- `SystemNative_FcntlSetFdFlags` → NuttX `fcntl(F_SETFD)`
- `SystemNative_FcntlGetFdFlags` → NuttX `fcntl(F_GETFD)`

### errno Helpers
- `SystemNative_SetErrNo` → `set_errno()` (NuttX-specific, not `errno =`)
- `SystemNative_GetErrNo` → `get_errno()`
- `SystemNative_ConvertErrorPlatformToPal` → POSIX errno → .NET Error enum
- `SystemNative_StrErrorR` → `strerror()` wrapper

### Process / Diagnostics
- `SystemNative_SysLog` → NuttX `syslog()`
- `SystemNative_Abort` → `_exit(134)`
- `SystemNative_Exit` → `_exit(exitCode)`
- `SystemNative_GetPid` → `getpid()`

## Deployed Assemblies (13)

| Assembly | Size | Notes |
|----------|------|-------|
| System.Private.CoreLib.dll | 5.8 MB | Cached in SDRAM |
| System.Collections.dll | 642 KB | NOT cached (selective skip) |
| System.Console.dll | 540 KB | NOT cached |
| System.Memory.dll | 481 KB | NOT cached |
| System.Runtime.InteropServices.dll | 429 KB | NOT cached |
| System.Threading.dll | 400 KB | NOT cached |
| System.Runtime.dll | 45 KB | Cached (small forwarder) |
| System.Threading.Tasks.dll | 17 KB | Cached |
| System.Diagnostics.Debug.dll | 16 KB | Cached |
| System.IO.dll | 16 KB | Cached |
| Microsoft.Win32.Primitives.dll | 16 KB | Cached |
| System.Buffers.dll | 15 KB | Cached |
| Meadow.dll | 4 KB | Cached (app) |

Framework assemblies source: `runtime/.dotnet/shared/Microsoft.NETCore.App/11.0.0-preview.3.26161.119/`

## Memory Budget

| Resource | Setting | Notes |
|----------|---------|-------|
| GC heap | max=8MB, nursery=512KB, soft-limit=4MB | marksweep, no concurrent |
| Interp stack | 1MB | Reduced from 2MB to save SDRAM |
| Task stack | 1MB | `CONFIG_EXAMPLES_MONO_STACKSIZE` |
| SDRAM caching | Selective | Skip assemblies 100KB–1MB |
| Total SDRAM | ~32MB | Tight — monitor when adding assemblies |

## Known Issues

1. **Console.Write throws IOException (EBADF) in emulator**: stdout (fd 1) is NuttX CDCACM
   with no USB host attached. The sysn_write mirror writes to syslog successfully, but the
   CDCACM driver returns EBADF. The hello world app catches this with try/catch. On real
   hardware with USB, this should work without the try/catch.

2. **P/Invoke resolve logging is verbose**: Every P/Invoke call is logged twice (once per
   resolution). This should be removed or gated behind a debug flag in Phase 4 cleanup.

3. **Catch-all no-op stub still active**: Unknown System.Native functions return a no-op
   stub that returns 0. This should be replaced with a proper error/abort for truly unknown
   functions once all needed functions are mapped.

4. **interp_runtime_invoke depth tracking**: Diagnostic logging in interp.c should be removed.

## Key Blockers Resolved During Track 06

1. **NuttX struct stat missing fields**: `st_uid`, `st_gid`, `st_ino`, `st_dev`, `st_rdev`
   don't exist in NuttX's minimal stat. Replaced with constant 0.

2. **OOM with 8.3MB assemblies**: All assemblies were SDRAM-cached, exhausting 32MB. Fixed
   with selective caching (skip 100KB–1MB assemblies) + reduced GC/interp allocations.

3. **Missing IIIIIIIII trampoline**: Console's terminal handling requires a P/Invoke with
   8 int/ptr parameters. Added to `nuttx_m2n_invoke.g.h`.

4. **Debug.Assert crash on MainAssembly == null**: ResourceManager created with null assembly
   during string resource loading. Fixed with `UseSystemResourceKeys=true`.

5. **LittleFS v1 vs v2**: NuttX uses LFS v1 (0x00010001). The Python `build_littlefs_image.py`
   creates v2 which NuttX silently formats over, losing all files. MUST use
   `tools/build_lfs_v1_image` (C tool).

6. **NuttX errno assignment**: `errno = value` doesn't compile on NuttX. Must use
   `set_errno()`/`get_errno()`.

## Key Files Modified

| File | Change |
|------|--------|
| `apps/examples/mono/mono_main.c` | 26 System.Native P/Invoke stubs, monovm properties, selective SDRAM caching |
| `runtime/src/mono/mono/mini/nuttx_m2n_invoke.g.h` | Added IIIIIIIII trampoline (9-int-arg) |
| `runtime/src/mono/mono/mini/interp/interp-internals.h` | INTERP_STACK_SIZE 1MB on NuttX |
| `conductor/tracks/mono-upgrade-05-bcl/hello-world/` | Hello World test app (Program.cs + Meadow.csproj) |

## What Track 07 Needs

Track 07 (Blinky) requires:

1. **Major System.Native PAL expansion** — Track 06 implemented 26 of 265 functions. Blinky's
   BCL dependencies (System.Threading, System.IO, System.Timers) will need threading PAL
   (CreateThread, LowLevelMonitor_*), time PAL (GetTimestamp, GetSystemTimeAsTicks), expanded
   file I/O (Pipe, Poll, Dup, ReadDir, MkDir), memory (Malloc/Free), random, and signal stubs.
   The upstream source at `runtime/src/native/libs/System.Native/pal_*.c` is the reference —
   most are standard POSIX that NuttX supports. Consider building the upstream files directly
   for NuttX rather than maintaining parallel stubs in mono_main.c.

2. **Meadow.Core retarget to net10.0** — the managed hardware abstraction layer

3. **DllImport("nuttx") P/Invoke mappings** — `mappings-meadow.h` already has UPD/GPIO stubs

4. **More assemblies** — Meadow.Core, Meadow.Contracts, Meadow.Foundation, etc.

5. **Memory management** — adding Meadow.Core assemblies will stress the 32MB SDRAM budget
