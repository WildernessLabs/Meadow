/****************************************************************************
 * configs/stm32f777zit6-meadow/src/hcom_nx/tests/hcom_nx_rtc_hardware_tests.c
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


#if HCOM_INCLUDE_RTC_HARDWARE_TESTS_IN_BUILD > 0

// #include <meadow/hcom_upd_shared.h>
#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/stat.h>
#include "stm32_rtc.h"

/****************************************************************************************************
 * Pre-processor Definitions
 ****************************************************************************************************/

/****************************************************************************************************
 * Private Data
 ****************************************************************************************************/
// Must parse the following formats for ISO 8601
// Using C#'s 's', 'o' and 'u' format specifiers are indicated below
//                 11111111112222222222333
//       01234567890123456789012345678901234567890
// 's' = 2022-04-01T21:34:05 - No utcTimeOffset information 
// 'o' = 2022-04-01T21:35:03.9174375+00:00 - UTC
// 'o' = 2022-04-01T14:38:25.1838288-07:00 - Local time
// 'u' = 2022-04-01 21:35:31Z - Notice no 'T' but a space
//       2022-04-01T14:37:34+00:00
//       2022-04-01T14:37:34-09:30
//       20220331T173425Z - only 'T' and 'Z' specified

// If any of these test cases are modified the tests will also need
// to be modified
static char *iso8601TestCases[] =
{
  "2022-04-01T14:37:34",
  "2022-04-01T14:37:34.9174375+00:00",
  "2022-04-01T14:37:34.1838288-07:00",
  "2022-04-01 14:37:34Z",
  "2022-04-01T14:37:34+00:00",
  "2022-04-01T14:37:34+09:30",
  "20220401T143734Z",
  NULL
};

/****************************************************************************************************
 * Functions
 ****************************************************************************************************/

// Not called since nothing to do
// int hcom_nx_exec_test_rtc_hardware_setup(void)
// {
//   return OK;
// }

//=================================================================
// Tests that the utc offset and fractional seconds have been correctly parsed.
static int VerifyISO8601UtcOffsetFracSec(int formatOff, int utcTimeOffset, double fracSec)
{
  // These tests are based on values defined above in the iso8601TestCases array
  switch(formatOff)
  {
    case 0:
      if(utcTimeOffset != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != 0.0) {syslog(1, "'%s' ERROR offset %d fracSec %f not 0.0\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 1:
      if(utcTimeOffset != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != .9174375) {syslog(1, "'%s' ERROR offset %d fracSec %f not .9174375\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 2:
      if(utcTimeOffset != -420) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not -420\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != -420) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != .1838288) {syslog(1, "'%s' ERROR offset %d fracSec %f not .1838288\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 3:
      if(utcTimeOffset != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not -420\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not -420\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != 0.0) {syslog(1, "'%s' ERROR offset %d fracSec %f not 0.0\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 4:
      if(utcTimeOffset != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n, formatOff",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n, formatOff",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != 0.0) {syslog(1, "'%s' ERROR offset %d fracSec %f not 0.0\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 5:
      if(utcTimeOffset != 570) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not +570\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 570) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not +570\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != 0.0) {syslog(1, "'%s' ERROR offset %d fracSec %f not 0.0\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    case 6:
      if(utcTimeOffset != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(meadow_rtc_get_utc_offset() != 0) {syslog(1, "'%s' ERROR offset %d utcTimeOffset %d not 0\n",
                iso8601TestCases[formatOff], formatOff, utcTimeOffset); return -1;}
      if(fracSec != 0.0) {syslog(1, "'%s' ERROR offset %d fracSec %f not 0.0\n",
                iso8601TestCases[formatOff], formatOff, fracSec); return -1;}
      break;
    default:
      syslog(1, "'%s' ERROR at default with offset %d\n", iso8601TestCases[formatOff], formatOff);
      break;
  }
  
  return OK;
}

//=================================================================
// Need to test this function for the above defined ISO 8601 formats
static int TestParsingISO8601DataTime(void)
{
  int ret;
  int formatOff = 0;
  struct tm tmResult;
  int utcTimeOffset;
  double fracSec;

  do
  {
    size_t testLen = strlen(iso8601TestCases[formatOff]);

    // This call returns date and time in a struct tm. It also returns the utc
    // offset and any fractional seconds
    ret = meadow_rtc_parse_iso8601_date_time(iso8601TestCases[formatOff], testLen, &tmResult,
              &utcTimeOffset, &fracSec);
    if(ret < 0)
    {
      syslog(1, "Test failed on '%s' element#:%d \n", iso8601TestCases[formatOff], formatOff + 1);
      return -1;
    }

    // Is the information right (compensate for unix time)?
    if(tmResult.tm_year + 1900 != 2022) {syslog(1, "ERROR in year:%d- '%s'\n",
              tmResult.tm_year + 1900, iso8601TestCases[formatOff]); return -1;}
    if(tmResult.tm_mon + 1 != 04)       {syslog(1, "ERROR in mon:%d - '%s'\n",
              tmResult.tm_mon + 1, iso8601TestCases[formatOff]); return -1;}
    if(tmResult.tm_mday != 01)          {syslog(1, "ERROR in mday:%d- '%s'\n",
              tmResult.tm_mday, iso8601TestCases[formatOff]); return -1;}
    if(tmResult.tm_hour != 14)          {syslog(1, "ERROR in hour:%d- '%s'\n",
              tmResult.tm_hour, iso8601TestCases[formatOff]); return -1;}
    if(tmResult.tm_min  != 37)          {syslog(1, "ERROR in min:%d - '%s'\n",
              tmResult.tm_min, iso8601TestCases[formatOff]); return -1;}
    if(tmResult.tm_sec  != 34)          {syslog(1, "ERROR in sec:%d - '%s'\n",
              tmResult.tm_sec, iso8601TestCases[formatOff]); return -1;}

    // Now verify the utc offset and fractional seconds
    ret = VerifyISO8601UtcOffsetFracSec(formatOff, utcTimeOffset, fracSec);
    if(ret < 0)
      return -1;

    formatOff++;
  } while (iso8601TestCases[formatOff] != NULL);
  
  syslog(1, "All %d ISO 8601 time tests Passed\n", formatOff);

  return OK;
}

//=================================================================
// These tests are for testing the power management implementation
int hcom_nx_exec_rtc_hardware_tests(struct hcom_nx_cmd_data *cmdData)
{
  uint32_t userData = cmdData->userData;
  int ret = OK;

  switch(userData)
  {
    // 60 - 69
    case 60:
      // Test ISO 8601 parsing feature for date, time, frac sec and utc offset
      ret = TestParsingISO8601DataTime();
      break;

    default:
    syslog(1, "Unknown value %u passed to RTC Tests()\n", userData);
    break;

  }
  return ret;
}

#endif    // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
