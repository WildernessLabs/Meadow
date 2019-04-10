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
    *)
    # unknown option
    ;;
esac
done

run_command() {
  if $VERBOSE; then
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
#   Flash the board with dfu-util
#

#dfu-util -a 0 -D $scriptdir/nuttx/nuttx.bin -s 0x08000000
#dfu-util -a 0 -D $scriptdir/nuttx/nuttx_user.bin -s 0x08040000

#
#   Flash the board with openocd
#

 openocd/src/openocd -s openocd/tcl -f interface/stlink-v2.cfg -f target/stm32f7x.cfg -f flash.cfg
