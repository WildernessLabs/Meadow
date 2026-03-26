# Track 7: Blinky on Interpreter — End-to-End App Validation

## Overview
Get the Blinky sample application running on the Meadow emulator using the interpreter. This validates the full stack: .NET 10 Mono runtime → Meadow.Core P/Invoke layer → NuttX UPD driver → GPIO hardware (emulated).

## Background

### System.Native PAL Dependency (Critical)

Meadow.Core uses `System.Threading`, `System.IO`, `System.Timers`, `System.Collections.Concurrent`,
and `System.Net` — all of which internally P/Invoke into `libSystem.Native`. Track 06 only
implemented 26 of the 265 System.Native functions (enough for Console.WriteLine). Blinky will
need substantially more, particularly threading (CreateThread, LowLevelMonitor_*), timers
(GetTimestamp, GetSystemTimeAsTicks), and expanded file I/O.

The upstream System.Native source is at `runtime/src/native/libs/System.Native/pal_*.c`.
Most functions are standard POSIX and should work on NuttX with minimal changes.
See `plan.md` Phase 1 for the full analysis.

### Blinky App
Located at `Meadow.Samples/Source/Meadow F7/Blinky/BlinkyCS/`. It:
- Inherits from `App<F7FeatherV2>` (or `App<F7CoreComputeV2>`)
- Creates an `RgbPwmLed` using device pins
- Cycles LED colors in a loop

### P/Invoke Chain
```
C# BlinkyCS → Meadow.Foundation.Leds → Meadow.Core (F7GPIOManager)
  → DllImport("nuttx") → ioctl(/dev/upd) → NuttX GPIO driver → STM32 GPIO
```

### Retargeting Required
- Current TFM: `netstandard2.1`
- Target TFM: `net10.0`
- Affected projects: Meadow.Core, Meadow.Contracts, Meadow.Foundation, Meadow.F7, BlinkyCS

### Board Configuration
- Emulator configured for F7FeatherV2 (or CCMv2)
- GPIO pin definitions must match the configured board
- Board version detection hook in emulator writes version byte to stack

## Functional Requirements
1. Retarget Meadow.Contracts, Meadow.Core, Meadow.F7 to `net10.0`
2. Retarget Meadow.Foundation (at minimum `Meadow.Foundation.Leds`) to `net10.0`
3. Retarget BlinkyCS to `net10.0`
4. Verify all P/Invoke signatures still match native UPD driver structs
5. Deploy BlinkyCS + dependencies to emulator via Meadow.CLI
6. App starts, initializes hardware, toggles GPIO

## Acceptance Criteria
- [ ] Meadow.Core compiles for `net10.0`
- [ ] Meadow.Foundation.Leds compiles for `net10.0`
- [ ] BlinkyCS compiles for `net10.0`
- [ ] All assemblies deploy via Meadow.CLI to emulator
- [ ] App<F7FeatherV2> initializes successfully
- [ ] GPIO state changes visible in Renode (pin toggles in emulator state)
- [ ] App runs continuously without crashes for at least 60 seconds
- [ ] HCOM remains responsive during app execution

## Out of Scope
- JIT execution (running interpreted only)
- Performance optimization
- Hardware testing (Track 8)
- Other Meadow.Foundation peripherals beyond LEDs
