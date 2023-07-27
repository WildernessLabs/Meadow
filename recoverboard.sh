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

if [ "$FIRST" = true ]; then
    find ~/.local/share/WildernessLabs/Firmware -name Meadow.OS.bin | sort | head -n 1 | xargs -I{} cp {} $scriptdir/nuttx
else
    find ~/.local/share/WildernessLabs/Firmware -name Meadow.OS.bin | sort | tail -n 1 | xargs -I{} cp {} $scriptdir/nuttx
fi

bash $scriptdir/flash.sh
