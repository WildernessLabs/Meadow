# Implementation Plan: Hello World + CLI Deployment

## Phase 1: Build Test Assembly

- [ ] Create minimal .NET 10 console app (`dotnet new console -n MeadowHello`)
- [ ] Target `net10.0`, reference only System.Runtime and System.Console
- [ ] Build: `dotnet publish -c Release -r linux-arm` (or appropriate RID for Mono)
- [ ] Identify all required assemblies from publish output
- [ ] Document sizes: SPCL + reference assemblies + app = total flash footprint

## Phase 2: Deploy via Baked LFS

- [ ] Update `tools/build_lfs_v1_image` to package: SPCL + reference assemblies + MeadowHello.dll
- [ ] Replace the 0-byte Meadow.dll placeholder with the real test assembly
- [ ] Rebuild LFS image, update Renode script
- [ ] Boot emulator, verify assembly loads

## Phase 3: Execute Hello World

- [ ] Verify `monovm_execute_assembly` reaches managed Main()
- [ ] Debug any missing icall trampolines (add to Track 5's trampoline set)
- [ ] Debug any missing reference assemblies (add to LFS image)
- [ ] Verify Console.WriteLine output appears on HCOM
- [ ] Verify `monovm_shutdown` completes cleanly
- [ ] Verify no reset loop after app exits (watchdog handles Mono exit correctly)

## Phase 4: Meadow.CLI Deployment

- [ ] Configure CLI: `meadow config route socket://localhost:4242`
- [ ] Test `meadow device info` over socket connection
- [ ] Test `meadow file list` — verify LFS contents visible
- [ ] Deploy test assembly: `meadow app deploy` or `meadow file write`
- [ ] Verify deployed files persist across Mono restarts
- [ ] Document CLI workflow for emulator testing

## Phase 5: Graceful Lifecycle

- [ ] Verify Mono startup → execute → shutdown → clean exit
- [ ] Verify monitor_mono_task detects Mono exit and handles it
- [ ] Test with missing app assembly (no Meadow.dll) — should not crash
- [ ] Test with invalid assembly (corrupt .dll) — should report error
- [ ] Run Hello World 5 times consecutively without issues
