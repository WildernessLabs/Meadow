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

// These are the sscanf supported iso 8601 time formats
static char *sscanfArgs[] = 
{
  "%04d%02d%02dT%02d%02d%02dZ",
  "%04d-%02d-%02dT%02d:%02d:%02d",
  "%04d-%02d-%02d %02d:%02d:%02d"
};

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_rtc_parse_iso8601_utc_offset(char *isoDateTime, \
          size_t isoDataTimeLen, int *utcTimeOffset, double *fractSec);

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
int meadow_rtc_get_utc_offset()
{
  uint32_t utcOffset = getreg32(MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
  return (int)utcOffset;
}

//================================================================================
// Get the UTC offset in a battery backed register so it wont be lost unless
// the F7 is power cycled. This is the behavior of the F7's RTC hardware.
void meadow_rtc_set_utc_offset(int utcOffset)
{
  putreg32((uint32_t)utcOffset, MEADOW_UTC_OFF_BATTERY_BACKED_REGISTER);
}

//================================================================================
// This function only processes the first part of the ISO8601 date time, which
// is enough to fully populate the struct tm. It then calls a helper function
// to finish the work.
//
// This function must parse the following ISO 8601 formatted time strings
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
// There are some "magic" numbers but since this is a fixed format I saved
// a few minutes by leaving them magic....
int meadow_rtc_parse_iso8601_date_time(char *isoDateTime, size_t isoDataTimeLen,
          struct tm *tmResult, int *utcTimeOffset, double *fractSec)
{
  int sscanfArgOff;

  // Identify how to parse this string
  if(isoDateTime[15] == 'Z' && isoDateTime[8] == 'T')
  {
    sscanfArgOff = 0;
  }
  else
  {
    if(isoDateTime[10] == 'T')
    {
      sscanfArgOff = 1;
    }
    else if(isoDateTime[10] == ' ')
    {
      sscanfArgOff = 2;
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
  
  // Make into unix time
  tmResult->tm_year -= 1900;
  tmResult->tm_mon -= 1;

  int ret = meadow_rtc_parse_iso8601_utc_offset(isoDateTime, isoDataTimeLen, utcTimeOffset,
            fractSec);

  // Save the utc offset so it can be returned if needed
  meadow_rtc_set_utc_offset(*utcTimeOffset);

  return ret;
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
  if(isoDataTimeLen <= 19)
  {
    // There can't be any other information just return with fields = 0
    return OK;
  }

  // The character at offset 19 can be '.', 'Z', '+' or '-'
  // Note: this assumes the 'Z' can only exist after the iso date time portion
  // with no '-' nor ':' delimiter or at offset 19. That is, the 'Z' cannot
  // exist after the fractional seconds field.
  if(isoDateTime[19] == 'Z')
  {
    // utcTimeOffset value is already set to 0
    return OK;
  }

  // Find the utc Time Offset string, it's procceeded by
  // '-' or '+'.
  offsetSign = strpbrk(isoDateTime + 19, "+-");
  if(offsetSign == NULL)
  {
    syslog(1, "utcTimeOffset parsing error, +/- not found at offset 19 '%s'\n",
              isoDateTime);
    return -EINVAL;
  }

  int utcOffsetHour, utcOffsetMin;

  int sscanfCnt = sscanf(offsetSign + 1, "%02d:%02d",
            &utcOffsetHour, &utcOffsetMin);
  if(sscanfCnt != 2)
  {
    syslog(1, "utcTimeOffset parsing error for '%s'\n", isoDateTime);
    return -EINVAL;
  }

  *utcTimeOffset = (utcOffsetHour * 60) + utcOffsetMin;

  if(*offsetSign == '-')
    *utcTimeOffset *= -1;

  // Convert fractional seconds field, if there is one. This assumes that
  // the the fractional seconds must be immediately after the Data Time
  // field (i.e. HH:MM:SS.fractsec)
  if(isoDateTime[19] == '.')
  {
    *fractSec = strtod(isoDateTime + 19, &fracEnd);
  }

  return OK;
}

//===================================================================
// Set Date and Time in Nuttx
int meadow_rtc_set_time(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize)
{
  int ret;
  struct tm tm;
  int utcTimeOffset;
  double fractSec;

  HcomProtoTextMsg_t *setTimeCmd = (HcomProtoTextMsg_t *) hdrMsg;
  size_t isoTimeLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  if(isoTimeLen == 0 || setTimeCmd->textData == NULL)
  {
    syslog(1, "Error:No time value found\n");
    return -EINVAL;
  }

  // Need a NULL terminate string
  char *isoDateTime = malloc(isoTimeLen + 1);
  if(isoDateTime == NULL)
  {
    syslog(1, "Memory Allocation error\n");
    return -ENOMEM;
  }
  memcpy(isoDateTime, setTimeCmd->textData, isoTimeLen);
  isoDateTime[isoTimeLen] = '\0';

  syslog(1, "Entered rtc Set Time, packetLen:%u, time:'%s''\n", packetSize, isoDateTime);
  
  // This call returns date and time in a struct tm. It also returns the utc
  // offset and any fractional seconds
  ret = meadow_rtc_parse_iso8601_date_time(isoDateTime, isoTimeLen, &tm, &utcTimeOffset, &fractSec);
  free(isoDateTime);
  if(ret < 0)
  {
    return ret;
  }

  // For testing show the date time
  syslog(1, "Setting time to:%4d-%02d-%02dT%02d:%02d:%02d\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

  // Give information to Nuttx, which includes the RTC hardware, if it has
  // been added to the Nuttx configuration.
  struct timespec tp;
  tp.tv_nsec = 0;
  tp.tv_sec = mktime(&tm);

  clock_settime(CLOCK_REALTIME, &tp);

  // Note: the utc offset and any fractional seconds are unused. Nuttx just
  // assumes the time is UTC.

  return OK;
}

//========================================================================
// Return the time from Nuttx assuming the RTC hardware
int meadow_rtc_read_time(struct hcom_nx_cmd_data *cmdData)
{
  // ISO 8601 format for UTC is 2022-03-31T17:34:25+00:00
  int ret;
  struct tm tm;
  int utcOffHour;
  int utcOffMin;

  // Get broken-out time from RTC hardware registers
  // long nsec;   // Can read fractional seconds from Nuttx
  // ret = stm32_rtc_getdatetime_with_subseconds(&tm, &nsec);
  ret = up_rtc_getdatetime(&tm);
  if(ret < 0)
  {
    return -EINVAL;
  }

  // Next read and process utc offset
  int utcOffset = meadow_rtc_get_utc_offset();
  utcOffHour = utcOffset/60;
  utcOffMin = utcOffset%60;

  // Build time string for host
  char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];

  snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
            "%4d-%02d-%02dT%02d:%02d:%02d%+02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            utcOffHour, utcOffMin);

  syslog(1, "HCOM-Sending time as '%s'\n", hostMsg);
#if defined (CONFIG_TIME_EXTENDED)
  syslog(1, "HCOM-FYI-Days since Sun:%d, Days since Jan 1:%03d\n",
             tm.tm_wday + 1, tm.tm_yday);
#endif
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          hostMsg, __FILE__, __LINE__);

  return OK;
}
