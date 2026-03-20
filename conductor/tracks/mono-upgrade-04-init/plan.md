# Implementation Plan: monovm Hosting API Integration

## Phase 1: Study the API
- [ ] Read `monovm.c` in detail — understand `monovm_initialize`, `monovm_execute_assembly`, `monovm_shutdown`
- [ ] Read `mono-private-unstable-functions.h` — get the function declarations
- [ ] Study how Android/iOS targets call monovm (look at `src/mono/sample/` for examples)
- [ ] Understand TPA list format (semicolon-separated full paths on NuttX)
- [ ] Understand PINVOKE_OVERRIDE mechanism — how to pass a function pointer as a string property

## Phase 2: Create Meadow Mono Entry Point
- [ ] Create `meadow_mono_main.c` (or modify `hcom_mono_control.c`) with new entry point
- [ ] Implement TPA list builder: enumerate `/meadow0/*.dll` and build semicolon-separated string
- [ ] Implement property array setup (keys + values for monovm_initialize)
- [ ] Implement P/Invoke override function for NuttX native library resolution
- [ ] Call `monovm_initialize` → `monovm_execute_assembly` → `monovm_shutdown`
- [ ] Add error handling and syslog output at each stage

## Phase 3: Update HCOM Integration
- [ ] Update `hcom_mono_control.c`: replace `mono_main` extern with `meadow_mono_main`
- [ ] Update required-file validation: check for `System.Private.CoreLib.dll` instead of `mscorlib.dll`
- [ ] Remove or update version matching logic (old mono version format may not apply)
- [ ] Re-enable mono startup in `hcom_startup_manager.c` (remove `#if(0)`)
- [ ] Update `MONO_TASK_STACKSIZE` if needed (new mono may need more stack)

## Phase 4: Emulator Validation
- [ ] Deploy `System.Private.CoreLib.dll` to emulator filesystem via Meadow.CLI
- [ ] Boot firmware in emulator
- [ ] Verify `monovm_initialize` succeeds (check syslog output)
- [ ] Verify corlib loads and type system initializes
- [ ] Verify HCOM still works after mono init
- [ ] Test graceful failure when app assembly is missing
- [ ] Update emulator hooks as needed for new init flow

## Phase 5: P/Invoke Validation
- [ ] Create a minimal test managed assembly that calls a P/Invoke function
- [ ] Verify DllImport("nuttx") resolves correctly through PINVOKE_OVERRIDE
- [ ] Test basic ioctl call path works end-to-end
