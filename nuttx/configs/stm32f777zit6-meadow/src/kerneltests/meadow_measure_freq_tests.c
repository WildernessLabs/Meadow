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
#if defined(CONFIG_MEASURE_FREQUENCY_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
// #pragma message "(--) meadow_measure_freq_tests.c"

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>
#include <meadow/meadow_measure_freq_shared.h>
#include "../hcom_nx/hcom_nx_common.h"
#include "stm32_gpio.h"   // stm32_configgpio
#include <arch/board/board.h>

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// Set 1 to use UART for syslog output. The specific UART can be changed by
// using HCOM_DIAG_SYSLOG_UART_NUMBER in
// /nuttx/include/meadow/hcom_shared_common.h.
// Set to 0 to send directly to CLI where the frequency measurement tests are
// initiated.
 #define MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT (0)

// F7FeatherV2 PH10 is D02 connected to Timer 5 32-bit, channel 1. This is the
// only exposed 32-bit timer pin on F7FeatherV2. Timer 2 is also 32-bits but
// used to control RGB LED
// #define MEADOW_FREQ_TEST_PH10_D02  (uint8_t) GPIO_PORTH | GPIO_PIN10

// F7FeatherV2 exposes Timer 3 and Timer 4 and all 4 channels are exposed on
// both.
// F7FeatherV2 PB6 is D08 connected to Timer4 16-bit, channel 1
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
// Configure only available 32-bit timer 5
// static int meadow_freq_dc_test_configure_32_Tim5_PH10(void)
// {
//   int ret = meadow_measure_freq_configure(5,  // Timer 5 D02 (32-bit)
//             1,                                // Channel
//             MEADOW_FREQ_TEST_PH10_D02);

//   if(ret < 0)
//   {
//     syslog(LOG_MTEST, "Error:meadow_measure_freq_configure() ret:%ld\n",
//       ret);
//     return ret;
//   }

//   return OK;
// }

//===============================================================
#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT == 0)
// Send syslog like messages to CLI. This is needed because for Issue #842
// we need to use PB14 and PB15 to do frequency measurements. But, these are
// the pins used for syslog.
static void syslogToHost(int priority, FAR const IPTR char *fmt, ...)
{
  size_t maxStringLen = 256;
  char * finalString = malloc(maxStringLen);
 
  va_list args;
  va_start(args, fmt);

  // Create the complete message with prefix
  // The Nuttx version of snprintf will truncate the string based on the
  // buffer size but will always place a terminating NULL at the end.
  int stringLen = vsnprintf(finalString, maxStringLen - 1, fmt, args);
  
  hcom_nx_route_text_to_host(HCOM_HOST_REQUEST_TEXT_INFORMATION,
    finalString, stringLen);

  va_end(args);

  free(finalString);
}
#endif

//===============================================================
// Display the frequency information
static void display_frequency_and_friends(
          mdwFreqReturnData_t mdwFreqReturnData, int ret)
{
  if(ret < 0)
  {
#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
    syslog(LOG_MTEST, "Timer %lu, Channel:%lu - Error %d\n",
            mdwFreqReturnData.timerNumber, 
            mdwFreqReturnData.channelNumber,
            ret);
#else
    syslogToHost(LOG_MTEST, "Timer %lu, Channel:%lu - Error %d\n",
            mdwFreqReturnData.timerNumber, 
            mdwFreqReturnData.channelNumber,
            ret);
#endif
    return;
  }

#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
  syslog(LOG_MTEST,
    "Timer %lu, Channel:%lu - Freq:%6.2fHz, DC:%02.2f%%, AvgFreq:%6.2fHz, Input Count:%lu\n",
    mdwFreqReturnData.timerNumber, 
    mdwFreqReturnData.channelNumber,
    ((double)mdwFreqReturnData.frequencyX1000)/1000.0,
    ((double)mdwFreqReturnData.dutyCycleX1000)/1000.0,
    ((double)mdwFreqReturnData.avgFreqX1000)/1000.0,
    mdwFreqReturnData.gpioCountForAvg);
#else  
  syslogToHost(LOG_MTEST,
    "Timer %lu, Channel:%lu - Freq:%6.2fHz, DC:%02.2f%%, AvgFreq:%6.2fHz, Input Count:%lu\n",
    mdwFreqReturnData.timerNumber, 
    mdwFreqReturnData.channelNumber,
    ((double)mdwFreqReturnData.frequencyX1000)/1000.0,
    ((double)mdwFreqReturnData.dutyCycleX1000)/1000.0,
    ((double)mdwFreqReturnData.avgFreqX1000)/1000.0,
    mdwFreqReturnData.gpioCountForAvg);
#endif
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

#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
  syslog(LOG_MTEST, "meadow_kt_measure_freq_tests 'set developer -p 19 -v %lu'\n",
            userData);
#else
  syslogToHost(LOG_MTEST, "meadow_kt_measure_freq_tests 'set developer -p 19 -v %lu'\n",
            userData);
#endif

  // userData 4 digits
  // 1st digit = action
  //  1=config no DC
  //  2=config with DC
  //  3=unconfigure
  //  4=display data
  // 2nd & 3rd digits = timer number (01-14)
  // 4th digit = channel number (1-4)
  switch(userData)
  {
    case 1:
      syslog(LOG_MTEST, "%s@%d-Invalid test:%lu\n",
        __FILE__, __LINE__, userData);
      break;
    
    //--------------------------------------------------------------
    // Create - No Duty Cycle  '1'
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
    // Create - Use duty cycle  '2'
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
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4042:
      // Timer 4 channel 2
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 2;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4043:
      // Timer 4 channel 3
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 3;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4044:
      // Timer 4 channel 4
      mdwFreqReturnData.timerNumber   = 4;
      mdwFreqReturnData.channelNumber = 4;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;

    //--------------------------------------------------------------
    // Create - No Duty Cycle  '1'
    // Issue #842 channel data 'ProjLab 3e'
    case 1121:
      // Timer 12 channel 1
      mdwCfgTimerChan.timerNumber   = 12;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = GPIO_TIM12_CH1IN_1; // PB14
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1122:
      // Timer 12 channel 2
      mdwCfgTimerChan.timerNumber   = 12;
      mdwCfgTimerChan.channelNumber = 2;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = GPIO_TIM12_CH2IN_1; // PB15
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 5121:    // 5 because uses GPIO not specified by Meadow in build.h
      // Timer 12 channel 1 but uses PH6. On ProjLab3e, it is exposed on
      // mikroBUS 1 SCK of ProjLab3e.
      mdwCfgTimerChan.timerNumber   = 12;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 1;
      mdwCfgTimerChan.portAndPin    = GPIO_TIM12_CH1IN_2; // PH6
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1101:
      // Timer 10 channel 1
      mdwCfgTimerChan.timerNumber   = 10;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 1;
     // F7Featherv2 D03
      mdwCfgTimerChan.portAndPin    = GPIO_TIM10_CH1IN_1; // PB8
      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;
    case 1051:
      // Timer 5 channel 1
      mdwCfgTimerChan.timerNumber   = 5;
      mdwCfgTimerChan.channelNumber = 1;
      mdwCfgTimerChan.configOption  = 1;
      // F7Featherv2 D02
      mdwCfgTimerChan.portAndPin    = GPIO_TIM5_CH1IN_2;  // PH10 

      ret = meadow_measure_freq_configure(&mdwCfgTimerChan);
      break;

    //--------------------------------------------------------------
    // Display
    // Issue #842 channel data 'ProjLab 3e'
    case 4121:
      // Timer 12 channel 1
      mdwFreqReturnData.timerNumber   = 12;
      mdwFreqReturnData.channelNumber = 1;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4122:
      // Timer 12 channel 2
      mdwFreqReturnData.timerNumber   = 12;
      mdwFreqReturnData.channelNumber = 2;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4101:
      // Timer 10 channel 1
      mdwFreqReturnData.timerNumber   = 10;
      mdwFreqReturnData.channelNumber = 1;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
    case 4051:
      // Timer 5 channel 2
      mdwFreqReturnData.timerNumber   = 5;
      mdwFreqReturnData.channelNumber = 1;
      ret = meadow_measure_freq_return_freq_info(&mdwFreqReturnData);
      display_frequency_and_friends(mdwFreqReturnData, ret);
      break;
      
    default:
#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
      syslog(LOG_MTEST, "meadow_measure_freq_tests, no test:%lu\n",
        userData);
#else
    syslogToHost(LOG_MTEST, "meadow_measure_freq_tests, no test:%lu\n",
      userData);
#endif
      break;
  }

  // If error, display it
  if(ret < 0)
  {
    char *errorStr;
    switch(ret)
    {
      case MEADOW_MEAS_FREQ_CONF_UNDEFINED_OPTION:
        errorStr = "CONF_UNDEFINED_OPTION";
        break;
      case MEADOW_MEAS_FREQ_CONF_TIM_NUMB_ILLEGAL:
        errorStr = "CONF_TIM_NUMB_ILLEGAL";
        break;
      case MEADOW_MEAS_FREQ_CONF_CHAN_NUMB_ILLEGAL:
        errorStr = "CONF_CHAN_NUMB_ILLEGAL";
        break;
      case MEADOW_MEAS_FREQ_CONF_PORT_PIN_NOT_FOR_TIM:
        errorStr = "CONF_PORT_PIN_NOT_FOR_TIM";
        break;
      case MEADOW_MEAS_FREQ_CONF_PORT_PIN_TIM_CHAN_INVALID:
        errorStr = "CONF_PORT_PIN_TIM_CHAN_INVALID";
        break;
      case MEADOW_MEAS_FREQ_CONF_TIM_NOT_USABLE:
        errorStr = "CONF_TIM_NOT_USABLE";
        break;
      case MEADOW_MEAS_FREQ_CONF_TIM_CHAN_IN_USE:
        errorStr = "CONF_TIM_CHAN_IN_USE";
        break;
      case MEADOW_MEAS_FREQ_CONF_CHAN_MEM_ALLOC_FAILED:
        errorStr = "CONF_CHAN_MEM_ALLOC_FAILED";
        break;
      case MEADOW_MEAS_FREQ_CONF_CONFIGGPIO_ERR:
        errorStr = "CONF_CONFIGGPIO_ERR";
        break;
      case MEADOW_MEAS_FREQ_CONF_INIT_CHAN_HW_FAIL:
        errorStr = "CONF_INIT_CHAN_HW_FAIL";
        break;
      case MEADOW_MEAS_FREQ_CONF_INIT_TIM_HW_FAIL:
        errorStr = "CONF_INIT_TIM_HW_FAIL";
        break;
      case MEADOW_MEAS_FREQ_READ_NO_CHANS_ACTIVITY:
        errorStr = "READ_NO_CHANS_ACTIVITY";
        break;
      case MEADOW_MEAS_FREQ_READ_INVALID_TIMER_NUMB:
        errorStr = "READ_INVALID_TIMER_NUMB";
        break;
      case MEADOW_MEAS_FREQ_READ_INVALID_CHANNEL_NUMB:
        errorStr = "READ_INVALID_CHANNEL_NUMB";
        break;
      case MEADOW_MEAS_FREQ_READ_TIMER_ACCESS_NULL:
        errorStr = "READ_TIMER_ACCESS_NULL";
        break;
      case MEADOW_MEAS_FREQ_READ_CHAN_NOT_CONFIG:
        errorStr = "READ_CHAN_NOT_CONFIG";
        break;
      case MEADOW_MEAS_FREQ_READ_CHANNEL_DATA_NULL:
        errorStr = "READ_CHANNEL_DATA_NULL";
        break;
      case MEADOW_MEAS_FREQ_READ_NO_CHANS_ACTIVE:
        errorStr = "READ_NO_CHANS_ACTIVE";
        break;
      case MEADOW_MEAS_FREQ_READ_FREQ_INPUT_NOT_DETECTED:
        errorStr = "READ_FREQ_INPUT_NOT_DETECTED";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_INVALID_TIMER_NUMB:
        errorStr = "UNCFG_INVALID_TIMER_NUMB";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_INVALID_CHANNEL_NUMB:
        errorStr = "UNCFG_INVALID_CHANNEL_NUMB";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_TIMER_ACCESS_NULL:
        errorStr = "UNCFG_TIMER_ACCESS_NULL";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_CHAN_NOT_CONFIG:
        errorStr = "UNCFG_CHAN_NOT_CONFIG";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_NO_CHANNEL:
        errorStr = "UNCFG_NO_CHANNEL";
        break;
      case MEADOW_MEAS_FREQ_UNCFG_IRQ_DETACH_ERR:
        errorStr = "UNCFG_IRQ_DETACH_ERR";
        break;
      default:
        errorStr = "Unknown error";
        break;
    }

#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
    syslog(LOG_MTEST, "%s@%d-Failure: Test:%lu, ret:%d (%s)\n",
              __FILE__, __LINE__, userData, ret, errorStr);
#else
    syslogToHost(LOG_MTEST, "%s@%d-Failure: Test:%lu, ret:%d (%s)\n",
              __FILE__, __LINE__, userData, ret, errorStr);
#endif
  }
  else
  {
    char *successStr;

    if(ret == MEADOW_MEAS_FREQ_CONF_SUCCESSFUL)
    {
      successStr = "CONF_SUCCESSFUL";
    }
    else if(ret == MEADOW_MEAS_FREQ_READ_SUCCESSFUL)
    {
      successStr = "READ_SUCCESSFUL";
    }
    else if(ret == MEADOW_MEAS_FREQ_UNCFG_SUCCESSFUL)
    {
      successStr = "UNCFG_SUCCESSFUL";
    }
    else
    {
      successStr = "Unknown return value";
    }

#if (MEADOW_FREQ_MEAS_TEST_USE_SYSLOG_OUTPUT > 0)
    syslog(LOG_MTEST, "%s@%d-Success: Test:%lu, ret:%d (%s)\n",
              __FILE__, __LINE__, userData, ret, successStr);
#else
    syslogToHost(LOG_MTEST,  "%s@%d-Success: Test:%lu, ret:%d (%s)\n",
              __FILE__, __LINE__, userData, ret, successStr);
#endif
  }
}

#endif      // #if defined(CONFIG_MEASURE_FREQUENCY_TESTS)
