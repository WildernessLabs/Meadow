#!/bin/bash

#set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

#trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

stutil="$scriptdir/stlink/build/Release/src/gdbserver/st-util"

VERBOSE=false
FORCE=false
OCD=true
MI=
QEMU=
GDB_SERVER_PORT=4242
LLDB=
ESP=

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -s|-server|--server)
    SERVER=true
    ;;
    -stlink|--stlink)
    OCD=false
    ;;
    -ocd|--ocd|-openocd|--openocd)
    OCD=true
    ;;
    --mi)
    MI=--interpreter=mi
    ;;
    --qemu)
    QEMU=true
    ;;
    --lldb)
    LLDB='lldb --'
    ;;
    --gdb)
    GDB='-S -gdb tcp::$GDB_SERVER_PORT'
    ;;
    --esp)
    ESP=true
    ;;
    *)
    # unknown option
    ;;
esac
done

run_command() {
  if $VERBOSE; then
    echo
    $1
  else
    $1 &>/dev/null
  fi
}

check_command_status() {
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    printf " ${red}error${reset}\n"
    if ! $VERBOSE; then
        printf "Re-run the script with --verbose flag to see the output.\n"
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

#
# Launch QEMU debug server if in QEMU mode.
#

QEMU_BIN_ARM="qemu/build/arm-softmmu/qemu-system-arm"
QEMU_BIN_XTENSA="qemu-esp32/build/xtensa-softmmu/qemu-system-xtensa"

function launchQEMUXtensa {
  if [ ! -r "$scriptdir/$QEMU_BIN_XTENSA" ]; then
    printf "${red}Error:${reset} QEMU could not be found at: $QEMU_BIN_XTENSA$\n"
    exit 0
  fi

  printf "QEMU server is now up.\n"

  QEMU_BOOT_BIN=$scriptdir/Meadow-ESP32/Source/MeadowComms/MeadowComms-qemu.bin
  $LLDB $scriptdir/$QEMU_BIN_XTENSA \
    -machine esp32,accel=tcg -nographic \
    -drive file=$QEMU_BOOT_BIN,if=mtd,format=raw \
    -chardev stdio,mux=on,id=terminal \
    -serial chardev:terminal \
    -monitor chardev:terminal \
    -d guest_errors,unimp
}

function launchQEMUArm {
  if [ ! -r "$scriptdir/$QEMU_BIN_ARM" ]; then
    printf "${red}Error:${reset} QEMU could not be found at: $QEMU_BIN_ARM$\n"
    exit 0
  fi

  printf "QEMU server is now up.\n"

  FLASH_FILE=$scriptdir/qemu/meadow_qspi_flash.raw
  FLASH_SIZE=32 # TODO: Read from NuttX .config

  if [ ! -r "$FLASH_FILE" ]; then
    printf "Creating RAW filesystem for QSPI flash block device"
    dd if=/dev/zero of=$FLASH_FILE bs=1m count=$FLASH_SIZE
  fi

  QEMU_BOOT_BIN=$scriptdir/nuttx/nuttx.bin
  QEMU_BOOT_ARGS='-bios $QEMU_BOOT_BIN'

  if [ -r "$scriptdir/nuttx/Meadow.OS.bin" ]; then
    QEMU_BOOT_BIN= $scriptdir/nuttx/Meadow.OS.bin
  fi

  $LLDB $scriptdir/$QEMU_BIN_ARM \
    -machine meadow,accel=tcg -nographic \
    -device loader,file=$QEMU_BOOT_BIN \
    -chardev stdio,mux=on,id=terminal \
    -serial chardev:terminal \
    -chardev socket,id=hcom,port=1234,host=0.0.0.0,server,nowait \
    -serial chardev:hcom \
    -monitor chardev:terminal \
    -drive file=$FLASH_FILE,format=raw,if=mtd,id=qspiflash \
    -S -gdb tcp::$GDB_SERVER_PORT \
    -d guest_errors,unimp
}

if [ "$QEMU" = true ] && [ "$SERVER" = true ]; then
  if [ "$ESP" = true ]; then
    launchQEMUXtensa
  else
    launchQEMUArm
  fi
  exit 0
fi

#
# Launch the GDB debug server if passed the --server flag.
#

if [ "$SERVER" = true ] ; then
  if [ "$OCD" = true ] ; then
    exec "$scriptdir/openocd/src/openocd" "-s$scriptdir/openocd/tcl" "-f$scriptdir/debug.cfg"
  else
    exec $stutil -v -m --semihosting
  fi
fi

#
#   Check if .gdbinit is available.
#

if [ ! -r "$scriptdir/gdb/.gdbinit" ] || $FORCE; then
    printf "Missing .gdbinit GDB configuration file.\n"
    exit 1
fi

nc -z localhost $GDB_SERVER_PORT &> /dev/null
if [ $? -ne 0 ]; then
    printf "${red}Error:${reset} GDB debugger server is not running.\n"
    printf "Run this command in another terminal: ${bold}./debug.sh --server${reset}\n"
    exit 1
fi

#
#   Launch GDB with Python scripting configurations
#

cd $scriptdir/gdb && ~/gdb/gdb/gdb $MI -q 