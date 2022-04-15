/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/misc/parse_iso8601_time.c
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

// This module contains code to parse the ISO 8601 time formats.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

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
#define MEADOW_ISO_8601_CON_T_ELEMENT_OFFSET  (8)
#define MEADOW_ISO_8601_STD_T_ELEMENT_OFFSET  (10)
#define MEADOW_ISO_8601_DOT_ELEMENT_OFFSET    (15)
#define MEADOW_ISO_8601_Z_ELEMENT_OFFSET      (19)
#define MEADOW_ISO_8601_MIN_INPUT_STR_LEN     (19)

// These are for the sscanfArgs array
#define MEADOW_RTC_CONDENSED_TIME_PARSER_OFFSET   (0)
#define MEADOW_RTC_STANDARD_TIME_PARSER_OFFSET    (1)
#define MEADOW_RTC_STD_NO_T_TIME_PARSER_OFFSET    (2)

/************************************************************************************
 * Private Data
 ************************************************************************************/

// These are the sscanf supported iso 8601 date/time formats
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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// This function will return the number of seconds defined in the period
// Turns out a parser is the only way to handle the ISO 8601 duration
// (aka period)
uint32_t meadow_parse_iso8601_time_period(const char *isoTimePeriod,
          time_t *secondsTillAlarm)
{
  double value;
  double secs = 0.0;
  const char *ptr = isoTimePeriod;
  uint32_t charsParsed = 0;
  uint32_t P_TTest = 0;
  uint8_t type;
  bool afterT = false;

  while (*ptr)
  {
    // Ignoring manditory 'P' and 'T' since these cannot be proceeded by a
    // value. All other characters must be proceeded by a value.
    if (*ptr == 'P' || *ptr == 'T')
    {
      if(*ptr == 'T')
        afterT = true;    // Flag for meaning of 'M'
      P_TTest++;
      ptr++;
      continue;
    }

    // Not supporting Year or Month 
    // The value can be a floating point (e.g 1.5 minutes). This should always
    // read 2 items, a floating point and a type
    if (sscanf(ptr, "%lf%c%n", &value, &type, &charsParsed) != 2)
    {
      syslog(LOG_ERR, "Parsing error in sscanf for '%s'\n", isoTimePeriod);
      return -EFTYPE; // Format error
    }

    switch (type)
    {
      case 'Y':
        break;      // Ignore 'Y'
      case 'D':
        secs +=  value * 86400.0;   // Seconds in a day
        break;
      case 'H':
        secs +=  value * 3600.0;    // Seconds in an hour
        break;
      case 'M':       // This may be hit twice
        if(afterT)    // Ignore 'M' if before 'T' it's a Month
          secs +=  value * 60.0;      // Seconds in a minute
        break;
      case 'S':
        secs +=  value;
        break;
      default:
        syslog(LOG_ERR, "Parsing error for '%s', '%c' not valid\n", isoTimePeriod, type);
        return -EINVAL; // Invalid argument
        break;
    }
    ptr += charsParsed;
  }

  if(P_TTest != 2)
  {
    // Error in provided string
    syslog(LOG_ERR, "Parsing error, must have one 'P' and one 'T', P_TTest '%s'\n", isoTimePeriod);
    return -ETIME; // Time error
  }

  *secondsTillAlarm = (uint32_t)secs;
  return OK;
}

//================================================================================
// This function only processes the first part of the ISO8601 date time string.
// This is enough to fully populate the struct tm. There is a helper function
// that can get the utc offset and any fractional seconds.
//
int meadow_parse_iso8601_date_time(char *isoDateTime, size_t isoDataTimeLen,
          struct tm *tmResult)
{
  int sscanfArgOff;

  // Identify the actual format so we know how to parse this string
  if(isoDateTime[MEADOW_ISO_8601_DOT_ELEMENT_OFFSET] == 'Z' &&
          isoDateTime[MEADOW_ISO_8601_CON_T_ELEMENT_OFFSET] == 'T')
  {
    sscanfArgOff = MEADOW_RTC_CONDENSED_TIME_PARSER_OFFSET;
  }
  else
  {
    if(isoDateTime[MEADOW_ISO_8601_STD_T_ELEMENT_OFFSET] == 'T')
    {
      sscanfArgOff = MEADOW_RTC_STANDARD_TIME_PARSER_OFFSET;
    }
    else if(isoDateTime[MEADOW_ISO_8601_STD_T_ELEMENT_OFFSET] == ' ')
    {
      sscanfArgOff = MEADOW_RTC_STD_NO_T_TIME_PARSER_OFFSET;
    }
    else
    {
      syslog(LOG_ERR, "Date Time format error for '%s'\n", isoDateTime);
      return -EINVAL;
    }
  }

  int sscanfCnt = sscanf(isoDateTime, sscanfArgs[sscanfArgOff],
              &tmResult->tm_year, &tmResult->tm_mon, &tmResult->tm_mday,
              &tmResult->tm_hour, &tmResult->tm_min, &tmResult->tm_sec);

  if(sscanfCnt != 6)
  {
    syslog(LOG_ERR, "Date Time parsing error for '%s'\n", isoDateTime);
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
int meadow_parse_iso8601_utc_offset(char *isoDateTime, size_t isoDataTimeLen,
          int *utcTimeOffset, double *fractSec)
{
  char *fracEnd;    // point to next character after fraction
  char *offsetSign; // pointer to the '=' or '-' before utcTimeOffset

  *utcTimeOffset = 0;
  *fractSec = 0.0;
  
  // 2022-04-01T14:37:34 = 19 characters
  if(isoDataTimeLen <= MEADOW_ISO_8601_MIN_INPUT_STR_LEN)
  {
    // There can't be any other information just return with fields = 0
    return OK;
  }

  // The character at offset 19 can be '.', 'Z', '+' or '-'. This may not be
  // an ISO 8601 compliant restriction.
  // Note: this assumes the 'Z' can only exist after the iso date time portion
  // with no '-' nor ':' delimiter at offset 19. That is, the 'Z' cannot
  // exist after the fractional seconds field.
  if(isoDateTime[MEADOW_ISO_8601_Z_ELEMENT_OFFSET] == 'Z')
  {
    // utcTimeOffset value is already set to 0
    return OK;
  }

  // If there is a fractional seconds field, this function will parse it.
  // This assumes that the fractional seconds data is immediately after
  // the Data Time field (i.e. HH:MM:SS.fractsec), according to the ISO
  // 8601 spec.
  if(isoDateTime[MEADOW_ISO_8601_Z_ELEMENT_OFFSET] == '.')
  {
    *fractSec = strtod(isoDateTime + MEADOW_ISO_8601_Z_ELEMENT_OFFSET, &fracEnd);
  }

  // Find the utc offset string, it's procceeded by '-' or '+'. This test
  // assumes that this function will not be called if there is no utc offset.
  offsetSign = strpbrk(isoDateTime + MEADOW_ISO_8601_Z_ELEMENT_OFFSET, "+-");
  if(offsetSign == NULL)
  {
    syslog(LOG_ERR, "utcTimeOffset parsing error, +/- not found in '%s'\n",
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
    syslog(LOG_ERR, "utcTimeOffset parsing error for '%s'\n", isoDateTime);
    return -EINVAL;
  }

  *utcTimeOffset = (utcOffsetHour * 60) + utcOffsetMin;

  // If the offset is negative, make the utc offset negative.
  if(*offsetSign == '-')
    *utcTimeOffset *= -1;

  return OK;
}
