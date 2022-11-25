/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\meadow_hw_version.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

// This module exists to allow the version of Meadow to be determined at runtime.
// This may include a number of different tests.

// Before the Core Compute module (late-fall 2021) the only way to determine the
// version was by finding the type of flash chip on the Meadow. The CCM added 4
// previously unused GPIOs for detecting hardware version.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/meadow_hw_version.h>
#include <nuttx/spi/qspi.h>

#include <arch/stm32f7/chip.h>
#include "stm32_gpio.h"

#ifdef CONFIG_STM32F7_QUADSPI
#  include <nuttx/mtd/mtd.h>
#  include "stm32_qspi.h"
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// Standard qspi flash read id command
#define MEADOW_QSPI_FLASH_READ_ID_COMMAND (0x9f)

// The following define the Manufacture, type and capacity of the flash
// memory chips used on the following models of the Meadow F7 Micro.
// F7v1 uses a spansion flash
// Spansion id 0x01, type 0x60, capacity 19 (256K bits of storage = 32MB)
#define MEADOW_QSPI_FLASH_SPANSION_S25FL256L    (0x00016019)

// F7v2 uses a winbond 512 flash
// Winbond id:0xef, type:0x40 (Q) or 0x70 (M), capacity:20 (512K bits of storage = 64MB)
#define MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxQ  (0x00EF4020) // 'Q' version, default QE = 1
#define MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxM  (0x00EF7020) // 'M' version, default QE = 0

// The first 2 versions of Meadow (F7v1 and F7v2) didn't have dedicated pins used
// for versioning. Newer versions use 4 dedicated version pins. At startup all 4
// version pins are assigned an internal pullup (these have a high resistance).
// All unconnected version pins are therefore pull high. However, on newer
// boards some of the versions pins are tied to GND via an external pulldown
// resistor, which causes these pins to be held low.
#define MEADOW_HARDWARE_VERSION_GPIO_BIT_0_TEST  (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTC | GPIO_PIN15)
#define MEADOW_HARDWARE_VERSION_GPIO_BIT_1_TEST  (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTC | GPIO_PIN14)
#define MEADOW_HARDWARE_VERSION_GPIO_BIT_2_TEST  (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTG | GPIO_PIN3)
#define MEADOW_HARDWARE_VERSION_GPIO_BIT_3_TEST  (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTC | GPIO_PIN13)

/************************************************************************************
 * Private Data
 ************************************************************************************/

static uint32_t _meadowVer;
static bool _meadowVersionKnown = false;

/**
 * @brief Hardware version names.
 */
char *_hardware_version_names[] = 
{
    "Unknown",                  // 0
    "F7FeatherV1",              // 1
    "F7FeatherV2",              // 2
    "F7CoreComputeV2"           // 3
};
//
//  Finally a name for the error condition.
//
#define MEADOW_F7_HW_VERSION_TEXT_NAME_ERROR "Error"

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/
static int meadow_hw_version_read_chip(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                                  FAR void *buffer, size_t buflen);
static uint32_t meadow_hw_version_from_flash_chip(FAR struct qspi_dev_s *qspi);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Just return the already found hardware version information
uint32_t meadow_hw_version_get(void)
{
  if(_meadowVersionKnown)
    return _meadowVer;
  
  return MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
}

//============================================================================
// Returns true if hardware and software support ethernet
bool meadow_hw_version_ethernet_supported(void)
{
  // Note: at the current time (20 Feb 2022) this can only detect if the
  // Core-Compute module is being used, not that it is used within hardware
  // than supports the Ethernet hardware.
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  switch(meadow_hw_version_get())
  {
    case MEADOW_F7_HW_VERSION_NUMB_CCMV2:
      return true;

    default:
      return false;
  }
#else
  return false;
#endif
}

//============================================================================
// Returns true if hardware and software support sd card
bool meadow_hw_verion_sdcard_supported(void)
{
  // Note: at the current time (20 Feb 2022) this can only detect if the
  // Core-Compute module is being used, not that it is used within hardware
  // than supports the SD Card hardware.
#if defined(CONFIG_STM32F7_SDMMC2)
  switch(meadow_hw_version_get())
  {
    case MEADOW_F7_HW_VERSION_NUMB_CCMV2:
      return true;

    default:
      return false;
  }
#else
  return false;
#endif
}

//============================================================================
char *meadow_hw_version_string_return(void)
{
  char *result;
  uint32_t hwVersion = 0;

  if (_meadowVersionKnown)
  {
    hwVersion = meadow_hw_version_get();
  }

  if ((hwVersion >= 0) && (hwVersion <= (sizeof(_hardware_version_names) / sizeof(char *))))
  {
    result = _hardware_version_names[hwVersion];
  }
  else
  {
    result = MEADOW_F7_HW_VERSION_TEXT_NAME_ERROR;
  }

  return(result);
}

//==================================================================
// Based on hardware version, return the flash chip size
uint32_t meadow_hw_version_flash_size(void)
{
  uint32_t qspiFlashSize;
  
  if(!_meadowVersionKnown)
  {
    return MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
  }

  switch(_meadowVer)
  {
    case MEADOW_F7_HW_VERSION_NUMB_F7V1:
    qspiFlashSize = MEADOW_F7_HW_VERSION_F7V1_FLASH_SIZE;
    break;

    case MEADOW_F7_HW_VERSION_NUMB_F7V2:
    qspiFlashSize = MEADOW_F7_HW_VERSION_F7V2_FLASH_SIZE;
    break;

    // CCMV2 used the same flash chip as F7v2
    case MEADOW_F7_HW_VERSION_NUMB_CCMV2:
    qspiFlashSize = MEADOW_F7_HW_VERSION_CCMV2_FLASH_SIZE;
    break;

    default:
    ferr("ERROR: Unknown Meadow version provided:%d\n", _meadowVer);
    qspiFlashSize = MEADOW_F7_HW_VERSION_NUMB_ERROR;
    break;
  }

  return qspiFlashSize;
}

//==================================================================
// Find out this board's version information from the GPIO pins
uint32_t meadow_hw_version_find_gpio_ver()
{
  int ret;
  uint32_t gpioValue = 0;

  // Configure the 4 GPIOs
  ret = stm32_configgpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_0_TEST);
  if(ret >= 0)
  {
    ret = stm32_configgpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_1_TEST);
    if(ret >= 0)
    {
      ret = stm32_configgpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_2_TEST);
      if(ret >= 0)
      {
        ret = stm32_configgpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_3_TEST);
      }
    }
  }

  if(ret < 0)
  {
    // Something went wrong
    return MEADOW_F7_HW_VERSION_NUMB_ERROR;
  }

  // Now read the pins and build the value
  if(stm32_gpioread(MEADOW_HARDWARE_VERSION_GPIO_BIT_0_TEST))
    gpioValue = 0x1;

  if(stm32_gpioread(MEADOW_HARDWARE_VERSION_GPIO_BIT_1_TEST))
    gpioValue |= 0x2;

  if(stm32_gpioread(MEADOW_HARDWARE_VERSION_GPIO_BIT_2_TEST))
    gpioValue |= 0x4;

  if(stm32_gpioread(MEADOW_HARDWARE_VERSION_GPIO_BIT_3_TEST))
    gpioValue |= 0x8;

  // We have what we need. stm32_unconfiggpio sets point to input, float.
  // This way it will waste the minumim power.
  stm32_unconfiggpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_0_TEST);
  stm32_unconfiggpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_1_TEST);
  stm32_unconfiggpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_2_TEST);
  stm32_unconfiggpio(MEADOW_HARDWARE_VERSION_GPIO_BIT_3_TEST);

  return gpioValue;
}

//==================================================================
// This function will first check the dedicated hardware version GPIO pins to
// determine the correct version. Older boards did not have this feature and
// the GPIO pins will all return '1' (i.e. 0x0f) any newer board will report
// a value used to determine the hardware version directly.
uint32_t meadow_hw_version_find_device_ver(FAR struct qspi_dev_s *qspi)
{
  uint32_t gpioValue = meadow_hw_version_find_gpio_ver();

  switch(gpioValue)
  {
    case MEADOW_F7_HW_VERSION_GPIO_ID_CCMV2:
    // Newer Meadow board
    _meadowVersionKnown = true;
    _meadowVer = MEADOW_F7_HW_VERSION_NUMB_CCMV2;
    return _meadowVer;

    // Original F7v1 or F7v2?
    case MEADOW_F7_HW_VERSION_GPIO_ID_F7V1_OR_F7V2:
    if(qspi != NULL)
    {
      // Must dig deeper using Flash Chip. This also sets the qspi value
      // Note: if the version was found using the qspi flash chip method
      // _meadowVersionKnown has already been set true.
      _meadowVer = meadow_hw_version_from_flash_chip(qspi);
    }
    else
    {
      _meadowVer = MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
    }
    break;

    default:
    _meadowVer = MEADOW_F7_HW_VERSION_NUMB_ERROR;
  }
  
  return _meadowVer;
}

//==================================================================
// This function will query the meadow's flash chip and read it's 3 byte ID
// and use it to determine the Meadow's version.
uint32_t meadow_hw_version_from_flash_chip(FAR struct qspi_dev_s *qspi)
{
  int ret;
  uint8_t devInfo[3];

  if(_meadowVersionKnown)
  {
    return _meadowVer;
  }

  // Read the 3 byte chip ID
  ret = meadow_hw_version_read_chip(qspi, MEADOW_QSPI_FLASH_READ_ID_COMMAND,
            devInfo, 3);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Initial QSPI read failed:%d\n", ret);
    return ret;
  }

  // Get the bits we need for the flash chip ID
  uint32_t flashId = (uint32_t) (devInfo[0] << 16 | devInfo[1] << 8 | devInfo[2]);
  
  switch(flashId)
  {
    // Spansion 32MB chip
    case MEADOW_QSPI_FLASH_SPANSION_S25FL256L:
    _meadowVer = MEADOW_F7_HW_VERSION_NUMB_F7V1;
    break;

    // WinBond 64MB chip. The F7v2 board can have either of these a 'Q' or
    // 'M' winbond chip
    case MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxQ:
    case MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxM:
    _meadowVer = MEADOW_F7_HW_VERSION_NUMB_F7V2;
    break;

    default:
    _meadowVer = MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
    break;
  }

  syslog(LOG_INFO, "Meadow hardware version:%d, Flash chip mfg:0x%02x chip type:0x%02x, capacity:0x%02x\n",
        _meadowVer, devInfo[0], devInfo[1], devInfo[2]);

  if(_meadowVer != MEADOW_F7_HW_VERSION_NUMB_UNKNOWN)
    _meadowVersionKnown = true;

  return _meadowVer;
}

//============================================================================
// Read bytes from the QSPI flash
int meadow_hw_version_read_chip(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                                  FAR void *buffer, size_t buflen)
{
  struct qspi_cmdinfo_s cmdinfo;

  // Lock the bus
  (void)QSPI_LOCK(qspi, true);

  QSPI_SETMODE(qspi, QSPIDEV_MODE3);
  QSPI_SETBITS(qspi, 8);

  // Pick a somewhat slow speed for this single read. After the appropriate
  // driver is initialized, the driver will pick the best speed for the chip
  (void)QSPI_SETFREQUENCY(qspi, 48000000);

  cmdinfo.flags   = QSPICMD_READDATA;
  cmdinfo.addrlen = 0;
  cmdinfo.cmd     = cmd;
  cmdinfo.buflen  = buflen;
  cmdinfo.addr    = 0;
  cmdinfo.buffer  = buffer;

  // Send the read command
  int ret = QSPI_COMMAND(qspi, &cmdinfo);
  
  // Unlock
  (void)QSPI_LOCK(qspi, false);

  return ret;
}
