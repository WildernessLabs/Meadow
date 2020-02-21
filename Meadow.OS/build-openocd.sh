#!/bin/bash

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

cd $scriptdir/openocd

#
#   Build OpenOCD
#

if [ ! -f $scriptdir/openocd/configure ] || $FORCE || $CLEAN; then
    printf "Running bootstrap...\n"
    run_command "./bootstrap"
fi

printf "Building OpenOCD...\n"

run_command "./configure --disable-werror --disable-ftdi --disable-ti-icdi \
    --disable-ulink --disable-usb-blaster-2 --disable-ft232r \
    --disable-vsllink --disable-xds110 --disable-osbdm \
    --disable-opendous --disable-aice --disable-usbprog \
    --disable-openprog --disable-rlink --disable-armjtagew \
    --disable-kitprog --disable-usb-blaster --disable-presto \
    --disable-openjtag --disable-jlink --enable-stlink"

run_command "make"
