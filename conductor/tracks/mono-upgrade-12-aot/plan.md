# Implementation Plan: AOT Compilation

## Phase 1: LLVM AOT Setup
- [ ] Study .NET 10 Mono AOT compiler (`src/mono/mono/mini/aot-compiler.c`)
- [ ] Identify LLVM version and ARM backend requirements
- [ ] Configure cross-compilation triple for NuttX ARM Thumb2
- [ ] Build the AOT compiler for the host platform (macOS/Linux)

## Phase 2: AOT Compile Core Libraries
- [ ] AOT-compile `System.Private.CoreLib.dll` targeting ARM Thumb2
- [ ] Verify output: `.so` / `.o` files with valid Thumb2 code
- [ ] Disassemble and spot-check instruction encoding

## Phase 3: Static Registration
- [ ] Implement `mono_aot_register_module()` calls in startup code
- [ ] Link AOT-compiled objects into firmware
- [ ] Configure `MONO_AOT_MODE_LLVMONLY_INTERP` at runtime
- [ ] Boot and verify AOT modules load

## Phase 4: App AOT
- [ ] AOT-compile Blinky app and dependencies
- [ ] Deploy AOT-compiled assemblies
- [ ] Verify execution from AOT code
- [ ] Benchmark startup time vs. JIT and interpreter

## Phase 5: Validation
- [ ] Run mono test suite with AOT-compiled assemblies
- [ ] Test on emulator
- [ ] Test on F7FeatherV2 hardware
- [ ] Document the AOT build pipeline
- [ ] Identify candidates for future CLI integration
