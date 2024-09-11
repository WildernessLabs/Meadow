/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_config_wakeup_alarm.c
 * 
 *   Copyright (C) 2023-2024 Wilderness Labs. All rights reserved.
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

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT) && defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)

// #pragma message "(--) pwrmgmt_config_wakeup_alarm.c"

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

// #pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/

static int _diagCount = 0;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Enter low-power mode for the period specified in seconds
// This is where the managed code enters
int pwrmgmt_config_rtc_alarm_wakeup_seconds(time_t secondsTillAlarm)
{
  int ret;
  struct tm tmHardware;
  struct tm tmAlarm;

  // Is the desired low-power period reasonable?
  if(secondsTillAlarm < 2)
  {
    return -EPERM;    // Operation not permitted
  }

  // This Nuttx function reads the date, time and sub-seconds from the MCU's
  // hardware into a struct tm. However, the STM32F77X Errata warns about a
  // possible problem in ES0334-Rev 9 2.12.1 related to the RTC calendar
  // register not locked properly. Therefore, we'll read nsec twice and
  // compare.
#ifdef CONFIG_STM32F7_HAVE_RTC_SUBSECONDS
  long nsec;
  long prevNsec;
do
  {
    ret = up_rtc_getdatetime_with_subseconds(&tmHardware, &prevNsec);
    if(ret < 0)
    {
      return ret;
    }

    // Read a second time per Errata
    ret = up_rtc_getdatetime_with_subseconds(&tmHardware, &nsec);
    if(ret < 0)
    {
      return ret;
    }

    // If they match we have good values
    if(prevNsec == nsec)
      break;
      
  } while (1);

#elif
  // This function is used if no sub-seconds.
  ret = up_rtc_getdatetime(&tmHardware)
#endif

  // With the current date and time established we'll add the number of
  // seconds we need to be in stop mode.
  time_t almSeconds = mktime(&tmHardware) + secondsTillAlarm;

  // Convert epoch time to calendar UTC time
  gmtime_r(&almSeconds, &tmAlarm);

// #if MEADOW_POWER_MANAGEMENT_SHOW_TIME_CALC > 0
  // syslog(2, "Wakeup in seconds - %u\n", secondsTillAlarm);
  _diagCount++;
  syslog(2, "Low-pwr Request # - %06d\n", _diagCount);
  syslog(2, "Hardware Time     - %02dT%02d:%02d:%02d\n",
            tmHardware.tm_mday, tmHardware.tm_hour,
            tmHardware.tm_min, tmHardware.tm_sec);

  // When show the Nuttx time if needed
  // struct timespec abstime;
  // struct tm tmNowNx;

  // clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  // gmtime_r(&abstime.tv_sec, &tmNowNx);

  // syslog(2, "Nuttx Time        - %02dT%02d:%02d:%02d\n",
  //           tmNowNx.tm_mday, tmNowNx.tm_hour, tmNowNx.tm_min, tmNowNx.tm_sec);

  syslog(2, "Wake up Time      - %02dT%02d:%02d:%02d\n",
            tmAlarm.tm_mday, tmAlarm.tm_hour, tmAlarm.tm_min, tmAlarm.tm_sec);
  usleep(20 * 1000);

// #endif  // #if MEADOW_POWER_MANAGEMENT_SHOW_TIME_CALC > 0

  // Set the alarm based on the calendar time
  ret = pwrmgmt_config_rtc_alarm_wakeup_tm(tmAlarm);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Error:Setting alarm time failed, ret:%d\n", ret);
  }

  return ret;
}

//==================================================================
// Enter low-power mode until the future time specified as struct tm
int pwrmgmt_config_rtc_alarm_wakeup_tm(struct tm tmAlarm)
{
  uint32_t regval;

  // Disable write protection on RTC registers
  pwrmgmt_rtc_wprunlock();
  pwrmgmt_rtc_enterinit();

  // Disable RTC alarm (will be set before exiting function)
#if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRAE;    // Disable Alarm A
  regval &= ~RTC_CR_ALRAIE;   // Disable Alarm A interrupt
  putreg32(regval, STM32_RTC_CR);
  // Wait for ALRAE to be written in the RTC_CR register
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);
#elif (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 1)
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRBE;    // Disable Alarm B
  regval &= ~RTC_CR_ALRBIE;   // Disable Alarm B interrupt 
  putreg32(regval, STM32_RTC_CR);
  // Wait for ALRBE to be written
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);
#else
  #error "Select a valid RTC Alarm"
#endif

  // Convert struct tm time to bcd values acceptable to the Alarm A and B
  // register. We don't care about sub-seconds only day of month and time.
  regval = (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_sec)  << RTC_ALRMR_SU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_min)  << RTC_ALRMR_MNU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_hour) << RTC_ALRMR_HU_SHIFT) |
           (pwrmgmt_rtc_bin2bcd(tmAlarm.tm_mday) << RTC_ALRMR_DU_SHIFT);

  // Take care of day/date control bits
  regval &= ~(RTC_ALRMR_MSK4);    // Bit 31: 0=date, 1=day must match
  regval &= ~(RTC_ALRMR_WDSEL);   // Bit 30: Date field 0=Date, 1=Day of Week

  // Take care of hour control bits
  regval &= ~(RTC_ALRMR_MSK3);    // Bit 23 : 0=Hour must match
  regval &= ~(RTC_ALRMR_PM);      // Bit 22 : 0=AM/24-hour, 1 = PM

  // Take care of minute control bit
  regval &= ~(RTC_ALRMR_MSK2);    // Bit 15 : 0=Minute must match

  // Take care of second control bit
  regval &= ~(RTC_ALRMR_MSK1);    // Bit 7 : 0=Second must match

  // syslog(2, "RTC Alm A register- 0x%08x\n", regval);
  // usleep(10 * 1000);

  // Set the time and day information in compare register.
#if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  putreg32(regval, STM32_RTC_ALRMAR);   // Populate Alarm A
  putreg32(0, STM32_RTC_ALRMBR);        // Not Alarm B
#elif (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 1)
  putreg32(0, STM32_RTC_ALRMAR);        // Not Alarm A
  putreg32(regval, STM32_RTC_ALRMBR);   // Populate Alarm B
#else
  #error "Select a valid RTC Alarm"
#endif
  // Set all A and B sub-second fields to 0 to disable comparing sub-seconds
  // in alarm generation
  putreg32(0, STM32_RTC_ALRMASSR);
  putreg32(0, STM32_RTC_ALRMBSSR);

  // Clear the RTC Alarm Pending bit
  // A value of '1' indicates a pending alarm and writing '1' clears this bit
  // and the RTC_ISR_ALRAF bit.
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);

  // Set the EXTI_RTC_ALARM bit in the following registers
  // Extended Interrupt and Event controller (EXTI). Note: the best
  // explanation is in the description of EXTI_SWIER 11.9.5 of ref man
  regval = getreg32(STM32_EXTI_RTSR); // Enable rising trigger selection register
  regval |= EXTI_RTC_ALARM;           // Enable rising edge RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_RTSR);
  
  regval = getreg32(STM32_EXTI_FTSR); // Clear falling trigger selection register
  regval &= ~EXTI_RTC_ALARM;          // Disable falling RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_FTSR);

  regval = getreg32(STM32_EXTI_IMR);  // Interrupt mask register
  regval |= EXTI_RTC_ALARM;           // Unmask RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_IMR);

  regval = getreg32(STM32_EXTI_EMR);  // Event mask register
  regval &= ~EXTI_RTC_ALARM;          // Ignore Event for RTC Alarm event (17)
  putreg32(regval, STM32_EXTI_EMR);

  // Clear ALRAF Alarm flag (This flag is set by hardware when the time/date
  // registers (RTC_TR and RTC_DR) match the Alarm register (RTC_ALRMxR)
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRAF;
  putreg32(regval, STM32_RTC_ISR);

  // Enable Interrupt and enable Alarm
#if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  // Set enable bits and wait for status flag to indicate that ALRAE/ALRBE
  // bit has been cleared.
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_FMT;    // Insure 24 hour time used for compare
  regval |= RTC_CR_ALRAIE;  // Set Alarm A Interrupt enable bit
  regval |= RTC_CR_ALRAE;   // Set Alarm A enable bit
  regval &= ~RTC_CR_ALRBIE; // Disable Alarm B Interrupt
  regval &= ~RTC_CR_ALRBE;  // Disable Alarm B Enable
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);
#elif (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 1)
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_FMT;    // Insure 24 hour time used for compare
  regval |= RTC_CR_ALRBIE;
  regval |= RTC_CR_ALRBE;   // Set Alarm B Enable bit to enable
  regval &= ~RTC_CR_ALRAIE; // Clear Alarm A Interrupt enable
  regval &= ~RTC_CR_ALRAE;  // Clear Alarm A enable
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);
#else
  #error "Select a valid RTC Alarm"
#endif

  // Exit init mode and prevent rtc register access
  pwrmgmt_rtc_exitinit();
  pwrmgmt_rtc_wprlock();

  return OK;
}

//==================================================================
// After exiting low-power mode disable the Alarm Timeout
void pwrmgmt_disable_rtc_alarm_wakeup()
{
  uint32_t regval;

  // Enable write access to RTC registers
  pwrmgmt_rtc_wprunlock();

#if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRAIE;  // Clear Alarm A Interrupt enable bit
  regval &= ~RTC_CR_ALRAE;   // Clear Alarm A enable bit
  putreg32(regval, STM32_RTC_CR);
  // Wait for status flag to indicate that ALRAE bit has been cleared
  // indicating updates are no longer allowed
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAWF) == 0);

  // Clear the Alarm A occurred flag
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRAF;
  putreg32(regval, STM32_RTC_ISR);

#elif (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 1)
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_ALRBIE;  // Clear Alarm B Interrupt enable bit
  regval &= ~RTC_CR_ALRBE;   // Clear Alarm B enable bit
  putreg32(regval, STM32_RTC_CR);
  // Wait for status flag to indicate that ALRAE bit has been cleared
  // indicating updates are no longer allowed
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_ALRBWF) == 0);

  // Clear the Alarm B occurred flag
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_ALRBF;
  putreg32(regval, STM32_RTC_ISR);
#else
  #error "Select a valid RTC Alarm"
#endif

  // Disable write access
  pwrmgmt_rtc_wprlock();
}

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT) && defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
