# Track 07 Phase 1 Handoff — Threading Test + Console.WriteLine

## Status: COMPLETE

Threading test app passes all four tests with exit code 42, clean monovm_shutdown,
and **Console.WriteLine working end-to-end** (output visible in syslog via mirror).

## What Was Done

### 1. Ported 9 Upstream System.Native PAL Files for NuttX

Compiled directly from `runtime/src/native/libs/System.Native/` (not hand-written stubs):
- `pal_threading.c` — LowLevelMonitor, CreateThread, SchedGetCpu
- `pal_time.c` — GetTimestamp, GetBootTimeTicks, GetCpuUtilization
- `pal_memory.c` — Malloc, Free, Calloc, Realloc, AlignedAlloc
- `pal_random.c` — GetCryptographicallySecureRandomBytes
- `pal_errno.c` — ConvertError, GetErrNo, SetErrNo, StrErrorR
- `pal_string.c` — SNPrintF variants
- `pal_runtimeinformation.c` — GetOSArchitecture, GetUnixVersion
- `pal_log.c` — Log, LogError
- `pal_datetime.c` — GetSystemTimeAsTicks

NuttX-specific `#ifdef __NuttX__` patches in:
- `pal_threading.c`: Thread stack capped at 64KB; pthread_detach after create
- `pal_time.c`: No `<utime.h>`, no utimes/futimes, no getrusage
- `pal_memory.c`: MALLOC_SIZE fallback (no malloc_usable_size)
- `pal_errno.c`: Use set_errno() instead of `errno = x`

### 2. Added ~55 NuttX-Specific Stubs to mono_main.c

Signal stubs (no-op), UID/GID stubs (root), filesystem wrappers (pipe, dup, mkdir, etc.),
process path, hostname, fcntl operations (GetIsNonBlocking, SetIsNonBlocking).
Total mapping table: ~85 entries (up from 26).

### 3. Fixed Three Runtime Blockers

| Blocker | Root Cause | Fix |
|---------|-----------|-----|
| OOM "Could not allocate 8352 bytes" on ThreadPool thread | SDRAM exhausted: 16MB GC + 6MB CoreLib cache + 5×1MB interp stacks > 29MB | (a) Cap thread stacks at 64KB, (b) reduce interp stack to 256KB/thread, (c) disable CoreLib SDRAM caching |
| Memory fault on thread 19 (MMFAR=0x10) | `mono_valloc_aligned(1MB)` fails → NULL interp stack_pointer | Reduced INTERP_STACK_SIZE to 256KB |
| `sem_trywait failed with errno=0` ABORT | NuttX PROTECTED mode sem proxy doesn't propagate errno | Workaround in mono-os-semaphore.h: treat errno=0 as EAGAIN/ETIMEDOUT |

### 4. Fixed Console.WriteLine (Two Blockers)

| Blocker | Root Cause | Fix |
|---------|-----------|-----|
| ConsolePal.EnsureConsoleInitialized crash | `lock(Console.Out)` in `EnsureInitializedCore` and `WriteFromConsoleStream` triggers re-entrant Monitor.Enter on SyncTextWriter, which crashes in the Mono interpreter on NuttX | Rebuilt `System.Console.dll`: (a) `EnsureConsoleInitialized` just sets `s_initialized = true` (skip terminal/signal init), (b) `WriteFromConsoleStream` removed `lock(Console.Out)` and `mayChangeCursorPosition`, (c) `Write(fd, buffer, false)` skips `UpdatedCachedCursorPosition` |
| `write(1, ...)` returns EBADF | Emulator has CDCACM on fd 1 without USB host; `write()` returns EBADF | `sysn_write`: treat EBADF same as EAGAIN — return bufferSize (output mirrored to syslog) |

### 5. SDRAM Memory Budget (Current)

| Resource | Size | Notes |
|----------|------|-------|
| GC heap | 16MB max | max-heap-size=16m, nursery=512k, soft=8m |
| Interpreter stacks | ~1.5MB | 6 threads × 256KB each |
| Thread stacks | ~0.4MB | 6 threads × 64KB each |
| Assembly caches | ~0.1MB | Only <100KB forwarders cached |
| Mono code + BSS | 3MB | 0xC0000000–0xC0300000 |
| Mono metadata + other | ~4MB | malloc'd by runtime |
| **Total** | **~25MB** | **of 29MB available** |

## Files Modified

### Meadow repo
- `apps/examples/mono/mono_main.c` — ~55 new stubs, 85 mapping entries, GC 16MB, no CoreLib cache, syslog mirror on stdout, EBADF handling, `DOTNET_SYSTEM_CONSOLE_SKIP_TERMINAL_INIT` env var
- `apps/examples/mono/Makefile` — 9 PAL source files, VPATH, include paths, defines
- `apps/examples/mono/mono_nuttx_stubs.c` — (minor updates)

### runtime repo
- `src/libraries/System.Console/src/System/ConsolePal.Unix.cs` — NuttX patches:
  - `EnsureConsoleInitialized()`: skip terminal init, just set s_initialized=true
  - `WriteFromConsoleStream()`: removed lock(Console.Out), pass mayChangeCursorPosition=false
- `src/native/libs/System.Native/pal_threading.c` — 64KB stack cap, pthread_detach
- `src/native/libs/System.Native/pal_time.c` — NuttX guards for utime/getrusage
- `src/native/libs/System.Native/pal_memory.c` — MALLOC_SIZE NuttX fallback
- `src/native/libs/System.Native/pal_errno.c` — set_errno() for NuttX
- `src/mono/mono/mini/interp/interp-internals.h` — INTERP_STACK_SIZE=256KB
- `src/mono/mono/utils/mono-os-semaphore.h` — NuttX sem errno workaround

### Rebuilt managed assembly
- `System.Console.dll` — rebuilt from patched source via `runtime/.dotnet/dotnet build`
  - Deployed as IL-only (87KB vs 553KB original with ReadyToRun)
  - Built with: `dotnet build src/libraries/System.Console/src/System.Console.csproj -f net11.0-unix -c Release`

### Test app
- `conductor/tracks/mono-upgrade-07-blinky/threading-test/Program.cs`
- `conductor/tracks/mono-upgrade-07-blinky/threading-test/Meadow.csproj`

## Console.Write Path (Working)

```
Console.WriteLine("Hello from Meadow!")
  → SyncTextWriter.WriteLine (synchronized wrapper)
  → StreamWriter.WriteSpan → StreamWriter.Flush (AutoFlush)
  → UnixConsoleStream.Write
  → ConsolePal.WriteFromConsoleStream
  → ConsolePal.Write(fd=1, buffer, mayChangeCursorPosition=false)
  → Interop.Sys.Write(fd, bufPtr, count)
  → [P/Invoke] SystemNative_Write → sysn_write()
  → syslog mirror: "[mono stdout] Hello from Meadow!"
  → write(1, ...) → EBADF (emulator) → return bufferSize (success)
```

On real hardware with HCOM: write(1, ...) → FIFO → MonoStdxxx thread → UART4 → CLI.

## Next: Phase 2 (GPIO Test)

Write a minimal C# test that P/Invokes NuttX UPD driver directly (open/ioctl/close on `/dev/upd`)
to toggle a GPIO pin. No Meadow.Core dependency. See plan.md Phase 2.
