# Track 8: Hardware Validation (Interpreter) — Physical Board Testing

## Overview
Validate the .NET 10 Mono runtime on real F7FeatherV2 and/or CCMv2 hardware, running Blinky in interpreter mode. This is a manual validation track that confirms emulator results translate to real hardware.

## Background
The emulator cannot catch all hardware issues:
- Real SPI/I2C timing
- USB CDC-ACM HCOM (vs TCP socket in emulator)
- Actual GPIO electrical behavior
- Flash/SDRAM performance characteristics
- Power management interactions

## Functional Requirements
1. Flash .NET 10 Mono firmware to physical F7FeatherV2 board via SWD
2. Deploy Blinky via USB-connected Meadow.CLI
3. LED physically blinks
4. Device responds to Meadow.CLI commands (device info, file list, etc.)
5. Repeat on CCMv2 if available

## Acceptance Criteria
- [ ] Firmware flashes successfully via SWD (OpenOCD/STLink)
- [ ] Board boots, HCOM responds over USB CDC-ACM
- [ ] `meadow device info` returns correct OS version
- [ ] Blinky deploys via `meadow app deploy`
- [ ] LED physically blinks on the board
- [ ] App runs continuously for 5+ minutes without crash
- [ ] Board can be reset and re-runs app from flash
- [ ] (Optional) Validated on CCMv2 board as well

## Manual Validation Steps
1. Connect F7FeatherV2 via USB
2. Flash firmware: `./flash.sh` (or OpenOCD command)
3. Wait for boot (watch serial output)
4. Configure CLI: `meadow config route` (USB serial port)
5. Check device: `meadow device info`
6. Deploy app: `meadow app run` from BlinkyCS directory
7. Observe LED — should cycle colors
8. Let run for 5 minutes, check for stability
9. Reset board (button or `meadow device reset`), verify auto-restart

## Out of Scope
- JIT mode testing (separate validation after Track 10)
- Networking / WiFi testing (Track 11)
- Performance benchmarking (informational only at this stage)
