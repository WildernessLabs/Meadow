# Track 6: Hello World — Managed Code Execution + CLI Deployment

## Overview
Get a minimal .NET 10 managed assembly executing on the Meadow emulator via the
interpreter, and establish the Meadow.CLI deployment workflow. This validates the full
runtime stack: init → assembly loading → interpreter execution → P/Invoke → output.

## Prerequisites
- Track 4: Runtime init complete ✅
- Track 5: Interp-to-native trampoline working (runtime can call native functions)

## Background

### What's Already Working (from Track 4)
- `monovm_initialize` → `monovm_execute_assembly` → `mono_main` → `mini_init` ✅
- SPCL loaded from LFS, type system initialized ✅
- Interpreter mode active (`--interpreter`, `mono_llvm_only = TRUE`) ✅
- SDRAM assembly cache (pre-loads DLLs from QSPI to SDRAM) ✅
- GC constrained for SDRAM (`max-heap-size=8m,nursery-size=512k`) ✅
- HCOM responsive on TCP:4242 ✅

### Assembly Deployment
Two paths:
1. **Baked LFS image** — `tools/build_lfs_v1_image` packages DLLs into `littlefs.bin` loaded
   by Renode. Used for emulator testing. Currently has SPCL + 0-byte Meadow.dll placeholder.
2. **Meadow.CLI** — `meadow config route socket://localhost:4242` then `meadow app deploy`.
   Production deployment path. Transfers files via HCOM protocol to `/meadow0/`.

### Test Assembly
A minimal .NET 10 console app:
```csharp
class Program {
    static int Main() {
        Console.WriteLine("Hello from Meadow!");
        return 0;
    }
}
```
This exercises: assembly loading, type resolution, string handling, Console icalls, and
managed-to-native P/Invoke (Console.WriteLine → write syscall).

## Functional Requirements
1. Build a minimal .NET 10 test assembly targeting Mono interpreter
2. Deploy to emulator (baked LFS initially, then CLI)
3. Execute via `monovm_execute_assembly`
4. Verify output appears on HCOM or syslog
5. Verify graceful shutdown via `monovm_shutdown`
6. Establish Meadow.CLI deployment workflow for emulator

## Acceptance Criteria
- [ ] Hello World assembly executes to completion
- [ ] Output visible ("Hello from Meadow!") via HCOM
- [ ] `monovm_shutdown` called and returns cleanly
- [ ] No reset loop after app exits
- [ ] Meadow.CLI can deploy assemblies to emulator over TCP:4242
- [ ] Reference assemblies identified (System.Runtime.dll, System.Console.dll, etc.)
- [ ] Assembly sizes documented (total flash footprint)

## Out of Scope
- Meadow.Core / Meadow.Foundation retargeting (Track 7)
- Exception handling validation (Track 7)
- P/Invoke to firmware-specific APIs (Track 7)
- Performance optimization
