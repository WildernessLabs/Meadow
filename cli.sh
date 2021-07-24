#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

CLI_ARGS=()

for arg in $@
do
case $arg in
    --qemu)
    QEMU=true
    ;;
    *)
    CLI_ARGS+=($arg)
    ;;
esac
done

MEADOW_CLI=$scriptdir/../Meadow.CLI/MeadowCLI/bin/Debug/net472/Meadow.CLI.exe

if [ ! -f $MEADOW_CLI ]; then
  printf " ${red}Error:${reset} Meadow.CLI.exe was not found, make sure it is built.\n"
  exit 1
fi

if [ "$QEMU" = true ]; then
  DEVICE="localhost:1234"
else
  if [ -c "/dev/tty.usbmodem1" ]; then
    DEVICE="/dev/tty.usbmodem1"
  else
    if [ -c "/dev/tty.usbmodem01" ]; then
      DEVICE="/dev/tty.usbmodem01"
    else
      printf " ${red}Error:${reset} Meadow CLI device was not found.\n"
      printf " Make sure is it conected or use --SerialPort option.\n"
      exit 1
    fi
  fi
fi

MONO=mono
if [ "$(uname)" == "Darwin" ]; then
  MONO_PATH=/Library/Frameworks/Mono.framework/Versions/Current/bin/
  MONO="$MONO_PATH$MONO"
fi

$MONO $MEADOW_CLI --SerialPort $DEVICE "${CLI_ARGS[@]}"
