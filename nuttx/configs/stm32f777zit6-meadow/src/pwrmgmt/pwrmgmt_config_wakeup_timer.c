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

// ISR indicating that the F7 is now awake
static int meadow_rtc_wakeup_isr_handler_setup(int irq, FAR void *context,
                                    FAR void *arg)
{
  uint32_t regval = 0;
  // RTC Wakeup interrupt through the EXTI line

  syslog(1, "+++===> Entered Wakeup ISR\n");

  // Reconfigure the internal clocks
  // arch/arm/src/stm32f7/stm32f76xx77xx_rcc.c
  stm32_clockenable();

  up_enable_irq(STM32_IRQ_SYSTICK);

  clock_synchronize();

  // Clear Wakeup timer flag
  rtc_wprunlock();
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  rtc_wprlock();

  // Clear the pending EXTI interrupt by setting the Pending Register correct
  // bit to 1
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);

  syslog(1, "+++===> Exit Wakeup ISR\n");

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Enter low-power mode until the Wakeup Timer wakes MCU up
int pwrmgmt_config_wakeup_timer(uint16_t wakeupPeriod)
{
  uint32_t regval;

  if(wakeupPeriod == 0 || wakeupPeriod > 0xffff)
  {
  // PeterM - the value can never be > 0xffff unless uint16_t is changed
  // to uint32_t
    syslog(LOG_ERR, "Error:The wakeup period must be > 0 and < 32768\n");
    return -1;
  }

  // (*)   // Sets the PWR_CR1_DBP bit in the STM32_PWR_CR1_OFFSET register
  // Ref Man 4.4.1 PWR power control register (PWR_CR1)
  // (*) stm32_pwr_enablebkp(true);,  PWR_CR1_DBP
  // (*) putreg32(0xca, STM32_RTC_WPR); putreg32(0x53, STM32_RTC_WPR);
  // Disable write protection on RTC registers
  // rtc_wprunlock();    // THIS IS OVERKILL

  // Disable write protection on RTC registers
  rtc_wprunlock();    // JUST TESTING-No difference in behavior?
  // putreg32(0xca, STM32_RTC_WPR);
  // putreg32(0x53, STM32_RTC_WPR);
  rtc_enterinit();    // JUST TESTING-No difference in behavior

  // Disable wakeup timer to allow modifications and wait till done
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUTE;   // Clear Wakeup Timer Enable bit
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) == 0);

  // Program the time value into the wakeup timer
  putreg16(wakeupPeriod, STM32_RTC_WUTR);  // RTC wakeup timer register

  // The ref man section 4.3.7
  // c) Configure the RTC to generate the RTC Wakeup event
  // Set the desired timer clock source
  // ck_spre (1Hz) this give a time range of 1 - 65536 seconds (18:12:16)
  // And Wakeup ouput enabled via OSEL_WUT
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_WUCKSEL_MASK;   // Clear all bits
  regval |= RTC_CR_WUCKSEL_CKSPRE;  // Connect to 1 Hz source
  putreg32(regval, STM32_RTC_CR);

  // Interrupt mask register
  // --> THIS SEEMS WRONG. WHY INTERRUPT AND NOT EVENT?
  regval = getreg32(STM32_EXTI_IMR);
  regval |= EXTI_RTC_WAKEUP;   //  Wakeup event (22)
  putreg32(regval, STM32_EXTI_IMR);
  
  // Event mask register
  // (*) Added by me from Cube
  regval = getreg32(STM32_EXTI_EMR);
  regval |= EXTI_RTC_WAKEUP;    // Wakeup event (22)
  putreg32(regval, STM32_EXTI_EMR);

  // Enable rising trigger selection register
  regval = getreg32(STM32_EXTI_RTSR);
  regval |= EXTI_RTC_WAKEUP;    // RTC Wakeup event (22)
  putreg32(regval, STM32_EXTI_RTSR);
  
  // (*) Added by me from Cube
  // Clear falling trigger selection register
  regval = getreg32(STM32_EXTI_FTSR);
  regval &= ~EXTI_RTC_WAKEUP;   // RTC Wakeup event (22)
  putreg32(regval, STM32_EXTI_FTSR);

  // OSEL decides which action drives the RTC_OUT
  // In our case, Wakeup output enabled
  // Part of STMCube RTC setup
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_OSEL_MASK;  // Clear Wakeup output enabled bits
  regval |= RTC_CR_OSEL_WUT;    // Wakeup output enabled
  putreg32(regval, STM32_RTC_CR);

  // PeterM-NOT SURE THIS IS NEEDED IF USING EVENTS NOT INTERRUPTS
  // After setting count, clear Wakeup timer flag, in case it's set.
  // Set by hardware when wakeup flag counts down to 0.
  // (fyi-for alarms it's ALRBF and ALRAF flags)
  regval = getreg32(STM32_RTC_ISR);
  // Clear WUTF flag
  // (*) Removed by me from Cube - RTC_ISR_INIT isn't needed here
  // regval &= ~(RTC_ISR_WUTF);
  regval &= ~(RTC_ISR_WUTF | RTC_ISR_INIT);   // (*) ADDED INIT
  putreg32(regval, STM32_RTC_ISR);
  
  // PeterM-ADDED AS AN EXPERIMENT
  // THIS CHANGE CRASHES THE OS
  if(true)
  {
    // Setup ISR and eanble IRQ
    irq_attach(STM32_IRQ_RTC_WKUP, meadow_rtc_wakeup_isr_handler_setup, NULL);
    up_enable_irq(STM32_IRQ_RTC_WKUP);
  }
  else
  {
    // Disable IRQ it's not needed for event
    up_disable_irq(STM32_IRQ_RTC_WKUP);
  }

  // NOTE: rtc_enterinit() HANDLES RTC_ISR_INIT ^ IN THE WAY THE REF MAN DESCRIBES
  // AND rtc_exitinit() EXITS. v BUT, TO CHANGE RTC_ISR_WUTF THIS SHOULD NOT BE
  // NECESSARY??? (COPIED FROM STM32CUBE)

  // MY GUESS IS THAT THE MACRO USED WAS EASY NOT KNOWING IT MESSED WITH INIT

  // (*) Removed by me from Cube
  // (*) ADDED INIT
  // Return to free running mode
  regval |= RTC_ISR_INIT;
  putreg32(regval, STM32_RTC_ISR);

  // Wakeup output enabled
  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_WUTIE;
  putreg32(regval, STM32_RTC_CR);

  regval = getreg32(STM32_RTC_CR);
  regval |= RTC_CR_WUTE;      // Wakeup Timer Enable
  putreg32(regval, STM32_RTC_CR);
  while ((getreg32(STM32_RTC_ISR) & RTC_ISR_WUTWF) != 0);

  rtc_exitinit();    // JUST TESTING
  // Enable wakeup timer and disable changes
  rtc_wprlock();
  // putreg32(0xff, STM32_RTC_WPR);

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
