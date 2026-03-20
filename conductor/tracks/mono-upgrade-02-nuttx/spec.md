# Track 2: NuttX Platform Port — Threading, Memory, Signals

## Overview
Port all NuttX platform abstractions from the Mono 6.9.0 fork to the .NET 10 Mono runtime. This includes threading, memory management, errno handling, signal context, and process/OS detection.

## Background
The current fork has these NuttX-specific files and patches:
- `mono/utils/mono-threads-nuttx.c` — Thread stack bounds via `pthread_get_stackaddr_np`
- `mono/utils/mono-threads-posix.c` — SCHED_RR priority 80 for Mono threads
- `mono/utils/mono-errno.h` — `set_errno()` instead of direct assignment
- `mono/utils/mono-mmap.c` — No mmap on NuttX
- `mono/utils/mono-proclib.c` — Process library disabled
- `mono/utils/mono-rand.c` — Random number generation adjustments
- `mono/utils/mono-sigcontext.h` — Signal context handling
- `mono/utils/mono-os-mutex.h` — Priority inheritance support
- `mono/metadata/mono-config.c` — `CONFIG_OS "nuttx"` detection
- `mono/mini/mini-arm.c` — Explicit null checks enabled
- `mono/mini/debugger-agent.c` — Disabled on NuttX
- `mono/sgen/sgen-*.c` — GC tweaks for no-MMU environment

## Functional Requirements
1. Port `mono-threads-nuttx.c` to new mono's threading infrastructure
2. Port SCHED_RR thread priority configuration
3. Port errno handling (`set_errno`)
4. Port memory management (no mmap, malloc-based allocation for SGen)
5. Port null check enforcement for ARM on NuttX
6. Port OS detection (`CONFIG_OS "nuttx"`)
7. Add `#ifdef __NuttX__` guards matching the patterns in the old fork
8. Ensure SGen GC works without virtual memory / MMU
9. Re-enable sockets for NuttX — sockets were disabled in Track 01 (`DISABLE_SOCKETS=1`, `HAVE_SYS_SOCKET_H=0`, etc.) because NuttX headers weren't wired up. NuttX does have a POSIX socket API. This track should re-enable sockets by providing the correct NuttX networking headers and fixing any remaining socket-related compile issues. Needed for HCOM, usrsock, and eventually network-based debugging.

## Acceptance Criteria
- [ ] All NuttX platform files compile in the new mono build
- [ ] No unresolved NuttX-specific symbols remain
- [ ] SGen GC initializes with malloc-based allocation (no mmap calls)
- [ ] Thread creation works with correct priority and scheduling policy
- [ ] `libmonosgen.a` links cleanly with NuttX object files
- [ ] Sockets compile and link (DISABLE_SOCKETS removed, NuttX socket headers found)

## Out of Scope
- Mbed TLS integration (Track 11)
- Thumb2 JIT backend (Track 10)
- Entry point / hosting API (Track 4)
