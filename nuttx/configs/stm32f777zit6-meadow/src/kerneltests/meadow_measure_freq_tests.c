/****************************************************************************
 * configs/stm32f777zit6-meadow/src/kerneltests/meadow_measure_freq_tests.c
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
#if defined(CONFIG_MEASURE_FREQUENCY_TESTS)
#pragma message "(--) meadow_measure_freq_tests.c"

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>
#include <meadow/meadow_measure_freq_shared.h>
#include "../hcom_nx/hcom_nx_common.h"
#include "stm32_gpio.h"   // stm32_configgpio

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// F7FeatherV2 PH10 is D02 connected to Timer5 32-bit, channel 1
#define MEADOW_FREQ_TEST_PH10_D02  (uint8_t) GPIO_PORTH | GPIO_PIN10

// F7FeatherV2 PB6 is D04 connected to Timer4 16-bit, channel 1
#define MEADOW_FREQ_TEST_PB6_D08   (uint8_t) GPIO_PORTB | GPIO_PIN6
// F7FeatherV2 PB7 is D07 connected to Timer4 16-bit, channel 2
#define MEADOW_FREQ_TEST_PB7_D07   (uint8_t) GPIO_PORTB | GPIO_PIN7
// F7FeatherV2 PB8 is D03 connected to Timer4 16-bit, channel 3
#define MEADOW_FREQ_TEST_PB8_D03   (uint8_t) GPIO_PORTB | GPIO_PIN8
// F7FeatherV2 PB9 is D04 connected to Timer4 16-bit, channel 4
#define MEADOW_FREQ_TEST_PB9_D04   (uint8_t) GPIO_PORTB | GPIO_PIN9

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
// Configure 32-bit timer
// static int meadow_freq_dc_test_configure_32_Tim5_PH10(void)
// {
//   int ret = meadow_measure_freq_configure(5,  // Timer 5 D02 (32-bit)
//             1,                                // Channel
//             MEADOW_FREQ_TEST_PH10_D02);

//   if(ret < 0)
//   {
//     syslog(2, "Error:meadow_measure_freq_configure() ret:%ld\n",  ret);
//     return ret;
//   }

//   return OK;
// }

//===============================================================
// Configure 16-bit timer 4
// Timer 4 all 4 channels, no duty cycle
static void meadow_freq_test_config_tim4_x4inputs(void)
{
  // Timer 4 channel 1
  meadow_kt_measure_freq_tests(10411);

  // Timer 4 channel 2
  meadow_kt_measure_freq_tests(10421);

  // Timer 4 channel 3
  meadow_kt_measure_freq_tests(10431);
  
  // Timer 4 channel 4
  meadow_kt_measure_freq_tests(10441);
}

//===============================================================
// Display the frequency information
static void display_frequency_and_friends(mdwFreqReturnData_t mdwFreqReturnData)
{
  syslog(2, "Timer %lu, Channel:%lu - Freq:%6.2fHz, DC:%02.2f%%, AvgFreq:%6.2fHz, Input Count:%lu\n",
          mdwFreqReturnData.timerNumber, 
          mdwFreqReturnData.channelNumber,
          ((double)mdwFreqReturnData.frequencyX1000)/1000.0,
          ((double)mdwFreqReturnData.dutyCycleX1000)/1000.0,
          ((double)mdwFreqReturnData.avgFreqX1000)/1000.0,
          mdwFreqReturnData.gpioCountForAvg);
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -p 19 comes here (developer -p 20 goes to the app-side)
void meadow_kt_measure_freq_tests(uint32_t userData)
{
  int ret = OK;

  mdwFreqReturnData_t mdwFreqReturnData;
  mdwFreqCfgTimer_t mdwCfgTimerChan;

  syslog(2, "meadow_kt_measure_freq_tests 'set developer -d 19 -v %lu'\n",
            userData);

  switch(userData)
  {
    case 1:
      syslog(2, "%s@%d-Invalid test:%lu\n", __FILE__, __LINE__, userData);
      break;
    
    // The 4 digits
    // 1st = action (1=config no DC, 2=config with DC, 3=unconfigure,
    //      4=display data)
    // 2nd & 3rd = timer number (01-14)
    // 4th = channel (1-4)
    //--------------------------------------------------------------
    // Create No Duty Cycle  '1'
    case 1041:
      // Timer 4 channel 1
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB6_D08;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1042:
      // Timer 4 channel 2
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 2;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB7_D07;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1043:
      // Timer 4 channel 3
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 3;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB8_D03;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1044:
      // Timer 4 channel 4
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 4;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB9_D04;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;

    //--------------------------------------------------------------
    // Create Use duty cycle  '2'
    case 2041:
      // Timer 4 channel 1
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 2;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB6_D08;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 2042:
      // Timer 4 channel 2
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 2;
      mdwCfgTimerChan.configOption  = 2;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB7_D07;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 2043:
      // Timer 4 channel 3
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 3;
      mdwCfgTimerChan.configOption  = 2;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB8_D03;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 2044:
      // Timer 4 channel 4
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 4;
      mdwCfgTimerChan.configOption  = 2;
      mdwCfgTimerChan.portAndPin    = MEADOW_FREQ_TEST_PB9_D04;
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;

    //------------------------------------------------------
    // Unconfigure channel '3'
    // ( also removes timer when no channels left)
    case 3041:
      // Timer 4 channel 1
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 3;    // Unconfigure
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 3042:
      // Timer 4 channel 2
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 2;
      mdwCfgTimerChan.configOption  = 3;    // Unconfigure
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 3043:
      // Timer 4 channel 3
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 3;
      mdwCfgTimerChan.configOption  = 3;    // Unconfigure
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 3044:
      // Timer 4 channel 4
      mdwCfgTimerChan.timerNumber   = 4;
      mdwCfgTimerChan.channelNumber = 4;
      mdwCfgTimerChan.configOption  = 3;    // Unconfigure
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;

    //--------------------------------------------------------------
    // Display channel data '4'
    case 4041:
      // Timer 4 channel 1
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 1;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData);
      break;
    case 4042:
      // Timer 4 channel 2
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 2;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData);
      break;
    case 4043:
      // Timer 4 channel 3
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 3;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData);
      break;
    case 4044:
      // Timer 4 channel 4
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 4;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData);
      break;

    default:
      syslog(2, "meadow_measure_freq_tests, no test:%lu\n", userData);
      break;
  }

  // Display error encountered
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error: Test:%lu, ret:%d\n",
              __FILE__, __LINE__, userData, ret);
  }
}

#endif      // #if defined(CONFIG_MEASURE_FREQUENCY_TESTS)
