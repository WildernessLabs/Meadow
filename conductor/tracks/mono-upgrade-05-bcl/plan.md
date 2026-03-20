# Implementation Plan: BCL Deployment

## Phase 1: Build System.Private.CoreLib
- [ ] Understand how SPCL is built in dotnet/runtime (it's a managed assembly, compiled by the build system)
- [ ] Identify the CMake/MSBuild targets for building SPCL for Mono
- [ ] Cross-compile SPCL for ARM Mono target (may need to use the host .NET SDK to compile)
- [ ] Verify the output DLL is compatible with the Mono runtime we built in Track 1

## Phase 2: Identify Required Assemblies
- [ ] Determine which reference assemblies are needed for basic execution
- [ ] Categorize: essential (SPCL, System.Runtime) vs. app-dependent (System.Collections, System.IO, etc.)
- [ ] Check if reference assemblies need special compilation or can use standard .NET 10 build output
- [ ] Document the full assembly list with sizes

## Phase 3: Deploy to Emulator
- [ ] Start emulator with `./run.sh`
- [ ] Configure Meadow.CLI route: `meadow config route socket://localhost:4242`
- [ ] Deploy SPCL: `meadow file write -f System.Private.CoreLib.dll`
- [ ] Deploy reference assemblies
- [ ] Verify files are on the device: `meadow file list`

## Phase 4: TPA Integration
- [ ] Update TPA list builder in `meadow_mono_main` (from Track 4) to find all deployed DLLs
- [ ] Verify TPA list includes full paths (e.g., `/meadow0/System.Private.CoreLib.dll`)
- [ ] Test monovm_initialize with complete TPA list
- [ ] Verify assembly preload hook resolves assemblies correctly

## Phase 5: Validation
- [ ] Create a minimal "Hello World" .NET 10 console app targeting Mono
- [ ] Deploy it via Meadow.CLI
- [ ] Execute via monovm_execute_assembly in interpreter mode
- [ ] Verify Console.WriteLine output appears on syslog/HCOM
- [ ] Document any type resolution failures and fix them
