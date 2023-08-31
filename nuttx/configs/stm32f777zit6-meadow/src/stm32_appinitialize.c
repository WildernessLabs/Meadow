/****************************************************************************
 * config/stm32f777zit6-meadow/src/stm32_appinitialize.c
 *
 *   Copyright (C) 2015-2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/spi/qspi.h>
#include <syslog.h>

#include <arch/board/boardctl.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <errno.h>

#include "stm32_qspi.h"
#include "stm32f777zit6-meadow.h"

#include<meadow/meadow_hw_version.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// The following are duplicates of what's available within each driver.
// They are here because previously they were hardcoded values here
// configs\stm32f777zit6-meadow\src\stm32_appinitialize.c and this is
// more obvious.
// If the driver is modified then the following must change too
#if defined(CONFIG_MTD_S25FL)
  // keep in sync with s25fl.c driver
  #define MTD_S25FL_FLASH_QSPI_ADDRLEN       (4)
  #define MTD_S25FL_FLASH_READ_QUADIO        (0xeb)
  #define MTD_S25FL_FLASH_NUMBER_DUMMIES     (10)
#endif
#if defined(CONFIG_MTD_W25QXXXJV)
  // Keep in sync with w25qxxxjv.c driver
  #define MTD_W25QJV_FLASH_QSPI_ADDRLEN       (4)   // W25QxxxJV, xxx < 256 ADDRLEN=3, for xxx >= 256 ADDRLEN=4
  #define MTD_W25QJV_FLASH_READ_QUADIO        (0xeb)
  #define MTD_W25QJV_FLASH_NUMBER_DUMMIES     (6)   // This needs to match the W25QxxxJV driver
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX; the meaning of the argument is a contract
 *         between the board-specific initalization logic and the
 *         matching application logic.  The value cold be such things as a
 *         mode enumeration value, a set of DIP switch switch settings, a
 *         pointer to configuration data read from a file or serial FLASH,
 *         or whatever you would like to do with it.  Every implementation
 *         should accept zero/NULL as a default configuration.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
#ifdef CONFIG_FS_PROCFS

#ifdef CONFIG_STM32_CCM_PROCFS
  /* Register the CCM procfs entry.  This must be done before the procfs is
   * mounted.
   */

  (void)ccm_procfs_register();
#endif

  /* Mount the procfs file system */

  int ret = mount(NULL, STM32_PROCFS_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs at %s: %d\n",
             STM32_PROCFS_MOUNTPOINT, ret);
    }
#endif

  return OK;
}

#ifdef CONFIG_BOARDCTL_IOCTL
struct qspi_dev_s *g_qspi;

int board_ioctl(unsigned int cmd, uintptr_t arg)
{
  switch (cmd)
    {
      case BIOC_ENTER_MEMMAP:
        {
          struct qspi_meminfo_s meminfo;

          /* Set up the meminfo like a regular memory transaction, many of
           * the fields are not used, the others are to set up for the
           * 'read' command that will automatically be issued by the
           * controller as needed.
           */
          meminfo.flags   = QSPIMEM_READ | QSPIMEM_QUADIO;
          meminfo.addr    = 0;
          meminfo.buflen  = 0;
          meminfo.buffer  = NULL;

          // Which version is on this board?
          uint32_t meadow_hw_ver = meadow_hw_version_get();
          
          // The following defines are in include\meadow\meadow_hw_version.h
          switch(meadow_hw_ver)
          {
#if defined(CONFIG_MTD_S25FL)
            case MEADOW_F7_HW_VERSION_NUMB_F7V1:
              meminfo.addrlen = MTD_S25FL_FLASH_QSPI_ADDRLEN;
              meminfo.cmd     = MTD_S25FL_FLASH_READ_QUADIO;
              meminfo.dummies = MTD_S25FL_FLASH_NUMBER_DUMMIES;
              break;
#endif

#if defined(CONFIG_MTD_W25QXXXJV)
            case MEADOW_F7_HW_VERSION_NUMB_F7V2:
            case MEADOW_F7_HW_VERSION_NUMB_CCMV2:
              meminfo.addrlen = MTD_W25QJV_FLASH_QSPI_ADDRLEN;
              meminfo.cmd     = MTD_W25QJV_FLASH_READ_QUADIO;
              meminfo.dummies = MTD_W25QJV_FLASH_NUMBER_DUMMIES;
              break;
#endif
            default:
              return -ENODEV;
          }

          // Nuttx STM32F7 specific function that puts the QSPI into memory
          // mapped mode. The last parameter LPTO is related to QSPI Low Power
          // Timeout. It's a 16-bit number that determines how many clock
          // cycles to wait before entering into low-power mode, if in
          // memory-mapped mode. Probably won't save any current, but easy.
          stm32f7_qspi_enter_memorymapped(g_qspi, &meminfo, 0x0100 /*LPTO=256*/);
        }
        break;

      case BIOC_EXIT_MEMMAP:
        stm32f7_qspi_exit_memorymapped(g_qspi);
        break;

      default:
        return -EINVAL;
    }

    return OK;
}
#endif