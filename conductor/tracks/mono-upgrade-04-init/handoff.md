# Track 04 Handoff: monovm Hosting API Integration

## Where We Are

We're upgrading the Meadow.OS firmware from Mono 6.9 to .NET 10 Mono. Track 04 covers
the runtime initialization — getting `monovm_initialize` → `monovm_execute_assembly` →
`monovm_shutdown` working on the Renode emulator.

**Phases 1-3 are complete** — the firmware builds, links with .NET 10's libmonosgen-2.0.a,
and the new `meadow_mono_main()` entry point calls the monovm hosting API.

**Phase 4 (emulator validation) is ~95% complete.** The full init chain works:
- `monovm_initialize()` returns 0 ✅
- `monovm_execute_assembly()` reached ✅
- `mono_main()` → `mini_init()` → `mono_init_internal()` reached ✅
- System.Private.CoreLib.dll (SPCL) deployed on LFS and pre-cached to tmpfs ✅
- HCOM responsive on TCP:4242 ✅
- `struct stat` ABI mismatch FIXED (see below) ✅
- `g_file_test` correctly identifies SPCL as regular file ✅
- `sgen_gc_init` → `alloc_nursery` reached with correct args ✅
- `mono_pagesize()` returns 4096 (was returning -1) ✅
- Concurrent GC disabled (`DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1`) ✅

**BLOCKER:** `mmap` stub crash — `munmap` → `free()` hits NULL pointer in `mm_free`.

## Current Crash (mmap/munmap stubs)

The sgen GC init → marksweep init → lock-free allocator → `desc_alloc` → `mono_valloc`
→ `mmap` (our stub). The `mmap` stub uses `posix_memalign` to allocate memory. But when
Mono later calls `munmap` (via `mono_vfree` or `mono_file_unmap`), the `free()` crashes
with a NULL pointer dereference at address `0x00000008` inside `mm_free`.

```
MMFAR: 00000008   ← NULL pointer + struct offset
PC: 0x08055c92    ← mm_free
Task: Mono
```

**Root cause hypothesis:** `mmap` returns aligned memory from `posix_memalign`, but
`munmap` is called with a different address (e.g., interior pointer or MAP_FAILED).
Check whether `mono_valloc` adjusts the returned pointer before storing it.

**Investigation steps:**
1. Add logging to `mmap`/`munmap` stubs to see what addresses are passed
2. Check if `mono_valloc_aligned` shifts the returned pointer
3. Check if `mono_vfree` passes a different address than `mmap` returned
4. Alternative: make `munmap` a no-op (leak memory) to see if init completes

## Major Fix: struct stat ABI Mismatch (RESOLVED)

Mono was compiled against `Meadow.OS/nuttx/include` (modern NuttX headers) but the
running firmware uses `Meadow/nuttx/include` (legacy headers). Two critical differences:

1. **struct stat field ordering**: Modern starts with `st_dev, st_ino, st_mode`; legacy
   starts with `st_mode` at offset 0. Mono read `st_mode` from the wrong offset.

2. **S_IFREG encoding**: Legacy = `5 << 11` = `0x2800`; modern = `8 << 12` = `0x8000`.
   Even with correct layout, `S_ISREG()` used the wrong constant.

**Fix:** Changed `build-nuttx.sh` to compile Mono against `Meadow/nuttx/include`.
This required a compatibility layer for missing POSIX declarations — see `nuttx-compat.h`
and `nuttx-include-overrides/errno.h` in the runtime repo.

## Previous Blockers (All Resolved)

- **sem_wait deadlock**: `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1` (no GC worker threads)
- **alloc_nursery garbage args**: Was from stale build; args are now 0/0/0 (correct)
- **corlib assertion**: `g_file_test` returned FALSE due to struct stat mismatch (fixed)
- **mono_pagesize() = -1**: NuttX `sysconf(_SC_PAGESIZE)` unsupported; fixed fallback
- **lock-free alloc block_size assertion**: Caused by `mono_pagesize()` = -1 (fixed)

## Immediate Next Step

Fix the `mmap`/`munmap` stub crash. The simplest approach to try first:
1. Make `munmap` a no-op (just return 0, don't call `free`) — this leaks memory but
   lets us see if the rest of GC init + SPCL loading works
2. If that works, implement proper tracking (store mmap'd addresses in a list,
   only free those in munmap)

Then rebuild and test:

```bash
# Rebuild Mono (if runtime source changed):
cd runtime/src/mono/build-nuttx-debug
cmake --build . -- -j8

# Rebuild firmware (ALWAYS use the build script):
cd Meadow.OS.Emulator
bash build-meadow.os-emulated.sh

# Copy artifacts to emulator:
cp build/nuttx_*.bin build/nuttx*.elf build/hooks.resc build/sdram-patches.resc \
   build/addresses.resc build/reset-sdram-patches.resc build/dotnet10/
# Update mono_bss_zero.bin:
BSS_START=$(arm-none-eabi-nm build/dotnet10/nuttx_user.elf | awk '/_s_mono_bss/{print $1}')
dd if=/dev/zero of=build/dotnet10/mono_bss_zero.bin bs=$((0xc0300000 - 0x$BSS_START)) count=1

# Test:
bash -c '(sleep 600) | mono /Applications/Renode.app/Contents/MacOS/bin/Renode.exe \
  --console --disable-xwt -e "include @scripts/meadow-dotnet10-headless.resc; \
  machine StartGdbServer 3333"' &
sleep 20
arm-none-eabi-gdb build/dotnet10/nuttx_user.elf -batch \
  -ex "target remote localhost:3333" -ex "break abort" -ex "continue" \
  -ex "bt 15" -ex "detach"
```

## After the mmap Fix

Once `sgen_gc_init` completes and SPCL loads:
1. **Test graceful shutdown** — Meadow.dll is 0-byte; Mono should fail to load it
   and exit cleanly via `monovm_shutdown`
2. **Phase 5: P/Invoke validation** — minimal managed assembly with DllImport("nuttx")

## Repos and Branches

| Repo | Branch | What's There |
|------|--------|-------------|
| `Wilderness_Labs/Meadow/` | `Feature_NuttX_dotnet10` | NuttX firmware with monovm hosting API, mmap stubs, .mono_bss zeroing |
| `Wilderness_Labs/Meadow.OS.Emulator/` | `feature/dotnet10-emulator` | Renode scripts, hooks, build script |
| `Wilderness_Labs/runtime/` | `Feature_NuttX_dotnet10` | .NET 10 Mono with NuttX ABI compat layer, signal fix, trampoline skip |

## Key Context

- **NEVER run `make` directly in Meadow/nuttx/.** Always use `Meadow.OS.Emulator/build-meadow.os-emulated.sh`.
- **Use GDB for debugging.** Start Renode with `machine StartGdbServer 3333`. Use `nuttx.elf` for kernel frames, `nuttx_user.elf` for user/mono frames.
- **Use cercano MCP tools** for analyzing large files/logs locally instead of sending to cloud. `cercano_summarize`, `cercano_extract`, `cercano_explain` for code flow analysis.
- **Mono runtime headers**: Now compiled against `Meadow/nuttx/include` (legacy headers). Compat layer in `runtime/src/mono/cmake/nuttx-compat.h` and `nuttx-include-overrides/errno.h`.
- **`.mono_bss` must be zeroed on every boot.** BSS_START = 0xC0254800, BSS_END = 0xC0300000.
- **SPCL build:** `cd runtime && ./build.sh -c Debug -subset Mono.CoreLib` (5.76 MB).
- **Syslog**: UART output goes to `/tmp/renode-uart-dotnet10.txt` (USART1 file backend in Renode).
- **Mono watchdog**: `monitor_mono_task` calls `meadow_os_reset_board(0)` when Mono task exits — this causes the SYSRESETREQ loop seen during crashes.

## Spec and Plan

- Spec: `conductor/tracks/mono-upgrade-04-init/spec.md`
- Plan: `conductor/tracks/mono-upgrade-04-init/plan.md`
