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

// Diagnostic only
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#define MEADOW_PWRMGMT_SHOW_EXTRA_DEBUG_MSG (0)

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
// ISR indicating that the F7 is now awake
static int meadow_rtc_wakeup_isr_handler_setup(int irq, FAR void *context, FAR void *arg)
{
  uint32_t regval = 0;

  // Clear SLEEPDEEP bit of Cortex System Control Register. Otherwise any
  // WFI or WFE will become a SLEEPDEEP event. And most of the time WFI/WFE
  // are used to sleep the MCU core till the next interrupt.
  regval  = getreg32(NVIC_SYSCON);
  regval &= ~NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // Clear sleep control bits
  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_LPDS | PWR_CR1_PDDS);
  regval &= ~(PWR_CR1_UDEN_ENABLE | PWR_CR1_MRUDS | PWR_CR1_LPUDS);
  putreg32(regval, STM32_PWR_CR1);

  // Reconfigure the internal clocks and enable nuttx systick. These must be
  // in ISR. Restart the clocks defined in board.h
  stm32_clockenable();

  // Restart Nuttx Systick
  up_enable_irq(STM32_IRQ_SYSTICK);

  // Clear Wakeup timer flag
  pwrmgmt_rtc_wprunlock();
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  pwrmgmt_rtc_wprlock();

  // Clear the EXTI Pending Register for the wakeup event
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int pwrmgmt_enter_stop_mode(void)
{
  uint32_t regval;

  pwrmgmt_rtc_wprunlock();
  regval  = getreg32(STM32_PWR_CR1);

  // Clear the bits used to control the various power levels
  regval &= ~(PWR_CR1_LPDS);        // Bit 0:0=Main regulator vs Low-power
  regval &= ~(PWR_CR1_PDDS);        // Bit 1:0=Enter Stop, 1=Enter Standby
  regval &= ~(PWR_CR1_FPDS);        // Bit 9:1=Flash power off in Stop mode
  regval &= ~(PWR_CR1_LPUDS);       // Bit 10:1=Low-power regulator in under-drive
  regval &= ~(PWR_CR1_MRUDS);       // Bit 11:Main regulator in under-drive
  regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19:11=Under-drive, 00=disable
 
  // Setting the following seem to be the highest power savings for the stop
  // mode. Without these the Meadow current drops to about 58 ma. With the
  // following settings added Meadow drops to about 52 ma.
  if(true)
  {
    regval |= PWR_CR1_LPDS;           // Low-power regulator on in Stop
    regval |= PWR_CR1_LPUDS;          // Low-power regulator in under-drive
    regval |= PWR_CR1_UDEN_ENABLE;    // Set both bits for underdrive
  }

  putreg32(regval, STM32_PWR_CR1);

  // Set SLEEPDEEP bit of Cortex System Control Register. This is the same
  // setting for Stop or Standby. PWR_CR1_PDDS controls Stop or Standby. This
  // setting determine to Sleep or Stop/Standby when WFI or WFE is executed.
  regval  = getreg32(NVIC_SYSCON);
  regval |= NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);
  
  // Setup the ISR for the RTC wakeup timer counting down to 0
  irq_attach(STM32_IRQ_RTC_WKUP, meadow_rtc_wakeup_isr_handler_setup, NULL);
  up_enable_irq(STM32_IRQ_RTC_WKUP);

  pwrmgmt_rtc_wprlock();

#if MEADOW_PWRMGMT_SHOW_EXTRA_DEBUG_MSG > 0
  struct timespec abstime;
  struct tm tmNowOs;
  struct tm tmNowRtc;

  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowOs);

  syslog(1, "Before Stop:%4d-%02d-%02dT%02d:%02d:%02d RTC - %4d-%02d-%02dT%02d:%02d:%02d OS\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
            tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);
#endif

#if defined (USE_MEADOW_DEBUG_HELPERS)
  MEADOW_TRACE_DEBUG("====> Calling WFE -> Stop-mode\n");
  usleep(20 * 1000);
#endif

  // Disabled Systick (it's re-enabled in ISR)
  up_disable_irq(STM32_IRQ_SYSTICK);

  // Force memory sync before wfi/wfe
  // Ensure that all instructions done before entering STOP mode
  // Data synchronous Barrier (DSB) just after the write operation. This
  // will force the CPU to respect the sequence of instruction (no
  // optimization).
  asm volatile ("dsb");
  asm volatile ("isb");

  // Request Wait For Event
  asm volatile ("sev");    // Set event
  asm volatile ("wfe");    // Clear just set Event, we know our state
  asm volatile ("wfe");    // This is the wait that "waits"

  // We are back from Stop-mode

  // Synch Nuttx clock with RTC hardware which maintained time while stopped
  clock_synchronize();

  MEADOW_TRACE_DEBUG("====> Running after being in Stop mode\n");

  // Make this a one shot event. Otherwise the wakeup timer will repeatedly
  // timeout.
  meadow_pwr_mgmt_disable_wakeup_timer();

#if MEADOW_PWRMGMT_SHOW_EXTRA_DEBUG_MSG > 0
  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowOs);

  syslog(1, "After Stop:%4d-%02d-%02dT%02d:%02d:%02d RTC - %4d-%02d-%02dT%02d:%02d:%02d OS\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
            tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);
#endif

  return OK;
}

#endif // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
