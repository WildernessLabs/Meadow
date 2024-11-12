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
#include <nuttx/config.h>

// Only build if configured
#if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)
#pragma message "(--) meadow_calc_freq_dc_tests.c"

#include <meadow/hcom_shared_common.h>
#include "specialized/meadow_calc_freq_dc.h"
#include "../hcom_nx/hcom_nx_common.h"
#include "stm32_gpio.h"   // stm32_configgpio

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#define MEADOW_FREQ_DC_TEST_PH10   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTH | GPIO_PIN10)
#define MEADOW_FREQ_DC_TEST_PB8    (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN8)

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
 * Private Functions
 ************************************************************************************/
// Test code for gated frequency and pulse width
int meadow_calc_freq_dc_test_see_data(int timerNumb)
{
  // Just feed pulse train into appropriate GPIO  
  struct freqDcTimerInfo_s *freqDcTimerInfo = meadow_calc_freq_dc_get_timer_info_pointer(timerNumb);
  struct freqDcData_s *freqDcData = (struct freqDcData_s *)freqDcTimerInfo->dataPtr;

  uint32_t validCheckCount = 0;

  // These are so once a valid value is found, a change in the timers data
  // structure won't affect the output.
  uint32_t fullPeriod;
  uint32_t partPeriod;

  // Find valid data. This is only an issue at higher frequencies.
  for(validCheckCount = 0; validCheckCount < 5; validCheckCount++)
  {
    fullPeriod = freqDcData->timerFullPeriod;
    partPeriod = freqDcData->timerPartPeriod;
    if(fullPeriod > 0 && partPeriod > 0)
      break;

    usleep(1 * 1000);   // delay 1 - 2 ms waiting for better data
  }

  if(fullPeriod > 0 && partPeriod > 0)
  {
    // Do floating point math then convert to integer times 1000
    double dutyCycle = (double)(partPeriod * 100.0) / (double)fullPeriod;
    double freq = (double)(MEADOW_FREQ_DC_CLOCK_FREQ)/ \
              (double)fullPeriod;

    uint32_t iDutyCycle = (dutyCycle * 1000.0);
    uint32_t iFrequency = (freq * 1000.0);

    syslog(2, "Freq:%06.4fHz [%lu], DC:%02.2f%% [%lu], CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
              freq, iFrequency, dutyCycle, iDutyCycle, fullPeriod, partPeriod, validCheckCount);
  }
  else
  {
    syslog(2, "Invalid data                   CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
              fullPeriod, partPeriod, validCheckCount);
  }

  // Prevent this count from being used when there's no input.
  freqDcData->timerFullPeriod = 0;
  freqDcData->timerPartPeriod = 0;
  return OK;
}

//===============================================================
// Configure 32-bit timer
static int meadow_freq_dc_test_configure_32_Tim5_PH10(void)
{
  int ret = meadow_calc_freq_dc_freq_duty_config(5,   // Timer 5 D02 (32-bit)
            MEADOW_FREQ_DC_TEST_PH10,
            0);                                   // Leading 0 = rising, 1 = falling

  if(ret < 0)
  {
    syslog(2, "Error:meadow_calc_freq_dc_freq_duty_config() ret:%ld\n",  ret);
    return ret;
  }

  return OK;
}

//===============================================================
// Configure 16-bit timer
static int meadow_freq_dc_test_configure_16_Tim11_PB8(void)
{
  int ret = meadow_calc_freq_dc_freq_duty_config(5,   // Timer 4 D08 (16-bit)
            MEADOW_FREQ_DC_TEST_PH10,
            0);                                   // Leading 0 = rising, 1 = falling

  if(ret < 0)
  {
    syslog(2, "Error:meadow_calc_freq_dc_freq_duty_config() ret:%ld\n",  ret);
    return ret;
  }

  return OK;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -d ?? comes here
void meadow_kt_calc_freq_dc_tests(uint32_t userData)
{
  int ret;
  
  syslog(2, "Frequency and Duty Cycle tests received 'set developer -d 19 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
      // See the converted measured frequency and duty cycle information
      ret = meadow_calc_freq_dc_test_see_data(userData);
      if(ret < 0)
        syslog(2, "Error meadow_calc_freq_dc_test_see_data() ret:%ld\n", ret);
      break;
      
    case 2:
      ret = meadow_freq_dc_test_configure_32_Tim5_PH10();
      if(ret < 0)
        syslog(2, "Error meadow_freq_dc_test_configure_32_Tim5_PH10() ret:%ld\n", ret);
      break;

    case 3:
      ret = meadow_freq_dc_test_configure_16_Tim11_PB8();
        syslog(2, "Error meadow_freq_dc_test_configure_16_Tim11_PB8() ret:%ld\n", ret);
      if(ret < 0)
      break;

    default:
      syslog(2, "Undefined test for meadow_kt_quick_misc_tests, userData:%lu. NO TEST DEFINED\n", userData);
      break;
  }
}

#endif      // #if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)
