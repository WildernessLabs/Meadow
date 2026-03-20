# Implementation Plan: Hardware Validation (Interpreter)

## Phase 1: Firmware Preparation
- [ ] Build firmware for real hardware (non-emulator defconfig)
- [ ] Verify binary sizes fit in flash
- [ ] Prepare SWD flashing script/command

## Phase 2: Flash and Boot
- [ ] Flash firmware to F7FeatherV2
- [ ] Monitor serial output for NuttX boot messages
- [ ] Verify HCOM responds over USB CDC-ACM
- [ ] Run `meadow device info` — verify version and hardware info

## Phase 3: Deploy and Run Blinky
- [ ] Deploy BlinkyCS via Meadow.CLI
- [ ] Monitor for app startup messages
- [ ] Observe LED behavior — confirm blinking/color cycling
- [ ] Run for 5+ minutes continuously

## Phase 4: Stability and Edge Cases
- [ ] Test board reset (physical button)
- [ ] Verify app auto-restarts after reset
- [ ] Test `meadow device reset` command
- [ ] Test file operations (list, delete, write)
- [ ] Document any hardware-specific issues vs emulator

## Phase 5: CCMv2 Validation (Optional)
- [ ] Flash CCMv2 board
- [ ] Repeat boot, deploy, and blinky tests
- [ ] Note any board-specific differences
