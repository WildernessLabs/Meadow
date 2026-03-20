# Track 6: Interpreter — Managed Code Running Interpreted

## Overview
Get the Mono interpreter running on NuttX/Cortex-M7, executing managed code without JIT compilation. The interpreter is the fastest path to running managed code because it doesn't require the Thumb2 JIT backend (which must be ported separately in Track 10).

## Background
The .NET 10 Mono interpreter is a mature, first-class execution engine:
- Lives at `src/mono/mono/mini/interp/`
- Supports tiered compilation (interpret first, JIT hot methods) — but we'll use interpret-only initially
- `MONO_AOT_MODE_INTERP_ONLY` mode disables JIT entirely
- `MONO_ARCH_INTERPRETER_SUPPORTED=1` is already set for ARM targets
- The interpreter does NOT emit native code — it executes IL directly via a C switch/dispatch loop

### Why Interpreter First
- No native codegen needed → no Thumb2 backend dependency
- Validates the entire runtime stack: init, BCL, type system, P/Invoke, GC
- Any issues found here are runtime/platform issues, not codegen issues
- Performance is slower than JIT but sufficient for validation

## Functional Requirements
1. Enable interpreter mode in the CMake build
2. Configure `monovm_initialize` or runtime options to use `MONO_AOT_MODE_INTERP_ONLY`
3. Execute a minimal managed assembly (Hello World) to completion
4. Verify basic .NET features work: string operations, collections, async/await, exceptions
5. Verify P/Invoke works (DllImport("nuttx") calls succeed)
6. Verify GC works under interpreter workload

## Acceptance Criteria
- [ ] Interpreter mode enabled and configured
- [ ] "Hello World" managed app runs to completion in emulator
- [ ] Console.WriteLine output visible via HCOM/syslog
- [ ] P/Invoke to NuttX native functions works (at minimum: open, ioctl)
- [ ] Exception handling works (try/catch/finally)
- [ ] Basic GC cycles complete without crashes
- [ ] No memory corruption or stack overflow during interpretation

## Out of Scope
- JIT compilation (Track 10)
- Full Blinky app (Track 7 — needs Meadow.Core retargeting)
- Performance optimization
