/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_config_wakeup_alarm.c
 * 
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

// This module is used to configure the RTC alarm feature, allowing it to
// wakeup the F7. Because the alarm feature uses a specific date and time
// as the alarm trigger it can wait from 1 second to 28 days - 1 second.

#warning "(--) Peter is Here (pwrmgmt_config_wakeup_alarm.c)"

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"

#include "stm32_gpio.h"
#include "stm32_pwr.h"    // FOR TESTING
#include "stm32_rcc.h"    // Re-init clocks
#include "stm32_alarm.h"

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include "pwrmgmt_local.h"

#include "chip/stm32f76xx77xx_pwr.h"
#include "chip/stm32_exti.h"
#include "nvic.h"

#include <arch/board/board.h>

#include "stm32f777zit6-meadow.h"
#include "hcom_nx/hcom_nx_common.h"

#include "stm32_alarm.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// 0 = Alarm A and 1 == Alarm B
#define MEADOW_PWRMGMT_ALRM_WAKEUP_ID 0

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

static int meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(time_t almTime);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Enter low-power mode for the period specified in seconds
// This is where the managed code enters
int meadow_pwr_mgmt_set_rtc_wakeup_alarm_after_seconds(time_t secondsTillAlarm)
{
  int ret;

  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "Error:'time(NULL)' call failed\n");
    return -ETIME;
  }

  // What time will this be (in seconds)?
  time_t almTime = secondsTillAlarm + currentTime;

  // Now use the future time in seconds
  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(almTime);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }
  
  return ret;
}

//==============================================================
// Enter low-power mode until the future time in time specified in seconds
static int meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(time_t almTime)
{
  int ret;
  struct tm tmAlarm;

  // Now convert alarm time to a future time in struct tm
  struct tm tmTemp;
  gmtime_r(&almTime, &tmTemp);
  memcpy(&tmAlarm, &tmTemp, sizeof(struct tm));

  // Set the alarm based on the tm time structure's information
  ret = pwrmgmt_config_rtc_alarm_wakeup(tmAlarm);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }

  return OK;
}

//==================================================================
// Enter low-power mode until the future time specified as struct tm
int pwrmgmt_config_rtc_alarm_wakeup(struct tm tmAlarm)
{
  int ret;
  struct timespec ts;
  uint32_t regval;

  // Get the current time
  ret = clock_gettime(CLOCK_REALTIME, &ts);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:clock_gettime call failed:%d\n",
              thisFile, __LINE__, ret);
    return -ETIME;
  }

  // Alarm time must be in the future
  time_t almTime = mktime(&tmAlarm);
  if(almTime <= ts.tv_sec)
  {
    syslog(LOG_ERR, "Error:Alarm time before current time\n");
    return -ETIME;
  }

  // Disable write protection on RTC registers
  pwrmgmt_rtc_wprunlock();
  pwrmgmt_rtc_enterinit();

  // Disable RTC alarm
  regval = getreg32(STM32_RTC_CR);
#if MEADOW_PWRMGMT_ALRM_WAKEUP_ID == 0
  regval &= ~RTC_CR_ALRAE;   // Clear Alarm A Enable bit to disable
#else
  regval &= ~RTC_CR_ALRBE;   // Clear Alarm B Enable bit to disable
#endif

  putreg32(regval, STM32_RTC_CR);

  // Wait for bit to be written
#if MEADOW_PWRMGMT_ALRM_WAKEUP_ID == 0
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);
#else
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);
#endif

  // Disable RTC register lock
  pwrmgmt_rtc_wprunlock();

  // Disable Alarm A enable and Alarm A interupt enable
  modifyreg32(STM32_RTC_CR, (RTC_CR_ALRAE | RTC_CR_ALRAIE), 0);

  // Convert struct tm time to bcd values acceptable to the Alarm Register
  regval = (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_sec)  << RTC_ALRMR_SU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_min)  << RTC_ALRMR_MNU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_hour) << RTC_ALRMR_HU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_mday) << RTC_ALRMR_DU_SHIFT);
  
  // Set the time and day information in the Alarm A compare register. Set
  // subsecond field to 0.
  putreg32(regval, STM32_RTC_ALRMAR);
  putreg32(0, STM32_RTC_ALRMASSR);

  // Setup RTC Alarm interrupt/event
  regval = getreg32(STM32_EXTI_IMR);  // Interrupt mask register
  regval |= EXTI_RTC_ALARM;           // RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_IMR);
  
  // Not used in current configuration
  regval = getreg32(STM32_EXTI_EMR);  // Event mask register
  regval &= ~EXTI_RTC_ALARM;          // RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_EMR);

  regval = getreg32(STM32_EXTI_RTSR); // Enable rising trigger selection register
  regval |= EXTI_RTC_ALARM;           // RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_RTSR);
  
  regval = getreg32(STM32_EXTI_FTSR); // Clear falling trigger selection register
  regval &= ~EXTI_RTC_ALARM;          // RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_FTSR);

  // Clear ALRAF flag (This flag is set by hardware when the time/date registers
  // (RTC_TR and RTC_DR) match the Alarm A register (RTC_ALRMAR)
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRAF;
  putreg32(regval, STM32_RTC_ISR);
  
  // Reenable Alarm A interrupt enable
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_ALRAIE;
  putreg32(regval, STM32_RTC_CR);

  // Reenable Alarm A Enable
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_ALRAE;
  putreg32(regval, STM32_RTC_CR);

  // Wait for status flag to indicate that ALRAE bit has been set
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) != 0);

  // Exit init mode and lock wakeup timer
  pwrmgmt_rtc_exitinit();
  pwrmgmt_rtc_wprlock();

  return OK;
}

#endif
