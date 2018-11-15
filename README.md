# Meadow OS

The Meadow OS stack is comprised of the following items:

 * **NuttX**
 * **Mono**
 * **Various tools**

## Development Build Instructions

Note that these instructions have been tested on a Mac, Windows (with ubuntu) and Linux, but are definitely optimized for Mac.

### Step 1: Configure Toolchain

You'll need a number of developer tools installed. Nearly everything is done via homebrew. These tools are general developer tools and required libraries (as in the case of libusb).

1. Install Homebrew [if not installed already]:

```bash
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

2. Install the following tools:

```bash
brew install osx-cross/arm/arm-gcc-bin
brew install ccache
brew install autoconf
brew install libtool
brew install cmake
brew install libusb
```

If you already have one or more of the above tools installed then these can be upgraded using the following command:

```bash
brew upgrade [library]
```

### Step 1a: Build custom STLINK utilities

We use a custom version of the STLink utility that adds semi-hosting features. The STLink is a [JTAG](https://en.wikipedia.org/wiki/JTAG) to USB adapter. Nearly any [STLink V2 adapter](https://www.amazon.com/s/ref=nb_sb_noss_2?url=search-alias%3Daps&field-keywords=stlink+v2) (including cheap clones) will work for this.

Roughly speaking; semi-hosting allows us to connect the host development computer to the Meadow device as if it were part of it. Specifically, we use it right now to connect the file system and execute our Mono/Meadow applications from the `/tmp` directory. We also use it to pipe the `STDIO` (`console.writeline`) out to the host computer over JTAG

Clone the WLabs private ST-UTIL repo which has the semi-hosting magic.

 1. `cd` to your Wilderness Labs repo root
 2. Clone the [ST-Util Repo](https://github.com/WildernessLabs/stlink):

```
git clone git@github.com:WildernessLabs/stlink.git
```

Compile per the instructions [here](https://github.com/WildernessLabs/stlink/blob/master/doc/compiling.md)

### Step 2: Repository Structure

1. Create a folder called `Meadow`. Open a terminal window, change directory to where you want the `Meadow` folder and execute:

```bash
mkdir Meadow
```

2. Download the `GetMeadow.sh` script from this repository and put the file in the  directory created above.

3. Ensure that `GetMeadow.sh` is executable by running the following command in the terminal window:

```bash
chmod +x GetMeadow.sh
```

4. Run the `GetMeadow.sh` script:

```
./GetMeadow.sh
```

At the end of the process you should have a folder structure similar to the following:

```
 - Meadow
   |- apps
   |- Nuttx
   |- tools
```

 5. Update path values on `NUTTX_HOME`, `COMMON_FLAGS` & `MONO_DIR`

 Three of the system files contain a hard coded path.  The path should be changed to point to the location of the source files on your machine.

 First, determine the full path of the local copy of the Meadow source files:

```bash
MacBook:Meadow mark$ pwd
/Users/mark/SoftwareDevelopment/WildernessLabs/Meadow
```

The following files need to be changed:

```
mono/build-meadow.sh
mono/meadow-build.sh
nuttx/configs/stm32f777zit6-meadow/kernel/Makefile
```

For example, change `/Users/plasma/Work/wl/meadow/mono/mono` to `/Users/mark/SoftwareDevelopment/WildernessLabs/Meadow/mono/mono`.

**TODO:** There is [branch](https://github.com/WildernessLabs/Meadow/tree/MeadowBaseVariable) of the Meadow OS stack that uses a variable in place of these hard coded paths.

### Step 3. Build the Meadow OS Stack


1. Run the build meadow script in the **mono** folder
 
  ```bash
  cd ./mono/
  ./build-meadow.sh
  ```

2. Compile mono using the **make** command

  ```bash
  make
  ```
 
  This makes Mono and should build successfully

3. Modify the Nuttx `.config` file to enable semi-hosting and filesystem access:

  Open the `/nuttx/.config` file (not `/nuttx/meadow.config`, and add the following to the end:

  ```
#
# SEMI Hosting stuffola.
#
CONFIG_SEMIHOSTING=y
CONFIG_SEMIHOSTING_OPEN=y
CONFIG_SEMIHOSTING_WRITE=y
CONFIG_SEMIHOSTING_READ=y
CONFIG_SEMIHOSTING_STAT=y
CONFIG_SEMIHOSTING_FSTAT=y
CONFIG_SEMIHOSTING_LSEEK=y
```

  Also, search for this line:

  ```
  # CONFIG_FS_READABLE is not set
 ```

  and change to:

  ```
  CONFIG_FS_READABLE=y
 ```

**TODO:** We need to figure out why the base config that sets `CONFIG_FS_READABLE=y` isn't getting propagated correctly to `.config`.


4. Make the Nuttx project:
  ```
  cd ..
  cd ./nuttx/
  make
  ```

  
### Step 5: Configure JTAG

 1. Wire the following STLink V2 pins to the JTAG connector on the board:

  | Meadow Connector | JTAG Pin Name | Meadow Pin |
|------------------|---------------|------------|
| `SWDIO`          | TMS_SWIO      | 2          |
| `GND`            | GND           | 4          |
| `SWCLK`          | TMS_SWCLK     | 6          |
| `3.3V`           | VAPP          | 8          |


  The JTAG pinout is as follows, where pin one is to the left of the connector cutout, on the nearest row:

  ![](Support_Files/JTAG.png)
   
  The end result should look similar to the following:

  ![](Support_Files/JTAG_Wiring.jpg)
  
  Note that your ST-Link adapter pinout may not match the one in the photo.

 2. Plug the ST-Link directly into your computer (or at least a powered USB hub). The ST-Link likely won't work in an unpowered hub. The ST-Link should blink red two or three times and then glow a steady red (maybe).

### Step 6: Test the ST-Link connection via the custom ST-Util

 1. From a terminal window, run:

```bash
stlink/build/Release/src/gdbserver/st-util --semihosting -v -m
```

The output should look something like the following:

```
bryans-MacBook-Pro:nuttx bryancostanich$ st-util
st-util 1.4.0
2018-03-01T19:11:27 INFO src/common.c: Loading device parameters....
2018-03-01T19:11:27 INFO src/common.c: Device connected is: F76xxx device, id 0x10006451
2018-03-01T19:11:27 INFO src/common.c: SRAM size: 0x80000 bytes (512 KiB), Flash: 0x200000 bytes (2048 KiB) in pages of 2048 bytes
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: Chip ID is 00000451, Core ID is  5ba02477.
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: Chip clidr: 09000003, I-Cache: off, D-Cache: off
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c:  cache: LoUU: 1, LoC: 1, LoUIS: 0
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c:  cache: ctr: 8303c003, DminLine: 32 bytes, IminLine: 32 bytes
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: D-Cache L0: 2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: f00fe019 LineSize: 8, ways: 4, sets: 128 (width: 12)
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: I-Cache L0: 2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: f01fe009 LineSize: 8, ways: 2, sets: 256 (width: 13)
2018-03-01T19:11:27 INFO src/gdbserver/gdb-server.c: Listening at *:4242...
```

The most important thing there is that it gets to `Listening at *:4242`. If it says `invalid chip ID` or other issue, see troubleshooting below. Likely it's not wired correctly.

Also, the ST-Link LED should change to green or blue or somesuch. 

If all is good, close ST-Util by pressing `ctrl-c`, to release ST-Util for VS Code to use.

### Step 7: Open Meadow in VS Code

There are multiple ways to build and upload the Meadow OS stack. The simplest, though not the most reliable method is to automatically build in VS Code and deploy that way. It uses the ST-Flash utility to deploy code to the flash over JTAG. It's not nearly as reliable as using DFU-Util to burn over USB, but that requires unplugging the device and putting it into DFU bootloader mode.

VS Code can be installed from [here](https://code.visualstudio.com/).

 1. Launch VS Code
 2. Open the `nuttx` folder. **File > Open**, navigate to the folder and click open.
 3. If it prompts you to install the C++ extension, install it and restart VS Code.
 4. Build the project by pressing `Command + Shift + B`. This should build, and deploy over USB by writing to the flash and start the OS.

**Note:** You must close the ST-Link/ST-Util connection in order to be able to deploy from VS Code. If it's still active, simply press `ctrl+c` in the active `ST-Util` session.


#### 7b: Debugging Nuttx

 1. Set a breakpoint somewhere. `__start` method in `stm32_start.c` is a good place to start, but sometimes that breakpoint isn't hit, so something in `arch/arm/src/common/up_initialize.c` might also be good.
 2. Deploy the app via `Command + Shift + B`.
 3. Wait for it to finish flashing.
 4. Switch to terminal and launch the custom ST-Util:
  
  ```
stlink/build/Release/src/gdbserver/st-util --semihosting -v -m
```
 5. Switch back to VS Code and hit `F5` or **Debug** menu > **Start Debugging**, and it should jump into the breakpoint.

# Other Info

## Flashing via DFU-Util

ST-Flash (part of STLINK) can be unreliable. DFU-Util is generally more reliable.

However, you must first disconnect the board completely from power (both JTAG and USB), hold down the `boot` button, and then plug in USB power. This will put the board in DFU bootloader mode.

Once it's in bootloader mode, you can flash the NuttX (Meadow) binary via (run this from the `Meadow/nuttx` directory:

```bash
dfu-util -a 0 -D nuttx.bin -s 0x08000000 && dfu-util -a 0 -D nuttx_user.bin -s 0x08040000
```

## Debugging via the GNU Debugger

In addition to debugging via VS code, you can use the [GNU Project Debugger (GDB)](https://www.gnu.org/software/gdb/) to debug NuttX on the device via the ST-Link semihosting session.

To launch a GDB debug session run the following command within the `Meadow/nuttx` directory:

```bash
arm-none-eabi-gdb nuttx
```

The `nuttx` argument loads the NuttX debug symbols so you can get source line numbers and such.

Once the debug session starts, you'll need to connect to the device via TTY. In the debug session, execute:

```
target remote :4242
```

The ST-Util session should then report that `GDB connected`.

If there are any breakpoints set (and there likely are), NuttX will pause execution. Type `continue` or `c` to step over and continue executing. If it fails, you can use the `bt` (backtrace) command to see what happened.

**Pro-tip:** Use two terminal windows here; one for ST-Util and one for GDB.

## Serial Troubleshooting

On the current prototype, the USB Serial debug has the TX/RX swapped, so you need to hook up a [Serial to USB adapter](need amazon link) to the following pins:

| pin   | function |
|-------|----------|
| `4`   | `GND`    |
| `D13` | `TX?`    |
| `D12` | `RX?`    |

### Configure Serial Port for Debugging

1. Plug a micro-USB cable into the **USB Serial Debugging** port (micro-usb plug on the right side of the Meadow board, above the JTAG plug).

2. Request the ID of your serial port:

```bash
ls /dev/tty.usbserial*
```

3. Save this ID. You'll use it later to view serial output via:

```bash
screen /dev/tty.usbserial-[ID] 115200
```

Where `[ID]` should be replaced with the ID of your serial port.



### Install Minicom (SerialTTY interface)

```bash
brew install minicom
```

# Troubleshooting

## Invalid Chip ID

If you get the invalid chip ID when trying to create an STLink session to the device, then the device is likely wired up/connected incorrectly.

# Building Mono from Master

1. clone the [Wilderness Labs Mono Repo](https://github.com/wildernessLabs/Mono)

```bash
git clone git@github.com:WildernessLabs/mono.git
```

2. Build:

```bash
cd ./mono
./autogen.sh
make
make
```

Outputs can be found in: `/mcs/class/lib/net_4_x`
