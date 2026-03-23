# Implementation Plan: BCL Deployment

## Status from Track 4

Several items originally scoped for Track 5 were completed during Track 4:
- [x] SPCL built: `cd runtime && ./build.sh -c Debug -subset Mono.CoreLib` (5.76 MB)
- [x] SPCL deployed to emulator via pre-baked LittleFS image
- [x] SPCL loads successfully — `mono_assembly_load_corlib` passes
- [x] TPA list builder works (`build_tpa_list` enumerates `/meadow0/*.dll`)
- [x] Assembly preload hook resolves SPCL correctly
- [x] SDRAM assembly cache pre-loads assemblies from LFS to SDRAM at startup

## Phase 1: Build System.Private.CoreLib ✅ (done in Track 4)
- [x] SPCL built via `./build.sh -c Debug -subset Mono.CoreLib`
- [x] Output: `artifacts/bin/mono/net10.0-none-Debug/System.Private.CoreLib.dll` (5.76 MB)
- [x] Compatible with our NuttX Mono runtime build

## Phase 2: Identify Required Assemblies
- [ ] Determine which reference assemblies are needed beyond SPCL for basic execution
- [ ] Check if a minimal Hello World needs System.Runtime.dll, System.Console.dll, etc.
- [ ] Check if reference assemblies can come from standard .NET 10 SDK output
- [ ] Document the full assembly list with sizes
- **Note:** The interpreter needs the interp-to-native trampoline (Track 6 blocker) before
  we can test if any assemblies beyond SPCL are actually needed at runtime.

## Phase 3: Deploy to Emulator via Meadow.CLI
- [ ] Configure Meadow.CLI for emulator: `meadow config route socket://localhost:4242`
- [ ] Test basic file operations: `meadow file list`, `meadow file write`
- [ ] Deploy SPCL via CLI (currently baked into LFS image — CLI deployment is the proper path)
- [ ] Deploy app assembly (Meadow.dll or Hello World)
- [ ] Verify files visible on device filesystem
- **Current workaround:** SPCL baked into `littlefs.bin` via `tools/build_lfs_v1_image`.
  CLI deployment replaces this with the proper production workflow.

## Phase 4: TPA Integration ✅ (done in Track 4)
- [x] TPA list builder enumerates `/meadow0/*.dll` with colon separators
- [x] `monovm_initialize` receives TPA via `TRUSTED_PLATFORM_ASSEMBLIES` property
- [x] `mono_core_preload_hook` resolves SPCL from TPA path
- [x] APP_PATHS and NATIVE_DLL_SEARCH_DIRECTORIES set to `/meadow0/`

## Phase 5: Validation
- [ ] Create a minimal .NET 10 console app targeting Mono
- [ ] Deploy via Meadow.CLI
- [ ] Execute via monovm_execute_assembly
- [ ] Verify output visible via HCOM
- **Blocked by:** Track 6 interp-to-native trampoline (interpreter can't call native
  methods without it)
