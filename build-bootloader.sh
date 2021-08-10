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
HELP=false

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
    -m|--mono)
    # MONO=true
    # No action in this script.
    ;;
    --netcore)
    # NETCORE=true
    # No action in this script.
    ;;
    --configure)
    # CONFIGURE_ONLY=true
    # No action in this script.
    ;;
    --debug)
    DEBUG=true
    ;;
    --config=*)
    # CONFIG=$(echo $i | cut -f2 -d=)
    # No action in this script.
    ;;
    --u|--unit-test)
    # UNITTEST=true
    # No action in this script.
    ;;
    *)
    echo "Unknown option $i"
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
#
#   Leaving these behind as examples of the stuff we should think about adding.
#
#   echo "  -f|--force                   Force build"
#   echo "  -c|--clean                   Clean build"
#   echo "  --wlclean                    Clean the Wilderness Labs object files"
#   echo "  --configure                  Configure the build"
#   echo "  --debug                      Build with debug symbols"
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

run_command "make -j12 -C $scriptdir/bootloader/Debug"
check_command_status
