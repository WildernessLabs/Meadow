# Track 5: BCL Deployment — System.Private.CoreLib + TPA

## Overview
Build and deploy the .NET 10 Base Class Library assemblies for the Meadow platform. The old `mscorlib.dll` is replaced by `System.Private.CoreLib.dll` plus individual reference assemblies. These are deployed to the device via Meadow.CLI.

## Background

### Old Model (Mono 6.9.0)
- `mscorlib.dll` — Single monolithic assembly containing all core types
- `System.Core.dll`, `System.dll` — Additional framework assemblies
- Deployed to `/meadow0/` on device filesystem
- Loaded via Mono's GAC / assembly probing

### New Model (.NET 10)
- `System.Private.CoreLib.dll` — Core types (shared source with CoreCLR, mono-specific parts in `src/mono/System.Private.CoreLib/`)
- Reference assemblies: `System.Runtime.dll`, `System.Collections.dll`, `System.IO.dll`, etc. — these type-forward to SPCL
- Loaded via TPA (Trusted Platform Assemblies) list passed to `monovm_initialize`
- Assembly resolution uses AssemblyLoadContext (ALC) instead of AppDomains

### Deployment
- Use Meadow.CLI to deploy assemblies to device/emulator
- CLI connects via `socket://localhost:4242` for emulator
- Files transferred via HCOM protocol to `/meadow0/` filesystem

## Functional Requirements
1. Cross-compile `System.Private.CoreLib.dll` for the Mono ARM target
2. Identify minimum set of reference assemblies needed for basic managed code execution
3. Deploy assemblies to device via Meadow.CLI (`meadow file write` or `meadow app deploy`)
4. Update TPA list builder (from Track 4) to enumerate all deployed assemblies
5. Verify type system fully initializes with the new BCL

## Acceptance Criteria
- [ ] `System.Private.CoreLib.dll` built for Mono ARM target
- [ ] Reference assemblies identified and available
- [ ] Assemblies deploy successfully via Meadow.CLI to emulator
- [ ] TPA list correctly enumerates all deployed assemblies
- [ ] Basic type resolution works (System.String, System.Int32, System.Object, etc.)
- [ ] A minimal "Hello World" assembly (Console.WriteLine) runs to completion
- [ ] Assembly sizes documented (total flash footprint)

## Out of Scope
- IL trimming / linker optimization (future enhancement)
- Meadow.Core / Meadow.Foundation retargeting (Track 7)
- Networking-related assemblies (Track 11)
