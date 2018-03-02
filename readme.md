# Meadow OS

## Build Instructions

### Step 1: Repo Structure

 1. Create a folder called `Meadow`
 2. In the `Meadow` folder, clone the _meadow_ repo into a folder called `Nuttx`:
```
git clone git@github.com:WildernessLabs/Meadow.git Nuttx
```
 3. Switch to the `wip` branch:
```
git checkout wip
```
 4. In the `Meadow` folder, clone the Nuttx `tools` repo:
```
git clone https://bitbucket.org/nuttx/tools.git
```
 5. In the `Meadow` folder, clone the Nuttx `apps` repo:
```
git clone https://bitbucket.org/nuttx/apps.git
```

Your Meadow folder should then look like:

```
 - Meadow
   |- apps
   |- Nuttx
   |- tools
```

### Step 2: Configure Toolchain

 1. Install ARM GCC:
```
brew install osx-cross/arm/arm-gcc-bin
```
 2. Install the STLink utilities:
```
brew install stlink
```

### Step 3: Configure the Build

4. Configure the `stm32f777zit6-meadow` build flavor:
```
./tools/configure.sh stm32f777zit6-meadow/nsh
```
5. Make the project:
```
make
```
 
 
### Configure JTAG

Wire the following STLink V2 pins to the JTAG connector on the board:

```
VCC -> JTAG 10 (NRST)
SWCLOCK -> JTAG6 (JTCK)
SWDIO  -> JTAG4 (JTMS)
```

The JTAG pinout is as follows, pin 1 is top left, pin 2 is top right:

![](Support_Files/JTAG.png)

The end result should look similar to the following:

![](Support_Files/JTAG_Photo.jpg)

