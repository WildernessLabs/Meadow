/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_time_support.c
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
// The ISO 8601 parsing is done in
// nuttx/configs/stm32f777zit6-meadow/src/misc/parse_iso8601_time.c
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
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

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

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// Convert the value in struct tm from local to UTC. The UTC offset is in
// minutes and can be positive or negative. A negative UTC offset means that
// UTC is behind by this amount. Therefore, to get UTC we must add the offset
// to the provided time value.
//
// Currently not needed.
// static int meadow_time_convert_local_and_offset_to_utc(struct tm *tm, int utcTimeOffset)
// {
//   time_t localTime = mktime(tm);
//   int localOffset = utcTimeOffset * 60;     // Convert minutes to seconds
//   time_t utcTime = localTime - localOffset; // Subtact to add negative offset

//   // Replace provided struct tm with utc time
//   struct tm tmTemp;
//   gmtime_r(&utcTime, &tmTemp);
//   memcpy(tm, &tmTemp, sizeof(struct tm));

//   return OK;
// }

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if 0
// Designed to be called by HCOM message when using ISO-8601 spec
// Sets the low-power wakeup time. It accepts ether an absolute time of the
// wakeup or a time duration.
// This code is incomplete
int pwrmgmt_mono_cmd_time_wakeup_period(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  size_t timePeriodLen;
  char *timePeriodStr;
  HcomProtoTextMsg_t *setPeriodCmd;

  setPeriodCmd = (HcomProtoTextMsg_t *) hdrMsg;
  timePeriodLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(timePeriodLen == 0 || setPeriodCmd->textData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:No period value found\n", thisFile, __LINE__);
    return -EINVAL;
  }

  // Need a NULL terminated string
  timePeriodStr = malloc(timePeriodLen + 1);
  if(timePeriodStr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Memory Allocation error\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  memcpy(timePeriodStr, setPeriodCmd->textData, timePeriodLen);
  timePeriodStr[timePeriodLen] = '\0';
  
  // The 'P' always proceeds a time period. Therefore, its easly to determine
  // what has been sent since it must be either a time period, which always
  // start with 'P' or a future time which doesn't.
  if(timePeriodStr[0] == MEADOW_ISO_8601_PERIOD_FORMAT_LEAD_IN)
  {
    time_t secondsTillAlarm = 0;

    // Parse ISO period (duration) formatted string
    ret = meadow_parse_iso8601_time_period(timePeriodStr, &secondsTillAlarm);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Error:Time Period parsing failed, Len:%u, time:'%s', ret:%d\n",
                 thisFile, __LINE__, timePeriodLen, timePeriodStr);
      free(timePeriodStr);
      return ret;
    }

    ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_for_seconds(secondsTillAlarm);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Error:Time Period parsing failed, Len:%u, time:'%s', ret:%d\n",
                thisFile, __LINE__, timePeriodLen, timePeriodStr);
    }
  }
  else
  {
    struct tm tmAlarm;

    // Must be an absolute time so parse it and set the alarm
    ret = meadow_parse_iso8601_date_time(timePeriodStr, timePeriodLen, &tmAlarm);
    if(ret < 0)
    {
      // Parsing time period failed
      syslog(LOG_ERR, "%s@%d-Error:Date/Time parsing failed, Len:%u, time:'%s'\n",
                thisFile, __LINE__, timePeriodLen, timePeriodStr);
      free(timePeriodStr);
      return ret;
    }

    // Set the alarm
    ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_based_on_tm(tmAlarm);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Error:Setting alarm time failed, ret:%d\n",
                thisFile, __LINE__, ret);
    }
  }

  free(timePeriodStr);
  return ret;
}
#endif

//===================================================================
// Designed to be called by HCOM message
// Set Date and Time in Nuttx
int pwrmgmt_mono_cmd_time_set_clock(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  struct tm tmSet;
  int utcTimeOffset;
  double fractSec;
  HcomProtoTextMsg_t *setTimeCmd;
  size_t dateTimeLen;
  char *dateTimeStr;

  setTimeCmd = (HcomProtoTextMsg_t *) hdrMsg;
  dateTimeLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(dateTimeLen == 0 || setTimeCmd->textData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:No time value found\n", thisFile, __LINE__);
    return -EINVAL;
  }

  // Need a NULL terminated string
  dateTimeStr = malloc(dateTimeLen + 1);
  if(dateTimeStr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Memory Allocation error\n", thisFile, __LINE__);
    return -ENOMEM;
  }
  memcpy(dateTimeStr, setTimeCmd->textData, dateTimeLen);
  dateTimeStr[dateTimeLen] = '\0';
  
  // This call returns date and time in a struct tm. It also returns the utc
  // offset and any fractional seconds
  ret = meadow_parse_iso8601_date_time(dateTimeStr, dateTimeLen, &tmSet);
  if(ret < 0)
  {
    // Error already logged
    free(dateTimeStr);
    return ret;
  }

  // Find UTC time offset and any fractional seconds in message
  ret = meadow_parse_iso8601_utc_offset(dateTimeStr, dateTimeLen,
          &utcTimeOffset, &fractSec);
  free(dateTimeStr);
  if(ret < 0)
  {
    // Error already logged
    return ret;
  }

  // Do we need to adjust the time to make it UTC?
  // NOT SUPPORTED AT THIS TIME
  // if(utcTimeOffset != 0)
  // {
  //   ret = meadow_time_convert_local_and_offset_to_utc(&tmSet, utcTimeOffset);
  //   if(ret < 0)
  //   {
  //     return ret;
  //   }
  // }
  
  // For testing show the date & time
  MEADOW_TRACE_DEBUG("Setting time to:%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmSet.tm_year + 1900, tmSet.tm_mon + 1, tmSet.tm_mday,
            tmSet.tm_hour, tmSet.tm_min, tmSet.tm_sec);

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
// ISO 8601 format for UTC is 2022-03-31T17:34:25+00:00
int pwrmgmt_mono_cmd_time_read_clock(struct hcom_nx_cmd_data *cmdData)
{
  int ret;
  struct tm tmNow;

#if defined(USE_MEADOW_DEBUG_HELPERS)
  // There are 2 ways to get time either clock_gettime or up_rtc_getdatetimer
  // Both were tested here.
  // Get broken-out time from Nuttx. If RTC is enabled this will come from
  // the RTC hardware.
  struct tm tmNuttx;
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  gmtime_r(&abstime.tv_sec, &tmNuttx);

  MEADOW_TRACE_DEBUG("DIAG-From clock_gettime() time:%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmNuttx.tm_year + 1900, tmNuttx.tm_mon + 1, tmNuttx.tm_mday,
            tmNuttx.tm_hour, tmNuttx.tm_min, tmNuttx.tm_sec);
#endif

  // Could read fractional seconds from Nuttx via
  // stm32_rtc_getdatetime_with_subseconds() instead of up_rtc_getdatetime().
  ret = up_rtc_getdatetime(&tmNow);
  if(ret < 0)
  {
    return -EINVAL;
  }

  MEADOW_TRACE_DEBUG("DIAG-From up_rtc_getdatetime() time:%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
            tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

  // Build UTC time string for host
  char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];

  snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
            "UTC time:%4d-%02d-%02dT%02d:%02d:%02d%+02d:%02d",
            tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
            tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec,
            0, 0);

  MEADOW_TRACE_DEBUG("HCOM-Sending time as '%s'\n", hostMsg);
  
#if defined (CONFIG_TIME_EXTENDED)
  MEADOW_TRACE_DEBUG("HCOM-FYI-Days since Sun:%d, Days since Jan 1:%03d\n",
            tmNow.tm_wday + 1, tmNow.tm_yday);
#endif

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          hostMsg, __FILE__, __LINE__);

  return OK;
}

#endif
