# Implementation Plan: Interp-to-Native Trampoline

## Phase 1: Understand the Interface

Study the existing trampoline implementations to understand exactly what the bridge needs
to do:

- [ ] Read the ARM JIT trampoline (`tramp-arm.c:845-1050`) — what does the generated code do?
- [ ] Read the WASM implementation (`aot-runtime-wasm.c`) — how does cookie dispatch work?
- [ ] Read `ves_pinvoke_method` in `interp.c` — how are args marshaled for the trampoline?
- [ ] Document the `InterpMethodArguments` struct layout
- [ ] Document the `BuildArgsFromSigInfo` / `build_args_from_sig` calling convention
- [ ] Identify all icall signatures hit during runtime init (use GDB to log each trampoline call)

## Phase 2: Create NuttX Trampoline Infrastructure

- [ ] Create `runtime/src/mono/mono/mini/aot-runtime-nuttx.c`
- [ ] Implement `mono_nuttx_get_interp_to_native_trampoline(MonoMethodSignature *sig)`
- [ ] Add `#ifdef HOST_NUTTX` path in `ves_pinvoke_method` (interp.c:1790, alongside HOST_WASM)
- [ ] Wire up per-signature cache (same `WasmPInvokeCacheData` pattern, rename to generic)
- [ ] Add to CMake build (`build-nuttx.sh` or CMakeLists.txt)

## Phase 3: Implement Signature Trampolines

Implement C trampolines for each signature pattern needed. ARM AAPCS rules:
- Integer/pointer args: R0, R1, R2, R3, then stack
- Float/double args: S0-S15 / D0-D7 (VFP)
- Return: R0 (int/ptr), R0+R1 (int64), S0/D0 (float/double)

Start with the signatures needed for runtime init, then expand:

- [ ] `void()` — no args, no return
- [ ] `ptr(ptr)` — one pointer arg, pointer return (most icalls)
- [ ] `ptr(ptr,ptr)` — two pointer args
- [ ] `ptr(ptr,ptr,ptr)` — three pointer args
- [ ] `void(ptr)`, `void(ptr,ptr)` — void return variants
- [ ] `int(ptr)`, `int(ptr,ptr)` — int return variants
- [ ] `ptr(ptr,int)`, `ptr(ptr,ptr,int)` — mixed arg types
- [ ] Generic fallback for signatures not yet covered (log and abort with signature info)

## Phase 4: Test and Iterate

- [ ] Rebuild runtime + firmware
- [ ] Boot emulator, verify no trampoline abort during init
- [ ] If new signature patterns are needed, add them and rebuild
- [ ] Repeat until runtime init completes cleanly
- [ ] Verify `monovm_execute_assembly` proceeds to assembly loading

## Phase 5: Validate native-to-interp (reverse direction)

The reverse trampoline (native code calling back into managed code) may also be needed:
- [ ] Check if `mono_arch_get_native_to_interp_trampoline` is called
- [ ] Implement if needed (same pattern, opposite direction)
