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
#include <stdio.h>

#include <nuttx/board.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/spi/qspi.h>

#include <arch/board/board.h>
#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

#include <nuttx/usb/usbdev.h>
#include <nuttx/usb/usbdev_trace.h>
#include <nuttx/usb/cdcacm.h>

#include "up_arch.h"
#include "stm32f777zit6-meadow.h"
#include "espcp/espcp_coprocessor.h"
#include "stm32_mpuinit.h"
#include "stm32_pwr.h"

#include "espcp/espcp_usrsock.h"

#include "mpu.h"

#ifdef CONFIG_STM32F7_QUADSPI
#  include <nuttx/mtd/mtd.h>
#  include "stm32_qspi.h"
#endif

int meadow_upd_initialize(void);
int hcom_nx_setup_mgr(FAR struct mtd_dev_s *mtd);

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

static int board_init_usbdev(void);

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

void up_netinitialize(void)
{
}

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

#ifdef CONFIG_STM32F7_OTGFS
  stm32_usbinitialize();
#endif
}

// Cannot use 20 MiB if Mono is active, it needs more than the remaining 12 megabytes.
#define MEADOW_RAM_MTD_SIZE (28 * 1024 * 1024) // Must divide by 4096 evenly for SMART FS

#if defined(CONFIG_RAMMTD)
struct mtd_dev_s * board_init_mtd_ram(size_t size)
{
  FAR struct mtd_dev_s *mtd;
  FAR uint8_t *ramstart = (uint8_t *)malloc(size);
  if (ramstart == NULL)
  {
    syslog(LOG_ERR, "ERROR: Could not allocate memory for RAM MTD.");
    return 0;
  }

  mtd = rammtd_initialize(ramstart, size);
  if (mtd == NULL)
  {
    syslog(LOG_ERR, "ERROR: RAM MTD initialization failed\n");
    free(ramstart);
    return 0;
  }

  /* Erase the RAM MTD */
  ret = mtd->ioctl(mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: ioctl mtd MTDIOC_BULKERASE failed\n");
    return 0;
  }

  return mtd;
}
#endif

#if defined(CONFIG_MTD_S25FL)
struct mtd_dev_s * board_init_mtd_s25fl(FAR struct qspi_dev_s *qspi)
{
  FAR struct mtd_dev_s *mtd;
  mtd = s25fl_initialize(qspi, true);
  if (!mtd)
  {
      syslog(LOG_ERR, "ERROR: S25FL Flash initialization failed\n");
      return 0;
  }

  return mtd;
}
#endif

/************************************************************************************
 * Name: board_late_initialize 
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional initialization call
 *   will be performed in the boot-up sequence to a function called
 *   board_initialize().  board_initialize() will be called immediately after
 *   up_initialize() is called and just before the initial application is started.
 *   This additional initialization phase may be used, for example, to initialize
 *   board-specific device drivers.
 *
 ************************************************************************************/

#if defined(CONFIG_STM32F7_QUADSPI)
extern struct qspi_dev_s *g_qspi;
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret;

  syslog(LOG_INFO, "\nMeadow Initialization has begun.\n");

#if defined(CONFIG_STM32F7_PWR)
  // Initialize the backup SRAM and the 32 registers
  stm32_pwr_initbkp(true);    // initialize as writable
#endif

#ifdef CONFIG_PWM
  /* Initialize PWM and register the PWM device. */
  ret = stm32_pwm_setup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: stm32_pwm_setup() failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_CDCACM)
  board_init_usbdev();
#endif

#ifdef CONFIG_BUILD_PROTECTED
 #if defined(CONFIG_ARM_MPU)
  // Map in the entire GPIO register range.
  // Due to MPU alignemnt requirements, size needs to be slightly larger
  // than the GPIO memory region, leaving the CRC, RCC and Flash interface
  // registers open to user code as well.
  size_t size = 1 << mpu_log2regionceil(STM32_GPIOK_BASE - STM32_GPIOA_BASE);
  stm32_mpu_uheap((uintptr_t)STM32_GPIOA_BASE, size);
 #endif
#endif

#ifdef CONFIG_EXAMPLES_MONO
  ret = meadow_upd_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: meadow_upd_initialize() failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_STM32F7_QUADSPI)
    FAR struct qspi_dev_s *qspi;
    qspi = stm32f7_qspi_initialize(0);
    if (!qspi)
    {
      syslog(LOG_ERR, "ERROR: STM32F7 QSPI initialization failed\n");
      return;
    }

    g_qspi = qspi;

  #if defined(CONFIG_ARM_MPU)
    // Allow user-space access to the QSPI flash memory region.
    stm32_mpu_uheap((uintptr_t)STM32_FMC_BANK4, CONFIG_STM32F7_QSPI_FLASH_SIZE);
  #endif

#endif

  FAR struct mtd_dev_s *mtd = 0;
#if defined(CONFIG_RAMMTD)
  mtd = board_init_mtd_ram(MEADOW_RAM_MTD_SIZE);
#elif defined(CONFIG_MTD_S25FL)
  if (mtd == NULL)
    mtd = board_init_mtd_s25fl(qspi);
#endif

#if defined(CONFIG_MTD)
  if (mtd != NULL)
  {
    ret = ftl_initialize(0, mtd);
    if (ret < 0)
    {
      ferr("ERROR: Initialize the FTL layer. returned %d\n", ret);
      return;
    }
  }
#endif

#if defined(CONFIG_MEADOW_HCOM)
  // Initialize Meadow HCOM nuttx
  ret = hcom_nx_setup_mgr(mtd);
  if(ret < 0)
  {
    syslog(LOG_EMERG, "ERROR: HCOM proxy initialization failed!\n");
    PANIC();
  }
#endif

// #if defined(CONFIG_MEADOW_ESPCP_MANAGER)
//   // Setup the ESP32 coprocessor.
//   ret = espcp_init();
//   espcp_enter_run_mode();
//   if(ret != OK)
//   {
//     syslog(LOG_EMERG, "ERROR: ESP32 initialization failed!\n");
//     PANIC();
//   }
//   usrsock_register_sockif(&g_usrsock_sockif_esp32);
// #endif
}

//--------------------------------------------------------------
// Called from above to initialize USB communications
int board_init_usbdev()
{
#if defined(CONFIG_BOARDCTL_USBDEVCTRL)
  FAR void *handle;
  struct boardioc_usbdev_ctrl_s ctrl;

#if defined(CONFIG_CDCACM)
  ctrl.usbdev   = BOARDIOC_USBDEV_CDCACM;
  ctrl.action   = BOARDIOC_USBDEV_CONNECT;
  ctrl.instance = 0;
  ctrl.handle   = &handle;
#else
  ctrl.usbdev   = BOARDIOC_USBDEV_PL2303;
  ctrl.action   = BOARDIOC_USBDEV_CONNECT;
  ctrl.instance = 0;
  ctrl.handle   = &handle;
#endif

  int ret = boardctl(BOARDIOC_USBDEV_CONTROL, (uintptr_t)&ctrl);
  if (ret < 0)
    {
      return ret;
    }
#endif

  return OK;
}
#endif // #ifdef CONFIG_BOARD_LATE_INITIALIZE
