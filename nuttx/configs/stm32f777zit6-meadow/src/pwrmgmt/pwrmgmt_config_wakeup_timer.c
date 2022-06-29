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

// This module sets up the wakeup timer for use

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

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

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
int pwrmgmt_config_wakeup_timer(uint16_t wakeupPeriod)
{
  uint32_t regval;

  // Disable write protection on RTC registers
  pwrmgmt_rtc_wprunlock();
  pwrmgmt_rtc_enterinit();

  // Disable wakeup timer to allow modifications and wait till done
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUTE;   // Clear Wakeup Timer Enable bit
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) == 0);

  // Program the time value into the wakeup timer
  putreg16(wakeupPeriod, STM32_RTC_WUTR);

  // Select the clock source for the wakeup timer
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUCKSEL_MASK;   // Clear all bits
  regval |= RTC_CR_WUCKSEL_CKSPRE;  // Connect to 1 Hz source
  putreg32(regval, STM32_RTC_CR);

  // Interrupt mask register
  regval = getreg32(STM32_EXTI_IMR);
  regval |= EXTI_RTC_WAKEUP;      //  Wakeup event (22)
  putreg32(regval, STM32_EXTI_IMR);
  
  // Event mask register
  // Not used in current configuration
  regval = getreg32(STM32_EXTI_EMR);
  regval &= ~EXTI_RTC_WAKEUP;     // Wakeup event (22)
  putreg32(regval, STM32_EXTI_EMR);

  // Enable rising trigger selection register
  regval = getreg32(STM32_EXTI_RTSR);
  regval |= EXTI_RTC_WAKEUP;      // Wakeup event (22)
  putreg32(regval, STM32_EXTI_RTSR);
  
  // Clear falling trigger selection register
  regval = getreg32(STM32_EXTI_FTSR);
  regval &= ~EXTI_RTC_WAKEUP;   // RTC Wakeup event (22)
  putreg32(regval, STM32_EXTI_FTSR);

  // OSEL decides which action drives the RTC_OUT line
  // Not needed in current configuration
  // regval = getreg32(STM32_RTC_CR);
  // regval &= ~RTC_CR_OSEL_MASK;  // Clear Wakeup output enabled bits
  // regval |= RTC_CR_OSEL_WUT;    // Wakeup output enabled
  // putreg32(regval, STM32_RTC_CR);

  // Clear WUTF flag (set by hardware when wakeup flag counts down to 0)
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~(RTC_ISR_WUTF);
  putreg32(regval, STM32_RTC_ISR);
  
  // Wakeup timer interrupt enable
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_WUTIE;
  putreg32(regval, STM32_RTC_CR);

  // Wakeup Timer Enable
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_WUTE;
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) != 0);

  // Exit init mode and lock wakeup timer
  pwrmgmt_rtc_exitinit();
  pwrmgmt_rtc_wprlock();

  return OK;
}

//==================================================================
// After exiting low-power mode disable the Wakeup Timer
void meadow_pwr_mgmt_disable_wakeup_timer()
{
  uint32_t regval;

  // Disable write protection on RTC registers
  putreg32(0xca, STM32_RTC_WPR);
  putreg32(0x53, STM32_RTC_WPR);

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

  // Enable wakeup timer
  putreg32(0xff, STM32_RTC_WPR);
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
