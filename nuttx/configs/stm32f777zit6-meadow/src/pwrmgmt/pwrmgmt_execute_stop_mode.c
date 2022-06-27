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

//==================================================================
// ISR indicating that the F7 is now awake
// static int meadow_isr_rtc_wakeup_handler_tests(int irq, FAR void *context,
//                                     FAR void *arg)
// {
//   uint32_t regval = 0;

// syslog(1, "===> Wakeup ISR executing\n");
//   rtc_wprunlock();

//   // Clear Wakeup timer flag
//   regval = getreg32(STM32_RTC_ISR);
//   regval &= ~RTC_ISR_WUTF;
//   putreg32(regval, STM32_RTC_ISR);

//   rtc_wprlock();

//   return OK;
// }

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int pwrmgmt_execute_stop_mode(bool lowestPwr, bool useInterrups)
{
  uint32_t regval;

  rtc_wprunlock();    // TESTING

  //------------------------------------------------------------
  // COPIED FROM STM32_PMSTOP() /arch/arm/src/stm32f7/stm32_pmstop.c
  // The Power Down Deep Sleep (PDDS) bit determines if we enter Stop or
  // Standby modes. So, clear the bit for Stop mode.
  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_PDDS);  // Clear the Power Down Deep Sleep (PDDS)

  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_LPDS | PWR_CR1_PDDS | PWR_CR1_FPDS);
  regval &= ~(PWR_CR1_UDEN_ENABLE | PWR_CR1_MRUDS | PWR_CR1_LPUDS);

  /* Set under-drive enabled with low-power regulator.  */

  // if (lpds)
  if (false)
    {
      regval |= PWR_CR1_UDEN_ENABLE | PWR_CR1_LPUDS | PWR_CR1_LPDS;
    }
  putreg32(regval, STM32_PWR_CR1);
  
  //-----------------------------------------------------------

  // // PeterM - LOOKS LIKE ROOM FOR IMPROVEMENT HERE, BITS ARE CLEARED THAT
  // // ARE ALWAYS RESET....
  // // Clear all the bits used to control the power state
  // regval &= ~(PWR_CR1_LPDS);        // Bit 0: Low-power deepsleep
  // regval &= ~(PWR_CR1_FPDS);        // Bit 9: Flash power down in Stop mode

  // // Clear and re-set if needed for lowest power
  // regval &= ~(PWR_CR1_LPUDS);       // Bit 10: Low-power regulator in deepsleep under-drive mode
  // regval &= ~(PWR_CR1_MRUDS);       // Bit 11: Main regulator in deepsleep under-drive mode
  // regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19: Under-drive
 
  // // The stop mode has a lot of optional power saving opportunites by using
  // // the UDEN, MRUDS, LPUDS, LPDS and FPDS bits.
  // // The followwing 2 options seem to be the highest and lowest power savings
  // // options for the stop mode.
  // if(lowestPwr)
  // {
  //   // Meadow drops to about 52 ma
  //   // With the STOP ULP-FPD voltage Regulator mode saving the most power.
  //   // Save the most power
  //   // Set the Low Power Deep Sleep (LPDS) bit to keep stop the Main voltage
  //   // regulator and enable the Low-power voltage regulator.
  //   regval |= PWR_CR1_LPDS;         // Bit 9: Flash power down in Stop mode
  //   regval |= PWR_CR1_LPUDS;        // Bit 10: Low-power regulator in deepsleep under-drive mode
  //   regval |= PWR_CR1_UDEN_ENABLE;  // Bits 18-19: Under-drive enable
  // }
  // else
  // {
  //   // Meadow drops to about 58 ma
  //   // Have the fastest startup clear by clearing these bit fields
  //   regval &= ~(PWR_CR1_MRUDS | PWR_CR1_LPDS | PWR_CR1_FPDS);
  // }
  // putreg32(regval, STM32_PWR_CR1);
  // // PeterM - end LOOKS LIKE ROOM FOR IMPROVEMENT HERE, BITS ARE CLEARED THAT


  // Set SLEEPDEEP bit of Cortex System Control Register to enable interrupts
  // When using events this is not needed as events set nothing
  regval  = getreg32(NVIC_SYSCON);    // 0x0000 0000 0000 0d10
  // regval |= NVIC_SYSCON_SLEEPONEXIT;  //
  regval |= NVIC_SYSCON_SLEEPDEEP;    // Stop not Standby
  putreg32(regval, NVIC_SYSCON);
  
  syslog(1, "====> CALLING WFE/WFI after 2 seconds\n");
  sleep(2);

  rtc_wprlock();    // TESTING

  // Force memory sync before wfi/wfe
  // Ensure that all instructions done before entering STOP mode
  // Data synchronous Barrier (DSB) just after the write operation. This
  // will force the CPU to respect the sequence of instruction (no
  // optimization).
  asm volatile ("dsb");
  asm volatile ("isb");

  syslog(1, "====> Stopping systick\n");
  up_disable_irq(STM32_IRQ_SYSTICK);

  if(useInterrups)
  {
    syslog(1, "====> CALLING WFI\n");
    usleep(20* 1000);

    asm volatile ("wfi");
  }
  else
  {
    // Request Wait For Event
    asm volatile ("sev");    // Set event
    asm volatile ("wfe");    // Event set so doesn't wait, we know our state
    syslog(1, "====> SEV and WFE #1 returned. CALLING final WFE, good bye\n");
    usleep(20* 1000);

    asm volatile ("wfe");    // This is the wait that "waits"
  }

  // Reconfigure the internal clocks
  // arch/arm/src/stm32f7/stm32f76xx77xx_rcc.c
  stm32_clockenable();

  clock_synchronize();

  syslog(1, "====> Re-starting systick\n");
  up_enable_irq(STM32_IRQ_SYSTICK);

  // Clear Wakeup timer flag
  rtc_wprunlock();
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  rtc_wprlock();

  // Make this a one shot event. Otherwise the wakeup timer will repeatedly
  // timeout.
  meadow_pwr_mgmt_disable_wakeup_timer();

  syslog(1, "=====> WOKEUP FROM STOP\n");
  usleep(20* 1000);

// PeterM-ADDED TO EXPERIMENT WITH WHY IT ISN'T WORKING TAKEN FORM NUTTX
// DIDN'T CHANGE BEHAVIOR BUT LEAVING BECAUSE THE COMMENTS MAKE SENSE

  /* Clear deep sleep bits, so that MCU does not go into deep sleep in idle. */

  /* Clear the Power Down Deep Sleep (PDDS), the Low Power Deep Sleep
   * (LPDS) bits, Under-Drive Enable in Stop Mode (UDEN), Main Regulator in
   * Deepsleep Under-Drive Mode (MRUDS), and Low-power Regulator in Deepsleep
   * Under-Drive Mode (LPUDS) in the power control register.
   */

  rtc_wprunlock();    // TESTING
  // Clear SLEEPDEEP bit of Cortex System Control Register
  regval  = getreg32(NVIC_SYSCON);
  regval &= ~NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // rtc_wprunlock();
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  // rtc_wprlock();

  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_LPDS | PWR_CR1_PDDS);
  regval &= ~(PWR_CR1_UDEN_ENABLE | PWR_CR1_MRUDS | PWR_CR1_LPUDS);
  putreg32(regval, STM32_PWR_CR1);
// PeterM-END

  rtc_wprlock();    // TESTING
  return OK;
}

#endif // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
