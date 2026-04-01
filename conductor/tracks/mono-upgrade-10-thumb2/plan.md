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
- [x] Fix OOM: SGen card table reduced from 8MB→128KB (CARD_TABLE_BITS 32→26)
- [x] Fix 5 JIT-specific test failures (or_large_imm, signed_ct_div, ovf11, bigmul6, ldsfld_soft_float)
- [x] Fix intptr_array_cast — updated test for modern .NET semantics
- [x] Fix atan_precision — NuttX libm workaround in Mono (isinf guard)
- [x] Fix cctor heap corruption — removed JIT FAILED diagnostic causing double-free
- [x] Fix dlmalloc ABORT — NuttX sysconf(_SC_PAGE_SIZE) returns -1, force page size 4096
- [x] Fix Thumb2 dynamic trampoline patching (LDR literal pool + Thumb bit alignment)
- [x] Rerun full test suite: 733 ran, 2 skipped, 1 failed (99.86% pass)
- [ ] Benchmark: JIT vs interpreter execution time

## Phase 6: Hardware Validation
- [x] Flash JIT-enabled firmware to F7CoreComputeV2 (build.sh --clean, DFU + HCOM)
- [x] Verify OS/Runtime must be from same build (mismatched .mono_signature → UNDEFINSTR HardFault)
- [x] Create flash-openocd.sh for autonomous ST-Link flashing (clears FPB breakpoints)
- [x] JIT runtime boots, mono reaches stage=10, monovm_execute_assembly returns 0
- [x] Managed exit_code=1 confirmed as Meadow.Core framework issue (not runtime)
- [x] Document hardware-specific findings in handoff.md
- [ ] Run stability test (5+ minutes continuous)
- [ ] Deploy .NET 10-compatible Meadow app after Meadow.Core fixes
