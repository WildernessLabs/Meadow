# Track 3: Emulator Bring-Up — Build Pipeline + Renode Boot

## Overview
Update the Meadow.OS.Emulator build pipeline to compile and boot the new .NET 10 Mono firmware in Renode. This gets the emulator running early so subsequent tracks (init, BCL, interpreter) can be validated without hardware.

## Background
The emulator has a mature build and hook pipeline:
- `build-meadow.os_legacy-emulated.sh` — Builds NuttX + Mono firmware for emulator
- `tools/extract-hook-addresses.py` — Extracts symbol addresses from ELF via nm/objdump, generates `hooks.resc`
- `scripts/meadow-legacy-hooks.resc` — Template with `$hook_*` placeholders replaced by hex addresses
- `run.sh` — Launches Renode with auto-regeneration of addresses if ELF is newer

The new .NET 10 mono build (CMake, different library name, different init flow) requires updates to the build script and address extraction.

## Functional Requirements
1. Update build script to use CMake-built `libmonosgen.a` instead of autoconf-built one
2. Update linker script for `.mono_*` sections with new mono binary layout
3. Update `extract-hook-addresses.py` for new symbol names (mono init functions will differ)
4. Update Renode hook scripts — remove/update hooks that reference old mono internals
5. Build firmware that boots NuttX `hcom_main` successfully in emulator
6. Mono binary loaded into SDRAM at 0xC0000000 (even if it can't init yet — that's Track 4)

## Acceptance Criteria
- [ ] Build script produces `nuttx.bin` and `nuttx_user.elf` with .NET 10 mono linked in
- [ ] `extract-hook-addresses.py` successfully extracts addresses from new ELF
- [ ] `run.sh` boots the firmware in Renode
- [ ] NuttX reaches `hcom_main` and HCOM is responsive on TCP port 4242
- [ ] `meadow device info` works via Meadow.CLI socket connection

## Out of Scope
- Mono runtime actually initializing (Track 4)
- App deployment and execution (Tracks 5-7)
- Old mono hooks that are no longer relevant (will be cleaned up organically)
