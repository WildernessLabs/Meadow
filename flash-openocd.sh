#!/bin/bash
# Flash Meadow.OS firmware to F7 via ST-Link/OpenOCD (no DFU mode required)
#
# Usage:
#   ./flash-openocd.sh              # Flash Meadow.OS.bin only
#   ./flash-openocd.sh --verify     # Flash and verify
#   ./flash-openocd.sh --runtime    # Also flash runtime via HCOM after boot
#
# Prerequisites:
#   - openocd installed and on PATH
#   - ST-Link connected (V2 or V3)
#   - Device does NOT need to be in DFU mode
#
# Notes:
#   - This script clears hardware breakpoints (FPB) to prevent GDB-induced boot hangs
#   - Uses SRST for clean reset (important after GDB sessions)
#   - Meadow.OS.bin is written to internal flash at 0x08000000
#   - The runtime (Meadow.OS.Runtime.bin on QSPI) is flashed via HCOM after boot

set -e
SCRIPTDIR="$( cd "$(dirname "$0")" ; pwd -P )"
FIRMWARE="${SCRIPTDIR}/nuttx/Meadow.OS.bin"
RUNTIME="${SCRIPTDIR}/nuttx/Meadow.OS.Runtime.bin"

VERIFY=""
FLASH_RUNTIME=false

for arg in "$@"; do
  case $arg in
    --verify) VERIFY="-c \"verify_image ${FIRMWARE} 0x08000000\"" ;;
    --runtime) FLASH_RUNTIME=true ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

if [ ! -f "$FIRMWARE" ]; then
  echo "Error: ${FIRMWARE} not found. Run build.sh first."
  exit 1
fi

echo "Flashing ${FIRMWARE} ($(stat -f%z "$FIRMWARE" 2>/dev/null || stat -c%s "$FIRMWARE") bytes)..."

openocd -f interface/stlink.cfg -f target/stm32f7x.cfg \
  -c "reset_config srst_only" \
  -c "init" \
  -c "reset init" \
  -c "halt" \
  -c "flash write_image erase ${FIRMWARE} 0x08000000" \
  ${VERIFY:+-c "verify_image ${FIRMWARE} 0x08000000"} \
  -c "mww 0xE0002000 0x00000002" \
  -c "mww 0xE0002008 0" -c "mww 0xE000200C 0" -c "mww 0xE0002010 0" -c "mww 0xE0002014 0" \
  -c "mww 0xE0002018 0" -c "mww 0xE000201C 0" -c "mww 0xE0002020 0" -c "mww 0xE0002024 0" \
  -c "resume 0x080118b0" \
  -c "shutdown"

echo "Flash complete. Device booting..."

if [ "$FLASH_RUNTIME" = true ]; then
  echo "Waiting for USB CDC ACM (up to 4 min for LFS deorphan on QSPI)..."
  for i in $(seq 1 240); do
    if meadow device info >/dev/null 2>&1; then
      echo "Device up after ${i}s"
      echo "Disabling runtime for flash..."
      meadow runtime disable
      sleep 2
      echo "Flashing runtime: ${RUNTIME}"
      meadow runtime flash -f "${RUNTIME}"
      sleep 2
      echo "Enabling runtime..."
      meadow runtime enable
      echo "Runtime flashed. Reset device to start mono."
      exit 0
    fi
    sleep 1
  done
  echo "Warning: Device did not respond to HCOM in 4 minutes."
  exit 1
fi
