#!/bin/bash

set -e

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
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

IDF_PATH=$scriptdir/esp-idf
if [ ! -d "$IDF_PATH" ]; then
  printf "${red}ERROR:${reset} ESP-IDF SDK was not found, make sure it is installed.\n"
  exit 0
fi

cd $scriptdir/Meadow-ESP32/Source/MeadowComms

#
#   Build ESP32 comms
#

export IDF_PATH=$IDF_PATH
. $IDF_PATH/export.sh

printf "Building ESP32 comms...\n"
make

printf "Creating QEMU image...\n"
./make-qemu-flash-img.sh 2> /dev/null
