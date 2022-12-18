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

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
#include <meadow/meadow_hw_version.h>
#include "stm32_ethernet.h"
#endif

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
  meadow_configuration_t *config;

  // One GPIO (PB4) is used D05 for F7FeatherV2 and CCM. But, at reset it
  // isn't initialized all the other GPIOs. It's one of the debugging 5 pins.
  // and therefore is configured as pull-up/pull-down at F7 restart. Howerver,
  // this pin isn't needed for our ST-Link debugging so it's free to use. But,
  // being configured diffrently is seen as not ideal. The following is used
  // to reconfigure it like the other GPIOs.
  stm32_configgpio(MEADOW_DEBUG_NJTRST_NOT_USED_GPIO);

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
  //  Saves a copy of mtd
  //
  //  This needs to be done before the config is set up as the block driver
  //  is accessed to copy Mono from the flash device to RAM.
  //
  ret = hcom_nx_exec_ex_flash_setup(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  //
  //  Initialise the configuration system.
  //
  hcom_nx_config_init();
  hcom_nx_config_lock();
  config = hcom_nx_config_get_pointer();
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

  //
  //  Set the system clock to the OS build time to help with SSL.
  //
  hcom_nx_config_set_time_to_os_build_time();
  
  // syslog(2, "YAML Config:Net I/F:%s, DHCP:%s\n",
  //           config->default_interface->interface_name,
  //           config->default_interface->use_dhcp == 1 ? "Yes" : "No");

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
  syslog(2,  "hcom_nx_setup_mgr 4\n"); usleep(5 * 1000);
#endif

  // Initialize hcom nuttx driver
  ret = hcom_nx_upd_initialize();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom nuttx upd:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 5\n"); usleep(5 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 6a\n"); usleep(5 * 1000);
#endif

#if HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD > 0
  ret = hcom_nx_exec_test_qspi_flash_setup(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup for testing qspi flash %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 6b\n"); usleep(5 * 1000);
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
  syslog(2,  "hcom_nx_setup_mgr 6c\n"); usleep(5 * 1000);
#endif

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
  // Initialize the power management code
  ret = meadow_power_mgmt_initialize();
  if (ret != OK)
  {
    syslog(LOG_ERR,"ERROR: Failed to initialize power mgmt:%d\n", ret);
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 7\n"); usleep(5 * 1000);
#endif

#if defined (CONFIG_STM32F7_SDMMC2)
  hcom_nx_config_lock();
  config = hcom_nx_config_get_pointer();

  if (config->sd_storage_supported)
  {
    ret = stm32_sdio_initialize_meadow();
    if (ret != OK)
    {
      config->sd_storage_supported = 0;
      syslog(LOG_ERR,"ERROR: Failed to initialize MMC/SD driver:%d\n", ret);
    }
  }
  hcom_nx_config_unlock();
#endif

  // Initialize sending HCOM messages to CLI from Nuttx side
  ret = hcom_nx_host_send_setup();
  if (ret != OK)
  {
    syslog(LOG_ERR,"ERROR: Failed to initialize host send:%d\n", ret);
    return ret;
  }

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD) && \
    defined(CONFIG_NETDEV_LATEINIT)
  if(meadow_hw_version_ethernet_supported())
  {
    hcom_nx_config_lock();
    config = hcom_nx_config_get_pointer();
    if(config->default_interface->interface_type == MEADOW_IFT_ETHERNET)
    {
      hcom_nx_config_unlock();
      // This call does the hardware initialization for the F7. This can only
      // be called if CONFIG_NETDEV_LATEINIT is defined. Otherwise,
      // stm32_ethinitialize is called very early in the nuttx startup code
      // in up_initialize.c's up_initialize() function (look for
      // CONFIG_NETDEV_LATEINIT).
      syslog(LOG_INFO, "Ethernet is being initialized\n");
      (void)stm32_ethinitialize(0);

      ret = meadow_eth_mgr_startup();
      if (ret < 0)
      {
        syslog(LOG_ERR, "ERROR: Failed to initialize ethernet:%d\n", ret);
        return ret;
      }
    }
    else
    {
      hcom_nx_config_unlock();
      syslog(LOG_INFO, "CCM device with Ethernet is not enabled\n");
    }
  }
  else
  {
    syslog(LOG_INFO, "Ethernet not supported by this device\n");
  }

#endif    // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD) && defined(CONFIG_NETDEV_LATEINIT)

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2,  "hcom_nx_setup_mgr 8-Successful exit\n"); usleep(5 * 1000);
#endif

  return OK;
}
