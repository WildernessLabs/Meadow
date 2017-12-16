1. Install the ARM GCC Compiler:
```
brew install osx-cross/arm/arm-gcc-bin`
```
2. Install STLink:
```
brew install stlink
```
3. Make a `Meadow` directory in your git repos folder:
```
mkdir meadow
```
4. Check out the meadow repo into a nuttx folder and switch to wip branch:
```
git clone git@github.com:WildernessLabs/Meadow.git Nuttx
```
5. Check out apps, side by side:
```
git clone https://bitbucket.org/nuttx/apps.git
```

6. Check out the Nuttx Tools:
 `git clone https://bitbucket.org/nuttx/tools.git`

7. build and install `kconfig`:
 `cd tools/kconfig-frontends`
 `./configure --enable-mconf --disable-nconf --disable-gconf --disable-qconf`
 `make`
 `make install`

8. change back to Nuttx directory and configure the meadow flavor nuttx:
 `cd ../../Nuttx`
 `./tools/configure.sh stm32f777zit6-meadow/nsh`
9. Build:
 `make`

Ways to flash:

1. Use vscode, the build task there will flash (using st-flash)
2. st-flash write nuttx.bin 0x08000000

To setup gdb:

1. st-util

To debug:

Open nuttx checkout in vscode, and run debug task.  It should do everything for you if st-util is running.
