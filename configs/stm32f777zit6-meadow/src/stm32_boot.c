/************************************************************************************
 * configs/stm32f777zit6-meadow/src/stm32_boot.c
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
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
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>

#include <nuttx/board.h>
#include <arch/board/board.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/spi/qspi.h>

#include "up_arch.h"
#include "stm32f777zit6-meadow.h"

#ifdef CONFIG_STM32F7_QUADSPI
#  include <nuttx/mtd/mtd.h>
#  include "stm32_qspi.h"

#  ifdef CONFIG_FS_NXFFS
#    include <sys/mount.h>
#    include <nuttx/fs/nxffs.h>
#  endif
//MEADOW FIXME: header clash?
extern FAR struct qspi_dev_s *stm32f7_qspi_initialize(int intf);
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the initialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

void stm32_boardinitialize(void)
{
#if defined(CONFIG_STM32F7_SPI1) || defined(CONFIG_STM32F7_SPI2) || \
    defined(CONFIG_STM32F7_SPI3) || defined(CONFIG_STM32F7_SPI4) || \
    defined(CONFIG_STM32F7_SPI5)
  /* Configure SPI chip selects if 1) SPI is not disabled, and 2) the weak function
   * stm32_spidev_initialize() has been brought into the link.
   */

  if (stm32_spidev_initialize)
    {
      stm32_spidev_initialize();
    }
#endif

#ifdef CONFIG_SPORADIC_INSTRUMENTATION
  /* This configuration has been used for evaluating the NuttX sporadic scheduler.
   * The following caqll initializes the sporadic scheduler monitor.
   */

  arch_sporadic_initialize();
#endif

#ifdef CONFIG_ARCH_LEDS
  /* Configure on-board LEDs if LED support has been selected. */

  board_autoled_initialize();
#endif

#ifdef CONFIG_STM32F7_FMC
  stm32_enablefmc();
#endif
}

/************************************************************************************
 * Name: board_initialize
 *
 * Description:
 *   If CONFIG_BOARD_INITIALIZE is selected, then an additional initialization call
 *   will be performed in the boot-up sequence to a function called
 *   board_initialize().  board_initialize() will be called immediately after
 *   up_initialize() is called and just before the initial application is started.
 *   This additional initialization phase may be used, for example, to initialize
 *   board-specific device drivers.
 *
 ************************************************************************************/

#ifdef CONFIG_BOARD_INITIALIZE
void board_initialize(void)
{
#ifdef CONFIG_STM32F7_QUADSPI
  FAR struct qspi_dev_s *qspi;
  FAR struct mtd_dev_s *mtd;
#endif

#ifdef CONFIG_STM32F7_QUADSPI
  {

    //struct qspi_meminfo_s meminfo;

    int ret;

    qspi = stm32f7_qspi_initialize(0);
    if (!qspi)
      {
        syslog(LOG_ERR, "ERROR: sam_qspi_initialize failed\n");
        return;
      }
    
    mtd = s25fl5_initialize(qspi, true);
    if (!mtd)
      {
        syslog(LOG_ERR, "ERROR: s25fl5_initialize failed\n");
        return;
      }
   
    ret = nxffs_initialize(mtd);
    if (ret < 0)
      {
        ferr("ERROR: NXFFS initialization failed: %d\n", -ret);
        return;
      }
    
    ret = mount(NULL, "/mnt/meadow0", "nxffs", 0, NULL);
    if (ret < 0)
      {
        ferr("ERROR: Failed to mount the NXFFS volume: %d\n", errno);
        return;
      }
/*
      meminfo.flags = QSPIMEM_READ | QSPIMEM_QUADIO;
      meminfo.addrlen = 3;
      meminfo.dummies = 6;
      meminfo.cmd = 0xeb; // S25FL1_FAST_READ_QUADIO;
      meminfo.addr = 0;
      meminfo.buflen = 0;
      meminfo.buffer = NULL;

      stm32f7_qspi_enter_memorymapped(qspi, &meminfo, 80000000);
      stm32_mpu_uheap((uintptr_t)0x90000000, 0x4000000);
      */
  }
#endif
}
#endif
