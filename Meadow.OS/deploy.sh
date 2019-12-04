#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

APP=HelloLED
DEVICE=/dev/tty.usbmodem1
MEADOW_CLI=$scriptdir/../Meadow.CLI/MeadowCLI/bin/Debug/MeadowCLI.exe

mono $MEADOW_CLI --SerialPort $DEVICE --WriteFile -f $scriptdir/mono/libs/bcl/mscorlib.dll
mono $MEADOW_CLI --SerialPort $DEVICE --WriteFile -f $scriptdir/mono/libs/bcl/System.dll
mono $MEADOW_CLI --SerialPort $DEVICE --WriteFile -f $scriptdir/mono/libs/bcl/System.Core.dll
mono $MEADOW_CLI --SerialPort $DEVICE --WriteFile -f $scriptdir/../Meadow.Core/source/Tests/$APP/bin/Debug/App.exe
mono $MEADOW_CLI --SerialPort $DEVICE --WriteFile -f $scriptdir/../Meadow.Core/source/Tests/$APP/bin/Debug/Meadow.dll
