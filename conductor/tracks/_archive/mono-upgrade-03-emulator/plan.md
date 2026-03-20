# Implementation Plan: Emulator Bring-Up

## Phase 1: Build Script Update — COMPLETE (2026-03-19)

### What was done
- Rewrote `build-meadow.os-emulated.sh` to build from `Meadow/nuttx` (legacy), not `Meadow.OS/nuttx`
  - The original script pointed at the wrong repo; Meadow.OS has a newer NuttX codebase
    with different symbol names, memory layout (0x08000000 vs 0x08040000), and build process
  - Fixed: configure separator (`/` not `:`), build steps (`pass2 → pass1deps → pass1`),
    toolchain setup (ld.lld, ARM GCC 10.3 from /Applications/ARM)
  - Added binary splitting: nuttx_vectors.bin, nuttx_kernel.bin, nuttx_user.bin, nuttx_mono.bin
  - Added version injection for both hcom_nuttx_shared.h and user-space.ld
- Updated `configs/legacy-os_modern-dotnet.yaml`: defconfig `mono` (not `meadow_os_emu`),
  corrected comment about NuttX source repo
- [x] Verify the mono binary extraction (`.mono_*` sections → `nuttx_mono.bin`)

### Key decision: Link with legacy Mono, disable at runtime
- The .NET 10 `libmonosgen-2.0.a` is **not link-compatible** with legacy `mono_main.c`:
  - Missing: `mono_main_driver`, `mono_dl_register_library` (legacy Mono APIs)
  - Missing: `SystemNative_*` functions (20 PAL symbols from System.Native)
  - .NET 10 Mono uses `mono_jit_init`/`mono_jit_exec`/`mono_assembly_open` instead
- **Workaround**: Build links with legacy Mono libraries (which exist in Meadow/mono/).
  Mono runtime disabled at boot via BBR bit 0x800 (`HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT`)
  in the Renode script. NuttX + HCOM boot fine without Mono executing.
- Proper .NET 10 Mono integration needs an updated `mono_main.c` + System.Native PAL build
  (deferred to a future phase)

### Files changed
- `build-meadow.os-emulated.sh` — complete rewrite for legacy Meadow/nuttx
- `configs/legacy-os_modern-dotnet.yaml` — defconfig + comment fix
- `scripts/meadow-dotnet10-headless.resc` — legacy memory layout, mono loading, BBR disable
- `scripts/meadow-dotnet10-gdb.resc` — same memory layout fixes

## Phase 2: Address Extraction Update — COMPLETE (2026-03-19)

### What was done
Since the dotnet10 build now uses the same `Meadow/nuttx` source as the legacy build
(with the same `mono` defconfig), the ELF symbols and disassembly patterns are **identical**
to the legacy build. All 36 hook addresses extracted successfully.

- [x] Inventory which symbols still exist — all present (same NuttX source)
- [x] `extract-hook-addresses.py` works unmodified against dotnet10 ELFs
- [x] Added new symbol: `hcom_nx_config_set_time_to_os_build_time` for RTC crash workaround
- [x] No symbols needed renaming or removal

### Note on the original problem statement
The original Phase 2 problem (missing symbols like `QSPI_WAITSTATUSFLAGS_CONSTPROP`,
`ESPCP_SPI_INTERFACE_LOCK`, etc.) was caused by building from Meadow.OS/nuttx which has
different compiled code. Fixing Phase 1 to use Meadow/nuttx eliminated all extraction issues.

## Phase 3: Hook Script Update — COMPLETE (2026-03-19)

### What was done
- Same `meadow-legacy-hooks.resc` template works for both legacy and dotnet10 builds
  (identical NuttX code, identical hook patterns)
- Added RTC time-set skip hook: `hcom_nx_config_set_time_to_os_build_time`
  - Renode's STM32F4_RTC crashes with `ArgumentOutOfRangeException` when firmware writes
    BCD date register with partially-populated fields (intermediate zero month/day)
  - Skip is safe — emulator doesn't need a real RTC clock
- No separate dotnet10 hook template needed

### Files changed
- `tools/extract-hook-addresses.py` — added `HCOM_NX_CONFIG_SET_TIME` symbol lookup
- `scripts/meadow-legacy-hooks.resc` — added `rtc_time_skip` hook (benefits both builds)
- Both `build/legacy/hooks.resc` and `build/dotnet10/hooks.resc` regenerated (36 addresses)

## Phase 4: Boot Test — COMPLETE (2026-03-19)

### Results
- [x] `./run.sh --config=configs/legacy-os_modern-dotnet.yaml` boots successfully
- [x] NuttX kernel boot verified (syslog: RNG init, MTD partitions, LittleFS mount)
- [x] `hcom_main` startup verified (HCOM threads start, listen on UART4/TCP:4242)
- [x] HCOM over TCP:4242 works — `hcom_test.py` GET_DEVICE_INFORMATION returns:
  - `"Mono is disabled"` (BBR workaround confirmed)
  - `OSVersion|2.5.7.0`, `Hardware|F7FeatherV2`, `DeviceName|MeadowF7`
- [x] Mono binary in SDRAM at 0xC0000000 and QSPI at 0x90000000 (2.2MB loaded)
- [x] ESPCP stub working (SPI bypass hooks installed)
- [x] UartRxBypass activated on first HCOM data

### Boot sequence verified
```
stm32_rng_initialize → meadow_upd_initialize → MTD partitions →
LittleFS mount /meadow0 → hcom_nx_config_init → HCOM threads →
ESPCP stub init → HCOM responsive on TCP:4242
```

## Phase 5: CLI Deployment Path — COMPLETE (2026-03-19)

### Results
- [x] Meadow.CLI v2.5.0 (from `Meadow.CLI` repo, branch `feature/TCP_Socket_Connection_for_EMU`)
  connects via `meadow config route socket://localhost:4242`
- [x] `meadow device info` returns full device info (model, HW version, OS version, etc.)
- [x] File upload works: `meadow file write -f /tmp/test-deploy.txt` → uploaded to `/meadow0/`
  with checksum verification
- [x] `meadow file list` confirms file on device (alongside dns.conf, meadow.log, system/)

### Deployment workflow
```bash
# 1. Start emulator
./run.sh --config=configs/legacy-os_modern-dotnet.yaml

# 2. Set CLI route to emulator socket
meadow config route socket://localhost:4242

# 3. Verify connection
meadow device info

# 4. Deploy files
meadow file write -f <path-to-file>
meadow file list
```

### Notes
- The installed `meadow` v2.2.0.2 (dotnet tool) does NOT support socket routes properly
  ("Cannot find port"). Must use v2.5.0 built from `Meadow.CLI` repo on the
  `feature/TCP_Socket_Connection_for_EMU` branch.
- File upload uses the UartRxBypass C# hook for bulk transfer performance

## Blockers for .NET 10 Mono Execution

To actually run .NET 10 Mono (beyond just booting NuttX + HCOM), these are needed:

1. **Updated `mono_main.c`**: Rewrite to use .NET 10 Mono embedding API
   (`mono_jit_init`/`mono_jit_exec` instead of `mono_main_driver`)
2. **`mono_dl_register_library` replacement**: .NET 10 uses `mono_dl_fallback_register`
   or a different native library registration mechanism
3. **System.Native PAL library**: Build `System.Native.a` for NuttX from
   `runtime/src/libraries/Native/` — provides 18 `SystemNative_*` functions
4. **Linker script update**: `user-space.ld` EXCLUDE_FILE patterns reference
   legacy `libmonosgen.a` — may need updates for `libmonosgen-2.0.a`
