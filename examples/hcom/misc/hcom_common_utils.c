/****************************************************************************
 * \apps\examples\hcom\misc\hcom_common_utils.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#include <ctype.h>
#include "hcom_common.h"

#include <nuttx/config.h>
#include "syslog.h"

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
int hcom_common_utils_setup()
{
  return OK;
}

//============================================================================
void hcom_common_utils_shutdown()
{
}

//===================================================================
// Returns the current time as a 64-bit number representing nanosec.
// Used for testing. Note: Only millisecond resolution.
uint64_t hcom_utils_get_current_time64(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}

//===================================================================
// Due to the number of places snprintf is called and the code required
// to determine success or failure. This function is designed so that
// users can generate less code and be confident that truncated are noted
// A macro that adds file name and line number exists
int hcom_common_utils_snprintf_chk(FAR char *buf, size_t size, char *fileName, int lineNumb,
          FAR const IPTR char *fmt, ...)
{
  int bufChk;
  va_list ap;

  va_start(ap, fmt);

  // Process the string
  bufChk = vsnprintf(buf, size, fmt, ap);
  va_end(ap);

  // Handle buffer overflow here
  if(bufChk >= size)
  {
    hcom_logging_syslog(LOG_WARNING, "%s@%d Host msg truncated, need:%d\n", fileName, lineNumb, bufChk + 1);
    // This modifies the standard Nuttx snprintf behavior which would normally
    // return the size of needed buffer.
    return -ENAMETOOLONG;
  }
  else if(bufChk < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d snprintf returned an error, ret:%d\n", fileName, lineNumb, bufChk);
  }

  // Must be operations as usual
  return bufChk;
}
