/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_config_wakeup_alarm.c
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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

// This module is used to configure the RTC alarm feature, allowing it to
// wakeup the F7. Because the alarm feature uses a specific date and time
// as the alarm trigger it can wait from 1 second to 28 days - 1 second.

#warning "(--) Peter is Here (pwrmgmt_config_wakeup_alarm.c)"

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"

#include "stm32_gpio.h"
#include "stm32_pwr.h"    // FOR TESTING
#include "stm32_rcc.h"    // Re-init clocks
#include "stm32_alarm.h"

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include "pwrmgmt_local.h"

#include "chip/stm32f76xx77xx_pwr.h"
#include "chip/stm32_exti.h"
#include "nvic.h"

#include <arch/board/board.h>

#include "stm32f777zit6-meadow.h"
#include "hcom_nx/hcom_nx_common.h"

#include "stm32_alarm.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

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

static int meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(time_t almTime);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Enter low-power mode for the period specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_after_seconds(time_t secondsTillAlarm)
{
  int ret;

  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "Error:'time(NULL)' call failed\n");
    return -ETIME;
  }

  // What time will this be (in seconds)?
  time_t almTime = secondsTillAlarm + currentTime;

  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(almTime);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }
  
  return ret;
}

//==============================================================
// Enter low-power mode until the time in seconds specified
static int meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(time_t almTime)
{
  int ret;
  struct tm tmAlarm;

  // Now convert alarm time to a future time in struct tm
  struct tm tmTemp;
  gmtime_r(&almTime, &tmTemp);
  memcpy(&tmAlarm, &tmTemp, sizeof(struct tm));

  // Set the alarm based on the tm time structure's information
  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_based_on_tm(tmAlarm);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }

  return OK;
}

//==================================================================
// Enter low-power mode until the time specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_based_on_tm(struct tm tmAlarm)
{
  struct alm_setalarm_s alminfo;        // defined in stm32_alarm.h
  int ret;

  // Alarm time must be in the future
  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "%s@%d-Error:time(NULL) call failed\n", thisFile, __LINE__);
    return -ETIME;
  }

  time_t almTime = mktime(&tmAlarm);
  if(almTime <= currentTime)
  {
    syslog(LOG_ERR, "Error:Alarm time before current time\n");
    return -ETIME;
  }

  alminfo.as_id = RTC_ALARMA; // or RTC_ALARMB
  alminfo.as_time = tmAlarm;  // Alarm time
  alminfo.as_cb = NULL;       // Callback
  alminfo.as_arg = NULL;      // Callback arguments

  // There's Nuttx code to set the alarm
  ret = stm32_rtc_setalarm(&alminfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }
  
  return ret;
}

#endif
