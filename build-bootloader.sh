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
fi

VERBOSE=true
FORCE=false
CLEAN=false
WLCLEAN=false
DEBUG=false
DEBUG_BL_CDC=false
DEBUG_BL_UART=false
HELP=false
MAKE_OPTIONS=

for i in "$@"
do
case $i in
    -h|--help)
    HELP=true
    ;;
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -c|--clean)
    CLEAN=true
    ;;
    --wlclean)
    WLCLEAN=true
    ;;
    --debug)
    DEBUG=true
    ;;
    -mfd|--makefiledebugging)
    MAKE_OPTIONS="--debug VERBOSE=1"
    ;;
    --dbc|--debug-bl-cdc)
    DEBUG_BL_CDC=true
    ;;
    --dbu|--debug-bl-uart)
    DEBUG_BL_UART=true
    ;;
    *)
    echo "${0##*/}: Unknown option $i"
    exit 1
    ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: build-bootloader.sh [options]"
  echo " "
  echo "Options:"
  echo "  -h|--help                    Show this help message"
  echo "  -v|--verbose                 Show verbose output"
  echo "  -mfd|--makefiledebugging     Turn on debug options for make"
  exit 0
fi




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
# Added the ability to clean only the code acced by Wilderness Labs
#

if $WLCLEAN || $CLEAN || $FORCE; then
    run_command "make -j12 $MAKE_OPTIONS -C $scriptdir/bootloader/Debug clean"
fi

if $DEBUG_BL_CDC; then
    run_command "make -j12 $MAKE_OPTIONS -C $scriptdir/bootloader/Debug debug-cdc"
elif $DEBUG_BL_UART; then
    run_command "make -j12 $MAKE_OPTIONS -C $scriptdir/bootloader/Debug debug-uart"
else
    run_command "make -j12 $MAKE_OPTIONS -C $scriptdir/bootloader/Debug"
fi


check_command_status
