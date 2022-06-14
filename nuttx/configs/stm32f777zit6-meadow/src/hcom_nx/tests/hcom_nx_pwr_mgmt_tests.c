/****************************************************************************
 * configs\stm32f777zit6-meadow\src\hcom_nx\tests\hcom_nx_pwr_mgmt_tests.c
 * 
 *   Copyright (C) 2019 - 2021 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#include <meadow/hcom_shared_common.h>

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

// #include <meadow/hcom_upd_shared.h>
#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/stat.h>
#include "stm32_rtc.h"

#include "../../pwrmgmt/pwrmgmt_local.h"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/
static char *thisFile = __FILE__;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_pwr_mgmt_full_wakeup_test(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/tests/hcom_nx_developer_3_tests.c
// These tests are for testing the power management implementation
int hcom_nx_exec_power_mgmt_tests(struct hcom_nx_cmd_data *cmdData)
{
  int ret = OK;
  uint32_t userData = cmdData->userData;

  switch(userData)
  {
    case 50:
      // Turn-off RGB leds
      syslog(1, "==>>power mgmt tests received %u - turn off leds\n", userData);
      ret = meadow_pwr_mgmt_turn_off_tri_color_leds();
      break;

    case 51:
      // Enter Sleep mode very low savings, wakes right up.
      syslog(1, "==>>power mgmt tests received %u - Sleep mode\n", userData);
      sleep(1);
      ret = meadow_pwr_mgmt_enter_sleep();
      break;

    case 52:
      // Enter Stop mode with max power savings & slowest restart
      syslog(1, "==>>power mgmt tests received %u - Stop mode MAX savings\n", userData);
      sleep(1);
      ret = meadow_pwr_mgmt_enter_stop(true);
      break;

    case 53:
      // Enter Stop mode with minimum power savings & fastest restart
      syslog(1, "==>>power mgmt tests received %u - Stop mode Min savings\n", userData);
      sleep(1);
      ret = meadow_pwr_mgmt_enter_stop(false);
      break;

    case 54:
      // Enter Standby mode. This is the lowest possible power mode
      syslog(1, "==>>power mgmt tests received %u - Standby mode\n", userData);
      usleep(100 * 1000);
      ret = meadow_pwr_mgmt_enter_standby();
      break;

    case 55:
      // Restore clock to HSE
      syslog(1, "==>>power mgmt tests received %u - HSE for clock\n", userData);
      usleep(100 * 1000);
      ret = meadow_pwr_mgmt_use_hse_for_rtc();
      break;

    case 56:
      // Restore clock to LSI
      syslog(1, "==>>power mgmt tests received %u - LSI for clock\n", userData);
      usleep(100 * 1000);
      // The following function calls will result in the the F7 being put into sleep mode for 45 seconds.
      ret = meadow_pwr_mgmt_use_lsi_for_rtc();
      break;

    case 57:
      // Set alarm for X sec, switch to LSI, enter Stop-mode, after alarm wake up switch to HSE.
      syslog(1, "==>>power mgmt tests received %u - Use LSI for clock\n", userData);
      usleep(100 * 1000);
      ret = meadow_pwr_mgmt_full_wakeup_test();
      break;

    default:
    syslog(1, "Unknown value %u passed to hcom_nx_exec_power_mgmt_tests()\n", userData);
    break;
  }

  return ret;
}

//=========================================================
// Set alarm for X sec, switch to LSI, enter Stop-mode, after alarm wake up switch to HSE.
int meadow_pwr_mgmt_full_wakeup_test()
{
  int ret;

  // Turn off tri-color LEDs
  meadow_pwr_mgmt_turn_off_tri_color_leds();

  // Set alarm
  ret = meadow_pwr_mgmt_set_wakeup_alarm_for_seconds(15);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-\n", thisFile, __LINE__);
    return ret;
  }

  // Switch to LSI clock
  ret = meadow_pwr_mgmt_use_lsi_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-\n", thisFile, __LINE__);
    return ret;
  }

  // Enter Stop-mode
  ret = meadow_pwr_mgmt_enter_stop(true);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-\n", thisFile, __LINE__);
    return ret;
  }
  return OK;
}

#endif    // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
