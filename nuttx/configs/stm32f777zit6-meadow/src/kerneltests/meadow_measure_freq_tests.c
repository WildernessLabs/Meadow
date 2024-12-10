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
#if defined(CONFIG_FREQUENCY_DUTY_CYCLE_TESTS)
#pragma message "(--) meadow_measure_freq_tests.c"

#include <meadow/hcom_shared_common.h>
#include "specialized/meadow_measure_freq.h"
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
#define MEADOW_FREQ_TEST_PH10_D02  (uint8_t) (GPIO_INPUT | GPIO_PULLDOWN | \
          GPIO_PORTH | GPIO_PIN10)

// F7FeatherV2 PB6 is D04 connected to Timer4 16-bit, channel 1
#define MEADOW_FREQ_TEST_PB6_D08   (uint8_t)  (GPIO_INPUT | GPIO_PULLDOWN | \
          GPIO_PORTB | GPIO_PIN6)
// F7FeatherV2 PB7 is D07 connected to Timer4 16-bit, channel 2
#define MEADOW_FREQ_TEST_PB7_D07   (uint8_t)  (GPIO_INPUT | GPIO_PULLDOWN | \
          GPIO_PORTB | GPIO_PIN7)
// F7FeatherV2 PB8 is D03 connected to Timer4 16-bit, channel 3
#define MEADOW_FREQ_TEST_PB8_D03   (uint8_t)  (GPIO_INPUT | GPIO_PULLDOWN | \
          GPIO_PORTB | GPIO_PIN8)
// F7FeatherV2 PB9 is D04 connected to Timer4 16-bit, channel 4
#define MEADOW_FREQ_TEST_PB9_D04   (uint8_t)  (GPIO_INPUT | GPIO_PULLDOWN | \
          GPIO_PORTB | GPIO_PIN9)

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
// Timer 4 all 4 channels
static int meadow_frec_test_config_tim4_x4inputs(void)
{
  int ret;
  // Timer 4 channel 1
  ret = meadow_measure_freq_configure(4, 1, MEADOW_FREQ_TEST_PB6_D08);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:meadow_measure_freq_configure() ret:%ld\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // // Timer 4 channel 2
  // ret = meadow_measure_freq_configure(4, 2, MEADOW_FREQ_TEST_PB7_D07);
  // if(ret < 0)
  // {
  //   syslog(2, "%s@%d-Error:meadow_measure_freq_configure() ret:%ld\n",
  //             __FILE__, __LINE__, ret);
  //   return ret;
  // }

  // // Timer 4 channel 3
  // ret = meadow_measure_freq_configure(4, 3, MEADOW_FREQ_TEST_PB8_D03);
  // if(ret < 0)
  // {
  //   syslog(2, "%s@%d-Error:meadow_measure_freq_configure() ret:%ld\n",
  //             __FILE__, __LINE__, ret);
  //   return ret;
  // }

  // // Timer 4 channel 4
  // ret = meadow_measure_freq_configure(4, 4, MEADOW_FREQ_TEST_PB9_D04);
  // if(ret < 0)
  // {
  //   syslog(2, "%s@%d-Error:meadow_measure_freq_configure() ret:%ld\n",
  //             __FILE__, __LINE__, ret);
  //   return ret;
  // }

  return OK;
}

//===============================================================
// Display the frequency information
static void display_frequency_and_friends(mdwFreqReturnData_t mdwFreqReturnData)
{
  syslog(2, "Timer %lu, Channel:%lu - Freq:%6.2fHz, DC:%02.2f%%, AvgFreq:%6.2fHz, Input Count:%lu\n",
          mdwFreqReturnData.timerNumber, 
          mdwFreqReturnData.timerChannel, 
          ((double)mdwFreqReturnData.frequencyX1000)/1000.0,
          ((double)mdwFreqReturnData.dutyCycleX1000)/1000.0,
          ((double)mdwFreqReturnData.avgFreqX1000)/1000.0,
          mdwFreqReturnData.totalGpioPulses);
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -p 19 comes here
void meadow_kt_calc_freq_dc_tests(uint32_t userData)
{
  int ret;
  mdwFreqReturnData_t mdwFreqReturnData;

  syslog(2, "Frequency tests received 'set developer -d 19 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
      // Configure timer 4 with 4 inputs
      ret = meadow_frec_test_config_tim4_x4inputs();
      if(ret < 0)
        syslog(2, "%s@%d-Error meadow_freq_dc_test_tim4, ch1 ret:%ld\n",
                  __FILE__, __LINE__, ret);
      break;
      
    case 2:
      // View data timer 4 all channels
      mdwFreqReturnData.timerNumber = 4;
      mdwFreqReturnData.timerChannel = 1;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      if(ret < 0)
        syslog(2, "%s@%d-Error meadow_measure_freq_return_freq_info() ret:%ld\n",
                  __FILE__, __LINE__, ret);
      display_frequency_and_friends(mdwFreqReturnData);

      // mdwFreqReturnData.timerChannel = 2;
      // ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      // if(ret < 0)
      //   syslog(2, "%s@%d-Error meadow_measure_freq_return_freq_info() ret:%ld\n",
      //             __FILE__, __LINE__, ret);
      // display_frequency_and_friends(mdwFreqReturnData);

      // mdwFreqReturnData.timerChannel = 3;
      // ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      // if(ret < 0)
      //   syslog(2, "%s@%d-Error meadow_measure_freq_return_freq_info() ret:%ld\n",
      //             __FILE__, __LINE__, ret);
      // display_frequency_and_friends(mdwFreqReturnData);

      // mdwFreqReturnData.timerChannel = 4;
      // ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      // if(ret < 0)
      //   syslog(2, "%s@%d-Error meadow_measure_freq_return_freq_info() ret:%ld\n",
      //             __FILE__, __LINE__, ret);
      // display_frequency_and_friends(mdwFreqReturnData);
      break;

    // case 3:
    //   // Configure timer 8
    //   ret = meadow_freq_dc_test_configure_16_Tim11_PB8();
    //     syslog(2, "Error meadow_freq_dc_test_configure_16_Tim11_PB8() ret:%ld\n", ret);
    //   if(ret < 0)
    //   break;

    // case 4:
    //   // View data timer 8
    //   mdwFreqReturnData.timerNumber = 8;
    //   mdwFreqReturnData.timerChannel = 1;
    //   ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
    //   if(ret < 0)
    //     syslog(2, "Error meadow_measure_freq_return_freq_info() ret:%ld\n", ret);
    //   display_frequency_and_friends(mdwFreqReturnData);
    //   break;

    default:
      syslog(2, "Undefined test for meadow_kt_calc_freq_dc_tests, userData:%lu. NO TEST DEFINED\n", userData);
      break;
  }
}

#endif      // #if defined(CONFIG_FREQUENCY_DUTY_CYCLE_TESTS)
