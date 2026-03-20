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

## Phase 4: Emulator Validation ✅ (partial — HCOM works, mono blocked by missing assemblies)
- [x] Extract `nuttx_user.bin`, `nuttx_kernel.bin`, `nuttx_vectors.bin`, `nuttx_mono.bin` for emulator
- [x] Regenerate hook addresses from new ELFs (`extract-hook-addresses.py`)
- [x] Remove BBR bit 0x800 mono-disable from `meadow-dotnet10-headless.resc`
- [x] Disable UartRxBypass.cs (API incompatible with Renode 1.16.1 — pre-existing issue)
- [x] Boot firmware in emulator — NuttX boots fully, HCOM up in ~20s wall time
- [x] Verify HCOM works (hcom_test.py GET_DEVICE_INFORMATION) — **PASS**
- [x] Mono startup attempted — correctly blocked by missing files check:
      `"Mono will not start - the following files are missing: System.Private.CoreLib.dll, Meadow.dll"`
- [ ] Build `System.Private.CoreLib.dll` from `runtime/src/mono/System.Private.CoreLib/`
- [ ] Deploy SPCL + Meadow.dll to emulator LittleFS image or via Meadow.CLI
- [ ] Verify `monovm_initialize` succeeds (check syslog for "monovm_initialize succeeded")
- [ ] Test graceful shutdown when app assembly is missing ("no app to execute" in syslog)

### Emulator notes
- UART log only shows kernel init (15 lines to 26ms) — HCOM/user messages go to host, not USART1
- CPU idle at `stm32_idle.c:123` when paused = boot completed successfully
- HCOM responds with device info: OSVersion=2.5.7.0, Hardware=F7FeatherV2
- Version stamps must be applied before build via `scripts/version_methods.sh`
- `System.Private.CoreLib.dll` not yet built — needs `dotnet build` of `runtime/src/mono/System.Private.CoreLib/`

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
