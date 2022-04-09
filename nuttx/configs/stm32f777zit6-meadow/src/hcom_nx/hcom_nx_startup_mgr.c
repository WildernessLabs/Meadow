/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_startup_mgr.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

// This module is called during nuttx startup

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "hcom_nx_common.h"
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/meadow_ethnet_common.h>
#include "../espcp/espcp_coprocessor.h"
#include <assert.h>
#include "hcom_nx_config_manager.h"

#include "stm32f777zit6-meadow.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Note: call added to stm32_boot.c
int hcom_nx_setup_mgr(FAR struct mtd_dev_s *mtd)
{
  int ret;

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 1a\n"); usleep(5 * 1000);
#endif

  if (mtd == NULL)
  {
    return ERROR;
  }

// Initialize the file system first so config file can be read by others
#if defined(CONFIG_HCOM_FILESYSTEM_INIT)    // defined in menuconfig
  ret = hcom_nx_create_fs_initialize(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup F/S helper %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 1b\n"); usleep(5 * 1000);
#endif

  //
  //  Initialise the configuration system.
  //
  hcom_nx_config_init();
  hcom_nx_config_lock();
  meadow_configuration_t *config = hcom_nx_config_get_pointer();
  if (config == NULL)
  {
    hcom_nx_config_unlock();
    //
    //  This means that there is not enough memory for a configuration object
    //  as an object containing default values is created if the config file
    //  cannot be found or it is empty.
    //
    syslog(LOG_EMERG, "%s@%d-Cannot obtain configuration.\n", thisFile, __LINE__);
    return ERROR;
  }
  bool reset_esp32 = config->reset_esp32_at_startup;
  
  // Start trace messaging if so configured
  hcom_nx_trace_insure_correct_config((config->use_uart1_for_trace ? true : false), false);
  hcom_nx_config_unlock();

  if (reset_esp32)
  {
    ret = espcp_init();
    if (ret != OK)
    {
      syslog(LOG_EMERG, "ERROR: ESP32 initialization failed:%d\n", ret);
      return ret;
    }

    ret = espcp_enter_run_mode();
    if (ret != OK)
    {
      syslog(LOG_EMERG, "ERROR: ESP32 enter run mode failed:%d\n", ret);
      return ret;
    }
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 1c\n"); usleep(5 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 2a\n"); usleep(5 * 1000);
#endif

  ret = hcom_nx_route_text_to_host_setup();
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to initialize host routing:%d\n", ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 2b\n"); usleep(5 * 1000);
#endif

  ret = hcom_nx_utils_startup_handling_of_trace_level();
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to initialize syslog level:%d\n", ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 3a\n"); usleep(5 * 1000);
#endif

#if (defined (CONFIG_FS_PROCFS) && defined (CONFIG_SYSTEM_NSH))
  ret = mount(NULL, STM32_PROCFS_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to mount procfs at %s: %d\n",
            STM32_PROCFS_MOUNTPOINT, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 3b\n"); usleep(5 * 1000);
#endif

#if defined (CONFIG_MEADOW_TIMER_SUPPORT)
  // Initialize meadow timer code
  ret = meadow_timer_support_setup();
  if (ret != OK)
  {
    syslog(LOG_ERR,"ERROR: Failed to initialize meadow timer: %d\n", ret);
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 3c\n"); usleep(5 * 1000);
#endif

#if defined (CONFIG_STM32F7_SDMMC2)
  // Initialize the SDIO block driver
  ret = stm32_sdio_initialize_meadow();
  if (ret != OK)
  {
    syslog(LOG_ERR,"ERROR: Failed to initialize MMC/SD driver:%d\n", ret);
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 4a\n"); usleep(5 * 1000);
#endif

  // Initialize hcom nuttx driver
  ret = hcom_nx_upd_initialize();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom nuttx upd:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 4b\n"); usleep(5 * 1000);
#endif

  // Initialize the Real-Time meadow support
  ret = meadow_rtc_hardware_initialize();
  if (ret != OK)
  {
    syslog(LOG_ERR,"ERROR: Failed to initialize rtc hardware:%d\n", ret);
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 5\n"); usleep(5 * 1000);
#endif

  // Saves a copy of mtd
  ret = hcom_nx_exec_ex_flash_setup(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 6\n"); usleep(5 * 1000);
#endif

#if HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD > 0
  ret = hcom_nx_exec_test_qspi_flash_setup(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup for testing qspi flash %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_INCLUDE_SD_CARD_TESTS_IN_BUILD > 0
  ret = hcom_nx_exec_test_sdcard_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup for testing sdcard %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif
  
#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 7-Successful exit\n"); usleep(5 * 1000);
#endif

// Eventually controlled by configuration option
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  ret = meadow_eth_mgr_startup();
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to initialize ethernet:%d\n", ret);
    return ret;
  }
#endif

  return OK;
}
