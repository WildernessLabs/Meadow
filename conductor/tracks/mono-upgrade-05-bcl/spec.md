# Track 5: Interp-to-Native Trampoline

## Overview
Implement the interpreter-to-native calling bridge so the Mono interpreter can call C
functions (icalls and P/Invoke). This is the critical blocker — without it, zero managed
code executes.

## Background

### The Problem
The Mono interpreter executes IL bytecode via a C dispatch loop. When it encounters a call
to a native function (internal call or P/Invoke), it needs a trampoline to bridge from the
interpreter's stack representation (`InterpMethodArguments`) to the ARM C calling convention
(AAPCS: args in R0-R3, then stack, return in R0/R1).

On x86/ARM/ARM64 with JIT enabled, this trampoline is generated as machine code at runtime
(`mono_arch_get_interp_to_native_trampoline` in `tramp-arm.c:845`). Our build uses
`DISABLE_JIT`, which compiles out all code generation. The stub at `tramp-arm.c:1073` hits
`g_assert_not_reached()`.

### How We Got Here (Track 4 Findings)
- Runtime initializes fully: `monovm_initialize` → `mini_init` → SPCL loads ✅
- Interpreter starts executing: `mono_interp_exec_method` reached ✅
- First native call (`RuntimeTypeHandle.GetAssembly` icall) → needs trampoline → abort
- The abort kills the Mono task via `pthread_exit(NULL)` — watchdog doesn't detect it

### How WASM Solves This
WASM uses per-signature C trampolines (`aot-runtime-wasm.c:159`):
1. Encode the function signature as a cookie string (e.g., "VII" = void(int,int))
2. Look up a pre-compiled C trampoline for that pattern
3. The trampoline unpacks `InterpMethodArguments` → calls target → returns result

This is a proven pattern that works with `DISABLE_JIT`.

## Functional Requirements
1. Implement NuttX interp-to-native trampolines (WASM-style per-signature C functions)
2. Handle ARM AAPCS calling convention (R0-R3 for first 4 args, stack for rest, VFP for floats)
3. Cover all icall signatures used during runtime init + Hello World execution
4. Integrate with the interpreter's `ves_pinvoke_method` dispatch

## Acceptance Criteria
- [ ] `RuntimeTypeHandle.GetAssembly` icall succeeds (first blocker)
- [ ] Runtime init completes without abort (all init-time icalls handled)
- [ ] `monovm_execute_assembly` reaches assembly loading (past init)
- [ ] System stable — no trampoline asserts during idle

## Key Source Files
| File | Role |
|------|------|
| `runtime/src/mono/mono/mini/interp/interp.c:1766` | `ves_pinvoke_method` — add HOST_NUTTX path |
| `runtime/src/mono/mono/mini/aot-runtime-wasm.c:159` | WASM reference implementation |
| `runtime/src/mono/mono/mini/tramp-arm.c:845` | ARM JIT trampoline (AAPCS reference) |
| `runtime/src/mono/mono/mini/interp/interp-internals.h` | `InterpMethodArguments` struct |

## Out of Scope
- P/Invoke to firmware (validated in Track 6)
- Hello World execution (Track 6 — depends on this track)
- JIT compilation (Track 10)
