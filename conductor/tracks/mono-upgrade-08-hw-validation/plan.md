# Implementation Plan: Hardware Validation (Interpreter)

## Phase 1: Firmware Preparation
- [x] Build firmware for real hardware (non-emulator defconfig)
- [x] Verify binary sizes fit in flash
- [x] Flash via OpenOCD SWD (OS to internal flash 0x08000000)

## Phase 2: Flash and Boot
- [x] Flash firmware to F7CoreComputeV2
- [x] Monitor serial output for NuttX boot messages
- [x] Verify HCOM responds over USB CDC-ACM
- [x] Fix: emulator .config was disabling USB — added guard in build.sh
- [x] Fix: version placeholders were hardcoded — restored ###TOKEN### format
- [x] Fix: macOS CDC-ACM needs Serial State notification — cdcacm.c patch (PR #800)

## Phase 3: Deploy and Run Blinky
- [x] Deploy BlinkyCS + framework assemblies via Meadow CLI
- [x] App starts, RGB LED cycles (R/G/B) on CoreComputeV2
- [x] Console.WriteLine output visible via `meadow listen`
- [x] Version 2.5.7.58 — OS/Runtime version match confirmed

## Phase 4: Stability
- [x] Board reset — app auto-restarts
- [x] Lockup bit bypass active during bringup (hcom_mono_control.c)

## Completed 2026-03-27
