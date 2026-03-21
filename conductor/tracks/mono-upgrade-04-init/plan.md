# Implementation Plan: monovm Hosting API Integration

## Repos and Branches

| Repo | Branch | Path |
|------|--------|------|
| Meadow (NuttX firmware) | `Feature_NuttX_dotnet10` | `Wilderness_Labs/Meadow/` |
| Meadow.OS.Emulator | `feature/dotnet10-emulator` | `Wilderness_Labs/Meadow.OS.Emulator/` |
| runtime (.NET 10 Mono) | default | `Wilderness_Labs/runtime/` |

Mono native build output: `runtime/src/mono/build-nuttx-debug/`

## Phase 1: Study the API ✅
- [x] Read `monovm.c` in detail — understand `monovm_initialize`, `monovm_execute_assembly`, `monovm_shutdown`
- [x] Read `mono-private-unstable-functions.h` — get the function declarations
- [x] Study how Android/iOS targets call monovm (look at `src/mono/sample/` for examples)
- [x] Understand TPA list format (colon-separated full paths on NuttX, not semicolon — G_SEARCHPATH_SEPARATOR_S)
- [x] Understand PINVOKE_OVERRIDE mechanism — function pointer cast to string via snprintf, parsed back via strtoull

## Phase 2: Create Meadow Mono Entry Point ✅
- [x] Rewrite `mono_main.c` with new monovm hosting API entry point (`meadow_mono_main`)
- [x] Implement TPA list builder: enumerate `/meadow0/*.dll` and build colon-separated string
- [x] Implement property array setup (keys + values for monovm_initialize)
- [x] Implement P/Invoke override function (`meadow_pinvoke_override`) for NuttX native library resolution
- [x] Call `monovm_initialize` → `monovm_execute_assembly` → `monovm_shutdown`
- [x] Add error handling and syslog output at each stage
- [x] Update `Make.defs` for .NET 10 libraries (libmonosgen-2.0.a + component libs)
- [x] Update `user-space.ld` EXCLUDE_FILE patterns and .mono section for new library names
- [x] Create `mono_nuttx_stubs.c` with stubs for missing symbols (crash handlers, POSIX, math, libc)
- [x] Rename entry point to `meadow_mono_main` to avoid symbol conflict with runtime's internal `mono_main`

### Build notes
- `mono_main` symbol conflict: .NET 10 libmonosgen-2.0.a exports its own `mono_main` (driver.c:1968), so our entry point is `meadow_mono_main`
- System.Native PAL (mappings-system-native.h) deferred — legacy corefx implementations not yet ported
- mbedtls and sqlite mappings similarly deferred
- `pppd` and `ntpc_start` were pre-existing missing symbols (unrelated to mono)
- Final ELF: 524KB NuttX text + 2.2MB mono in SDRAM .mono section

## Phase 3: Update HCOM Integration ✅
- [x] Update `hcom_mono_control.c`: replace `mono_main` extern with `meadow_mono_main`
- [x] Update `mono_main_proxy` to call `meadow_mono_main` instead of `mono_main`
- [x] Update required-file validation: `System.Private.CoreLib.dll` replaces `mscorlib.dll`, `System.Core.dll`, `System.dll`
- [x] Add `#include <sys/wait.h>` for pre-existing `waitpid` implicit declaration
- [ ] Remove or update version matching logic (old mono version format may not apply)
- [ ] Update `MONO_TASK_STACKSIZE` if needed (new mono may need more stack)

## Phase 4: Emulator Validation (monovm_initialize succeeds, interpreter mode needed)
- [x] Extract `nuttx_user.bin`, `nuttx_kernel.bin`, `nuttx_vectors.bin`, `nuttx_mono.bin` for emulator
- [x] Regenerate hook addresses from new ELFs (`extract-hook-addresses.py`)
- [x] Remove BBR bit 0x800 mono-disable from `meadow-dotnet10-headless.resc`
- [x] Disable UartRxBypass.cs (API incompatible with Renode 1.16.1 — pre-existing issue)
- [x] Boot firmware in emulator — NuttX boots fully, HCOM up in ~20s wall time
- [x] Verify HCOM works (hcom_test.py GET_DEVICE_INFORMATION) — **PASS**
- [x] Mono startup attempted — correctly blocked by missing files check:
      `"Mono will not start - the following files are missing: System.Private.CoreLib.dll, Meadow.dll"`
- [x] Build `System.Private.CoreLib.dll` from `runtime/src/mono/System.Private.CoreLib/`
      Built via: `./build.sh -c Debug -subset Mono.CoreLib` (5.76 MB output)
- [x] Deploy SPCL + Meadow.dll to emulator LittleFS image
      Used `tools/build_lfs_v1_image` with app_dir param to place DLLs at LFS root
      Enabled `sysbus LoadBinary @build/dotnet10/littlefs.bin 0x90500000` in Renode script
- [x] Fix stale `nuttx_mono.bin` — old binary had wrong code at monovm_initialize offset
      Re-extracted: `objcopy -O binary --only-section=.mono --only-section=.mono_data`
- [x] Verify `monovm_initialize` succeeds — **PASS** (confirmed via GDB backtrace)
      monovm_initialize returned 0; code proceeded through hcom_mono_ctrl_mono_appears_to_be_running
- [x] Fix heap corruption on second boot (SYSRESETREQ) — **FIXED**
      Root cause: `.mono_bss` stale pointers. Emulator fix: targeted WriteDoubleWord zeroing
      of 8 key statics in reset macro. Firmware fix: .mono_bss zeroing in mono_main.c.
- [x] Verify full init chain via GDB breadcrumbs:
      monovm_initialize(0) → mono_appears_running → chdir → monovm_execute_assembly → mono_main
- [x] Identify `mono_main` crash — **FOUND**: `exit(1)` at driver.c:~2445
      Root cause: runtime built with `DISABLE_JIT` (interpreter/AOT only).
      `mono_main` checks `mono_aot_only` (0xC0269F98) and `mono_use_interpreter` (0xC0269FC4).
      Both are 0 → prints "This runtime has been configured with --enable-minimal=jit,
      so the --full-aot command line option is required." → `exit(1)` → SYSRESETREQ.
- [x] **Enable interpreter mode** — added `MONO_ENV_OPTIONS=--interpreter` in `mono_main.c` (9f9b5b7cd38)
      Also set `mono_use_interpreter` via Renode WriteDoubleWord (emulator-only backup).
      Confirmed `mini_init` reached on boot 1 via Renode breadcrumb hooks.
- [x] Firmware rebuilt with BSS zeroing + interpreter mode (via `build-meadow.os-emulated.sh`)
      Build output in `build/dotnet10/`, hooks regenerated with new addresses.
- [x] **Traced crash inside mini_init** — narrowed via Renode breadcrumbs:
      `mini_init` → `mono_trampolines_init` → `create_trampoline_code` → `mono_arch_create_generic_trampoline`
      Crash is in `mono_arch_create_generic_trampoline` (0xC01A0A5C) which emits ARM machine
      code into a dynamically allocated buffer. This requires writable+executable (WX) memory.
      On NuttX with MPU, WX memory allocation likely fails — `mono_code_manager_new()` allocates
      via `mono_valloc` which falls back to `posix_memalign` (no PROT_EXEC on NuttX).
- [x] **Fix trampoline code generation** — `mono_arch_create_generic_trampoline` was stubbed
      with `g_assert_not_reached()` (DISABLE_JIT build). Added `HOST_NUTTX` to `disable_tramps`
      in `mono_trampolines_init` (same as WASM). Not a WX memory issue — trampolines not needed
      in interpreter mode.
- [x] **Fix signal number mismatch** — Mono compiled with Meadow.OS headers (SIGRTMIN=32,
      MAX_SIGNO=63) but firmware only supports signals 0-31. `sigaction(33)` returned -EINVAL.
      Added `HOST_NUTTX` signal selection (signals 20/21/22) in `mono-threads-posix-signals.c`.
- [x] **mono_init (CORLIB load) reached** — confirmed via Renode breadcrumbs. No crashes,
      no reset loop. System stable, Mono actively loading System.Private.CoreLib.dll.
- [x] **Assembly pre-caching to tmpfs** — firmware copies DLLs from `/meadow0/` (QSPI/LFS)
      to `/tmp/` (tmpfs in RAM) before `monovm_execute_assembly`. Same pattern as
      `hcom_via_nx_copy_mono_runtime_to_ram` for the native binary. QSPI caching completes
      in ~5 seconds under emulation. Requires `CONFIG_FS_TMPFS` (enabled in build script).
- [x] **Identified sem_wait deadlock in mono_init_internal** — GDB stack dump shows Mono task
      blocked in `sem_wait` inside `mono_init_internal` → `sgen_gc_init` → `sgen_thread_pool_start`.
      Root cause: NuttX `pthread_create` holds `sched_lock()` while waiting for child thread to
      signal, but child can't run because scheduler lock prevents context switching. Hard deadlock.
      - Legacy Mono 6.9 avoided this: no GC threads created during `mini_init()`
      - .NET 10 sgen creates worker threads eagerly in `sgen_gc_init()`
      - NuttX `sched_lock` + `sem_wait` interaction is fundamentally incompatible with this pattern
      - WASM handles same issue via `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1` (compiles out thread pool)
- [x] **Fix: disable concurrent GC for NuttX** — added `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1`
      to `build-nuttx.sh`. GC runs on main thread (correct for single-core Cortex-M7).
- [x] **Fix: struct stat ABI mismatch** — Mono compiled against wrong NuttX headers.
      Switched `build-nuttx.sh` to use `Meadow/nuttx/include` (firmware headers).
      Added compat layer: `nuttx-compat.h`, `nuttx-include-overrides/errno.h`.
- [x] **Fix: mono_pagesize() returning -1** — NuttX `sysconf(_SC_PAGESIZE)` unsupported.
      Fixed fallback to save the default value (4096) instead of returning it without caching.
- [x] **Fix: HAVE_MMAP=1 with mmap/munmap stubs** — Required for `mono_file_map_fileio`
      fallback. Stubs in `mono_nuttx_stubs.c` use `posix_memalign`+`read`/`free`.
- [ ] **Fix: munmap crash in mm_free** — `munmap` → `free()` hits NULL pointer (MMFAR=0x8).
      Likely `mono_vfree` passes wrong address. Try making munmap a no-op first.
- [ ] Test graceful shutdown when app assembly is missing ("no app to execute" in syslog)

### Key findings
- `.mono_bss` stale pointers: on SYSRESETREQ, Mono statics in SDRAM retain values from
  previous boot. Fixed in firmware: `mono_main.c` zeros `_s_mono_bss.._e_mono_bss` before
  `monovm_initialize`. Emulator initial boot zeros via `mono_bss_zero.bin` LoadBinary.
- Interpreter mode: runtime built with `DISABLE_JIT`. Must set `--interpreter` or
  `mono_use_interpreter=1`. Handled by `MONO_ENV_OPTIONS=--interpreter` in firmware.
- Signal number mismatch: Meadow.OS headers (SIGRTMIN=32) vs legacy firmware (MAX_SIGNO=31).
  Fixed with `HOST_NUTTX` signal selection (20/21/22) in `mono-threads-posix-signals.c`.
- Trampoline stubs: `mono_arch_create_generic_trampoline` stubbed with `g_assert_not_reached()`
  in DISABLE_JIT build. Fixed with `HOST_NUTTX` in `disable_tramps` (same as WASM).
- NuttX `pthread_create` deadlock: `sched_lock()` held during `sem_wait` prevents child thread
  from running. Affects any code that creates pthreads during init. Fix: disable concurrent GC.
- Assembly pre-caching: firmware copies DLLs to tmpfs before Mono loads them. Eliminates
  repeated QSPI reads. QSPI caching completes in ~5 seconds under emulation.

### Emulator notes
- Build firmware: `cd Meadow.OS.Emulator && bash build-meadow.os-emulated.sh`
  (handles toolchain, config, version stamps, extraction, hook generation — see workflow.md)
- UART log only shows kernel init — HCOM/user messages go to host on TCP:4242
- HCOM responds with device info: OSVersion=2.5.7.0, Hardware=F7FeatherV2
- SPCL built via: `cd runtime && ./build.sh -c Debug -subset Mono.CoreLib` (5.76 MB)
- GDB debugging: use `nuttx.elf` for kernel, `nuttx_user.elf` for user/mono frames
- Renode breadcrumb tracing: `cpu AddHook <addr> "self.Log(LogLevel.Error, \"msg\")"`
- GDB profiling: use `nuttx.elf` for kernel frames, `nuttx_user.elf` for user/mono frames
- Renode hooks for breadcrumb tracing: `cpu AddHook <addr> "self.Log(LogLevel.Error, \"msg\")"`

## Phase 5: P/Invoke Validation
- [ ] Create a minimal test managed assembly that calls a P/Invoke function
- [ ] Verify DllImport("nuttx") resolves correctly through PINVOKE_OVERRIDE
- [ ] Test basic ioctl call path works end-to-end

## Key Files Changed (Phase 1-3)
| File | Change |
|------|--------|
| `apps/examples/mono/mono_main.c` | Complete rewrite — monovm hosting API |
| `apps/examples/mono/mono_nuttx_stubs.c` | New — stubs for missing symbols |
| `apps/examples/mono/Make.defs` | .NET 10 library references |
| `apps/examples/mono/Makefile` | Added mono_nuttx_stubs.c |
| `nuttx/configs/.../scripts/user-space.ld` | New library names in EXCLUDE_FILE and .mono |
| `apps/examples/hcom/mono/hcom_mono_control.c` | meadow_mono_main + SPCL check |
