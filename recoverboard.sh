#!/bin/bash -e

#set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

FIRST=true
for i in "$@"
do
case $i in
    -h|--help)
    HELP=true
    ;;
    -f|--first)
    FIRST=true
    ;;
    -l|--last)
    FIRST=false=true
    ;;
    *)
    echo "${0##*/} - Unknown option $i"
    exit 1
    ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: ${0##*/} [options]"
  echo "Copy an installed Meadow.OS.bin file to the nuttx directory and flash the STM32."
  echo " "
  echo "Options:"
  echo "  -h|--help                    Show this help message"
  echo "  -f|--first                   Use the first Meadow.OS.bin file (default)"
  echo "  -l|--last                    Use the last Meadow.OS.bin file"
  exit 0
fi

#
#   If we have a Meadow.OS.bin file then preserve it.
#
if test -f "$scriptdir/nuttx/Meadow.OS.bin"; then
    cp -f $scriptdir/nuttx/Meadow.OS.bin $scriptdir/nuttx/Meadow.OS.bin.bak
fi

#
#   Grab the Meadow.OS.bin file from a previos download.
#
if [ "$FIRST" = true ]; then
    find ~/.local/share/WildernessLabs/Firmware -name Meadow.OS.bin | sort | head -n 1 | xargs -I{} cp {} $scriptdir/nuttx
else
    find ~/.local/share/WildernessLabs/Firmware -name Meadow.OS.bin | sort | tail -n 1 | xargs -I{} cp {} $scriptdir/nuttx
fi

#
#   Write the OS to the board
#
bash $scriptdir/flash.sh

#
#   Recover any preserved Meadow.OS.bin file.
#
if test -f "$scriptdir/nuttx/Meadow.OS.bin.bak"; then
    cp -f $scriptdir/nuttx/Meadow.OS.bin.bak $scriptdir/nuttx/Meadow.OS.bin
    rm $scriptdir/nuttx/Meadow.OS.bin.bak
fi
