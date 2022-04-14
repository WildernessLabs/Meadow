/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/meadow_rtc_hardware.c
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

// This module contains code to manage the Real-Time hardware within the F7.

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
// The following represents the ISO 8601 formatted time strings
// The C# 's', 'o' and 'u' format specifiers are indicated below
//                 11111111112222222222333
//       01234567890123456789012345678901234567890
// 's' = 2022-04-01T21:34:05 - No utcTimeOffset information 
// 'o' = 2022-04-01T21:35:03.9174375+00:00 - UTC
// 'o' = 2022-04-01T14:38:25.1838288-07:00 - Local time
// 'u' = 2022-04-01 21:35:31Z - Notice no 'T' but a space
//       2022-04-01T14:37:34+00:00
//       2022-04-01T14:37:34-09:30
//       20220331T173425Z - no delimiters but 'T' and 'Z'
//
// ISO 8601 also defines a duration format called time period. I it is pretty
// simple compared to the time format above. It must start with 'P' (for period)
// and contain a 'T' if time is provided included. In this implementation 'Y'
// and 'M' are not supported as they seem to be unnecessary for Meadow.
// P[n]Y[n]M[n]DT[n]H[n]M[n]S
//
// These are based on the ISO 8601 Date/Time 
#define MEADOW_RTC_ISO_8601_CON_T_ELEMENT_OFFSET  (8)
#define MEADOW_RTC_ISO_8601_STD_T_ELEMENT_OFFSET  (10)
#define MEADOW_RTC_ISO_8601_DOT_ELEMENT_OFFSET    (15)
#define MEADOW_RTC_ISO_8601_Z_ELEMENT_OFFSET      (19)
#define MEADOW_RTC_ISO_8601_MIN_INPUT_STR_LEN     (19)

// These are for the sscanfArgs array
#define MEADOW_RTC_CONDENSED_TIME_PARSER_OFFSET   (0)
#define MEADOW_RTC_STANDARD_TIME_PARSER_OFFSET    (1)
#define MEADOW_RTC_STD_NO_T_TIME_PARSER_OFFSET    (2)

/************************************************************************************
 * Private Data
 ************************************************************************************/

// These are the sscanf supported iso 8601 time formats
static char *sscanfArgs[] = 
{
  "%04d%02d%02dT%02d%02d%02dZ",     // Condensed
  "%04d-%02d-%02dT%02d:%02d:%02d",  // Standard
  "%04d-%02d-%02d %02d:%02d:%02d"   // Standard no 'T'
};

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

// This function will return the number of seconds defined in the period
// Turns out a parser is the only way to handle the ISO 8601 duration
// (aka period)
static uint32_t meadow_rtc_parse_iso8601_time_period(const char *isoTimePeriod,
          time_t *secondsTillAlarm)
{
  double secs = 0.0;
  double value;
  uint32_t charsRead = 0;
  uint8_t type = 0;
  const char *ptr = isoTimePeriod;
  uint32_t PandTTest = 0;

  // syslog(1, "Parsing '%s'\n", isoTimePeriod);

  while (*ptr)
  {
    // Ignoring manditory 'P' and 'T' 
    if (*ptr == 'P' || *ptr == 'T')
    {
      PandTTest++;
      ptr++;
      continue;
    }

    // Not supporting Year or Month 
    if (*ptr == 'Y' || *ptr == 'M')
    {
      ptr++;
      continue;
    }

    // The value can be a floating point (e.g 0.5). This should always
    // read 2 items, a floating point and a type
    if (sscanf(ptr, "%lf%c%n", &value, &type, &charsRead) != 2)
    {
      syslog(1, "Parsing error in sscanf for '%s'\n", isoTimePeriod);
      return -ETIME; // Parser error
    }

    switch (type)
    {
      case 'D':
        secs +=  value * 86400.0;   // Seconds in a day
        break;
      case 'H':
        secs +=  value * 3600.0;    // Seconds in an hour
        break;
      case 'M':
        secs +=  value * 60.0;      // Seconds in a minute
        break;
      case 'S':
        secs +=  value;
        break;
      default:
        return -ETIME; // Parser error
        break;
    }
    ptr += charsRead;
  }

  if(PandTTest != 2)
  {
    // Error in provided string
    syslog(1, "Parsing error, must have one 'P' and one 'T', '%s'\n", isoTimePeriod);
    return -ETIME; // Parser error
  }

  *secondsTillAlarm = (uint32_t)secs;
  return OK;
}

//===================================================================
// Convert the time, given the UTC offset, to UTC. The UTC offset is in
// minutes and can be positive or negative. A negative offset means that
// UTC is ahead by this amount and therefore, must be added to the struct
// tm provided value.
static int meadow_rtc_convert_local_time_to_utc(struct tm *tm, int utcTimeOffset)
{
  // Note: Knowing that the largest UTC offset is +/-13 hours, it would have
  // been possible to adjust the struct tm's elements directly. However, doing
  // so would have been risky.
  time_t localTime = mktime(tm);
  int localOffset = utcTimeOffset * 60;     // Convert minutes to seconds
  time_t utcTime = localTime - localOffset; // Add negative offset

  // Modify provided struct tm with utc time
  struct tm *tmTemp = gmtime(&utcTime);
  memcpy(tm, tmTemp, sizeof(struct tm));

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_rtc_hardware_initialize()
{
  return OK;
}

//================================================================================
// Set the UTC offset in a battery backed register so it wont be lost unless
// the F7 is power cycled. This is the behavior of the F7's RTC hardware.
int meadow_rtc_get_bbr_utc_offset()
{
  uint32_t utcOffset = getreg32(MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
  return (int)utcOffset;
}

//================================================================================
// Get the UTC offset in a battery backed register so it won't be lost unless
// the F7 is power cycled. This is the behavior of the F7's RTC hardware. i.e.
// it keeps the clock values unless the power is cycled.
void meadow_rtc_set_bbr_utc_offset(int utcOffset)
{
  putreg32((uint32_t)utcOffset, MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
}

//================================================================================
// This function only processes the first part of the ISO8601 date time string.
// This is enough to fully populate the struct tm. There is a helper function
// that can get the utc offset and any fractional seconds.
//
int meadow_rtc_parse_iso8601_date_time(char *isoDateTime, size_t isoDataTimeLen,
          struct tm *tmResult)
{
  int sscanfArgOff;

  // Identify the actual format so we know how to parse this string
  if(isoDateTime[MEADOW_RTC_ISO_8601_DOT_ELEMENT_OFFSET] == 'Z' &&
          isoDateTime[MEADOW_RTC_ISO_8601_CON_T_ELEMENT_OFFSET] == 'T')
  {
    sscanfArgOff = MEADOW_RTC_CONDENSED_TIME_PARSER_OFFSET;
  }
  else
  {
    if(isoDateTime[MEADOW_RTC_ISO_8601_STD_T_ELEMENT_OFFSET] == 'T')
    {
      sscanfArgOff = MEADOW_RTC_STANDARD_TIME_PARSER_OFFSET;
    }
    else if(isoDateTime[MEADOW_RTC_ISO_8601_STD_T_ELEMENT_OFFSET] == ' ')
    {
      sscanfArgOff = MEADOW_RTC_STD_NO_T_TIME_PARSER_OFFSET;
    }
    else
    {
      syslog(1, "Date Time format error for '%s'\n", isoDateTime);
      return -EINVAL;
    }
  }

  int sscanfCnt = sscanf(isoDateTime, sscanfArgs[sscanfArgOff],
              &tmResult->tm_year, &tmResult->tm_mon, &tmResult->tm_mday,
              &tmResult->tm_hour, &tmResult->tm_min, &tmResult->tm_sec);

  if(sscanfCnt != 6)
  {
    syslog(1, "Date Time parsing error for '%s'\n", isoDateTime);
    return -EINVAL;
  }
  
  // Convert to unix time
  tmResult->tm_year -= 1900;
  tmResult->tm_mon -= 1;

  return OK;
}

//===================================================================
// Find the UTC offset and any fractional seconds that are are part of 
// the ISO 8601 provided time string
int meadow_rtc_parse_iso8601_utc_offset(char *isoDateTime, size_t isoDataTimeLen,
          int *utcTimeOffset, double *fractSec)
{
  char *fracEnd;    // point to next character after fraction
  char *offsetSign;    // pointer to the '=' or '-' before utcTimeOffset

  *utcTimeOffset = 0;
  *fractSec = 0.0;
  
  // 2022-04-01T14:37:34 = 19 characters
  if(isoDataTimeLen <= MEADOW_RTC_ISO_8601_MIN_INPUT_STR_LEN)
  {
    // There can't be any other information just return with fields = 0
    return OK;
  }

  // The character at offset 19 can be '.', 'Z', '+' or '-'. This may not be
  // an ISO 8601 compliant restriction.
  // Note: this assumes the 'Z' can only exist after the iso date time portion
  // with no '-' nor ':' delimiter at offset 19. That is, the 'Z' cannot
  // exist after the fractional seconds field.
  if(isoDateTime[MEADOW_RTC_ISO_8601_Z_ELEMENT_OFFSET] == 'Z')
  {
    // utcTimeOffset value is already set to 0
    return OK;
  }

  // If there is a fractional seconds field, this function will parse it.
  // This assumes that the fractional seconds data is immediately after
  // the Data Time field (i.e. HH:MM:SS.fractsec), according to the ISO
  // 8601 spec.
  if(isoDateTime[MEADOW_RTC_ISO_8601_Z_ELEMENT_OFFSET] == '.')
  {
    *fractSec = strtod(isoDateTime + MEADOW_RTC_ISO_8601_Z_ELEMENT_OFFSET, &fracEnd);
  }

  // Find the utc offset string, it's procceeded by '-' or '+'. This test
  // assumes that this function will not be called if there is no utc offset.
  offsetSign = strpbrk(isoDateTime + MEADOW_RTC_ISO_8601_Z_ELEMENT_OFFSET, "+-");
  if(offsetSign == NULL)
  {
    syslog(1, "utcTimeOffset parsing error, +/- not found in '%s'\n",
              isoDateTime);
    return -EINVAL;
  }

  // Look for the offsets components. No point looking for offset sign, we
  // already know where it is.
  int utcOffsetHour, utcOffsetMin;
  int sscanfCnt = sscanf(offsetSign + 1, "%02d:%02d",
            &utcOffsetHour, &utcOffsetMin);
  if(sscanfCnt != 2)
  {
    syslog(1, "utcTimeOffset parsing error for '%s'\n", isoDateTime);
    return -EINVAL;
  }

  *utcTimeOffset = (utcOffsetHour * 60) + utcOffsetMin;

  // If the offset is negative, make the utc offset negative.
  if(*offsetSign == '-')
    *utcTimeOffset *= -1;

  return OK;
}

//===================================================================
// Called via HCOM this functin will set the low-power wakeup duration.
// It accepts ether an absolute time of the wakeup or a time duration.
int meadow_rtc_wakeup_time_period(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  struct tm tmAlarm;

  HcomProtoTextMsg_t *setPeriodCmd = (HcomProtoTextMsg_t *) hdrMsg;
  size_t isoPeriodLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(isoPeriodLen == 0 || setPeriodCmd->textData == NULL)
  {
    syslog(1, "Error:No period value found\n");
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

  syslog(1, "-->Time Period parsing, packetLen:%u, time:'%s'\n",
            isoPeriodLen, isoPeriodStr);

  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(1, "-->time(NULL); Failed! Len:%u, time:'%s', currentTime:-1\n",
              isoPeriodLen, isoPeriodStr);
    free(isoPeriodStr);
    return -ETIME;
  }

  // The 'P' always proceeds a time period. Therefore, its easly to determine
  // what has been sent since it must be either a time period, which always
  // start with 'P' or a future time which doesn't.
  if(isoPeriodStr[0] == 'P')
  {
    time_t secondsTillAlarm = 0;

    // Parse ISO period (duration) formatted string
    ret = meadow_rtc_parse_iso8601_time_period(isoPeriodStr, &secondsTillAlarm);
    if(ret < 0)
    {
      syslog(1, "-->Time Period parsing Failed! Len:%u, time:'%s', ret:%d\n",
                isoPeriodLen, isoPeriodStr);
      free(isoPeriodStr);
      return ret;
    }

    time_t almTime = secondsTillAlarm + currentTime;

    // Now convert alarm time to a future time in struct tm
    struct tm *tmTemp;
    tmTemp = gmtime(&almTime);
    memcpy(&tmAlarm, tmTemp, sizeof(struct tm));
  }
  else
  {
    // Convert to the provide date time into an absolute wakeup time
    ret = meadow_rtc_parse_iso8601_date_time(isoPeriodStr, isoPeriodLen,
              &tmAlarm);
    if(ret < 0)
    {
      // Parsing time period failed
      syslog(1, "-->Date/Time parsing Failed! Len:%u, time:'%s'\n",
                isoPeriodLen, isoPeriodStr);
      free(isoPeriodStr);
      return ret;
    }

    // Alarm time must be in the future
    time_t almTime = mktime(&tmAlarm);
    if(almTime <= currentTime)
    {
      syslog(1, "-->Alarm time before current time\n");
      free(isoPeriodStr);
      return ret;
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
    syslog(1, "-Setting alarm time failed\n");
    return ret;
  }

  return OK;
}

//===================================================================
// Set Date and Time in Nuttx clock
int meadow_rtc_set_time(const HcomProtoHdrMsg_t *hdrMsg,
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
    syslog(1, "Error:No time value found\n");
    return -EINVAL;
  }

  // Need a NULL terminated string
  char *isoDateTimeStr = malloc(isoTimeLen + 1);
  if(isoDateTimeStr == NULL)
  {
    syslog(1, "Memory Allocation error\n");
    return -ENOMEM;
  }
  memcpy(isoDateTimeStr, setTimeCmd->textData, isoTimeLen);
  isoDateTimeStr[isoTimeLen] = '\0';

  syslog(1, "Entered rtc Set Time, packetLen:%u, time:'%s'\n", packetSize, isoDateTimeStr);
  
  // This call returns date and time in a struct tm. It also returns the utc
  // offset and any fractional seconds
  ret = meadow_rtc_parse_iso8601_date_time(isoDateTimeStr, isoTimeLen, &tmSet);
  if(ret < 0)
  {
    free(isoDateTimeStr);
    return ret;
  }

  // Find UTC time offset and any fractional seconds in message
  ret = meadow_rtc_parse_iso8601_utc_offset(isoDateTimeStr, isoTimeLen,
          &utcTimeOffset, &fractSec);
  free(isoDateTimeStr);
  if(ret < 0)
  {
    return ret;
  }

  // Save the utc offset so it's available
  meadow_rtc_set_bbr_utc_offset(utcTimeOffset);

  // Do we need to adjust the time to make it UTC?
  if(utcTimeOffset != 0)
  {
    ret = meadow_rtc_convert_local_time_to_utc(&tmSet, utcTimeOffset);
    if(ret < 0)
    {
      return ret;
    }
  }
  
  // For testing show the date & time
  syslog(1, "Setting time to:%4d-%02d-%02dT%02d:%02d:%02d\n",
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
// Return the time from Nuttx to CLI assuming the RTC hardware
int meadow_rtc_read_time(struct hcom_nx_cmd_data *cmdData)
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
  int utcOffset = meadow_rtc_get_bbr_utc_offset();
  utcOffHour = utcOffset/60;
  utcOffMin = utcOffset%60;

  // Build time string for host
  char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];

  snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
            "UTC time:%4d-%02d-%02dT%02d:%02d:%02d%+02d:%02d",
            tmRead.tm_year + 1900, tmRead.tm_mon + 1, tmRead.tm_mday,
            tmRead.tm_hour, tmRead.tm_min, tmRead.tm_sec,
            utcOffHour, utcOffMin);

// BEGIN ONLY FOR DEVELOPMENT
  syslog(1, "HCOM-Sending time as '%s'\n", hostMsg);
#if defined (CONFIG_TIME_EXTENDED)
  syslog(1, "HCOM-FYI-Days since Sun:%d, Days since Jan 1:%03d\n",
             tmRead.tm_wday + 1, tmRead.tm_yday);
#endif
// END ONLY FOR DEVELOPMENT

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          hostMsg, __FILE__, __LINE__);

  return OK;
}
