#!/bin/bash

set -e

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=false
FORCE=false
CLEAN=false
DEBUG=false
ESP=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -c|--clean)
    CLEAN=true
    ;;
    -d|--debug)
    DEBUG=true
    ;;
    -esp|--esp)
    ESP=true
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

QEMU_DIR=$scriptdir/qemu
TARGET=arm-softmmu

if [ "$ESP" = true ]; then
  QEMU_DIR=$scriptdir/qemu-esp32
  TARGET=xtensa-softmmu
fi

function buildQEMU {
  if $FORCE || $CLEAN; then
    rm -rf $QEMU_DIR/build
  fi

  cd $QEMU_DIR
  mkdir -p build && cd build

  printf "Running configure...\n"
  ../configure --target-list=$TARGET --disable-kvm --disable-docs \
      --enable-debug --disable-plugins --enable-cocoa --disable-sdl \
      --cc="ccache cc" --cxx="ccache c++"

  printf "Building QEMU...\n"
  run_command "make -j8"
}

buildQEMU