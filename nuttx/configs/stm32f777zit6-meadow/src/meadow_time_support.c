/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/meadow_time_support.c
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

// This module contains code to set and read the Nuttx time and to set the
// RTC Alarm.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <nuttx/arch.h>

#include <meadow/hcom_upd_shared.h>
#include "hcom_nx/hcom_nx_common.h"
#include "stm32f777zit6-meadow.h"
#include "up_arch.h"

#include "stm32_alarm.h"

#include <meadow/hcom_bbreg_defn.h>

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
// // #undef USE_MEADOW_DEBUG_HELPERS
// #include <meadow/meadow_debug_helpers.h>
// #include "stm32_gpio.h"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/


/****************************************************************************
 * Private Functions
 ****************************************************************************/
// Convert the value in struct tm from local to UTC. The UTC offset is in
// minutes and can be positive or negative. A negative UTC offset means that
// UTC is behind by this amount. Therefore, to get UTC we must add the offset
// the provided time value.
static int meadow_time_convert_local_and_offset_to_utc(struct tm *tm, int utcTimeOffset)
{
  // Note: Knowing that the largest UTC offset is +/-13 hours, it would have
  // been possible to adjust the struct tm's elements directly. However, doing
  // so would have been risky.
  time_t localTime = mktime(tm);
  int localOffset = utcTimeOffset * 60;     // Convert minutes to seconds
  time_t utcTime = localTime - localOffset; // Subtact to add negative offset

  // Replace provided struct tm with utc time
  struct tm tmTemp;
  gmtime_r(&utcTime, &tmTemp);
  memcpy(tm, &tmTemp, sizeof(struct tm));

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Set the UTC offset in a battery backed register so it wont be lost unless
// the F7 is power cycled. This is the behavior of the F7's RTC hardware.
int meadow_time_get_bbr_utc_offset()
{
  uint32_t utcOffset = getreg32(MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
  return (int)utcOffset;
}

//================================================================================
// Get the UTC offset in a battery backed register so it won't be lost unless
// the F7 is power cycled. This is the behavior of the F7's RTC hardware. i.e.
// it keeps the clock values unless the power is cycled.
void meadow_time_set_bbr_utc_offset(int utcOffset)
{
  putreg32((uint32_t)utcOffset, MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
}

//===================================================================
// Called by HCOM message
// Sets the low-power wakeup time. It accepts ether an absolute time of the
// wakeup or a time duration.
int meadow_time_wakeup_period(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  struct tm tmAlarm;

  HcomProtoTextMsg_t *setPeriodCmd = (HcomProtoTextMsg_t *) hdrMsg;
  size_t isoPeriodLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(isoPeriodLen == 0 || setPeriodCmd->textData == NULL)
  {
    syslog(LOG_ERR, "Error:No period value found\n");
    return -EINVAL;
  }

  // Need a NULL terminated string
  char *isoPeriodStr = malloc(isoPeriodLen + 1);
  if(isoPeriodStr == NULL)
  {
    syslog(1, "Memory Allocation error\n");
    return -ENOMEM;
  }
  memcpy(isoPeriodStr, setPeriodCmd->textData, isoPeriodLen);
  isoPeriodStr[isoPeriodLen] = '\0';

  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "Error:time(NULL) call failed, Len:%u, time:'%s', currentTime:-1\n",
              isoPeriodLen, isoPeriodStr);
    free(isoPeriodStr);
    return -ETIME;
  }

  // The 'P' always proceeds a time period. Therefore, its easly to determine
  // what has been sent since it must be either a time period, which always
  // start with 'P' or a future time which doesn't.
  if(isoPeriodStr[0] == MEADOW_ISO_8601_PERIOD_FORMAT_LEAD_IN)
  {
    time_t secondsTillAlarm = 0;

    // Parse ISO period (duration) formatted string
    ret = meadow_parse_iso8601_time_period(isoPeriodStr, &secondsTillAlarm);
    if(ret < 0)
    {
      syslog(LOG_ERR, "Error:Time Period parsing failed, Len:%u, time:'%s', ret:%d\n",
                isoPeriodLen, isoPeriodStr);
      free(isoPeriodStr);
      return ret;
    }

    time_t almTime = secondsTillAlarm + currentTime;

    // Now convert alarm time to a future time in struct tm
    struct tm tmTemp;
    gmtime_r(&almTime, &tmTemp);
    memcpy(&tmAlarm, &tmTemp, sizeof(struct tm));
  }
  else
  {
    // Must be an absolute time so convert it
    ret = meadow_parse_iso8601_date_time(isoPeriodStr, isoPeriodLen,
              &tmAlarm);
    if(ret < 0)
    {
      // Parsing time period failed
      syslog(LOG_ERR, "Error:Date/Time parsing failed, Len:%u, time:'%s'\n",
                isoPeriodLen, isoPeriodStr);
      free(isoPeriodStr);
      return ret;
    }

    // Alarm time must be in the future
    time_t almTime = mktime(&tmAlarm);
    if(almTime <= currentTime)
    {
      syslog(LOG_ERR, "Error:Alarm time before current time\n");
      free(isoPeriodStr);
      return -ETIME;
    }
  }

  free(isoPeriodStr);

  // Set the alarm
  struct alm_setalarm_s alminfo;
  alminfo.as_id = RTC_ALARMA; // or RTC_ALARMB
  alminfo.as_time = tmAlarm;  // Alarm time
  alminfo.as_cb = NULL;       // Callback
  alminfo.as_arg = NULL;      // Callback arguments

  ret = stm32_rtc_setalarm(&alminfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
    return ret;
  }

  return OK;
}

//===================================================================
// Called by HCOM message
// Set Date and Time in Nuttx clock
int meadow_time_set_clock(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  struct tm tmSet;
  int utcTimeOffset;
  double fractSec;

  HcomProtoTextMsg_t *setTimeCmd = (HcomProtoTextMsg_t *) hdrMsg;
  size_t isoTimeLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(isoTimeLen == 0 || setTimeCmd->textData == NULL)
  {
    syslog(LOG_ERR, "Error:No time value found\n");
    return -EINVAL;
  }

  // Need a NULL terminated string
  char *isoDateTimeStr = malloc(isoTimeLen + 1);
  if(isoDateTimeStr == NULL)
  {
    syslog(LOG_ERR, "Memory Allocation error\n");
    return -ENOMEM;
  }
  memcpy(isoDateTimeStr, setTimeCmd->textData, isoTimeLen);
  isoDateTimeStr[isoTimeLen] = '\0';
  
  // This call returns date and time in a struct tm. It also returns the utc
  // offset and any fractional seconds
  ret = meadow_parse_iso8601_date_time(isoDateTimeStr, isoTimeLen, &tmSet);
  if(ret < 0)
  {
    // Error already logged
    free(isoDateTimeStr);
    return ret;
  }

  // Find UTC time offset and any fractional seconds in message
  ret = meadow_parse_iso8601_utc_offset(isoDateTimeStr, isoTimeLen,
          &utcTimeOffset, &fractSec);
  free(isoDateTimeStr);
  if(ret < 0)
  {
    // Error already logged
    return ret;
  }

  // Save the utc offset so it's available
  meadow_time_set_bbr_utc_offset(utcTimeOffset);

  // Do we need to adjust the time to make it UTC?
  if(utcTimeOffset != 0)
  {
    ret = meadow_time_convert_local_and_offset_to_utc(&tmSet, utcTimeOffset);
    if(ret < 0)
    {
      return ret;
    }
  }
  
  // // For testing show the date & time
  // syslog(1, "Setting time to:%4d-%02d-%02dT%02d:%02d:%02d\n",
  //           tmSet.tm_year + 1900, tmSet.tm_mon + 1, tmSet.tm_mday,
  //           tmSet.tm_hour, tmSet.tm_min, tmSet.tm_sec);

  // Give information to Nuttx, which updates the RTC hardware, assuming it's
  // been added to the Nuttx configuration.
  struct timespec tp;
  tp.tv_nsec = 0;
  tp.tv_sec = mktime(&tmSet);

  clock_settime(CLOCK_REALTIME, &tp);

  return OK;
}

//========================================================================
// Called by HCOM message
// Return the time from Nuttx to CLI assuming the RTC hardware
int meadow_time_read_clock(struct hcom_nx_cmd_data *cmdData)
{
  // ISO 8601 format for UTC is 2022-03-31T17:34:25+00:00
  int ret;
  struct tm tmRead;
  int utcOffHour;
  int utcOffMin;

  // Get broken-out time from Nuttx. If RTC is enabled this will come from
  // the RTC hardware.

  // If needed can read fractional seconds from Nuttx via
  // stm32_rtc_getdatetime_with_subseconds() instead of up_rtc_getdatetime();
  ret = up_rtc_getdatetime(&tmRead);
  if(ret < 0)
  {
    return -EINVAL;
  }

  // Read the utc offset
  int utcOffset = meadow_time_get_bbr_utc_offset();
  utcOffHour = utcOffset/60;
  utcOffMin = utcOffset%60;

  // Build time string for host
  char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];

  snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
            "UTC time:%4d-%02d-%02dT%02d:%02d:%02d%+02d:%02d",
            tmRead.tm_year + 1900, tmRead.tm_mon + 1, tmRead.tm_mday,
            tmRead.tm_hour, tmRead.tm_min, tmRead.tm_sec,
            utcOffHour, utcOffMin);

  //   syslog(1, "HCOM-Sending time as '%s'\n", hostMsg);
  // #if defined (CONFIG_TIME_EXTENDED)
  //   syslog(1, "HCOM-FYI-Days since Sun:%d, Days since Jan 1:%03d\n",
  //             tmRead.tm_wday + 1, tmRead.tm_yday);
  // #endif

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          hostMsg, __FILE__, __LINE__);

  return OK;
}
