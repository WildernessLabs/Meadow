20 February 2022
'defconfig.ccmEthSd' is a defconfig that contains Ethernet and SD Card support
                     for the CCM breakout board v2b. This file is essentially the
                     the current defconfig file with 'addconfig.ethernet' and
                     'addconfig.sdcard' aready added. When used to replace the
                     defconfig file it creates a build contining Ethernet and SD
                     card support.
'addconfig.ethernet' contains defconfig items that can be added to the standard
                     defconfig, thus added an Ethernet configuation that works on
                     the CCM breakout board v2b.
'addconfig.sdcard'   contains defconfig items that can be added to the standard
                     defconfig, thus added a SD Card configuation that works on
                     the CCM breakout board v2b.
'addconfig.nsh6'    contains defconfig items that can be added to the standard
                     defconfig, thus added a NuttShell configuation that is tied
                     to UART6 and works on F7v1, F7v2 and CCM breakout board v2b.