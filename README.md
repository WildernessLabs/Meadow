[![Build Status](https://dev.azure.com/WildernessLabs/Meadow/_apis/build/status/Meadow%20OS?branchName=main)](https://dev.azure.com/WildernessLabs/Meadow/_build/latest?definitionId=1&branchName=main)
# Meadow OS

The Meadow OS stack is comprised of the following items:

 * **nuttx** - [NuttX](http://nuttx.org) is the core OS.
 * **mono** - [Mono](https://mono-project.com) is the .NET VM used to host the OS's userspace
 * **apps** - This contains our Mono app, which is the app that runs on NuttX and launches our custom meadow applications.
 * **mbedTLS** - [mbedTLS](https://tls.mbed.org/) Is our TLS (HTTPS) library
 * **Various tools** - In addition to the various off-the-shelf dev tools, we use a custom build of ST-Link utilities to communicate with the board via JTAG.

Binaries of many of some of the build artfiacts can be found on [`Google Drive/Engineering/Meadow Build Artifacts`](https://drive.google.com/drive/u/0/folders/1qAgv49SRB585jm14eg7LBkMCPg_XJzNC).

## Current Branches for Building Meadow for Development

| Repo           | Branch          | Notes                     |
|----------------|-----------------|---------------------------|
| [STLink](https://github.com/WildernessLabs/stlink/tree/meadow) | Meadow   | Has our semi-hosting work. |
| [MbedTLS](https://github.com/WildernessLabs/mbedtls) | meadow-develop | |

## Development Environment Requirements

Mac or Linux is required to build the various pieces of Meadow.

If you want to use Docker, see [Build in a Docker Dev Container](#building-in-a-docker-dev-container)

## Development Build Instructions

### Step 0: Clone Git Submodules

For proper setup of the submodules, a Git recursive clone should be used when cloning the Meadow repository:

```
git clone --recurse-submodules -j8 git@github.com:WildernessLabs/Meadow.git
```

Alternative, if the repository is already cloned:

```
git submodule update --init --recursive
```

### Step 1: Configure Toolchain

You'll need a number of developer tools which we install via Homebrew.
These tools are general developer tools and required libraries (as in the case of libusb).

1. Run `install-packages.sh` script which will install all the needed packages.

### Step 2: Build tools

This step builds the `kconfig` build configuration tool required by NuttX,
as well as our custom version of the ST-Link utility that adds semi-hosting features.

ST-Link is a [JTAG](https://en.wikipedia.org/wiki/JTAG) to USB adapter.
Nearly any [ST-Link V2 adapter](https://www.amazon.com/s/ref=nb_sb_noss_2?url=search-alias%3Daps&field-keywords=stlink+v2) (including cheap clones) will work for this.

Roughly speaking; semi-hosting allows us to connect the host development computer to the Meadow device as if it were part of it. Specifically, we use it right now to connect the file system and execute our Mono/Meadow applications from the `/tmp` directory. We also use it to pipe the `STDIO` (`Console.WriteLine`) out to the host computer over JTAG.

Run the Meadow OS `build-tools.sh` script:

```bash
./build-tools.sh
```

### Step 3. Build the Meadow OS Stack

```bash
./build.sh
```

The script defaults to a quiet mode with little output.

If you want to see compilation progress in real-time, then pass the `--verbose` flag.

The script also checks and skips re-configuration of NuttX and Mono. If you want to force re-configuration, then pass the `--force` flag.

### Step 4: Configure JTAG

  See [Connect your Meadow F7 debug board to the ST-Link V2](http://beta-developer.wildernesslabs.co/guides/Getting_Started/Setup/stlink/index.html).

  Alternatively, check the following instructions:

 1. Wire the following STLink V2 pins to the JTAG connector on the board:

  | Meadow Connector | JTAG Pin Name | Meadow Pin |
|------------------|---------------|------------|
| `SWDIO`          | `TMS_SWIO`    | `7`        |
| `GND`            | `GND`         | `4`        |
| `SWCLK`          | `TMS_SWCLK`   | `9`        |
| `3.3V`           | `3.3`         | `2`        |


2 = 3.3V
4 = GND
7 = IO
9 = CLK

  The JTAG pinout is as follows, where pin one is to the left of the connector cutout, on the nearest row:

  ![](Support_Files/Current_JTAG_Pinout.png)

  The end result should look similar to the following:

  ![](Support_Files/Current_JTAG.jpg)

  Note that your ST-Link adapter pinout may not match the one in the photo.

 2. Plug the ST-Link directly into your computer (or at least a powered USB hub). The ST-Link likely won't work in an unpowered hub. The ST-Link should blink red two or three times and then glow a steady red (maybe).

### Step 5: Test the ST-Link connection via the custom ST-Util

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

### Step 6: Open Meadow in VS Code

There are multiple ways to build and upload the Meadow OS stack. The simplest, though not the most reliable method is to automatically build in VS Code and deploy that way. It uses the ST-Flash utility to deploy code to the flash over JTAG. It's not nearly as reliable as using DFU-Util to burn over USB, but that requires unplugging the device and putting it into DFU bootloader mode.

VS Code can be installed from [here](https://code.visualstudio.com/).

 1. Launch VS Code
 2. Open the `nuttx` folder. **File > Open**, navigate to the folder and click open.
 3. If it prompts you to install the C++ extension, install it and restart VS Code.
 4. Build the project by pressing `Command + Shift + B`. This should build, and deploy over USB by writing to the flash and start the OS.

**Note:** You must close the ST-Link/ST-Util connection in order to be able to deploy from VS Code. If it's still active, simply press `ctrl+c` in the active `ST-Util` session.

**Note:** You might need to add these options to your VS Code config to get it to pick up `.bash_profile`:

```json
    "terminal.integrated.shell.osx": "bash",
    "terminal.integrated.shellArgs.osx": [ "-l" ]
```

#### 6b: Debugging Nuttx

 1. Set a breakpoint somewhere. `__start` method in `stm32_start.c` is a good place to start, but sometimes that breakpoint isn't hit, so something in `arch/arm/src/common/up_initialize.c` might also be good.
 2. Deploy the app via `Command + Shift + B`.
 3. Wait for it to finish flashing.
 4. Switch to terminal and launch the custom ST-Util:

  ```
stlink/build/Release/src/gdbserver/st-util --semihosting -v -m
```
 5. Switch back to VS Code and hit `F5` or **Debug** menu > **Start Debugging**, and it should jump into the breakpoint.

## Launching a Mono Application

The Meadow OS stack is currently configured to use semi-hosting to run a mono app that is hosted in the `/tmp` directory on the host computer. The app must be named `App.exe`, and a copy of the `mscorlib.dll` built from mono master should also be in there.

You can either build mscorlib manually, or use the binary [here](https://drive.google.com/drive/u/0/folders/1qAgv49SRB585jm14eg7LBkMCPg_XJzNC). If you want to build it yourself, see the _Building Mono from Master_
s below.

# Appendix

## Flashing via DFU-Util

ST-Flash (part of STLINK) can be unreliable. DFU-Util is generally more reliable.

However, you must first disconnect the board completely from power (both JTAG and USB), hold down the `boot` button, and then plug in USB power. This will put the board in DFU bootloader mode.

Once it's in bootloader mode, you can flash the NuttX (Meadow) binary via (run this from the `Meadow/nuttx` directory:

```bash
dfu-util -a 0 -D Meadow.OS.bin -s 0x08000000
```

If you have more than one DFU capabable device connected, you can specify the serial number in the dfu-util calls by using the -S argument. To find the serial number use `dfu-util --list`. Replace DEVICE_SERIAL with your serial number in the command below:

```bash
dfu-util -a 0 -S DEVICE_SERIAL -D Meadow.OS.bin -s 0x08000000
```

## Deploy Mono runtime

For B0.4.0 and later, the mono runtime is deployed as a seperate binary and needs to be copied to Meadow after the OS has been updated.

Using a local build of the Meadow CLI command line tool:

 1. Find your device serial `ls /dev/tty.*`
 2. Disable mono (may need to run twice if you get an exception the first time)
  `mono ./Meadow.CLI/Meadow.CLI.exe -s /dev/tty.usbmodem01 --MonoDisable`
 3. Upload new Mono Runtime
  `mono ./Meadow.CLI/Meadow.CLI.exe --WriteFile Meadow.OS.Runtime.bin --KeepAlive`
   After "Download success," hit space again.
 4. Move the runtime into it's special home on the 2MB partition
  `mono ./Meadow.CLI/Meadow.CLI.exe --MonoFlash --KeepAlive`
   After "Mono runtime successfully flashed," hit space to exit.
 5. Reset F7

 Using an installed build of the Meadow CLI command line tool:

 0. Update Meadow CLI `dotnet tool update WildernessLabs.Meadow.CLI --global`
 1. Find your device serial `ls /dev/tty.*`
 2. Disable mono (may need to run twice if you get an exception the first time)
  `meadow /dev/tty.usbmodem01 --MonoDisable`
 3. Upload new Mono Runtime
  `meadow --WriteFile Meadow.OS.Runtime.bin`
   After "Download success," hit space again.
 4. Move the runtime into it's special home on the 2MB partition
  `meadow --MonoFlash`
   After "Mono runtime successfully flashed," hit space to exit.
 5. Reset F7


## Deploy ESP32 binaries

1. Upload the ESP32 bootloader: `meadow --Esp32WriteFile -f bootloader.bin --McuDestAddr 0x1000` (note progress percentage may be incorrect)
2. Upload the ESP32 partition table: `meadow --Esp32WriteFile -f partition-table.bin --McuDestAddr 0x8000`
3. Upload the ESP32 Meadow Comms application: `meadow --Esp32WriteFile -f MeadowComms.bin --McuDestAddr 0x10000`

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

2. Request the ID of your serial port, typically shown as tty.usbserial* or tty.USBtoSerial*:

```bash
ls /dev/tty.usbserial*
ls /dev/tty.USB*
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

### Configure Minicom
Meadow settings: 115200 8N1

```
sudo minicom -s
```

[Click here for more info on Minicom](https://help.ubuntu.com/community/Minicom)

## How to bump a submodule

This information has been moved to the intranet on the page [Build the OS through the Continuous Integration (CI) Server](https://www.wildernesslabs.co/intranet/engineering/BuildOSThroughCI/).

## Flashing OS and Runtime Using STM32CubeProgrammer

STM32CubeProgrammer offers a number of enhanced features for controlling the STM32 microcontrollers.  The `flash.sh` script offers two options for programming the OS and the Runtime system as a single step.  Normally multiple steps are required.

The `--cube` option causes the `flash.sh` script to perform the following steps:

* Disable mono
* Erase the STM32
* Write Meadow.OS.bin to the board
* Perform a hardware reset (twice as I’ve seen some problems with just one reset)
* Copy the runtime system Meadow.OS.Runtime.bin to the board
* Move the runtime into flash

### Prerequisites

* The Meadow F7 board must be connected to the pogo board
* ST-Link must be connected to the pogo board
* STM32CubeProgrammer software is installed on the computer
* The environment variable `MEADOW_CLI_APP` is set and points to the `Meadow.CLI.exe`
* The environment variable `CUBE_APP` is set to the location of the STM32CubeProgrammer CLI application (on a Mac this is `/Applications/STMicroelectronics/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI`)

### Usage

Two command line options have been added to the `flash.sh` script:

* `--cube`
* `--osonly`

The `--cube` options instructs the `flash.sh` script to use the STM32CubeProgrammer to program the STM32.

The `--osonly` option is only applicable it the `--cube` option is used.  This tells the script to flash the OS only and not to process the runtime library.  This omits the last two steps listed above.

## Building In A Docker Dev Container

Instead of building directly on the host machine, you can build in a Docker container. This is useful if you don't have
the necessary tools installed or if you want to ensure a consistent build environment. The easiest way to do this is to
utilize [dev containers](https://containers.dev/). The meadow dev container utilizes the Dockerfile in the `DockerFiles` folder
for the build and runtime environment.

VSCode will automatically detect the dev container and prompt you to open it. To use the dev containers outside of VSCode
you can use the followign commands:

* `npm install -g @devcontainers/cli` - Install the devcontainers CLI application
* `devcontainer up --workspace-folder .` - Start the dev container (assuming running from the Meadow folder)
* `devcontainer exec --workspace-folder . -- ./build.sh ` - Build the Meadow firmware
* `devcontainer exec --workspace-folder . -- ./build.sh --wlclean --force` - Build the Meadow firmware with clean build and force

# Troubleshooting

## Invalid Chip ID

If you get the invalid chip ID when trying to create an STLink session to the device, then the device is likely wired up/connected incorrectly.
