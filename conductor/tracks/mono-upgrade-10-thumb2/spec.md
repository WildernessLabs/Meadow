# Track 10: Thumb2 JIT — Port JIT Backend to .NET 10 Mono

## Overview
Port the Wilderness Labs Thumb2 JIT backend from the Mono 6.9.0 fork to the .NET 10 Mono runtime. This enables native code generation for the Cortex-M7, providing significant performance improvement over the interpreter.

## Background

### Current Thumb2 Backend (Mono 6.9.0 Fork)
- `mono/arch/arm/thumb-codegen.h` — 4,454 lines, complete T32 instruction emitter
- `mono/arch/arm/thumb-offsets.h` — 335 lines, cross-compiler offsets for 32-bit ARM
- Custom SDB trampolines in `tramp-arm.c` for Thumb2
- Exception handling adaptations in `exceptions-arm.c`

### Upstream .NET 10 Mono ARM Backend
- `src/mono/mono/mini/mini-arm.c` — ~7,461 lines (vs. fork's ~7,664)
- `src/mono/mono/arch/arm/arm-codegen.h` — ARM (A32) instruction emitter only
- Has `thumb_supported` flag but only for interworking (BLX), not codegen
- No `thumb2_supported` variable or Thumb2 instruction emission

### The Gap
The Cortex-M7 is Thumb2-only (no ARM mode). The upstream JIT emits A32 instructions which cannot execute on M7. The fork's `thumb-codegen.h` provides the T32 encoding layer, but it needs to be integrated with the new `mini-arm.c` which has diverged from the 6.9.0 version.

## Functional Requirements
1. Port `thumb-codegen.h` and `thumb-offsets.h` to new `src/mono/mono/arch/arm/`
2. Integrate Thumb2 codegen into new `mini-arm.c` instruction emission
3. Add `thumb2_supported` detection and use it to control codegen mode
4. Port custom SDB trampolines in `tramp-arm.c`
5. Port ARM exception handling changes in `exceptions-arm.c`
6. Ensure all JIT-compiled code uses Thumb2 (T32) encoding
7. Validate with Blinky and mono tests in JIT mode

## Acceptance Criteria
- [ ] `thumb-codegen.h` integrates cleanly with new mono build
- [ ] JIT produces valid Thumb2 instructions (verified by disassembly)
- [ ] Hello World runs JIT-compiled (not interpreted)
- [ ] Blinky runs JIT-compiled in emulator
- [ ] JIT-compiled code is measurably faster than interpreted
- [ ] Mono test suite passes in JIT mode (at least same pass rate as interpreter)
- [ ] Hardware validation: Blinky runs JIT-compiled on F7FeatherV2
- [ ] No regressions in interpreter mode

## Out of Scope
- AOT compilation (Track 12)
- LLVM backend integration (Track 12)
- Performance optimization beyond basic JIT correctness
