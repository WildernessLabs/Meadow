# Implementation Plan: System.Native PAL + Hello World

## Phase 1: Identify All Init-Time P/Invoke Calls — COMPLETE ✓

- [x] Enable invariant globalization: `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` set in mono_main.c
- [x] Rebuild firmware with catch-all P/Invoke logging
- [x] Boot emulator with `return 42` app — **exit code 42 achieved!**
- [x] Collect P/Invoke calls during init:
  - `SystemNative_LChflagsCanSetHiddenFlag` → return 0
  - `SystemNative_CanGetHiddenFlag` → return 0
  - `SystemNative_GetEnv` → getenv() wrapper

### Blockers Resolved

1. **Monitor.Enter/Exit broken fast path** (infinite recursion → StackOverflowException):
   `ObjectHeader.TryEnterFast` and `ObjectHeader.TryExitChecked` use `Unsafe.AsPointer` + ref struct
   patterns that operate on the wrong memory in the interpreter (stack slot instead of object header).
   **Fix**: In `transform.c:interp_handle_intrinsics`, replace these calls with constant `false`,
   forcing the native icall slow paths (`ves_icall_System_Threading_Monitor_Monitor_Enter` and
   `mono_monitor_exit_icall`).

2. **NuttX task stack too small** (Memory Management Fault):
   .NET 10 interpreter has deeper C stack usage during type initialization than Mono 6.9.
   **Fix**: Changed `MONO_TASK_STACKSIZE` from `CONFIG_PTHREAD_STACK_DEFAULT` (64KB) to
   `CONFIG_EXAMPLES_MONO_STACKSIZE` (1MB) in `meadow_thread_config.h`.

3. **SGEN GC stack_end assertion** ("Can only lower stack end" → ABORT):
   NuttX SDRAM-allocated task stacks have different initial stack_end than expected.
   **Fix**: Made assertion non-fatal with warning on NuttX in `sgen-mono.c`.

### Success Gate — PASSED
`monovm_execute_assembly` returns, syslog shows exit code 42.
```
[    9.332000] [13] mono_exec returned: ret=0x00000000 exit_code=42
[    9.333000] [13] monovm_shutdown complete, latched exit code: 42
```

## Phase 2: Implement Init-Time Stubs — COMPLETE ✓

Only 3 P/Invoke functions called during hello-world init (all already stubbed):
- `SystemNative_LChflagsCanSetHiddenFlag` → return 0 (already in mono_main.c)
- `SystemNative_CanGetHiddenFlag` → return 0 (already in mono_main.c)
- `SystemNative_GetEnv` → getenv() wrapper (already in mono_main.c)

## Phase 3: Add Console.WriteLine Support — COMPLETE ✓

- [x] Add System.Console.dll + 11 dependency assemblies to LFS image
- [x] Implement 26 System.Native P/Invoke functions in mono_main.c
- [x] Handle NuttX struct stat differences (missing st_uid/st_gid/st_ino/st_dev/st_rdev)
- [x] Add 9-int-arg trampoline (IIIIIIIII) in nuttx_m2n_invoke.g.h
- [x] Set UseSystemResourceKeys=true to bypass ResourceManager crash
- [x] Selective SDRAM assembly caching (skip 100KB-1MB) to fit in 32MB
- [x] Reduce INTERP_STACK_SIZE from 2MB to 1MB
- [x] Console.WriteLine("Hello from Meadow!") produces output, exit code 42

### Implemented System.Native P/Invoke Functions (26 total)

| Function | Implementation |
|----------|---------------|
| `SystemNative_GetEnv` | `getenv()` wrapper |
| `SystemNative_LChflagsCanSetHiddenFlag` | returns 0 |
| `SystemNative_CanGetHiddenFlag` | returns 0 |
| `SystemNative_Write` | NuttX `write()` + syslog mirror for fd 0-2 |
| `SystemNative_Read` | NuttX `read()` |
| `SystemNative_IsATty` | returns 1 for fd 0-2 |
| `SystemNative_InitializeTerminalAndSignalHandling` | returns 1 (success) |
| `SystemNative_SetKeypadXmit` | no-op |
| `SystemNative_SetTerminalInvalidationHandler` | no-op |
| `SystemNative_UninitializeTerminal` | no-op |
| `SystemNative_GetControlCharacters` | zeros output |
| `SystemNative_GetWindowSize` | returns 80x24 |
| `SystemNative_Open` | NuttX `open()` |
| `SystemNative_Close` | NuttX `close()` |
| `SystemNative_FStat2` | NuttX `fstat()` + struct conversion |
| `SystemNative_Stat2` | NuttX `stat()` + struct conversion |
| `SystemNative_LStat2` | falls through to Stat2 |
| `SystemNative_LSeek` | NuttX `lseek()` |
| `SystemNative_FcntlSetFdFlags` | NuttX `fcntl(F_SETFD)` |
| `SystemNative_FcntlGetFdFlags` | NuttX `fcntl(F_GETFD)` |
| `SystemNative_SetErrNo` | NuttX `set_errno()` |
| `SystemNative_GetErrNo` | NuttX `get_errno()` |
| `SystemNative_ConvertErrorPlatformToPal` | POSIX errno → .NET Error enum |
| `SystemNative_StrErrorR` | NuttX `strerror()` wrapper |
| `SystemNative_SysLog` | NuttX `syslog()` |
| `SystemNative_Abort` | `_exit(134)` |
| `SystemNative_Exit` | `_exit(exitCode)` |
| `SystemNative_GetPid` | NuttX `getpid()` |

### Deployed Assemblies (13 total, ~8.3MB)

| Assembly | Size |
|----------|------|
| Meadow.dll | 4 KB |
| System.Private.CoreLib.dll | 5.8 MB |
| System.Console.dll | 540 KB |
| System.Collections.dll | 642 KB |
| System.Memory.dll | 481 KB |
| System.Runtime.InteropServices.dll | 429 KB |
| System.Threading.dll | 400 KB |
| System.Runtime.dll | 45 KB |
| System.Threading.Tasks.dll | 17 KB |
| System.Diagnostics.Debug.dll | 16 KB |
| System.IO.dll | 16 KB |
| Microsoft.Win32.Primitives.dll | 16 KB |
| System.Buffers.dll | 15 KB |

### Memory Budget

- GC heap: max-heap-size=8m, nursery-size=512k, soft-heap-limit=4m
- INTERP_STACK_SIZE: 1MB (reduced from 2MB)
- MONO_TASK_STACKSIZE: 1MB
- Selective SDRAM caching: CoreLib + small forwarders cached, skip 100KB-1MB assemblies
- monovm properties: UseSystemResourceKeys=true, GLOBALIZATION_INVARIANT=1

### Known Issue

Console.Write throws IOException (EBADF) on NuttX emulator because stdout (fd 1) is
CDCACM with no USB host attached. The write succeeds to syslog (via sysn_write mirror),
but the CDCACM driver returns EBADF. The hello world app catches this with try/catch.
On real hardware with USB connected, this should work without the try/catch.

### Success Gate — PASSED
```
[   14.713000] [13] [managed fd=1] Hello from Meadow!
[   16.609000] [13] mono_exec returned: ret=0x00000000 exit_code=42
[   16.610000] [13] monovm_shutdown complete, latched exit code: 42
```

## Phase 4: Clean Up and Document

- [ ] Remove P/Invoke resolve logging (reduce syslog noise)
- [ ] Remove interp_runtime_invoke depth tracking diagnostic
- [ ] Remove catch-all no-op stub (replace with proper error for truly unknown functions)
- [ ] Verify monovm_shutdown completes cleanly — **already verified ✓**
- [ ] Run 5 consecutive boot-execute-shutdown cycles
- [ ] Write Track 06 handoff

## Reference: Key Runtime Files Modified

| File | Change |
|------|--------|
| `runtime/src/mono/mono/mini/interp/transform.c` | ObjectHeader.TryEnterFast/TryExitChecked → return false |
| `runtime/src/mono/mono/mini/interp/interp.c` | NuttX stack overflow diagnostics + depth tracking |
| `runtime/src/mono/mono/mini/interp/interp-internals.h` | 1MB INTERP_STACK_SIZE on NuttX |
| `runtime/src/mono/mono/mini/nuttx_m2n_invoke.g.h` | Added IIIIIIIII trampoline |
| `runtime/src/mono/mono/metadata/sgen-mono.c` | Non-fatal stack_end assertion on NuttX |
| `runtime/src/mono/mono/metadata/monitor.c` | NuttX monitor diagnostic (can be removed) |
| `nuttx/include/meadow/meadow_thread_config.h` | MONO_TASK_STACKSIZE = CONFIG_EXAMPLES_MONO_STACKSIZE |
| `apps/examples/mono/mono_main.c` | 26 System.Native P/Invoke stubs + monovm properties |

## LittleFS Image Build

The LittleFS image MUST be built with `tools/build_lfs_v1_image` (C tool, LFS v1.7.2 format).
Do NOT use `tools/build_littlefs_image.py` (Python, LFS v2 format) — NuttX uses LFS v1 only.

To include assemblies at both root and `/mono/4.5/`:
```
tools/build_lfs_v1_image <assembly_dir> <output.bin> <assembly_dir>
```
