#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
bold=`tput bold`

stutil=`$scriptdir/stlink/build/Release/src/gdbserver/st-util`

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
    -s|-server|--server)
    SERVER=true
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
# Launch the GDB debug server if passed the --server flag.
#

if [ "$SERVER" = true ] ; then
  exec $stutil -v -m --semihosting
fi

#
#   Check if .gdbinit is available.
#

if [ ! -r "$scriptdir/gdb/.gdbinit" ] || $FORCE; then
    printf "Missing .gdbinit GDB configuration file.\n"
    exit 1
fi

GDB_SERVER_PORT=4242
nc -z localhost $GDB_SERVER_PORT &> /dev/null
if [ $? -ne 0 ]; then
    printf "${red}Error:${reset} ST-Link GDB debugger server is not running.\n"
    printf "Run this command in another terminal: ${bold}st-util --semihosting -v -m${reset}\n"
    exit 1
fi

#
#   Launch GDB with Python scripting configurations
#

cd $scriptdir/gdb && arm-none-eabi-gdb-py -q 
