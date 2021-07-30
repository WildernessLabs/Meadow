#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

VERBOSE=false
FORCE=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
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
#   Disassemble binaries
#

NUTTX=$scriptdir/nuttx/nuttx.elf
if [ ! -r "$NUTTX" ]; then
    printf "Missing NuttX ELF binary.\n"
    exit 1
fi

printf "Disassembling nuttx to nuttx.S... "
arm-none-eabi-objdump -D -S $NUTTX > $scriptdir/nuttx/nuttx.S
check_command_status

NUTTX_USER=$scriptdir/nuttx/nuttx_user.elf
if [ -r "$NUTTX_USER" ]; then
    printf "Disassembling nuttx_user to nuttx_user.S... "
    arm-none-eabi-objdump -D -S $NUTTX_USER > $scriptdir/nuttx/nuttx_user.S
    check_command_status
fi
