/****************************************************************************
 * \apps\examples\hcom\tests\diag_misc_test.c
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
#define HCOM_OVERLOAD_CYCLE_TIME_MS (100)
#define HCOM_OVERLOAD_CYCLE_TIME_NS (HCOM_OVERLOAD_CYCLE_TIME_MS * 1000000)

// The timing can be seen on an oscilloscope if desired
#define HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT 0
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
 #include "diag/hcom_diag_gpio.h"
#endif

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static bool _firstTime = true;
static bool _keepRunning = true;
static int _overload_pid;
static int _overload_percent;
static int _nx_access_fd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int overload_main(int argc, char *argv[]);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This code will create a task on first invocation and if userData
// equals 1000 will kill the task.
// Otherwise, userData values of 0-100 will determine the percentage of time
// the task will consume 100% of CPU. This task will enter a loop that takes
// about 100 msec to execute. It will sleep and overload the MCU for the
// an amount of time based on the userData value.
// The created tasks priority is set at 100, which is not that high.
void diag_misc_tests_overload_mcu(uint32_t userData)
{
  // Since the userData number is here used as millseconds we need to protect
  // against over run.
  if(userData == 1000)
  {
    // Stop the task
    _keepRunning = false;
    _firstTime = true;
    syslog(2, "AP-In Overload Test-Stopping the Overload Thread\n");
    return;
  }

  if(userData > 100)
  {
    syslog(2, "AP-The entered value %u is out of the 0-100 range, 1000 to exit\n",
              userData);
    return;
  }

  // Save the new overload percentage
  _overload_percent = userData;
  syslog(2, "AP-In Overload Test-Target overload %u msec, runtime:%u msec\n",
      _overload_percent, HCOM_OVERLOAD_CYCLE_TIME_MS - userData);
  
  if(_firstTime)
  {
    _firstTime = false;
    
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
    // Configure GPIOs
    stm32_configgpio(DEBUG_PIN_V2_A1);
    stm32_configgpio(DEBUG_PIN_V2_A2);
#endif

    // Create a unique task for Overload
    _overload_pid = task_create("overload",
                            100,        // Priority
                            1024,       // Stack size
                            (main_t)overload_main,
                            (FAR char * const *) NULL);
    if(_overload_pid > 0)
    {
      syslog(LOG_INFO, "%s@%d-Overload Test thread now running [pid:%d, pri:%d, stack:%d]\n",
              __FILE__, __LINE__, _overload_pid, 100, 1024);
    }
    else
    {
      syslog(LOG_ERR, "%s@%d-Overload Test task creation failed\n",
              __FILE__, __LINE__);
    }
  }
}

//---------------------------------------------------
// Endless loop
#pragma GCC push_options
#pragma GCC optimize("O0")    // Prevent compiler from changing the code

int overload_main(int argc, char *argv[])
{
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
  // For GPIO output, must uniquely open upd driver because we created new
  // thread and the normal upd driver handle is opened by the HCOM task.
  _nx_access_fd = hcom_via_nx_upd_driver_open();
  if (_nx_access_fd < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error: Opening hcom nx access failed:%d\n",
              __FILE__, __LINE__, _nx_access_fd);
    return 0;
  }
#endif

  while(_keepRunning)
  {
    uint64_t startTime = hcom_utils_get_current_time64_ns();
    uint64_t stopTime = startTime + HCOM_OVERLOAD_CYCLE_TIME_NS;
    
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
    stm32_gpiowrite(DEBUG_PIN_V2_A1, true);
#endif
    usleep(_overload_percent * 1000);
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
    stm32_gpiowrite(DEBUG_PIN_V2_A1, false);
#endif
    
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
    stm32_gpiowrite(DEBUG_PIN_V2_A2, true);
#endif
    while(hcom_utils_get_current_time64_ns() < stopTime)
    {
      // Optimizing compiler may remove this loop
    }
#if (HCOM_OVERLOAD_INCLUDE_GPIO_OUTPUT > 0)
    stm32_gpiowrite(DEBUG_PIN_V2_A2, false);
#endif
  }

  // May want to restart
  _keepRunning = true;

  close(_nx_access_fd);
  syslog(2, "AP-In Overload Test-Exited Overload Thread\n");
  return 0;
}

#pragma GCC pop_options

//============================================================
// The snprintf return value can be an error or some value that represents
// the string produced. This group of tests will provide concrete examples
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
    syslog(2, "AP-Error - snprintf test using snprintf_chk ret:%d\n", ret);
  }

  // Output results
  syslog(2, "AP-snprintf test:%s, Buffer Len:%d snprintf ret:%d\n", testDefn, bufLen, ret);
  hcom_diag_print_buffer((uint8_t *)buffer, 16, 1);
}

