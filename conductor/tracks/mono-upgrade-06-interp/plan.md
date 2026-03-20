# Implementation Plan: Interpreter

## Phase 1: Enable Interpreter
- [ ] Verify `MONO_ARCH_INTERPRETER_SUPPORTED` is set for ARM in CMake
- [ ] Ensure interpreter source files (`src/mono/mono/mini/interp/`) are included in the build
- [ ] Rebuild `libmonosgen.a` with interpreter enabled
- [ ] Verify no new link errors from interpreter code

## Phase 2: Configure Interpreter Mode
- [ ] Set `MONO_AOT_MODE_INTERP_ONLY` in the hosting initialization
- [ ] Option A: Pass `--interpreter` in mono options (if using mono_main-style entry)
- [ ] Option B: Set via runtime property in monovm_initialize
- [ ] Verify the runtime starts in interpreter-only mode (check syslog for interpreter init messages)

## Phase 3: Hello World Test
- [ ] Create a minimal .NET 10 console app: `Console.WriteLine("Hello from Meadow!")`
- [ ] Compile for `net10.0` targeting Mono
- [ ] Deploy to emulator via Meadow.CLI
- [ ] Execute via monovm_execute_assembly
- [ ] Verify output appears on HCOM or syslog
- [ ] Debug any crashes (likely candidates: missing types, P/Invoke resolution, stack issues)

## Phase 4: Feature Validation
- [ ] Test string operations (concatenation, formatting, StringBuilder)
- [ ] Test collections (List<T>, Dictionary<K,V>)
- [ ] Test exception handling (try/catch/finally, nested exceptions)
- [ ] Test async/await (Task, Task<T>)
- [ ] Test basic P/Invoke: open("/dev/upd"), ioctl with a simple command
- [ ] Test GC: allocate objects in a loop, verify no OOM or corruption
- [ ] Test threading: create a pthread from managed code

## Phase 5: Stability
- [ ] Run Hello World 10 times consecutively without crashes
- [ ] Monitor memory usage during interpretation
- [ ] Check for memory leaks (SGen heap growth)
- [ ] Document interpreter performance baseline (time to execute Hello World)
