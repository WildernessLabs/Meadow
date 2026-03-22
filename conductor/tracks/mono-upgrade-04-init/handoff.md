# Track 04 Handoff: monovm Hosting API Integration

## Where We Are

We're upgrading the Meadow.OS firmware from Mono 6.9 to .NET 10 Mono. Track 04 covers
the runtime initialization — getting `monovm_initialize` → `monovm_execute_assembly` →
`monovm_shutdown` working on the Renode emulator.

**Phases 1-3 are complete** — the firmware builds, links with .NET 10's libmonosgen-2.0.a,
and the new `meadow_mono_main()` entry point calls the monovm hosting API.

**Phase 4 (emulator validation) is COMPLETE.** The full init chain works:
- `monovm_initialize()` returns 0 ✅
- `monovm_execute_assembly()` reached ✅
- `mono_main()` → `mini_init()` → `mono_init_internal()` reached ✅
- System.Private.CoreLib.dll (SPCL) loaded from LFS/QSPI ✅
- sgen GC init complete (nursery, marksweep) ✅
- Exception system initialized (llvmonly path) ✅
- Type system init (class layout) complete ✅
- HCOM responsive on TCP:4242 ✅
- HCOM reports "Meadow successfully started MONO" ✅
- System stable — no reset loop, CPU idle after init ✅

## Blockers Resolved This Session

### 1. munmap crash in mm_free (FIXED)
Root cause: `mono_valloc_aligned` calls munmap on sub-regions of a single mmap allocation
(prefix/suffix trimming). Our mmap stub uses `posix_memalign`, but `free()` only works on
the exact pointer returned — not interior pointers.
Fix: Made munmap a no-op (leaks memory). Proper tracking deferred — not urgent since
Mono doesn't frequently unmap during steady state.

### 2. SPCL fstat returns st_size=0 (FIXED)
Root cause: tmpfs caching failed silently. `write()` to tmpfs returned -1 because NuttX
tmpfs uses kernel heap (limited SRAM, ~200KB) which can't hold the 6MB SPCL file. The
file was created but empty (0 bytes). `mono_file_map_size` → `fstat` correctly returned 0.
Fix: Removed tmpfs caching entirely. Mono now reads assemblies directly from LFS (`/meadow0/`).
The mmap stub allocates from user heap (SDRAM, plenty of space) and reads via `read()`.
TPA and APP_PATHS now point to `/meadow0/` instead of `/tmp/`.

### 3. Exception trampoline asserts (FIXED)
Root cause: DISABLE_JIT builds stub out `mono_arch_get_restore_context`,
`mono_arch_get_throw_exception`, etc. with `g_assert_not_reached()`. In interpreter mode,
these aren't needed but the init code still called them. NuttX's INTERP_ONLY mode didn't
set `mono_llvm_only`, so trampoline skip checks (`if (!mono_llvm_only)`) still executed.
Fix: Set `mono_llvm_only = TRUE` for NuttX in the INTERP_ONLY path. This is correct because
NuttX DISABLE_JIT + interpreter is functionally equivalent to INTERP_LLVMONLY. Also added
HOST_NUTTX stubs returning NULL in arch-stubs.c and exceptions-arm.c.

### 4. dlopen(NULL) crash (FIXED)
Root cause: Mono calls `dlopen(NULL)` to get a handle to the main program (standard POSIX).
NuttX's dlopen dereferences `file[0]` immediately → NULL pointer crash at MMFAR=0x00000000.
Fix: Added dlopen/dlsym/dlclose/dlerror stubs in mono_nuttx_stubs.c. Returns a sentinel
handle for all requests. Mono falls back to `pinvoke_override` for P/Invoke resolution.

## Previous Blockers (All Resolved)

- **sem_wait deadlock**: `DISABLE_SGEN_MAJOR_MARKSWEEP_CONC=1` (no GC worker threads)
- **alloc_nursery garbage args**: Was from stale build; args are now 0/0/0 (correct)
- **corlib assertion**: `g_file_test` returned FALSE due to struct stat mismatch (fixed)
- **mono_pagesize() = -1**: NuttX `sysconf(_SC_PAGESIZE)` unsupported; fixed fallback
- **lock-free alloc block_size assertion**: Caused by `mono_pagesize()` = -1 (fixed)
- **struct stat ABI mismatch**: Mono compiled against wrong NuttX headers (fixed)
- **trampoline code generation**: Stubbed with HOST_NUTTX in `disable_tramps` (same as WASM)
- **signal number mismatch**: HOST_NUTTX signal selection (20/21/22) in posix-signals.c

## Current State

Mono init is complete. The system is stable with HCOM responding "Meadow successfully
started MONO". The Mono task reaches idle state (all init done, Meadow.dll is 0-byte
placeholder so no app executes).

## Next Steps

1. **Deploy real assemblies via Meadow.CLI** — SPCL is baked into LFS, but Meadow.dll
   needs to be a real .NET 10 app assembly. CLI deployment over TCP:4242 is needed.
2. **Test graceful shutdown** — when Meadow.dll fails to load (0-byte), Mono should exit
   cleanly via `monovm_shutdown`. Verify the watchdog behavior.
3. **Phase 5: P/Invoke validation** — minimal managed assembly with DllImport("nuttx")
4. **Proper munmap** — implement address tracking to actually free mmap'd memory (currently
   leaked). Not urgent but needed for long-running apps.

## Repos and Branches

| Repo | Branch | What's There |
|------|--------|-------------|
| `Wilderness_Labs/Meadow/` | `Feature_NuttX_dotnet10` | NuttX firmware with monovm hosting API, mmap stubs, dlopen stubs |
| `Wilderness_Labs/Meadow.OS.Emulator/` | `feature/dotnet10-emulator` | Renode scripts, hooks, build script |
| `Wilderness_Labs/runtime/` | `Feature_NuttX_dotnet10` | .NET 10 Mono with NuttX ABI compat, mono_llvm_only, exception stubs |

## Key Context

- **NEVER run `make` directly in Meadow/nuttx/.** Always use `Meadow.OS.Emulator/build-meadow.os-emulated.sh`.
- **Use GDB for debugging.** Start Renode with `machine StartGdbServer 3333`.
- **Mono reads assemblies from `/meadow0/`** (LFS/QSPI), NOT `/tmp/` (tmpfs caching removed).
- **mono_llvm_only = TRUE** for NuttX interpreter mode — skips all JIT trampoline/icall code.
- **`.mono_bss` must be zeroed on every boot.** BSS_START = 0xC0254800, BSS_END = 0xC0300000.
- **Syslog**: UART output goes to `/tmp/renode-uart-dotnet10.txt` (USART1 file backend).
- **HCOM**: TCP:4242, responds with device info and "Meadow successfully started MONO".
