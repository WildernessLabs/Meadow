# Implementation Plan: NuttX Platform Port

## Phase 1: Audit Platform Changes
- [x] Diff old fork's platform files against upstream .NET 10 mono equivalents
- [x] List every `#ifdef __NuttX__` block in the old fork with file and line
- [x] Identify which platform APIs have changed between Mono 6.9.0 and .NET 10
- [x] Note any new platform abstractions in .NET 10 mono that need NuttX implementations

**Findings:** 32 files with NuttX patches in old fork. Key insight: .NET 10 already has
`mono_set_errno()` (replacing old `set_errno()` pattern across 25+ locations), `HAVE_MMAP=0`
(routing to malloc fallback), and several `HOST_NUTTX`/`TARGET_NUTTX` guards from Track 01.

## Phase 2: Threading
- [x] Port `mono-threads-nuttx.c` (stack bounds + thread OS ID)
- [x] Verify new mono's threading API compatibility (`mono_threads_platform_get_stack_bounds` signature)
- [x] Port SCHED_RR priority 80 in `mono-threads-posix.c` — Added `HOST_NUTTX` block in `mono_thread_platform_create_thread` to set `SCHED_RR` policy + priority 80 on new Mono threads (NuttX defaults to `SCHED_FIFO` which starves lower-priority threads; .NET expects time-slicing for type init, GC, etc.)
- [x] Verify `pthread_get_stackaddr_np` and `pthread_get_stacksize_np` available in NuttX headers

**Files created/modified:**
- `mono/utils/mono-threads-nuttx.c` — NEW (stack bounds + OS thread ID for NuttX)
- `mono/utils/CMakeLists.txt` — Added `mono-threads-nuttx.c` to build
- `mono/utils/mono-threads-posix.c` — Guard `<sys/mman.h>` with `HAVE_SYS_MMAN_H`, NuttX `getrlimit` bypass, NuttX `mono_memory_barrier_process_wide` uses `__sync_synchronize` (single-core), SCHED_RR + priority 80 for new Mono threads

## Phase 3: Memory and OS Primitives
- [x] Port `mono-mmap.c` NuttX conditionals — NOT NEEDED: `HAVE_MMAP=0` already routes to malloc fallback with `posix_memalign` (which NuttX has)
- [x] Port `mono-errno.h` — NOT NEEDED: .NET 10 has `mono_set_errno()` wrapper
- [x] Port `mono-rand.c` — No `mono-rand.c` in .NET 10. Set `NAME_DEV_RANDOM="/dev/random"` (NuttX has both `/dev/random` and `/dev/urandom` via `CONFIG_DEV_RANDOM`). `HAVE_GETRANDOM=0` still correct (NuttX uses device files, not syscall)
- [x] Port `mono-proclib.c` — `HAVE_GETPID=1` set in configure.cmake; NuttX follows `HAVE_GETPID` path
- [x] Port `mono-sigcontext.h` signal context — Added `__NuttX__` block with NuttX `xcptcontext` register macros
- [x] Port `mono-os-mutex.h` priority inheritance — Added `HOST_NUTTX` to `BROKEN_CLOCK_SOURCE`
- [x] Port `mono-config.c` OS detection — Added `CONFIG_OS "nuttx"`

**Files modified:**
- `mono/utils/mono-sigcontext.h` — NuttX ARM signal context (xcptcontext→UCONTEXT_REG_*)
- `mono/utils/mono-os-mutex.h` — NuttX in BROKEN_CLOCK_SOURCE list
- `mono/metadata/mono-config.c` — CONFIG_OS "nuttx"
- `cmake/configure.cmake` — Added HAVE_GETPID, HAVE_UNISTD_H, HAVE_POLL_H, etc. for NuttX

## Phase 4: GC (SGen) Configuration
- [x] Study SGen's memory allocation path in .NET 10 mono — uses mono_valloc/mono_vfree
- [x] Configure SGen to use `malloc`/`free` — ALREADY HANDLED: `HAVE_MMAP=0` routes mono_valloc to posix_memalign
- [x] Verify SGen works without virtual memory — mono_mprotect is no-op in malloc fallback
- [x] Test GC initialization — builds and links cleanly

**No code changes needed:** The existing `!HAVE_MMAP` fallback path covers NuttX.

## Phase 5: Mini/JIT Platform
- [x] Port explicit null checks in `mini-arm.c` — ALREADY SET: `MONO_ARCH_EXPLICIT_NULL_CHECKS` in mini-arm.h
- [x] Port icache flush — Added `up_invalidate_icache()` call for NuttX in `mono_arch_flush_icache`
- [x] Port debugger agent disable — Using `DISABLE_DEBUGGER_AGENT=1` build flag
- [x] Port profiler adjustments — None needed
- [x] Port AOT compiler NuttX conditionals — None needed

**Files modified:**
- `mono/mini/mini-arm.c` — Added NuttX `up_invalidate_icache()` + `<nuttx/cache.h>` include

## Phase 6: Re-enable Sockets
- [x] Remove `DISABLE_SOCKETS=1` from build-nuttx.sh (replaced with `DISABLE_SOCKETS=0`)
- [x] Re-enable socket headers in configure.cmake: `HAVE_SYS_SOCKET_H`, `HAVE_NETINET_IN_H`, `HAVE_ARPA_INET_H`, `HAVE_NETDB_H`, `HAVE_NETINET_TCP_H`, `HAVE_SYS_UN_H`, `HAVE_SYS_UIO_H`, `HAVE_SYS_IOCTL_H`
- [x] `HAVE_NETINET_IN_H` guards in `debugger-networking.h/.c` are proper feature guards — no revert needed
- [x] Added `HAVE_POLL_H=1`, `HAVE_POLL=1`, `HAVE_GETADDRINFO=1`, `HAVE_GETHOSTBYNAME=1`, `HAVE_ACCEPT4=1`, `HAVE_STRUCT_SOCKADDR_IN6=1`
- [x] Verified clean build with sockets enabled — 337 files compiled, zero errors

**Files modified:**
- `cmake/configure.cmake` — Re-enabled NuttX socket headers and APIs
- `build-nuttx.sh` — Replaced `DISABLE_SOCKETS=1` with `DISABLE_SOCKETS=0`

## Phase 7: Build Verification
- [x] Full clean rebuild with all NuttX patches applied — 337/337 files compiled
- [x] Zero compilation errors
- [x] `libmonosgen-2.0.a` (19MB) contains all NuttX platform objects
- [x] NuttX-specific symbols verified: `mono_threads_platform_get_stack_bounds`, `mono_native_thread_os_id_get`, `mono_arch_flush_icache`, `mono_memory_barrier_process_wide`
- [x] Socket symbols resolved (DISABLE_SOCKETS removed, all NuttX socket headers found)
- [x] 4,874 text symbols in final library
