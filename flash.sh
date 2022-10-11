#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

#
#   Work out the OS so that we can change actions per OS where necessary.
#
shopt -s nocasematch
case "$(uname -a)" in
  *darwin*)
    OS="mac"
    ;;
  *linux*)
    OS="linux"
    ;;
  cygwin*|mingw32*|msys*|mingw*)
    OS="windows"
    ;;
  *)
    OS="unknown"
    ;;
esac

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
INCLUDE_RUNTIME=false
RESET_BOARD=false

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
    -rt|--includeruntime)
    INCLUDE_RUNTIME=true
    ;;
    -r|--reset)
    RESET_BOARD=true
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

#
#   Reset the board if requested and possible.  This requires STM32CuberProgrammer
#   to be installed and the CUBE_APP environment variable set to point to the CLI component.
#   On Mac the CUBE_APP will be something like this:
#
#   CUBE_APP=/Applications/STMicroelectronics/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI 
#
reset_meadow() {
  if [[ ! -z "${CUBE_APP}" ]] && [ "$RESET_BOARD" = true ] ; then
    run_command "${CUBE_APP} -c port=swd -hardRst"
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
#   Flash the board with openocd
#
OCD=true
printf "Flashing nuttx binaries using OpenOCD... "
cd $scriptdir
#
#   First check if we should be writing the runtime system to the board.
#
if [ "$INCLUDE_RUNTIME" = true ] ; then
  #
  #   We reset the Meado board first in case it has recently been used in a debug session.
  #   In the debug case it could be that GDB has left the board in a halted state.  This is
  #   only required for the RT and flashing the OS with OCD will work even in the halted state.
  #
  reset_meadow
  #
  #   The sleep is necessary to allow the serial port to be represented by the Meadow board.
  #
  sleep 2
  run_command "meadow mono disable"
  run_command "meadow file write -f $scriptdir/nuttx/Meadow.OS.Runtime.bin"
  run_command "meadow mono flash"
fi
#
#   Next up try to flash the OS.
#
if [[ "$OS" == "mac" ]]; then
  #
  # Custom version of nuttx aware openocd
  #
  run_command "$scriptdir/openocd/src/openocd -s$scriptdir/openocd/tcl -f$scriptdir/flash.cfg"
elif [[ "$OS" == "linux" ]]; then
  # 
  # For Linux the default openocd is being used
  # 
  run_command "openocd -s //usr/local/share/openocd/scripts -f $scriptdir/flash.cfg"
else
  printf "Unsupported OS ${bold}$OS${reset}.\n"
  exit -1
fi
reset_meadow
check_command_status
