# Track 04 Handoff: monovm Hosting API Integration

## Where We Are

We're upgrading the Meadow.OS firmware from Mono 6.9 to .NET 10 Mono. Track 04 covers
the runtime initialization — getting `monovm_initialize` → `monovm_execute_assembly` →
`monovm_shutdown` working on the Renode emulator.

**Phases 1-3 are complete** — the firmware builds, links with .NET 10's libmonosgen-2.0.a,
and the new `meadow_mono_main()` entry point calls the monovm hosting API.

**Phase 4 (emulator validation) is ~90% complete.** The full init chain works:
- `monovm_initialize()` returns 0 ✅
- `monovm_execute_assembly()` reached ✅
- `mono_main()` → `mini_init()` → `mono_init_internal()` reached ✅
- System.Private.CoreLib.dll (SPCL) deployed on LFS and pre-cached to tmpfs ✅
- HCOM responsive on TCP:4242 ✅
- No crash, no reset loop ✅

**BLOCKER:** Mono task deadlocks in `sem_wait` inside `mono_init_internal()`.

## The Deadlock (Root-Caused)

`.NET 10 sgen GC creates worker threads during initialization:`
```
mono_init_internal → mono_gc_base_init → sgen_gc_init → sgen_thread_pool_start
  → pthread_create (for each GC worker thread)
```

NuttX's `pthread_create` calls `sched_lock()` before `task_activate()`, then calls
`pthread_sem_take()` to wait for the child to signal it started. But the child can't
run because `sched_lock()` prevents context switching. Hard deadlock.

Legacy Mono 6.9 avoided this — it didn't create GC threads during `mini_init()`.

WASM has the same issue and solves it with `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1`,
which compiles out `sgen_thread_pool_start()` entirely.

## Immediate Next Step

Add `-DDISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1` to `runtime/src/mono/build-nuttx.sh`.
This makes `sgen_thread_pool_start()` an empty function — no `pthread_create` calls.
GC runs on the main thread, which is correct for single-core Cortex-M7.

Then rebuild Mono and firmware, and test:

```bash
# 1. Rebuild Mono runtime
cd runtime/src/mono/build-nuttx-debug
cmake . -DDISABLE_SGEN_MAJOR_MARKSWEEP_CONC=ON
cmake --build . -- -j8

# 2. Rebuild firmware (uses the proper build script — NEVER run make directly)
cd Meadow.OS.Emulator
bash build-meadow.os-emulated.sh

# 3. Copy artifacts to emulator build dir
cp build/nuttx_*.bin build/nuttx*.elf build/hooks.resc build/sdram-patches.resc \
   build/addresses.resc build/reset-sdram-patches.resc build/dotnet10/

# 4. Update mono_bss_zero.bin (BSS address may shift)
BSS_START=$(arm-none-eabi-nm build/dotnet10/nuttx_user.elf | awk '/_s_mono_bss/{print $1}')
dd if=/dev/zero of=build/dotnet10/mono_bss_zero.bin bs=$((0xc0300000 - 0x$BSS_START)) count=1
# Update the address in scripts/meadow-dotnet10-headless.resc if it changed

# 5. Test
bash -c '(sleep 600) | mono /Applications/Renode.app/Contents/MacOS/bin/Renode.exe \
  --console --disable-xwt -e "include @scripts/meadow-dotnet10-headless.resc; \
  machine StartGdbServer 3333"' &
sleep 300
arm-none-eabi-gdb -batch -ex "target remote localhost:3333" -ex "bt 15" -ex "detach" \
  build/dotnet10/nuttx_user.elf
```

**Expected result:** Mono should get past `sgen_gc_init` and proceed to load SPCL.
If `monovm_shutdown` fires, Mono completed and exited (good). Check HCOM for status.

## After the GC Fix

Once `mono_init_internal` completes:
1. **Test graceful shutdown** — Meadow.dll is a 0-byte dummy. The code at `mono_main.c:488`
   checks for it and calls `monovm_execute_assembly`. With a 0-byte PE, Mono should fail
   to load it and exit cleanly via `monovm_shutdown`.
2. **Phase 5: P/Invoke validation** — create a minimal managed assembly that does a
   `DllImport("nuttx")` call, deploy it as Meadow.dll, verify the pinvoke override resolves.

## Repos and Branches

| Repo | Branch | What's There |
|------|--------|-------------|
| `Wilderness_Labs/Meadow/` | `Feature_NuttX_dotnet10` | NuttX firmware with monovm hosting API, .mono_bss zeroing, interpreter mode, tmpfs caching |
| `Wilderness_Labs/Meadow.OS.Emulator/` | `feature/dotnet10-emulator` | Renode scripts, hooks, build script with tmpfs/kconfig tweaks |
| `Wilderness_Labs/runtime/` | `Feature_NuttX_dotnet10` | .NET 10 Mono with NuttX signal fix (20/21/22), trampoline skip, HOST_NUTTX guards |

## Key Context

- **NEVER run `make` directly in Meadow/nuttx/.** Always use `Meadow.OS.Emulator/build-meadow.os-emulated.sh`. See `conductor/workflow.md`.
- **Use GDB for debugging.** Start Renode with `machine StartGdbServer 3333`, connect with `arm-none-eabi-gdb -batch -ex "target remote localhost:3333"`. Use `nuttx.elf` for kernel frames, `nuttx_user.elf` for user/mono frames.
- **Mono runtime headers mismatch:** Mono is compiled against `Meadow.OS/nuttx/include` (MAX_SIGNO=63) but firmware uses `Meadow/nuttx/include` (MAX_SIGNO=31). Signal numbers are hardcoded to 20/21/22 to work around this.
- **`.mono_bss` must be zeroed on every boot.** Firmware does it in `mono_main.c`. Emulator does initial zero via `mono_bss_zero.bin` LoadBinary. The `_s_mono_bss` address shifts when firmware is rebuilt.
- **SPCL build:** `cd runtime && ./build.sh -c Debug -subset Mono.CoreLib` produces `artifacts/bin/mono/osx.arm64.Debug/System.Private.CoreLib.dll` (5.76 MB).
- **LFS image:** `tools/build_lfs_v1_image /dev/null build/dotnet10/littlefs.bin build/dotnet10/assemblies` places DLLs at LFS root.

## Spec and Plan

- Spec: `conductor/tracks/mono-upgrade-04-init/spec.md`
- Plan: `conductor/tracks/mono-upgrade-04-init/plan.md` (comprehensive with all findings)
