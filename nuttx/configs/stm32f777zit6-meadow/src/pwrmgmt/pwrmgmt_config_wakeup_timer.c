/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_config_wakeup_timer.c
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

// This module is used to configure the RTC Alarm for low-power wakeup. This
// makes possible a low-power mode that lasts 28 days - 1 second.

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

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT) && defined (PWRMGMT_LOW_PWR_EXIT_USE_WAKEUP_TIMER)

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
// static char *thisFile = __FILE__;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Configure the RTC Wakeup Timer
int pwrmgmt_config_rtc_timer_wakeup_seconds(uint16_t wakeupPeriod)
{
  uint32_t regval;

  // Disable write protection on RTC registers
  pwrmgmt_rtc_wprunlock();
  pwrmgmt_rtc_enterinit();

  // Disable wakeup timer to allow modifications till done
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUTE;   // Clear Wakeup Timer Enable bit
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) == 0);

  // Program the time value into the wakeup timer. Testing has shown that
  // the time spent in stop mode is 1 second greater than the value programmed.
  putreg16(wakeupPeriod - 1, STM32_RTC_WUTR);

  // Select the clock source for the wakeup timer
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUCKSEL_MASK;   // Clear all bits
  regval |= RTC_CR_WUCKSEL_CKSPRE;  // Connect to 1 Hz source
  putreg32(regval, STM32_RTC_CR);

  // Clear the RTC Wakeup Pending bit
  // This bit is cleared by programming it to '1'
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);

  // Extended Interrupt mask and Event controller (EXTI)
  regval = getreg32(STM32_EXTI_RTSR); // Rising trigger selection register
  regval |= EXTI_RTC_WAKEUP;          // Enable Wakeup event (22)
  putreg32(regval, STM32_EXTI_RTSR);
  
  regval = getreg32(STM32_EXTI_FTSR); // Falling trigger selection register
  regval &= ~EXTI_RTC_WAKEUP;         // RTC Wakeup event (22)
  putreg32(regval, STM32_EXTI_FTSR);

  regval = getreg32(STM32_EXTI_IMR);  // Interrupt mask register
  regval |= EXTI_RTC_WAKEUP;          // Wakeup event (22)
  putreg32(regval, STM32_EXTI_IMR);
  
  regval = getreg32(STM32_EXTI_EMR);  // Event mask register
  regval &= ~EXTI_RTC_WAKEUP;         // Wakeup event (22)
  putreg32(regval, STM32_EXTI_EMR);

  // Clear WUTF flag (set by hardware when wakeup flag counts down to 0)
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_WUTIE;   // Wakeup timer interrupt enable
  regval |= RTC_CR_WUTE;    // Wakeup Timer Enable
  putreg32(regval, STM32_RTC_CR);

  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) != 0);

  // Exit init mode and lock wakeup timer
  pwrmgmt_rtc_exitinit();
  pwrmgmt_rtc_wprlock();

  return OK;
}

//==================================================================
// After exiting low-power mode disable the Wakeup Timer
void pwrmgmt_disable_wakeup_timer_wakeup()
{
  uint32_t regval;

  // Enable write access to RTC registers
  pwrmgmt_rtc_wprunlock();

  // Disable wakeup timer and wait to complete
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUTE;
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) == 0);

  // Clear Wakeup timer flag
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);

  // Per ref man 4.3.7 near end disable and enable WUTIE
  // Disable Wakeup timer interrupts
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUTIE;
  putreg32(regval, STM32_RTC_CR);

  // Disable write access
  pwrmgmt_rtc_wprlock();
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
