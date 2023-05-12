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
// (--) NOT IN LOVE WITH THE 'A'/'B' thing
// Alarm A or Alarm B
#define PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE 'A'

/************************************************************************************
 * Private Data
 ************************************************************************************/

static char *thisFile = __FILE__;

#if 0
static uint32_t SysCtrlReg;
#endif

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
int pwrmgmt_config_rtc_alarm_wakeup_seconds(time_t secondsTillAlarm)
{
  time_t currentTime = time(NULL);
  if(currentTime == (time_t)(-1))
  {
    syslog(LOG_ERR, "Error:'time(NULL)' call failed\n");
    return -ETIME;
  }

  // What time will this be (in seconds)?
  time_t almTime = secondsTillAlarm + currentTime;

  // Now use the future time in seconds
  int  ret = meadow_pwr_mgmt_set_rtc_wakeup_alarm_at_time(almTime);
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
  // Now convert alarm time to a future time
  struct tm tmAlarm;
  struct tm tmTemp;
  gmtime_r(&almTime, &tmTemp);
  memcpy(&tmAlarm, &tmTemp, sizeof(struct tm));

  // Set the alarm based on the tm time structure
  int ret = pwrmgmt_config_rtc_alarm_wakeup_tm(tmAlarm);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }

  return OK;
}

//==================================================================
// Enter low-power mode until the future time specified as struct tm
int pwrmgmt_config_rtc_alarm_wakeup_tm(struct tm tmAlarm)
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

#if 1 // ONLY FOR TESTING
  struct timespec abstime;
  struct tm tmNowNx;
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowNx);

  syslog(2, "Current Time-%02dT%02d:%02d:%02d\n",
            tmNowNx.tm_mday, tmNowNx.tm_hour, tmNowNx.tm_min, tmNowNx.tm_sec);
  // From Wakeup time argument
  syslog(2, "Wake up Time-%02dT%02d:%02d:%02d\n",
            tmAlarm.tm_mday, tmAlarm.tm_hour, tmAlarm.tm_min, tmAlarm.tm_sec);
  usleep(20 * 1000);
#endif // ONLY FOR TESTING

  // Disable write protection on RTC registers
  pwrmgmt_rtc_wprunlock();
  pwrmgmt_rtc_enterinit();

  // Disable RTC alarm
  regval = getreg32(STM32_RTC_CR);
#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  regval &= ~RTC_CR_ALRAE;    // Clear Alarm A Enable bit to disable
  regval &= ~RTC_CR_ALRAIE;   // Disable Alarm A enable 
#else
  regval &= ~RTC_CR_ALRBE;   // Clear Alarm B Enable bit to disable
  regval &= ~RTC_CR_ALRBIE;   // Disable Alarm B enable 
#endif
  putreg32(regval, STM32_RTC_CR);

  // Wait for bit to be written
#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);
#else
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);
#endif

  // Convert struct tm time to bcd values acceptable to the Alarm Register
  // Only care about day of month and time
  regval = (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_sec)  << RTC_ALRMR_SU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_min)  << RTC_ALRMR_MNU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_hour) << RTC_ALRMR_HU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_mday) << RTC_ALRMR_DU_SHIFT);

syslog(1, "--> Wakeup Time set to:%08x\n", regval);

  // Set the time and day information in compare register.
#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  putreg32(regval, STM32_RTC_ALRMAR);   // Using Alarm A
  putreg32(0, STM32_RTC_ALRMBR);        // Not using Alarm B
#else
  putreg32(0, STM32_RTC_ALRMAR);        // Not using Alarm A
  putreg32(regval, STM32_RTC_ALRMBR);   // Using Alarm B
#endif
  // Set both A and B subsecond fields to 0
  putreg32(0, STM32_RTC_ALRMASSR);
  putreg32(0, STM32_RTC_ALRMBSSR);

  // Clear the RTC Alarm Pending bit
  // A value of '1' indicates a pending alarm and writing '1' clears this bit
  // and the RTC_ISR_ALRAF bit.
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);

  // TESTING
  // stm32_exti_wakeup(bool risingedge, bool fallingedge, bool event,
  //                     xcpt_t func, void *arg)
  // stm32_exti_wakeup(true, false, true, NULL, NULL);
  // TESTING

  // Extended Interrupt and Event controller (EXTI). Note: the best
  // explaination is in the description of EXTI_SWIER 11.9.5.
  regval = getreg32(STM32_EXTI_RTSR); // Enable rising trigger selection register
  regval |= EXTI_RTC_ALARM;           // Enable rising edge RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_RTSR);
  
  regval = getreg32(STM32_EXTI_FTSR); // Clear falling trigger selection register
  regval &= ~EXTI_RTC_ALARM;          // Disable falling RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_FTSR);

  regval = getreg32(STM32_EXTI_EMR);  // Event mask register
  // (--) LEAVING UNTIL WAKEUP IS WORKING
  // regval &= ~EXTI_RTC_ALARM;          // Ignore Event for RTC Alarm event (17)
  regval |= EXTI_RTC_ALARM;          // Unmask Event for RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_EMR);

  regval = getreg32(STM32_EXTI_IMR);  // Interrupt mask register
  regval |= EXTI_RTC_ALARM;           // Unmask RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_IMR);

  // Clear ALRAF Alarm flag (This flag is set by hardware when the time/date
  // registers (RTC_TR and RTC_DR) match the Alarm register (RTC_ALRMxR)
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRAF;
  putreg32(regval, STM32_RTC_ISR);

  // Enable Interrupt and enable Alarm
  regval = getreg32(STM32_RTC_CR);
#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  regval |= RTC_CR_ALRAIE;  // Set Alarm A Interrupt enable bit
  regval |= RTC_CR_ALRAE;   // Set Alarm A enable bit
#else
  regval |= RTC_CR_ALRBIE;
  regval |= RTC_CR_ALRBE;   // Clear Alarm B Enable bit to disable
#endif
  regval |= RTC_CR_FMT;     // Make sure 24 hour time used for compare
  putreg32(regval, STM32_RTC_CR);
  
  // Wait for status flag to indicate that ALRAE bit has been cleared
  // indicating updates are no longer allowed
#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);
#else
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);
#endif

#if 0
  // Set SEVONPEND bit of Cortex System Control Register. Setting this bit
  // will allow any interrupt of any priority to wakeup the MCU from one of
  // the Sleep modes.
  // See PM0253 Programming manual for more details
  regval = getreg32(NVIC_SYSCON);
  SysCtrlReg = regval;        // Save for restoration
  regval |= NVIC_SYSCON_SEVONPEND;
  putreg32(regval, NVIC_SYSCON);
#endif

  // Exit init mode and lock wakeup timer
  pwrmgmt_rtc_exitinit();
  pwrmgmt_rtc_wprlock();

  syslog(1, "==> Reached end of RTC Alarm initialization\n");
  usleep(20 * 1000);

// #if defined(CONFIG_POWER_MANAGEMENT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
//   pwrmgmt_rtc_dumpregs("After RTC Alarm A initialization");
// #endif

  return OK;
}

//==================================================================
// After exiting low-power mode disable the Alarm Timeout
void pwrmgmt_disable_rtc_alarm_wakeup()
{
  uint32_t regval;

#if 0
  // Restore the Cortex System Control Register to it's original state.
  putreg32(SysCtrlReg, NVIC_SYSCON);
#endif

  // Enable write access to RTC registers
  pwrmgmt_rtc_wprunlock();

#if PWRMGMT_WHICH_WAKEUP_ALARM_TO_USE == 'A'
  // Disable alarm A timeout and wait to complete
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRAE;
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) != 0);

  // Clear Alarm A alarm flag
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRAF;
  putreg32(regval, STM32_RTC_ISR);

  // Disable Alarm A interrupt flag
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRAIE;
  putreg32(regval, STM32_RTC_CR);
#else

  // Disable alarm B timeout and wait to complete
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRBE;
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) != 0);

  // Clear Alarm A alarm flag
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRBF;
  putreg32(regval, STM32_RTC_ISR);

  // Disable Alarm B interrupt flag
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRBIE;
  putreg32(regval, STM32_RTC_CR);
#endif

  // Disable write access
  pwrmgmt_rtc_wprlock();
}

#endif
