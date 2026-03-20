# Implementation Plan: Emulator Bring-Up

## Phase 1: Build Script Update
- [ ] Fork/copy `build-meadow.os_legacy-emulated.sh` to a new `.NET 10` variant
- [ ] Replace autoconf mono build steps with CMake invocation (from Track 1)
- [ ] Update library path: point to new `libmonosgen.a` from CMake output
- [ ] Update linker script if mono section layout has changed
- [ ] Verify the mono binary extraction (`.mono_*` sections → `nuttx_mono.bin`)

## Phase 2: Address Extraction Update
- [ ] Inventory which symbols in `extract-hook-addresses.py` still exist in the new build
- [ ] Update symbol names for any renamed functions (e.g., `mono_main_driver` → new entry point)
- [ ] Add new symbols needed for .NET 10 mono hooks
- [ ] Remove references to symbols that no longer exist
- [ ] Test `extract-hook-addresses.py` against the new ELF files

## Phase 3: Hook Script Update
- [ ] Review `meadow-legacy-hooks.resc` — identify hooks that are mono-version-specific
- [ ] Create updated hook template for .NET 10 mono
- [ ] Keep NuttX/hardware hooks unchanged (GPIO version, RCC, QUADSPI — these are OS-level)
- [ ] Update mono-specific hooks (copy bypass, version bypass, assembly scan) or mark as TBD
- [ ] Verify hook generation produces valid `.resc` file

## Phase 4: Boot Test
- [ ] Run `./run.sh` with new firmware
- [ ] Verify NuttX kernel boot (syslog output)
- [ ] Verify `hcom_main` startup sequence
- [ ] Test HCOM over TCP:4242 with `meadow device info`
- [ ] Verify mono binary is in SDRAM (even if not executing yet)
- [ ] Document any new hooks needed for .NET 10 mono boot

## Phase 5: CLI Deployment Path
- [ ] Verify Meadow.CLI socket connection works with new firmware
- [ ] Test file upload via HCOM (needed for later assembly deployment)
- [ ] Document the deployment workflow for subsequent tracks
