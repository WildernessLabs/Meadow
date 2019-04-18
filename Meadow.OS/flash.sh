#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
bold=`tput bold`

VERBOSE=false
FORCE=false

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
run_command "openocd/src/openocd -s openocd/tcl -f interface/stlink.cfg -f target/stm32f7x.cfg -f flash.cfg"
check_command_status