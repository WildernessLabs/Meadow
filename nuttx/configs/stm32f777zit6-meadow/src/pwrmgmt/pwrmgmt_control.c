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

#include "../espcp/espcp_coprocessor.h"

#include "stm32_alarm.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#if !defined (CONFIG_ARCH_IDLE_CUSTOM)
#error "CONFIG_MEADOW_PWR_MGMT_SUPPORT requires CONFIG_ARCH_IDLE_CUSTOM"
#endif

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/

static char *thisFile = __FILE__;
static uint32_t _rgbLedState;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// This function modifies the idle threads behavior by prevent it from calling
// the WFI or WFE op codes until the stop-mode has completed. If this isn't
// done, when the configuration for stop mode was incomplete the MCU can be
// locked up.
static void pwrmgmt_idle_behavior_control(bool allowWaitOp)
{
  irqstate_t flags;

  flags = enter_critical_section();
  up_idle_pwrmgmt_set_idle_behavior(allowWaitOp);
  leave_critical_section(flags);
}

//===============================================================
// Return the tri-color leds to orginal state
static void pwrmgmt_tri_color_leds_restore(void)
{
  if((_rgbLedState & 0x00000001) == 0)
    stm32_gpiowrite(GPIO_LED_BLUE, false);

  if((_rgbLedState & 0x00000002) == 0)
    stm32_gpiowrite(GPIO_LED_GREEN, false);

  if((_rgbLedState & 0x00000004) == 0)
    stm32_gpiowrite(GPIO_LED_RED, false);
}

//===============================================================
// The RGB LED use power too. Get the status and turn RGB off. They'll be
// restored when F7 has exited stop-mode
static void pwrmgmt_tri_color_leds_off(void)
{
  // What is there state before turning off? They are all on port A and bits
  // blue = bit 0, green = bit 1 and red = bit 2
  _rgbLedState = getreg32(STM32_GPIOA_IDR);

  // Saves 0-6 ma depending on which leds are on
  stm32_gpiowrite(GPIO_LED_RED, true);
  stm32_gpiowrite(GPIO_LED_GREEN, true);
  stm32_gpiowrite(GPIO_LED_BLUE, true);
}

/************************************************************************************
 * Public Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function is called during initialization and is responsible for calling
// the other initialization function within this block of code.
int meadow_power_mgmt_initialize()
{
  int ret = OK;

  // Initialize internals needed for the LSI clock to be used with RTC
  ret = pwrmgmt_init_lsi_calib();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    return ret;
  }
 
  ret = pwrmgmt_init_rtc_clk_switch();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
  }

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D06);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D07);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D08);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D09);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D10);

  DEBUG_SET_LOW(DEBUG_PIN_V2_D06);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D07);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D08);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D09);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D10);

  return ret;
}

//=======================================================================
// Contains the steps to put F7 into Stop mode
int pwrmgmt_enter_low_power_mode(uint32_t wakeupPeriod)
{
  int ret = OK;

  // It should not be possible to call this twice since in low-power state the
  // MCU isn't running.

  if(wakeupPeriod == 0)
    return OK;

  // Using stop-mode with wakeup timer has a 16-bit limit
  if(wakeupPeriod > 0xffff)
  {
    return -EINVAL;      // 22
  }

// syslog(1, "--> Sending low-power message to CLI\n"); usleep(20 * 1000);

//   // Let CLI know, if its listening.
//   // sequence number, version and extra are handled in function being called
//   uint8_t msgBuf[HCOM_TINY_HOST_STRING_BUFF_LENGTH + HCOM_PROTOCOL_HEADER_MSG_LENGTH];
//   HcomProtoTextMsg_t *textMsg = (HcomProtoTextMsg_t *)msgBuf;
//   textMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_PWRMGMT_ENTER_LOWPWR;
//   textMsg->stdHeader.userData = 0;
//   size_t txtLen = snprintf_chk(textMsg->textData,
//         HCOM_TINY_HOST_STRING_BUFF_LENGTH,
//         "Meadow entering low-power mode for %d seconds", wakeupPeriod);

//   // Send message to CLI
//   ret = hcom_nx_host_send_std_msg_data((HcomProtoHdrMsg_t *) textMsg,
//           txtLen + HCOM_PROTOCOL_HEADER_MSG_LENGTH, thisFile, __LINE__);
//   if(ret < 0)
//   {
//     syslog(LOG_ERR, "%s@%d-Error:sending msg to host\n", thisFile, __LINE__);
//   }

syslog(1, "--> Entering low-power in 100 ms\n");
  // Insure CLI has time to get the message and act before comm port is closed
  usleep(100 * 1000);

  // Prevent up_idle from using WFI or WFE commands
  pwrmgmt_idle_behavior_control(false);

  // espcp_deep_sleep();

  // Turn off tri-color LEDs as a power saving measure
  pwrmgmt_tri_color_leds_off();

  // Switch on LSI clock
  // Note: this must be first because it does a backup domain reset which
  // will clear some of the registers configured by following steps  
  ret = meadow_pwr_mgmt_use_lsi_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

  // Configure wakeup hardware and stop period
  ret = pwrmgmt_config_wakeup_timer(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

  // Enter stop mode and wait for specified time
  ret = pwrmgmt_enter_stop_mode();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

  // The F7 must be awake for the thread to have gotten here. Switch back
  // to crystal controlled HSE clock.
  ret = meadow_pwr_mgmt_use_hse_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
  }
  
  // Restore the tri-color LEDs to there original state
  pwrmgmt_tri_color_leds_restore();

  // espcp_wakeup();

  // Allow up_idle function to again use WFI and WFE to save power in normal
  // operation.
  pwrmgmt_idle_behavior_control(true);

  return ret;
}

// The next 3 functions are for future use, when the RTC's alarm is used to
// wakeup the F7. This has the advantage of a much longer timeout periods.
#if 0
//==============================================================
// Enter low-power mode for the period specified
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_after_seconds(time_t secondsTillAlarm)
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
    syslog(LOG_ERR, "%s@%d-Error:time(NULL) call failed\n", thisFile, __LINE__);
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
#endif

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
