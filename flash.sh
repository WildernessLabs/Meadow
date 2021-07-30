#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

VERBOSE=true
FORCE=false
ESP=false
CUBE=false
OS_ONLY=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -dfu|--dfu)
    DFU=true
    ;;
    -ocd|--ocd|--openocd)
    OCD=true
    ;;
    -esp|--esp)
    ESP=true
    ;;
    -cube|--cube)
    CUBE=true
    ;;
    -osonly|--osonly)
    OS_ONLY=true
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
      if [ "$DFU" = true ] ; then
        printf "Try --ocd mode or --verbose flag to see the output.\n"
      else
        printf "Try --dfu mode or --verbose flag to see the output.\n"
      fi
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

if [ "$ESP" = true ] ; then
  printf "Flashing ESP32 chip using ESP-PROG tool\n"

  IDF_PATH=$scriptdir/esp-idf
  if [ ! -d "$IDF_PATH" ]; then
    printf "${red}ERROR:${reset} ESP-IDF SDK was not found, make sure it is installed.\n"
    exit 0
  fi
  export IDF_PATH=$IDF_PATH
  . $IDF_PATH/export.sh

  ESP_DEVICE=/dev/cu.usbserial-1424101

  cd $scriptdir/Meadow-ESP32/Source/MeadowComms
  idf.py flash --port $ESP_DEVICE
  exit 0
fi

#
#   Flash the board with dfu-util
#

DFU_COMMON_FLAGS='--alt 0 --path 20-1'
if [ "$DFU" = true ] ; then
  printf "Flashing nuttx.bin using DFU... "
  run_command "dfu-util $DFU_COMMON_FLAGS --download  $scriptdir/nuttx/nuttx.bin -s 0x08000000"
  check_command_status
  if [ -f $scriptdir/nuttx/nuttx_user.bin ]; then
    printf "Flashing nuttx_user.bin using DFU... "
    run_command "dfu-util $DFU_COMMON_FLAGS --download $scriptdir/nuttx/nuttx_user.bin -s 0x08040000"
    check_command_status
  fi
  exit 0
fi

#
#   Use the STMCubeProgrammer and the CLI tool to flash the board.
#     - Disable Mono
#     - Erase flash memory
#     - Flash the OS
#     - Write the runtime system to the board
#     - Copy the runtime into flash
#
if [ "$CUBE" = true ] ; then
  printf "Flashing binaries using STMCubeProgrammer and Meadow.CLI.exe tool.\n"
  if [ -z "${CUBE_APP}" ]; then
    printf "The environment variable CUBE_APP must be set to point to the STMCubeProgrammer CLI application.\n"
    printf "Typically this is something like /Applications/STMicroelectronics/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI\n"
    exit -1
  fi
  if [ -z "${MEADOW_CLI_APP}" ]; then
    printf "The environment variable MEADOW_CLI_APP should point to the Meadow.CLI.exe binary\n"
    exit -1
  fi
  COMMAND="mono ${MEADOW_CLI_APP} --MonoDisable -s /dev/tty.usbmodem01"
  printf "Executing: $COMMAND"
  run_command "$COMMAND"
  COMMAND="${CUBE_APP} -c port=swd --erase all"
  printf "Executing: $COMMAND"
  run_command "$COMMAND"
  COMMAND="${CUBE_APP} -c port=swd --write $scriptdir/nuttx/Meadow.OS.bin 0x08000000 --verify"
  printf "Executing: $COMMAND"
  run_command "$COMMAND"
  COMMAND="${CUBE_APP} -c port=swd -hardRst"
  printf "Executing: $COMMAND"
  run_command "$COMMAND"
  sleep 1
  printf "Executing: $COMMAND"
  run_command "$COMMAND"
  sleep 1
  if [ "$OS_ONLY" != true ]; then
    COMMAND="mono ${MEADOW_CLI_APP} --WriteFile -f $scriptdir/nuttx/Meadow.OS.Runtime.bin -s /dev/tty.usbmodem01"
    printf "Executing: $COMMAND"
    run_command "$COMMAND"
    COMMAND="mono ${MEADOW_CLI_APP} --MonoFlash -s /dev/tty.usbmodem01"
    printf "Executing: $COMMAND"
    run_command "$COMMAND"
  fi
  exit 0
fi

#
#   Flash the board with openocd
#

OCD=true
printf "Flashing nuttx binaries using OpenOCD... "
cd $scriptdir
run_command "$scriptdir/openocd/src/openocd -s $scriptdir/openocd/tcl -f $scriptdir/flash.cfg"
check_command_status
