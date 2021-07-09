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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/meadow_hw_version.h>
#include <nuttx/spi/qspi.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// Standard qspi flash read id command
#define MEADOW_QSPI_FLASH_READ_ID_COMMAND (0x9f)

// The following define the Manufacture, type and capacity of the flash
// memory chips used on the following models of the Meadow F7 Micro.
// F7v1 uses a spansion flash
// Spansion id 0x01, type 0x60, capacity 19 (256 bytes)
#define MEADOW_QSPI_FLASH_SPANSION_S25FL256L    (0x00016019)

// F7v2 uses a winbond 512 flash
// Winbond id 0xef, type 0x40 (Q) or 0x70 (M), capacity 20 (512 bytes)
#define MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxQ  (0x00EF4020) // 'Q' version, default QE = 1
#define MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxM  (0x00EF7020) // 'M' version, default QE = 0

/************************************************************************************
 * Private Data
 ************************************************************************************/

static uint32_t _meadowVer;
static bool _meadowVersionKnown = false;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_read_qspi_hw_version(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                                  FAR void *buffer, size_t buflen);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Just return the already found hardware version information
uint32_t meadow_hw_version_return(void)
{
  if(_meadowVersionKnown)
    return _meadowVer;
  
  return MEADOW_MICRO_VERSION_UNKNOWN;
}

//==================================================================
// This function will query the meadow's flash chip and read it's 3 byte ID
// and return the Meadow version
uint32_t meadow_hw_version_determine(FAR struct qspi_dev_s *qspi)
{
  int ret;
  uint32_t flashId;
  uint8_t devInfo[3];

  // We only want to do use the QSPI interface at startup. During
  // initialization phase the QSPI communications parameters are
  // changed to more specific values that will only work for the
  // board specific flash chip. For this phase the parameters are
  // general and generic.
  if(_meadowVersionKnown)
    return _meadowVer;
    
  ret = meadow_read_qspi_hw_version(qspi, MEADOW_QSPI_FLASH_READ_ID_COMMAND, devInfo, 3);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Initial QSPI read failed:%d\n", ret);
    return ret;
  }

  flashId = (uint32_t) (devInfo[0] << 16 | devInfo[1] << 8 | devInfo[2]);

  switch(flashId)
  {
    case MEADOW_QSPI_FLASH_SPANSION_S25FL256L:
    _meadowVer = MEADOW_MICRO_VERSION_F7v1;
    break;

    case MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxQ:
    case MEADOW_QSPI_FLASH_WINBOND_W25Q512JVxxM:
    _meadowVer = MEADOW_MICRO_VERSION_F7v2;
    break;

    default:
    _meadowVer = MEADOW_MICRO_VERSION_UNKNOWN;
  }

  syslog(LOG_INFO, "Meadow hardware version:%d, Flash chip mfg:0x%02x chip type:0x%02x, capacity:0x%02x\n",
        _meadowVer, devInfo[0], devInfo[1], devInfo[2]);

  _meadowVersionKnown = true;
  return _meadowVer;
}

//============================================================================
// Read bytes from the QSPI flash
int meadow_read_qspi_hw_version(FAR struct qspi_dev_s *qspi, uint8_t cmd,
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

//============================================================================
char *meadow_hw_version_string_return(void)
{
  uint32_t hwVersion = meadow_hw_version_return();

  static char *verName[] = 
  {
    MEADOW_MICRO_VERSION_NAME_UNKNOWN,
    MEADOW_MICRO_VERSION_NAME_F7v1,
    MEADOW_MICRO_VERSION_NAME_F7v2
  };

  return (hwVersion < 1 || hwVersion > 2) ? verName[0] : verName[hwVersion];
}
