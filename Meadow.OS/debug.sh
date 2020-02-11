#!/bin/bash

set -e

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
QEMU=false
GDB_SERVER_PORT=4242
LLDB=

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
# Launch QEMU debug server if passed --qemu
#

QEMU_BIN="qemu/build/arm-softmmu/qemu-system-arm"

if [ "$QEMU" = true ] ; then
  if [ ! -r "$scriptdir/$QEMU_BIN" ]; then
    printf "${red}Error:${reset} QEMU could not be found at: $QEMU_BIN$\n"
    exit 0
  fi

  printf "QEMU server is now up.\n"
  if [ -r "$scriptdir/nuttx/nuttx_user.bin" ]; then
    QEMU_ARGS="-device loader,file=$scriptdir/nuttx/nuttx_user.bin,addr=0x08040000"
  fi

  FLASH_FILE=$scriptdir/qemu/meadow_qspi_flash.raw
  FLASH_SIZE=32 # TODO: Read from NuttX .config

  if [ ! -r "$FLASH_FILE" ]; then
    printf "Creating RAW filesystem for QSPI flash block device"
    dd if=/dev/zero of=$FLASH_FILE bs=1m count=$FLASH_SIZE
  fi

  $LLDB $scriptdir/$QEMU_BIN \
    -machine meadow,accel=tcg -nographic -kernel $scriptdir/nuttx/nuttx.elf \
    -chardev stdio,mux=on,id=terminal \
    -serial chardev:terminal \
    -chardev socket,id=hcom,port=1234,host=0.0.0.0,server,nowait -serial chardev:hcom \
    -monitor chardev:terminal $QEMU_ARGS \
    -drive file=$FLASH_FILE,format=raw,if=mtd,id=qspiflash \
    -S -gdb tcp::$GDB_SERVER_PORT -d guest_errors,unimp
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

cd $scriptdir/gdb && arm-none-eabi-gdb-py $MI -q 