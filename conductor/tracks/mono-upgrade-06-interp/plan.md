# Implementation Plan: Interpreter

## Status from Track 4

Several items originally scoped for Track 6 were completed during Track 4:
- [x] Interpreter mode enabled: `MONO_ENV_OPTIONS=--interpreter` in firmware
- [x] `MONO_AOT_MODE_INTERP_ONLY` confirmed active
- [x] `DISABLE_JIT` build compiles out all JIT codegen
- [x] `mono_llvm_only = TRUE` set for NuttX (skips JIT trampoline creation)
- [x] Exception handling uses llvmonly path (interpreter's own unwinding)
- [x] SPCL loads and type system initializes under interpreter
- [x] `mono_interp_exec_method` reached — interpreter actively executing code

**BLOCKER:** `interp_to_native` trampoline — the interpreter cannot call ANY native
function (icalls, P/Invoke) because the ARM trampoline requires JIT codegen which is
disabled. This must be solved before any managed code can execute.

## Phase 1: Enable Interpreter ✅ (done in Track 4)
- [x] `MONO_ARCH_INTERPRETER_SUPPORTED` set for ARM
- [x] Interpreter source files included in build
- [x] `libmonosgen.a` built with interpreter + `DISABLE_JIT`
- [x] `--interpreter` mode activates correctly
- [x] `mono_llvm_only = TRUE` for NuttX INTERP_ONLY (skips JIT-dependent code)

## Phase 2: Interp-to-Native Trampoline (CURRENT BLOCKER)

### The Problem
The Mono interpreter needs to call native C functions for:
- Internal calls (icalls): `RuntimeTypeHandle.GetAssembly`, `String.InternalAllocateStr`, etc.
- P/Invoke: `DllImport("nuttx")` → firmware functions
- Runtime support: GC, threading, exception handling

The `interp_to_native` trampoline bridges the interpreter's stack to the ARM C calling
convention (AAPCS). On x86/ARM/ARM64 with JIT, this trampoline is generated as machine
code at runtime. With `DISABLE_JIT`, the code generation is compiled out and the stubs
hit `g_assert_not_reached()` in `tramp-arm.c:1073`.

### Crash Details
```
Assertion: should not be reached at tramp-arm.c:1073
Call chain: ves_pinvoke_method → get_interp_to_native_trampoline
            → mono_arch_get_interp_to_native_trampoline → g_assert_not_reached
Triggered by: RuntimeTypeHandle.GetAssembly icall during runtime init
```

### How WASM Solves This
WASM uses per-signature C trampolines (`mono_wasm_get_interp_to_native_trampoline`):
1. Build a "cookie" string describing the function signature (e.g., "VII" = void(int,int))
2. Look up a pre-compiled C trampoline for that signature
3. The C trampoline unpacks `InterpMethodArguments` and calls the target function

Source: `runtime/src/mono/mono/mini/aot-runtime-wasm.c:159`

### Solution Options for NuttX

**Option A: WASM-style per-signature C trampolines (recommended)**
- Create `aot-runtime-nuttx.c` with `mono_nuttx_get_interp_to_native_trampoline(sig)`
- Add NuttX path in `ves_pinvoke_method` (interp.c:1790) alongside `#ifdef HOST_WASM`
- Generate C trampolines for common signature patterns using AAPCS calling convention
- The trampoline takes `InterpMethodArguments *margs` → unpacks into R0-R3/stack → calls target
- Use `BuildArgsFromSigInfo` to marshal interpreter stack → `InterpMethodArguments`
- Pro: No JIT needed, works with DISABLE_JIT, proven pattern from WASM
- Con: Need to enumerate all possible signature shapes (or use a generic varargs approach)

**Option B: Single generic C trampoline using libffi**
- Use libffi's `ffi_call` to handle arbitrary function signatures
- Trampoline: unpack `InterpMethodArguments` → build `ffi_cif` → `ffi_call(target)`
- Pro: Handles any signature without enumeration
- Con: Adds libffi dependency, may not be available for NuttX/Cortex-M7

**Option C: Hand-written ARM assembly trampoline**
- Implement `mono_arch_get_interp_to_native_trampoline` in ARM assembly (not JIT)
- The trampoline is static code, not dynamically generated — so DISABLE_JIT is fine
- Pro: Exact match for ARM AAPCS, fast, no dependencies
- Con: Complex assembly code, hard to maintain, architecture-specific

### Implementation Plan (Option A)
1. Create `runtime/src/mono/mono/mini/aot-runtime-nuttx.c`
2. Implement `mono_nuttx_get_interp_to_native_trampoline(MonoMethodSignature *sig)`
3. Add `#ifdef HOST_NUTTX` path in `ves_pinvoke_method` (interp.c, alongside HOST_WASM)
4. Register `BuildArgsFromSigInfo` per-cache-entry (same pattern as WASM)
5. Implement common signature trampolines: void(), void(ptr), ptr(ptr), void(ptr,ptr), etc.
6. Test with `RuntimeTypeHandle.GetAssembly` icall (the first one that fires)
7. Add more signature patterns as needed when new icalls are encountered

### Key Source Files
- `runtime/src/mono/mono/mini/interp/interp.c:1766` — `ves_pinvoke_method` (add HOST_NUTTX path)
- `runtime/src/mono/mono/mini/interp/interp.c:1731` — `get_interp_to_native_trampoline`
- `runtime/src/mono/mono/mini/aot-runtime-wasm.c:159` — WASM reference implementation
- `runtime/src/mono/mono/mini/tramp-arm.c:845` — ARM JIT trampoline (reference for AAPCS)
- `runtime/src/mono/mono/mini/interp/interp-internals.h` — `InterpMethodArguments` struct

## Phase 3: Hello World Test
- [ ] Create minimal .NET 10 console app: `Console.WriteLine("Hello from Meadow!")`
- [ ] Deploy to emulator via Meadow.CLI (or baked LFS)
- [ ] Execute via monovm_execute_assembly
- [ ] Verify output on HCOM or syslog
- [ ] Debug missing trampolines for additional signature patterns

## Phase 4: Feature Validation
- [ ] String operations (concatenation, formatting)
- [ ] Exception handling (try/catch/finally)
- [ ] Basic P/Invoke: DllImport("nuttx") → open, ioctl
- [ ] GC: allocate objects in loop, verify no OOM
- [ ] Collections (List<T>, Dictionary<K,V>)
- [ ] async/await (Task, Task<T>)

## Phase 5: Stability
- [ ] Run Hello World 10 times without crashes
- [ ] Monitor SDRAM usage during interpretation
- [ ] Document interpreter performance baseline
