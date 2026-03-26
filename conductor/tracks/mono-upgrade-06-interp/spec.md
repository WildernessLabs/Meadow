# Track 6: System.Native PAL + Hello World Execution

## Overview

Implement minimal System.Native (and other PAL library) stubs so the .NET runtime can
complete type system initialization and execute a managed Main() method. Then get
Console.WriteLine working for visible "Hello from Meadow!" output.

## Prerequisites
- Track 4: Runtime init complete (monovm_initialize succeeds)
- Track 5: Trampolines working (P/Invoke calls reach firmware, native-to-interp ready)

## Background

### The Blocker (from Track 05 Phase 6)

Even a minimal `static int Main() => 42` app cannot run because the .NET runtime calls
System.Native P/Invoke functions during type system initialization — before reaching Main().
With Track 05's diagnostic logging, the known calls are:

1. `System.Native::SystemNative_LChflagsCanSetHiddenFlag` — returns bool
2. `System.Native::SystemNative_CanGetHiddenFlag` — returns bool
3. `System.Native::SystemNative_GetEnv` — returns `const char*`
4. Unknown additional calls from `System.Globalization.Native` or other PAL libraries

A generic no-op stub (returns 0/NULL) gets past the first three but the runtime hangs
afterward. Track 06 needs to identify ALL P/Invoke calls during startup and provide
appropriate implementations.

### What is System.Native?

`libSystem.Native` is the C shim library the .NET BCL P/Invokes into for POSIX operations:
file I/O, console I/O, sockets, environment variables, process info, signals, time.

On normal Linux/macOS, it's a shared library loaded via dlopen. On NuttX there's no dynamic
linker — functions must be statically linked and resolved through the `PINVOKE_OVERRIDE`
callback that mono_main.c already registers with `monovm_initialize`.

### Other PAL Libraries

The .NET runtime has several native PAL libraries beyond System.Native:
- **System.Globalization.Native** — ICU bindings for globalization (can be disabled with invariant mode)
- **System.Security.Cryptography.Native** — crypto operations (deferred to Track 11)
- **System.IO.Compression.Native** — zlib bindings (likely not needed for Hello World)
- **System.Net.Security.Native** — TLS (deferred to Track 11)

### Console.WriteLine Path

```
Console.WriteLine("Hello")
  → System.Console.dll / System.IO.StreamWriter
  → System.Runtime / System.IO.Stream.Write
  → P/Invoke: System.Native::SystemNative_Write(fd, buf, len)
  → NuttX write() to /var/log/monousermod-stdout FIFO
  → HCOM transport → TCP:4242
```

The firmware creates stdout/stderr FIFOs at `/var/log/monousermod-stdout` and
`/var/log/monousermod-stderr`. Console.WriteLine needs `SystemNative_Write` targeting
the stdout file descriptor.

## Approach

### Strategy: Stub Table + Invariant Globalization

1. **Enable invariant globalization** — set `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` via
   monovm properties or environment. This eliminates all System.Globalization.Native calls.
2. **Implement a stub table** in `mono_main.c` mapping System.Native function names to
   real or stub implementations.
3. **Start with diagnostic stubs** that log calls and return safe defaults.
4. **Replace stubs with real implementations** function by function, starting with the
   ones needed for `return 42`, then Console.WriteLine.

### Minimum Functions for `return 42`

Based on Track 05 findings, likely ~5-10 System.Native functions called during init:
- `SystemNative_GetEnv` — implement using NuttX `getenv()`
- `SystemNative_LChflagsCanSetHiddenFlag` — return false (no HFS+ on NuttX)
- `SystemNative_CanGetHiddenFlag` — return false
- Others TBD (diagnostics will reveal them)

### Additional Functions for Console.WriteLine

- `SystemNative_Write` — implement using NuttX `write()`
- `SystemNative_Open` — implement using NuttX `open()`
- `SystemNative_Close` — implement using NuttX `close()`
- `SystemNative_FStat` / `SystemNative_Stat` — implement using NuttX `fstat()` / `stat()`
- `SystemNative_IsATty` — return 1 for fd 0/1/2

## Functional Requirements

1. Identify all P/Invoke calls made during runtime initialization (before Main)
2. Implement stubs/real functions for each one
3. Execute `static int Main() => 42` to completion (exit code 42 in syslog)
4. Implement SystemNative_Write and dependencies for Console output
5. Execute Console.WriteLine and verify output on TCP:4242

## Acceptance Criteria

- [x] `return 42` app executes to completion, exit code 42 visible in syslog
- [x] All System.Native P/Invoke calls during init have documented implementations (26 functions)
- [x] Console.WriteLine("Hello from Meadow!") output visible (via syslog — HCOM stdout requires USB host)
- [x] monovm_shutdown completes cleanly after app exit
- [x] No crash, no hang, no reset loop
- [x] System.Globalization.Native calls eliminated via invariant mode
- [x] Reference assemblies (System.Runtime.dll, System.Console.dll, etc.) sizes documented (13 assemblies, 8.3MB)

## Out of Scope
- Full System.Native port (only implement what's needed for Hello World)
- Networking / sockets (Track 11)
- Cryptography (Track 11)
- Meadow.Core / GPIO (Track 07)
- Meadow.CLI deployment workflow (deferred — baked LFS is sufficient)
- Exception handling edge cases
