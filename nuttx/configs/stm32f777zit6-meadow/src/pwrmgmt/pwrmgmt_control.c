/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_control.c
 * 
 *   Copyright (C) 2022-2023 Wilderness Labs. All rights reserved.
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

// This module controls the power management features (low-power stop mode) of
// the Meadow F7.

// Note: Nuttx has it's own power management implementation but after studying
// it, I decided to not use it because it made some assumptions about behavior
// that I thought were not in line with how Meadow was to operate. That said
// I did use the Nuttx implemention for "inspirition". Peter Moody 25Mar22

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <syslog.h>
#include <nuttx/irq.h>
#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"
#include "stm32_pwr.h"    // FOR TESTING

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_pwr_mgmt.h>
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

// Space for n callbacks for notification of entering low-power mode
#define PWR_MGMT_MAX_CALLBACKS_AVAILABLE (6)
static pwr_mgmt_notify_callback _regCallback[PWR_MGMT_MAX_CALLBACKS_AVAILABLE];

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

// Notify subscribers that the power mode will change. This is not a full
// featured implementation. The number that can signup is fixed at build
// time and there's no unsubscribe.
// Possible future feature:
// To allow a registered receipient to post-pone the entry into low-power
// would require first telling each receipient of the pending change. Each
// receipient can responds with yes or no. In either case the receipient is
// responsible to prevent entry into a state where it will get busy.
// If all respond yes, the low-power mode is entered immediately, by again
// notifying each callback that low-power transition is happening now.
// If one or more callbacks indicated that they were busy then entry into
// the low-power state is post-poned and the caller is responsible to keep
// attempting to enter the low-power state until the busy situation passes.
static int pwrmgmt_notify_registered_modules(bool lpStart)
{
  int ret = OK;
  int slotOffset = 0;

  for(slotOffset = 0; slotOffset < PWR_MGMT_MAX_CALLBACKS_AVAILABLE; slotOffset++)
  {
    pwr_mgmt_notify_callback callback = _regCallback[slotOffset];

    if(callback == NULL)
    {
      continue;
    }

    // syslog(2, "%s@%d-Notifying - callback:%p, %s\n", __FILE__, __LINE__, callback, lpStart ? "true" : "false");

    // Notify registered receipient announcing what's about to happen
    ret = callback(lpStart);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Callback:%d returned:%d\n",
                thisFile, __LINE__, slotOffset + 1, ret);
      return ret;
    }
  }

  return OK;
}

//===============================================================
// This function controls the idle threads behavior by prevent it from calling
// the WFI or WFE op codes until the stop-mode has completed. If this isn't
// done, when the configuration for stop mode is incomplete the MCU can lock
// up.
static void pwrmgmt_idle_behavior_control(bool allowWaitOp)
{
  irqstate_t flags;

  flags = enter_critical_section();
  up_idle_pwrmgmt_set_idle_behavior(allowWaitOp);
  leave_critical_section(flags);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function is called when a module wants to subscribe for power
// management notifications.
int pwrmgmt_subscribe_for_low_pwr_notifications(pwr_mgmt_notify_callback callback)
{
  int slotOffset;

  // syslog(2, "%s@%d-Subscribing callback:%p\n", __FILE__, __LINE__, callback);

  // Find free slot
  for(slotOffset = 0; slotOffset < PWR_MGMT_MAX_CALLBACKS_AVAILABLE; slotOffset++)
  {
    if(_regCallback[slotOffset] == NULL)
    {
      // Found a slot save the callback
      _regCallback[slotOffset] = callback;
      return OK;
    }
  }

  syslog(LOG_ERR, "Notification for Low-Power failed. %d slots are not enough.\n",
            PWR_MGMT_MAX_CALLBACKS_AVAILABLE);

  return -EBADSLT;
}

//==============================================================
// This function is called during initialization and is responsible for calling
// the other initialization function within this block of code.
int meadow_power_mgmt_initialize()
{
  int ret = OK;

  for (int i = 0; i < PWR_MGMT_MAX_CALLBACKS_AVAILABLE; i++)
  {
    _regCallback[i] = NULL;
  }

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

  return ret;
}

//=======================================================================
// This is the public entry point for mono to initiate entering stop mode.
// It contains the steps to put F7 into Stop mode and recover
int pwrmgmt_enter_stm32f7_stop_mode(uint32_t wakeupPeriod)
{
  int ret = OK;

  MEADOW_TRACE_INFORMATION( "Received command to sleep for %d seconds\n",
          wakeupPeriod);

  // It should not be possible to call this twice since in low-power mode the
  // MCU isn't running.

  if(wakeupPeriod == 0)
    return OK;

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // The STM32F7's internal alarm clock uses day of month and HH:mm:ss but not
  // the month or year. Therefore, the worse case, maximum length, of a delay
  // is 28 days minus 1 second. It could be longer during some months but for
  // consistency this establishes a known maximum.
  // ((28 days * 24 * 60 * 60 = 2419200) - 1 second) = 2419199
  if(wakeupPeriod > 2419199)
  {
    return -ETIME;      // -62
  }
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Using the wakeup timer limits to maximum to its 16-bit timer or 65535
  // seconds
  if(wakeupPeriod > 0xffff)
  {
    return -ETIME;      // -62
  }
#else
#error "Select Low-Power timing scheme"
#endif

  // Notify CLI (if listening) of imminent low-power mode entry.
  uint8_t hdrMsg[HCOM_PROTOCOL_HEADER_MSG_LENGTH];
  HcomProtoTextMsg_t *msgHdrMsg = (HcomProtoTextMsg_t *)hdrMsg;
  msgHdrMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_TEXT_NEXT_LOW_PWR;
  msgHdrMsg->stdHeader.userData = 0;
  msgHdrMsg->stdHeader.extraData = 0;
  
  ret = hcom_nx_host_send_std_msg_data(msgHdrMsg, HCOM_PROTOCOL_HEADER_MSG_LENGTH,
            thisFile, __LINE__);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error attempting to send msg to host, ret:%d\n",
              thisFile, __LINE__, ret);
    // Keep going, this is unavoidable
  }

  // Notify registered modules that low-power is about to begin.
  // Currently, (10Jul23) there are 4 modules that are notified before entering
  // a low-power state. These are:hcom_host_receive, hcom_host_send,
  // hcom_stderr_redirect and hcom_stdout_redirect.
  ret = pwrmgmt_notify_registered_modules(true);
  if(ret != OK)
  {
    // Some code module is busy.
    return -EBUSY;
  }

  // Prevent up_idle from using WFI or WFE commands till we wakeup
  pwrmgmt_idle_behavior_control(false);

  // Switch on LSI clock
  // Note: this must be early because it does a backup domain reset which
  // will clear some of the registers configured in the following steps.
  ret = meadow_pwr_mgmt_use_lsi_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    (void) pwrmgmt_notify_registered_modules(false);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Configure Wakeup/Alarm hardware and stop period
  // Using the RTC Alarm allows waking up at a future time. However, since
  // there's no year or month comparison, only day of the month, this only
  // allows, at most, a period of one month ahead. This has been limited
  // to 28 days - 1 second so it is consistent and not different for each
  // month.
  ret = pwrmgmt_config_rtc_alarm_wakeup_seconds(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    (void) pwrmgmt_notify_registered_modules(false);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Using the RTC Wakeup Timer allows setting a future time up to 0xffff seconds
  // into the future ( a bit over 18 hours).
  ret = pwrmgmt_config_rtc_timer_wakeup_seconds(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);

    (void) pwrmgmt_notify_registered_modules(false);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

#else
#error "Select Low-Power timing scheme"
#endif

  //---------------------------------------------------------------------
  // Enter stop mode and wait for specified time to expire. Actually, not
  // "waiting" but being in stop mode. This, call returns when the F7 has
  // returned to normal operation.
  ret = pwrmgmt_enter_stop_mode();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }
  
  // Running again
  //---------------------------------------------------------------------

  // Switch back to crystal controlled HSE clock.
  ret = meadow_pwr_mgmt_use_hse_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
  }

  // Notify concerned modules that low-power mode has ended. If a module has
  // a problem restarting it will be returned as an error, which will be output
  // and ignored.
  ret = pwrmgmt_notify_registered_modules(false);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
  }

  // Allow up_idle function to again use WFI and WFE to save power in normal
  // operation.
  pwrmgmt_idle_behavior_control(true);

  return ret;
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
