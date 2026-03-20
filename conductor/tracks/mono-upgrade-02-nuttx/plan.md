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

## Phase 6: Build Verification
- [ ] Full rebuild with all NuttX patches applied
- [ ] Verify no unresolved symbols from platform layer
- [ ] Verify `libmonosgen.a` includes all NuttX platform objects
