/****************************************************************************
 * configs/stm32f777zit6-meadow/src/kerneltests/meadow_calc_freq_dc_tests.c
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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
#include <meadow/hcom_shared_common.h>

#if MEADOW_INCLUDE_FREQ_DUTY_CYCLE_TESTS_IN_BUILD > 0

// Only build if configured
#if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)
#pragma message "(--) meadow_calc_freq_dc_tests.c"

#include "../hcom_nx/hcom_nx_common.h"
#include "specialized/meadow_calc_freq_dc.h"
#include "stm32_gpio.h"   // stm32_configgpio

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

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

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -d ?? comes here
void meadow_kt_calc_freq_dc_tests(uint32_t userData)
{
  // int ret;
  
  syslog(2, "Frequency and Duty Cycle tests received 'set developer -d 19 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
      break;

    default:
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/


#endif      // #if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)


#endif      // #if MEADOW_INCLUDE_FREQ_DUTY_CYCLE_TESTS_IN_BUILD > 0
