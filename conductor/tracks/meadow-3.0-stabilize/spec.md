# Meadow 3.0 — Stabilization Track

## Overview

Final pre-release cleanup of the .NET 10 Mono upgrade work. Tracks 01–12
landed feature work; this track audits the codebase, decides on lingering
"experimental" knobs, re-validates baselines on hardware, and prepares
release artifacts.

## Goals

1. **Reproducible builds.** Anyone on a clean checkout can build the
   firmware, BCL, and bench app and get a working device — without
   manual patch reversion or stale-state surprises.
2. **No mystery toggles.** Every `#define`, `cmake -D`, and `setenv` in
   the build/runtime path has a clear committed default for v3.0.
3. **Hardware-validated numbers.** Bench results in
   `Meadow_Performance_Benchmarks/README.md` reflect the final firmware
   and are within tight noise bands.
4. **Story for AOT.** Even though full AOT execution isn't shipping in
   3.0, the document trail (Track 12 handoff) is complete enough that
   the next person can pick it up.

## Acceptance Criteria

- [ ] Fresh-checkout build of Meadow.OS produces a working firmware
      (boot to App Up, bench passes) without any uncommitted edits.
- [ ] `DISABLE_AOT` default in `build-nuttx.sh` is committed at the
      intended value with a comment explaining the choice.
- [ ] Explicit-null-checks policy decided and committed (currently the
      linter keeps reverting our experiments — we need to decide on a
      stable state for 3.0).
- [ ] Cross-AOT compiler build path documented (handoff.md updated if
      we want to keep it; deleted from runtime/ if not).
- [ ] `apps/examples/mono/mono_main.c` JIT-opt experiment scaffolding
      either committed-and-dormant (current state) or removed — pick one.
- [ ] `MONO_LOG_LEVEL` and `MONO_LOG_MASK` at production-appropriate
      values (currently `info`/`aot` — fine for now, but verify quiet
      enough at runtime).
- [ ] Bench run on hardware with final firmware, README updated.
- [ ] No regressions vs Track 08 hardware validation results.
- [ ] CI pipeline green on the meadow-3.0 branch tip.
- [ ] Memory budget audit: SDRAM, flash, LFS sizes documented.
- [ ] Release notes drafted.

## Inventory of Loose Ends

Following items came up during the session(s) immediately preceding this
track. Treat as initial backlog — refine as we go.

### Build configuration

| File | Status | Decision needed |
|------|--------|-----------------|
| `runtime/src/mono/build-nuttx.sh` `DISABLE_AOT=0` | Committed in eba6f256a4d | Keep on (carries +2MB libmonosgen; enables future AOT) vs revert to 1 |
| `runtime/src/mono/mono/arch/arm/arm-codegen.h` forced `__thumb2__` | Committed | Migrate to `thumbv7em-none-eabi-nuttx` AOT target triple before final |
| `runtime/src/mono/mono/sgen/sgen-cardtable.h` `MEADOW_AOT_NUTTX_CARDS` | Committed | OK as-is (only triggered with explicit cross-AOT define) |
| `runtime/src/mono/mono/mini/mini-arm.h` `MONO_ARCH_EXPLICIT_NULL_CHECKS` | Linter keeps reverting | Decide policy; current state ON (safe but ~3% slower on Pi) |

### Runtime state

| Path | Status |
|------|--------|
| `apps/examples/mono/mono_main.c` AOT register loop | Committed; runs only if `*.dll.so` present in `/meadow0/` (no-op when absent) |
| `apps/examples/mono/mono_main.c` JIT-opt scaffolding | Committed; dormant (no `#define MEADOW_JIT_OPT_OVERRIDE`) |
| `apps/examples/mono/mono_main.c` MONO_LOG_LEVEL=info, MASK=aot | Committed; verify production-quiet |
| `nuttx/libs/libc/dlfcn/lib_dlopen.c` NULL-handling | Committed |
| `apps/examples/mono/mono_nuttx_stubs.c` dl* stub removal | Committed |

### External coordination

* CLI agent: pending response to .so deploy-whitelist request
  (`agent-comms/os-to-cli.md`).

### Hardware re-validation matrix (Track 08 redux)

- [ ] Boot to App Up
- [ ] Bench: list, GPIO, Pi calc, SoftPWM
- [ ] NetworkInterface enumeration
- [ ] DNS
- [ ] HTTP/1.0 GET
- [ ] HTTPS GET (cert validation callback path)
- [ ] WiFi connect/disconnect

## Out of Scope

* Completing AOT execution (CoreLib invocation crash) — that's Track 12.
* New features outside the .NET 10 upgrade scope.
* CLI changes (other than .so whitelist coordination).
