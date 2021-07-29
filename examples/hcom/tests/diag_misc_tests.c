/****************************************************************************
 * \apps\examples\hcom\tests\developer_tests.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#include <stdio.h>
#include "hcom_common.h"
#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
// static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if HCOM_INCLUDE_SNPRINTF_ON_NUTTX_TESTS_IN_BUILD > 0

// The snprintf return value can be an error or some value that represents
// the string produced. This group of tests will privide concrete examples
// to clarify the behavior as it differs across the internet.
// These tests should cover the possible outcomes.
// #1 - buffer larger that resulting string and terminating \0 (this is the ideal)
// #2 - buffer is exactly big enough for the resulting string and terminating \0
// #3 - buffer is exactly big enough for the resulting string but not terminating \0
// #4 - buffer is smaller than the resulting string by 1 byte
//
// Surprise - Nuttx did not follow the expected pattern. It always included a
// terminating NULL. Nuttx would truncate the final string to allow room for
// the NULL. The snprintf return value was always 14, the needed length for the
// text without the terminating NULL.
void diag_misc_tests_snprintf_on_nuttx(uint32_t userData)
{
  //               12345678901234567890
  char *testStr = "Hi userData: %d";
  char buffer[16];
  int bufLen;
  char *testDefn;
  int ret;

  memset(buffer, 0xff, 16);

  switch(userData)
  {
    case 1:
      bufLen = 16;
      testDefn = "Buf Large";
      break;

    case 2:
      bufLen = 15;
      testDefn = "Buf Exact";
      break;

    case 3:
      bufLen = 14;
      testDefn = "Buf 1 too small";
      break;

    case 4:
      bufLen = 13;
      testDefn = "Buf 2 too small";
      break;

    default:
      return;
  }

  // There are 3 ways to handle snprintf error return
  // This is the "normal" way to determine that the string is truncated
  // ret = snprintf(buffer, bufLen, testStr, userData);
  // if(ret >= bufLen)
  //   syslog(LOG_WARNING, "%s@%d snprintf buf too small need:%d\n", __FILE__, __LINE__, ret + 1);

  // This is a macro that doesn't return a result but tests and outputs a warning
  // if the string is truncated
  // snprintf_check(buffer, bufLen, testStr, userData);

  // This is a simple macro that calls a function hcom_common_utils_snprintf_chk()
  // That outputs a warning if the string is truncated and returns a negative
  // value on error.
  // Note: No where in the exiting Meadow code was snprintf's negative return
  // checked or processed.
  //
  ret = snprintf_chk(buffer, bufLen, testStr, userData);
  if(ret < 0)
  {
    syslog(1, "Error - snprintf test using snprintf_chk ret:%d\n", ret);
  }

  // Output results
  syslog(1, "snprintf test:%s, Buffer Len:%d snprintf ret:%d\n", testDefn, bufLen, ret);
  hcom_diag_print_buffer((uint8_t *)buffer, 16, 1);
}

#endif // #if HCOM_INCLUDE_SNPRINTF_ON_NUTTX_TESTS_IN_BUILD > 0
