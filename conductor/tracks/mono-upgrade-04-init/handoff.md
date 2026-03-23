# Track 04 Handoff: monovm Hosting API Integration — COMPLETE

## Where We Are

Track 04 is **complete**. The .NET 10 Mono runtime initializes fully on the Renode emulator.
HCOM confirms "Meadow successfully started MONO". System stable, no reset loop.

**Next track: Track 5 (Interp-to-Native Trampoline)** — the interpreter can't call native
functions yet. See below for the blocker discovered at the end of Track 4.

## What Works

- `monovm_initialize()` returns 0 ✅
- `monovm_execute_assembly()` → `mono_main()` → `mini_init()` ✅
- System.Private.CoreLib.dll (SPCL, 6MB) loaded from LFS/QSPI ✅
- SDRAM assembly cache: DLLs pre-loaded from QSPI to SDRAM at startup ✅
- sgen GC init complete (nursery=512K, max-heap=8M, marksweep) ✅
- Exception system initialized (llvmonly path) ✅
- Type system init (class layout) complete ✅
- Interpreter starts executing (`mono_interp_exec_method` reached) ✅
- HCOM responsive on TCP:4242, reports "Meadow successfully started MONO" ✅
- System stable — single boot, CPU idle after init ✅

## Known Issue: Interp-to-Native Trampoline (Track 5 Blocker)

After init completes, the interpreter tries to call `RuntimeTypeHandle.GetAssembly` (an
icall). This requires the `interp_to_native` trampoline which bridges the interpreter stack
to the ARM C calling convention. The ARM implementation in `tramp-arm.c:1073` generates
this as JIT code — compiled out by `DISABLE_JIT` → `g_assert_not_reached()` → abort.

The abort calls `pthread_exit(NULL)` which silently kills the Mono task. The watchdog
(`monitor_mono_task`) doesn't detect the exit, so the system stays idle instead of resetting.

**Solution (Track 5):** WASM-style per-signature C trampolines. See Track 5 spec and plan.

## All Blockers Resolved in Track 4

| # | Blocker | Root Cause | Fix |
|---|---------|-----------|-----|
| 1 | munmap crash (MMFAR=0x8) | `mono_valloc_aligned` frees sub-regions; `free()` needs exact pointer | munmap is no-op (leak memory) |
| 2 | SPCL fstat st_size=0 | tmpfs `write()` failed (kernel heap too small for 6MB) | Removed tmpfs; SDRAM cache pre-loads from LFS |
| 3 | Exception trampoline asserts | DISABLE_JIT stubs assert; `mono_llvm_only` not set | Set `mono_llvm_only = TRUE` for NuttX INTERP_ONLY |
| 4 | dlopen(NULL) crash | NuttX dlopen dereferences NULL | dlopen/dlsym/dlclose stubs return sentinel |
| 5 | GC exhausts SDRAM (MMFAR=0x10) | Default GC allocates 8MB×2 = 16MB; 29MB heap exhausted | `MONO_GC_PARAMS` matching legacy firmware |
| 6 | sem_wait deadlock | `pthread_create` holds `sched_lock` during `sem_wait` | `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1` |
| 7 | struct stat ABI mismatch | Mono compiled against wrong NuttX headers | Compile against firmware headers + compat layer |
| 8 | mono_pagesize() = -1 | NuttX `sysconf(_SC_PAGESIZE)` unsupported | Fixed fallback to cache default (4096) |
| 9 | Signal number mismatch | Meadow headers SIGRTMIN=32 vs firmware MAX_SIGNO=31 | HOST_NUTTX signal selection (20/21/22) |
| 10 | Trampoline code gen assert | `mono_arch_create_generic_trampoline` stubbed | HOST_NUTTX in `disable_tramps` |

## Memory Layout (SDRAM)

- **Physical SDRAM**: 32MB (FMC: 2^13 × 2^9 × 4 banks × 16-bit)
- **Mono runtime sections**: 0xC0000000–0xC0300000 (3MB: .mono + .mono_data + .mono_bss)
- **User heap**: 0xC0300000+ (~29MB via CONFIG_HEAP2)
- **SDRAM cache**: ~6MB (SPCL pre-loaded from LFS at startup)
- **GC heap**: max 8MB (nursery 512K + major marksweep)
- **Interpreter stack**: 1MB per context

## Repos and Branches

| Repo | Branch | What's There |
|------|--------|-------------|
| `Wilderness_Labs/Meadow/` | `Feature_NuttX_dotnet10` | NuttX firmware with monovm hosting API, mmap/dlopen stubs, SDRAM cache |
| `Wilderness_Labs/Meadow.OS.Emulator/` | `feature/dotnet10-emulator` | Renode scripts, hooks, build script |
| `Wilderness_Labs/runtime/` | `Feature_NuttX_dotnet10` | .NET 10 Mono with NuttX ABI compat, mono_llvm_only, exception stubs |

## Key Context

- **NEVER run `make` directly in Meadow/nuttx/.** Always use `Meadow.OS.Emulator/build-meadow.os-emulated.sh`.
- **Use GDB for debugging.** Start Renode with `machine StartGdbServer 3333`.
- **Assemblies served from SDRAM cache** — pre-loaded from LFS at startup, mmap stub uses memcpy.
- **mono_llvm_only = TRUE** for NuttX interpreter mode — skips all JIT trampoline/icall code.
- **GC constrained**: `MONO_GC_PARAMS="max-heap-size=8m,nursery-size=512k,soft-heap-limit=4m,major=marksweep"`
- **`.mono_bss` must be zeroed on every boot.** BSS_START = 0xC0254800, BSS_END = 0xC0300000.
- **Syslog**: UART output goes to `/tmp/renode-uart-dotnet10.txt`. User-space syslog suppressed by syslog_skip hook (semaphore deadlock if enabled).
- **HCOM**: TCP:4242, responds with device info and "Meadow successfully started MONO".
- **abort() behavior**: Mono calls `abort()` → NuttX `pthread_exit(NULL)` → task dies silently. Watchdog doesn't detect it. Needs fixing.
