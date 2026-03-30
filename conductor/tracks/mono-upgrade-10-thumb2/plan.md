# Implementation Plan: Thumb2 JIT

## Phase 1: Diff Analysis
- [x] Diff old `mini-arm.c` (6.9.0 fork) against new `.NET 10 mini-arm.c`
- [x] Identify all Thumb2-specific changes in the old fork
- [ ] Diff old `tramp-arm.c` against new
- [ ] Diff old `exceptions-arm.c` against new
- [ ] Catalog every change that needs porting (with old/new line references)

## Phase 2: Codegen Integration
- [x] Copy `thumb-codegen.h` and `thumb-offsets.h` to `src/mono/mono/arch/arm/`
- [x] Add `#include "thumb-codegen.h"` in appropriate locations
- [x] Add `thumb2_supported` boolean to `mini-arm.c`
- [x] Wire `thumb2_supported` detection via `mono_hwcap_arm_has_thumb2`
- [x] Modify instruction emission to use Thumb2 macros when `thumb2_supported` is true
- [x] Update `cpu-arm.mdesc` — arm_rsc_imm: 4→16, float/r4 comparison lengths increased
- [x] Fix Thumb2 condition code codegen — ARM_GET_CC for integer comparisons, ARMCOND_AL for float
- [x] Fix preprocessor guards — `__thumb2__` not `__THUMB__`

## Phase 3: Trampolines and Exceptions
- [x] P/Invoke trampolines: per-signature C trampolines via nuttx_m2n_invoke.g.h (done in prior session)
- [ ] Port Thumb2 SDB trampolines to new `tramp-arm.c`
- [ ] Port exception handling changes to new `exceptions-arm.c`
- [ ] Port any `mini-arm-gsharedvt.c` changes
- [ ] Verify trampoline code emits valid Thumb2

## Phase 4: Build and Basic Validation
- [x] Rebuild `libmonosgen.a` with Thumb2 JIT enabled
- [x] Run Hello World in JIT mode (not interpreter)
- [x] JitDiag v16: 64-bit remainder, conv.ovf.i, Dictionary all passing
- [ ] Disassemble JIT output to verify Thumb2 encoding
- [ ] Fix any remaining codegen bugs
- [x] Audit mdesc lengths vs legacy — all 6 flagged entries safe (int_add/int_sub: no SP in JIT regs; switch: +4 dynamic adjustment; aotconst: fixed 16-byte sequence; float_rem/r4_rem: dead code)

## Phase 5: App and Test Validation
- [x] Run mono test suite in JIT mode (484/735 ran; OOM prevented full run)
- [x] Compare pass rate against interpreter baseline — 7 new JIT failures, 1 same as interp
- [ ] Fix OOM: JIT code cache exhausting SDRAM
- [ ] Fix 6 new JIT-specific test failures (or_large_imm, signed_ct_div, ovf11, bigmul6, ldsfld_soft_float, intptr_array_cast)
- [ ] Fix SharedArrayPool.Rent NullRef (BCL JIT bug)
- [ ] Rerun full 735 tests after fixes
- [ ] Benchmark: JIT vs interpreter execution time

## Phase 6: Hardware Validation
- [ ] Flash JIT-enabled firmware to F7FeatherV2
- [ ] Run Blinky in JIT mode on hardware
- [ ] Run stability test (5+ minutes continuous)
- [ ] Document any hardware-specific JIT issues
