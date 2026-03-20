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
- [ ] **Fix: enable interpreter mode** — either pass `--interpreter` in argv to `mono_main`,
      or set `mono_use_interpreter = 1` before `monovm_execute_assembly`, or configure via
      a monovm property. Then verify mono_main reaches mini_init and loads SPCL.
- [ ] Test graceful shutdown when app assembly is missing ("no app to execute" in syslog)

### Key findings
- `.mono_bss` stale pointers: on SYSRESETREQ reset, Mono static globals in SDRAM retain
  values from previous boot. Must zero key statics (8 addresses) in Renode reset macro.
  Firmware's mono_main.c zeroing handles this for real hardware.
- `LoadBinary` of zero files in reset macro causes infinite reset loop (unknown Renode issue).
  Workaround: use targeted `sysbus WriteDoubleWord` for specific addresses instead.
- SPCL loading under emulation is slow (~260 bytes/QSPI read, ~500 reads/sec).
  FileReadBypass.cs created (hooks mono_file_map_fileio) but disabled pending validation.

### Emulator notes
- UART log only shows kernel init — HCOM/user messages go to host on TCP:4242
- HCOM responds with device info: OSVersion=2.5.7.0, Hardware=F7FeatherV2
- Version stamps must be applied before build via `scripts/version_methods.sh`
- `.NET 10` mono binary must be re-extracted from ELF when firmware is rebuilt:
  `arm-none-eabi-objcopy -O binary --only-section=.mono --only-section=.mono_data nuttx_user.elf nuttx_mono.bin`
- `.mono_bss` key statics zeroed via WriteDoubleWord in reset macro (8 addresses)
- SPCL built via: `cd runtime && ./build.sh -c Debug -subset Mono.CoreLib` (5.76 MB output)
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
