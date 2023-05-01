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

#warning "(--) Peter is Here"

/************************************************************************************
 * Private Data
 ************************************************************************************/

static char *thisFile = __FILE__;
static uint32_t _rgbLedState;

// Space for n callbacks for notification of entering low-power mode
#define PWR_MGMT_MAX_CALLBACKS_AVAILABLE (4)
static pwr_mgmt_notify_callback _regCallback[PWR_MGMT_MAX_CALLBACKS_AVAILABLE];

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

// Notify subscribers that the power mode will change. This is not a full
// featured implementation. The number that can signup is fixed at build
// time and there's no unscribing.
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
// This function is called when a module wants to subscribe for power
// management notifications.
int pwrmgmt_subscribe_for_low_pwr_notifications(pwr_mgmt_notify_callback callback)
{
  int slotOffset;

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

#if TEMP_USE_ALARM_NOT_WAKEUP_TIMER > 0
  // The STM32F7's internal alarm clock uses HH:mm:ss and date but not the
  // month or year. Therefore, the worse case, maximum length, of a delay is
  // 28 days minus 1 second.
  // Or ((28 days * 24 * 60 * 60 = 2419200) - 1) = 2419199
  if(wakeupPeriod > 2419199)
  {
    return -ETIME;      // -62
  }
#else
  // Using the wakeup timer limits to maximum to its 16-bit timer or 65535
  // seconds
  if(wakeupPeriod > 0xffff)
  {
    return -ETIME;      // -62
  }
#endif

  // Notify registered modules that low-power is about to begin.
  ret = pwrmgmt_notify_registered_modules(true);
  if(ret != OK)
  {
    // Something wrong with entering low-power for this module.
    return -EBUSY;
  }

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

// What scheme will be used to wakeup the F7, Alarm or Wakeup timer?
#if TEMP_USE_ALARM_NOT_WAKEUP_TIMER > 0
syslog(1, "==> Using ALARM A for low-power sleep duration\n");

  // Configure Wakeup/Alarm hardware and stop period
  // Using the RTC Alarm allows waking up at a future time that is almost one
  // month ahead, since there's no year comparison only day of the month.
  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_after_seconds(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }
syslog(1, "==> Returned from setting ALARM Time for sleep, about to enter sleep\n");
usleep(20 * 1000);

#else

syslog(1, "==> Using WAKEUP TIMEOUT for low-power sleep\n");
  // Using the RTC Wakeup Timer allows setting a future time up to 0xffff seconds
  // into the future a bit over 18 hours.
  ret = pwrmgmt_config_wakeup_timer(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }
#endif

  // Enter stop mode and wait for specified time
  ret = pwrmgmt_enter_stop_mode();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
    pwrmgmt_idle_behavior_control(true);
    return ret;
  }

  // Doing this first because some internal threads have been terminated
  // before entering low-power mode.
  // Notify concerned that low-power mode has ended. If a module has a problem
  // restarting it will be returned as an error
  ret = pwrmgmt_notify_registered_modules(false);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", thisFile, __LINE__);
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

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
