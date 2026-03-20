# Implementation Plan: Blinky on Interpreter

## Phase 1: Retarget Managed Libraries
- [ ] Retarget Meadow.Contracts to `net10.0` (or multi-target `netstandard2.1;net10.0`)
- [ ] Retarget Meadow.Core to `net10.0`
- [ ] Retarget Meadow.F7 to `net10.0`
- [ ] Fix any API incompatibilities from netstandard2.1 → net10.0
- [ ] Verify all projects build successfully

## Phase 2: Retarget Meadow.Foundation
- [ ] Retarget Meadow.Foundation core to `net10.0`
- [ ] Retarget Meadow.Foundation.Leds (RgbPwmLed) to `net10.0`
- [ ] Verify builds — fix any breaking API changes

## Phase 3: P/Invoke Verification
- [ ] Compare managed P/Invoke struct layouts against native UPD structs
- [ ] Verify struct sizes match (IntPtr size on 32-bit ARM, padding/alignment)
- [ ] Check ioctl function codes haven't changed
- [ ] Verify DllImport("nuttx") resolution works (validated in Track 6)
- [ ] Test UPD.open("/dev/upd") and basic ioctl calls

## Phase 4: Build and Deploy Blinky
- [ ] Retarget BlinkyCS to `net10.0`
- [ ] Ensure correct `App<F7FeatherV2>` definition (match emulator board config)
- [ ] Build BlinkyCS in Release configuration
- [ ] Deploy to emulator: `meadow app deploy` (or `meadow app run`)
- [ ] Monitor syslog for app startup messages

## Phase 5: Debug and Validate
- [ ] Debug app initialization (App<T> constructor, Initialize(), Run())
- [ ] Verify GPIO manager initializes correctly
- [ ] Verify pin configuration calls reach the UPD driver
- [ ] Observe GPIO state changes in Renode
- [ ] Run for 60+ seconds, check for stability
- [ ] Document any issues for hardware validation track
