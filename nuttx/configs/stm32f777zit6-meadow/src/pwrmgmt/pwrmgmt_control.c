/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_control.c
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

// This module controls the power management features (sleep modes) of the
// Meadow F7.

// It also calls functions that control the ESP32 sleep modes.

// Note: Nuttx has it's own power management implementation but after studying
// it, I decided to not use it because it made some assumptions about behavior
// that I thought were not in line with how Meadow was to operate. That said
// I did use the Nuttx implemention for "inspirition". Peter Moody 25Mar22

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"

#include "stm32_pwr.h"    // FOR TESTING

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include "pwrmgmt_local.h"

#include "chip/stm32f76xx77xx_pwr.h"
#include "chip/stm32_exti.h"
#include "nvic.h"

#include <arch/board/board.h>
#include "stm32_gpio.h"

#include "stm32f777zit6-meadow.h"
#include "hcom_nx/hcom_nx_common.h"

#include "stm32_alarm.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#warning PeterM added diagnostic code here

// Diagnostic only
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/
static char *thisFile = __FILE__;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

//==================================================================
// ISR indicating that the F7 is now awake
// static int meadow_isr_rtc_wakeup_handler_tests(int irq, FAR void *context,
//                                     FAR void *arg)
// {
//   uint32_t regval = 0;

// syslog(1, "===> Wakeup ISR executing\n");
//   rtc_wprunlock();

//   // Clear Wakeup timer flag
//   regval = getreg32(STM32_RTC_ISR);
//   regval &= ~RTC_ISR_WUTF;
//   putreg32(regval, STM32_RTC_ISR);

//   rtc_wprlock();

//   return OK;
// }

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function is called during initialization and is responsible for calling
// the other initialization function within this block of code.
int meadow_power_mgmt_initialize()
{
  int ret;

  syslog(1, "+++ Doing LSI calib, and switch clock as defined\n");

  // Initialize internals needed for the LSI clock to be used with RTC
  ret = pwrmgmt_init_lsi_calib();
  ret = pwrmgmt_init_rtc_clk_switch();

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_RED_LED);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_GREEN_LED);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_BLUE_LED);

  // // High turns leds off
  // DEBUG_SET_HIGH(DEBUG_PIN_V2_RED_LED);
  // DEBUG_SET_HIGH(DEBUG_PIN_V2_GREEN_LED);
  // DEBUG_SET_HIGH(DEBUG_PIN_V2_BLUE_LED);

  // DEBUG_SET_LOW(DEBUG_PIN_V2_RED_LED);

  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D06);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D07);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D08);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D09);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D10);

  // DEBUG_SET_LOW(DEBUG_PIN_V2_D06);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D07);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D08);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D09);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D10);
#endif    // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

  return ret;
}

//===============================================================
// The RGB LEDs use power too
int meadow_pwr_mgmt_turn_off_tri_color_leds()
{
  // Saves 0-6 ma depending on which leds are on
  stm32_gpiowrite(GPIO_LED_RED, true);
  stm32_gpiowrite(GPIO_LED_GREEN, true);
  stm32_gpiowrite(GPIO_LED_BLUE, true);

  return OK;
}

// /****************************************************************************
//  * Public Functions
//  ****************************************************************************/
//=========================================================
// Set RTC auto-reset for wakeupPeriod sec, switch to LSI, enter Stop-mode
int pwrmgmt_execute_stop_mode(uint16_t wakeupPeriod, bool useInterrupt)
{
  int ret;

  // Turn off tri-color LEDs as a power saving measure
  // meadow_pwr_mgmt_turn_off_tri_color_leds();

  // Switch to LSI clock
  // Note: this must be first because it does a backup domain reset which
  // will clear some of the register configured by following steps
  syslog(1, "==> EVENT-Switching to LSI clock\n");
  ret = meadow_pwr_mgmt_use_lsi_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    return ret;
  }

  // Delay so we can see the test thread display seconds
  usleep(2000 * 1000);

  syslog(1, "==> Disabling wakeup timer\n");
  usleep(20 * 1000);
  meadow_pwr_mgmt_disable_wakeup_timer();

  // Configure wakeup hardware and period
  syslog(1, "==> Setting up and starting wakeup timer\n");
  usleep(20 * 1000);
  ret = pwrmgmt_config_wakeup_timer(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    return ret;
  }

  // Enter stop mode and wait for specified time
  ret = pwrmgmt_enter_stop_mode(false, useInterrupt);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    return ret;
  }

  // The F7 must be awake for the thread to have gotten here
  
  // Disable Wakeup timer till needed again
  // The wakeup timer is designed to run forever
  // syslog(1, "==> Disable Wakeup timer\n");
  // usleep(20 * 1000);
  // meadow_pwr_mgmt_disable_wakeup_timer();

  syslog(1, "==> Switching back to HSE clock\n");
  usleep(20 * 1000);
  ret = meadow_pwr_mgmt_use_hse_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    return ret;
  }

  return OK;
}

//==============================================================
// Enter low-power mode for the period specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_for_seconds(time_t secondsTillAlarm)
{
  int ret;

  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "Error:'time(NULL)' call failed\n");
    return -ETIME;
  }

  time_t almTime = secondsTillAlarm + currentTime;

  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(almTime);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }
  
  return ret;
}

//==============================================================
// Enter low-power mode until the time specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(time_t almTime)
{
  int ret;
  struct tm tmAlarm;

  // Now convert alarm time to a future time in struct tm
  struct tm tmTemp;
  gmtime_r(&almTime, &tmTemp);
  memcpy(&tmAlarm, &tmTemp, sizeof(struct tm));

  // Set the alarm
  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_based_on_tm(tmAlarm);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }

  return OK;
}

//==================================================================
// Enter low-power mode until the time specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_based_on_tm(struct tm tmAlarm)
{
  int ret;

  // Alarm time must be in the future
  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "%s@%d-Error:time(NULL) call failed\n",thisFile, __LINE__);
    return -ETIME;
  }

  time_t almTime = mktime(&tmAlarm);
  if(almTime <= currentTime)
  {
    syslog(LOG_ERR, "Error:Alarm time before current time\n");
    return -ETIME;
  }

  struct alm_setalarm_s alminfo;

  alminfo.as_id = RTC_ALARMA; // or RTC_ALARMB
  alminfo.as_time = tmAlarm;  // Alarm time
  alminfo.as_cb = NULL;       // Callback
  alminfo.as_arg = NULL;      // Callback arguments

  ret = stm32_rtc_setalarm(&alminfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }
  
  return ret;
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
