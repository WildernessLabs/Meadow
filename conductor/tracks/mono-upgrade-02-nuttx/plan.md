# Implementation Plan: NuttX Platform Port

## Phase 1: Audit Platform Changes
- [ ] Diff old fork's platform files against upstream .NET 10 mono equivalents
- [ ] List every `#ifdef __NuttX__` block in the old fork with file and line
- [ ] Identify which platform APIs have changed between Mono 6.9.0 and .NET 10
- [ ] Note any new platform abstractions in .NET 10 mono that need NuttX implementations

## Phase 2: Threading
- [ ] Port `mono-threads-nuttx.c` (stack bounds + thread OS ID)
- [ ] Verify new mono's threading API compatibility (`mono_threads_platform_get_stack_bounds` signature)
- [ ] Port SCHED_RR priority 80 in `mono-threads-posix.c`
- [ ] Verify `pthread_get_stackaddr_np` and `pthread_get_stacksize_np` available in NuttX headers

## Phase 3: Memory and OS Primitives
- [ ] Port `mono-mmap.c` NuttX conditionals (fallback to malloc for code memory)
- [ ] Port `mono-errno.h` (`set_errno` usage)
- [ ] Port `mono-rand.c` adjustments (NuttX random number source)
- [ ] Port `mono-proclib.c` disabling
- [ ] Port `mono-sigcontext.h` signal context
- [ ] Port `mono-os-mutex.h` priority inheritance support
- [ ] Port `mono-config.c` OS detection

## Phase 4: GC (SGen) Configuration
- [ ] Study SGen's memory allocation path in .NET 10 mono
- [ ] Configure SGen to use `malloc`/`free` instead of `mmap`/`munmap` for heap pages
- [ ] Verify SGen works without virtual memory (no guard pages, no address space reservation)
- [ ] Test GC initialization in isolation if possible

## Phase 5: Mini/JIT Platform
- [ ] Port explicit null checks in `mini-arm.c`
- [ ] Port debugger agent disable
- [ ] Port profiler adjustments if any
- [ ] Port any AOT compiler NuttX conditionals

## Phase 6: Re-enable Sockets
Track 01 disabled sockets (`DISABLE_SOCKETS=1`) and zeroed out all networking headers
(`HAVE_SYS_SOCKET_H=0`, `HAVE_NETINET_IN_H=0`, `HAVE_NETDB_H=0`, etc.) because the
NuttX headers weren't properly wired in and the debugger-networking code had unconditional
references to `struct sockaddr_in` / `struct in_addr`. Now that the base build works:
- [ ] Remove `DISABLE_SOCKETS=1` from build-nuttx.sh and verify NuttX socket headers are found
- [ ] Re-enable `HAVE_SYS_SOCKET_H`, `HAVE_NETINET_IN_H`, `HAVE_ARPA_INET_H`, `HAVE_NETDB_H` in configure.cmake overrides
- [ ] Fix any remaining socket-related compile errors (NuttX has POSIX sockets but may lack some Linux-specific extensions like `accept4`, `IP_PKTINFO`, etc.)
- [ ] Revert the `HAVE_NETINET_IN_H` guards added to `debugger-networking.h/.c` in Track 01 (they should compile normally once headers are available)
- [ ] Verify socket-related symbols resolve (may still have link-time unresolved symbols until NuttX libc is linked — that's OK)

## Phase 7: Build Verification
- [ ] Full rebuild with all NuttX patches applied
- [ ] Verify no unresolved symbols from platform layer
- [ ] Verify `libmonosgen.a` includes all NuttX platform objects
