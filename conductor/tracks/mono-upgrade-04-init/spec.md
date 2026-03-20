# Track 4: Initialization — monovm Hosting API Integration

## Overview
Implement the new Mono runtime initialization using the `monovm_initialize` / `monovm_execute_assembly` hosting API, replacing the legacy `mono_main` / `mono_jit_init` entry point. This is the critical integration point between NuttX and the .NET 10 Mono runtime.

## Background

### Old Entry Point (Mono 6.9.0)
```
hcom_main() → hcom_mono_ctrl_start_mono_main() → task_create("mono", mono_main, argv)
  mono_main (renamed to mono_main_driver in fork)
    → mono_jit_init("meadow")
    → mono_domain_assembly_open("/meadow0/Meadow.dll")
    → mono_jit_exec(domain, assembly, argc, argv)
```

### New Entry Point (.NET 10 Mono)
```
hcom_main() → hcom_mono_ctrl_start_mono_main() → task_create("mono", meadow_mono_main, argv)
  meadow_mono_main (new function):
    → monovm_initialize(propCount, keys, values)  // TPA, APP_PATHS, etc.
    → monovm_execute_assembly(argc, argv, "/meadow0/App.dll", &exitCode)
    → monovm_shutdown(&exitCode)
```

### Key Properties for monovm_initialize
- `TRUSTED_PLATFORM_ASSEMBLIES` — Semicolon-separated list of all assembly paths (must include System.Private.CoreLib.dll)
- `APP_PATHS` — Directory containing the application assemblies (`/meadow0/`)
- `NATIVE_DLL_SEARCH_DIRECTORIES` — Where to find native libraries for P/Invoke
- `PINVOKE_OVERRIDE` — Optional function pointer for custom P/Invoke resolution

### Required File Changes
- `hcom_mono_control.c` — Main integration point, mono launch logic
- `hcom_startup_manager.c` — Un-ifdef the mono startup call (`#if(0)` block)
- Mono headers — Include `mono-private-unstable-functions.h` for monovm API

## Functional Requirements
1. Create `meadow_mono_main()` function that uses the new hosting API
2. Build TPA list by enumerating assemblies on `/meadow0/` filesystem
3. Set required properties: TPA, APP_PATHS, NATIVE_DLL_SEARCH_DIRECTORIES
4. Configure PINVOKE_OVERRIDE for NuttX native library resolution (DllImport("nuttx") → NuttX syscalls)
5. Update required-file checks: `mscorlib.dll` → `System.Private.CoreLib.dll`
6. Remove old `System.Core.dll` / `System.dll` existence checks (now part of TPA)
7. Handle mono startup errors gracefully (log + don't crash the OS)
8. Re-enable the mono startup in `hcom_startup_manager.c` (remove `#if(0)`)

## Acceptance Criteria
- [ ] `monovm_initialize()` returns 0 (success) in emulator — **code written, needs emulator test**
- [ ] `System.Private.CoreLib.dll` loads successfully — **needs emulator test**
- [ ] Type system initializes (basic types resolve) — **needs emulator test**
- [ ] Runtime shuts down cleanly when no app assembly is present — **code written, needs emulator test**
- [ ] HCOM remains responsive during and after mono init — **needs emulator test**
- [ ] P/Invoke resolution for `"nuttx"` library works (tested with a trivial managed call) — **Phase 5**

## Implementation Status
- **Phases 1-3 COMPLETE**: Code written, firmware links successfully
- **Phase 4 IN PROGRESS**: Emulator validation needed
- **Phase 5 TODO**: P/Invoke validation

## Out of Scope
- Full BCL deployment (Track 5 — this track only needs SPCL)
- Interpreter execution of user apps (Track 6)
- Managed hardware access (Track 7)
