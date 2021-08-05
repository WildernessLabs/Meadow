# ESP Coprocessor

This directory contains the code supporting:

* Initialisation of the ESP32
* Communication between the STM32 and the ESP32
* Implement the POSIX methods used for networking
* Miscellaneous WiFi functions (scan for access pojnts, start network etc.)
* Bluetooth support
* Mesh networking support

## Overview

The STM32 and the ESP32 act as a controller and peripheral pair of devices.  The STM32 is the controller with the ESP32 acting as a peripheral.  As such, the ESP32 cannot initiate communication with the STM32.  It must be started and operated under the direction of the STM32 code.

In addition to providing support services for the STM32 it is also possible for the STM32 to upload new firmware to the STM32.  This is made possible through the serial programming interface available on all ESP32 microcontrollers.

### Network and Support Services

Communication between the two devices is made possible through one of the SPI interfaces and a pair of GPIO lines for signalling.

The two GPIO lines indicate the following situations:

* ESP32 SPI interface ready to accept data packets
* ESP32 has a message waiting for transmission to the STM32

At startup, the ESP32 will set the Message Waiting line and the SPI Ready lines low.  In this state the ESP32 cannot accept any data over the SPI interface.

Soon after startup the SPI interface on the ESP32 will enter a ready state and will be capable of accepting data packets from the STM32.  When this occurs, the SPI Ready line will go high.

In this state the STM32 is free to send messages to the ESP32.

### Firmware Upload

The firmware upload process uses the serial upload process as documented on the [Espressif web site](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-flash).  The process is as follows (all lines refer to the ESP32):

* Pull the Boot line low
* Pull the Reset line low and hold for approximately 20 ms
* Pull the Reset line high and wait for about 20 ms
* Pull the Boot line high

At this point the ESP will enter programming mode and can be programmed over the [serial interface](https://github.com/espressif/esptool/wiki/Serial-Protocol).

## Hardware Connections

The connections used for both processes, Programming and Communication, share some common lines, namely, the ESP32 Boot pin and the ESP32 UART Rx pin.  This makes it essential to ensure that the interface lines are configured correctly for the current mode.  The shared lines are therefore reconfigured upon each mode change.

While some pins are shared, some are dedicated, namely the SPI interface lines and the ESP32 Reset line.  These can be configured once and they can remain configured until the chip is reset.

## Software Support

The ESP Coprocessor software provides a number of methods to support the two different modes along with a third mode, holding the ESP32 in reset (effectively turning it off).

### Firmware Upload

The firmware upload process can be entered by calling the `espcp_enter_programming_mode` method.

### Reset and Hold

Pulling the Reset line low and holding it low will effectively turn off the ESP32.  This can be achieved by calling the `espcp_hold_in_reset` method.

### Execute Application

Executing the application on the ESP32 is a two stage process.  Firstly, the NuttX tasks and message queues must be setup and the various systems on the STM32 need to be started.  Next, the system must configure the shared pins for communication between the two chips and then reset the ESP32.  This is achived by calling the following methods in sequence:

* `espcp_init`
* `espcp_enter_run_mode`

The `espcp_init` method starts the task on the STM32 that will process the messages.  The start up process (in the task) will halt waiting for the SPI interface to be initialised.

`espcp_enter_run_mode` will reset the ESP32 and allow the ESP32 to start the firmware programmed into the chip.
