# Meadow OS

## Build Instructions

### Step 1: Configure Toolchain

In this section we will cover the steps to install the tools required to build the system.  These instructions are for the Mac and are normally followed once in order to set up a machine.

1. Install Homebrew [if not installed already]:

```bash
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

2. Install `ARM GCC`:

```bash
brew install osx-cross/arm/arm-gcc-bin
```

3. Install `CCache`:

```bash
brew install ccache
```

4. Install autoconf:

```bash
brew install autoconf
```

6. Install libtool:

```bash
brew install libtool
```

If you already have one or more of the above tools installed then these can be upgraded using the following command:

```bash
brew upgrade
```

 7. Install `cmake`

```bash
brew install cmake
```
 
 8. Install `Libusb`

```bash
brew install libusb
```

### Step 1a: Build custom STLINK utilities

Clone the WLabs private ST-UTIL repo (we have a custom build that has some semi-hosting magic)

 1. CD to your wilderness labs repo root
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

 ### Step 3. Build the software

1. Run the build meadow script in the **mono** folder
 
 ```bash
 cd ./mono/
./build-meadow.sh
```

2. Compile mono unsing the **make** command

```bash
make
```
 
This makes Mono and should build successfully

3. Make the Nuttx project:
```
cd ..
cd ./nuttx/
make
```

### (Optional) Step 4. Configure Serial Port for Debugging

1. Plug a micro-USB cable into the **USB Serial Debugging** port (micro-usb plug on the right side of the Meadow board, above the JTAG plug).

2. Request the ID of your serial port:

```bash
ls /dev/tty.usbserial*
```

3. Save this ID. You'll use it later to view serial output via:

```bash
screen /dev/tty.usbserial-[ID] 115200
````

Where `[ID]` should be replaced with the ID of your serial port.
  
### Step 5: Configure JTAG

 1. Wire the following STLink V2 pins to the JTAG connector on the board:

| Meadow Connector | JTAG Pin Name | Meadow Pin |JTAG Pin |
|------------------|---------------|------------|---------|
| V<sub>cc</sub>   | VAPP          | 8          | 1       |
| GND              | GND           | 3          | 20      |
| SWDIO            | TMS_SWIO      | 2          | 7       |
| SWCLK            | TMS_SWCLK     | 6          | 9       |
| Reset_L          | NRST          | 1          | 15      |


The JTAG pinout is as follows, pin 1 is top left, pin 2 is top right:

![](Support_Files/JTAG.png)
   
The end result should look similar to the following:

![](Support_Files/JTAG_Photo.jpg)

 2. Plug the ST-Link directly into your computer (or at least a powered USB hub). The ST-Link likely won't work in an unpowered hub. The ST-Link should blink red two or three times and then glow a steady red.

### Step 6: Test ST-Util

 1. From a terminal window, run:

```bash
st-util
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

Also, the ST-Link LED should change to green. 

If all is good, close ST-Util by pressing `ctrl-c`, to release ST-Util for VS Code to use.

### Step 7: Open Meadow in VS Code

VS Code can be installed from [here](https://code.visualstudio.com/).

 1. Launch VS Code
 2. Open the `nuttx` folder. **File > Open**, navigate to the folder and click open.
 3. If it prompts you to install the C++ extension, install it and restart VS Code.
 4. Build the project by pressing `Command + Shift + B`. This should build, and deploy over USB by writing to the flash and start the OS.


#### 7b: Debugging

 1. Set a breakpoint somewhere. `__start` method in `stm32_start.c` is a good place to start, but sometimes that breakpoint isn't hit, so something in `arch/arm/src/common/up_initialize.c` might also be good.
 2. Deploy the app via `Command + Shift + B`.
 3. Wait for it to finish flashing.
 4. Switch to terminal and run:
```
st-util
```
 5. Switch back to VS Code and hit `F5` or **Debug** menu > **Start Debugging**, and it should jump into the breakpoint.


## Manual Flashing

Meadow can be manually flashed to the device via:

```
st-flash write nuttx.bin 0x08000000
```
