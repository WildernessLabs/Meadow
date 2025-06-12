Meadow Updater
==

The Updater is a set of components + an API of Meadow OS, providing the core capability of partially or wholly replacing both the operating system and the installed Meadow App, usually to update or upgrade it to the latest and best version. The updater can, in a single reboot cycle, update the Meadow operating system (both "Core" and "Runtime" parts) and/or the currently installed Meadow App, and proceed in normally running the updated software. It also supports roll-back of failed updates, and aims to always end with a functioning system even while performing many risky operations.

## Principles ##

* Reliability: The update process must always receive and honor a request for the device to be updated, even when offline.
* Security: The update must arrive on device 
* Atomicity: The update must always be fully applied or not applied at all (atomicity)
* The update process must be resistant to interruption (power loss, network intermittence, etc.)
* Availability: The device must be updateable after an update

## Technical/Implementation Details ##

### Meadow Bootloader:
Being the first piece of code that runs on the Meadow, the bootloader usually just boots Meadow's NuttX-based kernel. Before it does that, it loads the *Updater state data partition*, which tell it  e.g. whether or not we have a staged OS update to install, or if a previous update failed and therefore the bootloader should roll the OS back to the previous version. The bootloader then acts as a simple state machine that returns the system to its stable state: the state where no update or rollback is due, and the installed software just gets to run.

 - The bootloader source code wholly resides in `./bootloader`
 - It reserves 256KB from the Meadow OS's core 2MB binary
 - It cannot update itself; the bootloader can only be updated via DFU-Util or JTAG.

### Updater state data partition: 

A partition of Meadow's external flash where the OS and the Meadow Bootloader exchange information. For example, the OS can prepare an replacement OS binary in the *OS Updater stage partition* partition, then "flag" the Meadow Bootloader about this update, then restart. Inversely, the bootloader can let the OS know it's an updated OS, at which point the OS lets the bootloader know that it has successfully booted itself up (if it hadn't, the bootloader will go ahead roll-back the OS).

 - Its size and location are described at `nuttx/include/meadow/hcom_shared_common.h` as the part of `HCOM_NX_FS_OTA_RESERVED_SPACE` past the `HCOM_NX_FS_NUTTX_UPDATE_SIZE` (currently 256KB)
 - Only a few bytes of this space are currently used as they are enough to convey messages and state between the OS and the bootloader


### OS Updater stage partition:

A partition of Meadow's external flash where a staged OS update may reside. Never is the OS firmware directly written over by anything other than the bootloader, and never does the bootloader use anything other than the contents of this partition to update to OS.

- Its size and location are described at `nuttx/include/meadow/hcom_shared_common.h` as `HCOM_NX_FS_NUTTX_UPDATE_SIZE` and `HCOM_NX_FS_OTA_RESERVED_SPACE` respectively

## Design ##

### "Tier-0" Updater

This is the last piece of code that runs on the Meadow before it is actively running its Meadow App, the bookend to the *Meadow bootloader*. It orchestrates OS, App and OS+App updates after being given the "raw material" (files) of an update in its assigned file system path.

The code checks for OS update files: then performs part 1 of the OS update (`Meadow.OS.bin`) with the help of the *bootloader*, and part 2 of the OS update (`Meadow.OS.Runtime.bin`) with the help of a kernel call that writes to external flash.

It also checks for app update files : every app update file will be copied over the existing Meadow App code directory (currently `/meadow0/`), replacing any existing files in there. Existing files are not deleted, allowing for differential (delta) updates.

- The assigned file system path for updates is hardcoded: `/meadow0/update/`.
- The subdirectory for OS updates is `/meadow0/update/os`. If this directory contains both `Meadow.OS.bin` and `Meadow.OS.Runtime.bin`, the update is applied.
- The subdirectory for Meadow App updates is `/meadow0/update/app`. If this directory exists, an app update using the directory's contents is applied.
- The subdirectory for ESP32 firmware update is `/meadow0/update/firmware`. If this directory contains all 3 ESP firmware .bins, the update is applied

- This code mostly resides in `apps/examples/mono/mono_main.c` (`app_update()/os_update()/firmware_update()`). It really should be moved to a separate `update.c` 


### "Tier-1" Downloader

The downloader 

### "Tier-1" Updater

 Updater API

This API's purpose is to allow Meadow.Core (the managed part of Meadow OS) to take an archive (ZIP file) containing an update, and deposit the files in the right place for the *"Tier-0" Updater API* to apply it. It is only to be used by an *Update Supervisor*

- Code is in MeadowCloudUpdateService.cs in Meadow.Core
- A well-formed ZIP file containing both an OS and App update would have the following directory structure (mirroring the "Tier-0" structure described above):
```
  update.zip/
     os/
        Meadow.OS.bin
        Meadow.OS.Runtime.bin
     app/
        App.dll
        mscorlib.dll
        Meadow.dll
        System.dll
 ```
- A ZIP file containing just one or any combination of the `os/` , `app/`, or `firmware/`  directories is also a valid update file.

