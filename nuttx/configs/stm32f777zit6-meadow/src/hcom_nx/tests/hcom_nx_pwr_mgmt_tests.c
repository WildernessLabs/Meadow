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

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

// #include <meadow/hcom_upd_shared.h>
#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/stat.h>
#include "stm32_rtc.h"

//=================================================================
int hcom_nx_exec_test_pwr_mgmt_setup(void)
{
  return OK;
}

//=================================================================
// These tests are for testing the power management implementation
int hcom_nx_exec_power_mgmt_tests(struct hcom_nx_cmd_data *cmdData)
{
  uint32_t userData = cmdData->userData;
  int ret = OK;

  switch(userData)
  {
    case 50:
      // Turn-off RGB leds
      ret = meadow_pwr_mgmt_turn_off_leds();
      break;

    case 51:
      // Enter Stop mode with max power savings & slowest restart
      ret = meadow_pwr_mgmt_change_state(mpm_state_stop_save_max);
      break;

    case 52:
      // Enter Stop mode with minimum power savings & fastest restart
      ret = meadow_pwr_mgmt_change_state(mpm_state_stop_save_min);
      break;

    case 53:
      // Enter Standby mode. This is the lowest possible power mode
      ret = meadow_pwr_mgmt_change_state(mpm_state_standby);
      break;

    case 55:
      // Play with LSI clock. Determine it's speed
      break;

    case 60:
      // Set clock
      {
        struct timespec tp;
        struct tm tm;

        tm.tm_sec  = 21;
        tm.tm_min  = 14;
        tm.tm_hour = 17;
        tm.tm_mday = 30;
        tm.tm_mon  = 3 - 1;   // March
        tm.tm_year = 2022 - 1900;

        tp.tv_nsec = 0;
        tp.tv_sec = mktime(&tm);

        clock_settime(CLOCK_REALTIME, &tp);
        syslog(1, "Time set\n");
      }
      break;

    case 61:
      // Get clock from RTC hardware
      {
        struct tm tm;
        long nsec;

        // Get broken-out time
        // ret = up_rtc_getdatetime(&tm);
        // subseconds can only be read.
        ret = stm32_rtc_getdatetime_with_subseconds(&tm, &nsec);

        // int tm_sec - seconds after the minute – [0, 61] (until C99)[0, 60] (since C99)[note 1]
        // int tm_min - minutes after the hour – [0, 59]
        // int tm_hour - hours since midnight – [0, 23]
        // int tm_mday - day of the month – [1, 31]
        // int tm_mon - months since January – [0, 11]
        // int tm_year - years since 1900
        // CONFIG_TIME_EXTENDED adds the following:
        // int tm_wday - days since Sunday – [0, 6]
        // int tm_yday - days since January 1 – [0, 365]
        // int tm_isdst - Daylight Saving Time flag. The value is positive if DST is in effect,
        //                zero if not and negative if no information is available
        //
        // Build time string
        syslog(1, "Current time is %4d-%02d-%02dT%02d:%02d:%02d (0x%08x)\n", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
        syslog(1, "Days since Sun:%d, Days since Jan 1:%03d, DST:%d\n",
                  tm.tm_wday + 1, tm.tm_yday, tm.tm_isdst);
      }
      break;

    default:
    syslog(1, "Unknown value %u passed to hcom_nx_exec_power_mgmt_tests()\n", userData);
    break;

  }
  return ret;
}

#endif    // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
