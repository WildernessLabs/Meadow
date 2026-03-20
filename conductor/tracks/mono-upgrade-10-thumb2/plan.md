# Implementation Plan: Thumb2 JIT

## Phase 1: Diff Analysis
- [ ] Diff old `mini-arm.c` (6.9.0 fork) against new `.NET 10 mini-arm.c`
- [ ] Identify all Thumb2-specific changes in the old fork
- [ ] Diff old `tramp-arm.c` against new
- [ ] Diff old `exceptions-arm.c` against new
- [ ] Catalog every change that needs porting (with old/new line references)

## Phase 2: Codegen Integration
- [ ] Copy `thumb-codegen.h` and `thumb-offsets.h` to `src/mono/mono/arch/arm/`
- [ ] Add `#include "thumb-codegen.h"` in appropriate locations
- [ ] Add `thumb2_supported` boolean to `mini-arm.c`
- [ ] Wire `thumb2_supported` detection via `mono_hwcap_arm_has_thumb2`
- [ ] Modify instruction emission to use Thumb2 macros when `thumb2_supported` is true
- [ ] Update `cpu-arm.mdesc` if new instruction patterns are needed

## Phase 3: Trampolines and Exceptions
- [ ] Port Thumb2 SDB trampolines to new `tramp-arm.c`
- [ ] Port exception handling changes to new `exceptions-arm.c`
- [ ] Port any `mini-arm-gsharedvt.c` changes
- [ ] Verify trampoline code emits valid Thumb2

## Phase 4: Build and Basic Validation
- [ ] Rebuild `libmonosgen.a` with Thumb2 JIT enabled
- [ ] Run Hello World in JIT mode (not interpreter)
- [ ] Disassemble JIT output to verify Thumb2 encoding
- [ ] Fix any codegen bugs (incorrect encodings, wrong register usage, etc.)

## Phase 5: App and Test Validation
- [ ] Run Blinky in JIT mode on emulator
- [ ] Run mono test suite in JIT mode
- [ ] Compare pass rate against interpreter baseline
- [ ] Fix any JIT-specific test failures
- [ ] Benchmark: JIT vs interpreter execution time

## Phase 6: Hardware Validation
- [ ] Flash JIT-enabled firmware to F7FeatherV2
- [ ] Run Blinky in JIT mode on hardware
- [ ] Run stability test (5+ minutes continuous)
- [ ] Document any hardware-specific JIT issues
