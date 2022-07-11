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

#include "stm32_fmc.h"    // Needed for SDRAM access

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>

#include "pwrmgmt_local.h"

// These 3 are needed for otg register access. This allows the USB transceiver
// to be turned off during low-power modes.
#include "chip/stm32f76xx77xx_memorymap.h"
// #include "stm32_otg.h" introduces a build warning due to the fact that
// a nuttx specific definition is here. This is the only line needed from
// stm32_otg.h. The file is located at /arch/arm/src/stm32f7/stm32_otg.h.
#  define STM32_OTG_BASE        STM32_USBOTGFS_BASE
#include "chip/stm32_otg.h"

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
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
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
// ISR this indicates that the F7 has completed the low-power mode. It is necessary to
// do a few things to get the F7 back to a running state.
static int meadow_rtc_wakeup_isr(int irq, FAR void *context, FAR void *arg)
{
  // Reconfigure the internal clocks. Restarts the clocks as defined in
  // board.h
  stm32_clockenable();

  // Restart Nuttx Systick
  up_enable_irq(STM32_IRQ_SYSTICK);

  // Clear the EXTI Pending Register for the wakeup event
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This call will put the F7 into stop mode
int pwrmgmt_enter_stop_mode(void)
{
  uint32_t regval;

  // FOR MEADOW WITH ETHERNET IT NEEDS TO BE POWERED DOWN TOO!
  // Might be clues in stmcube ETH_PhyEnterPowerDownMode

  // Turn-off USB OTG's power to its transceiver. This will cause the USB
  // serial port on the host PC (CLI) to cease to exist. This is the desired
  // behavior because without this action the USB serial gets corrupted when
  // entering low-power modes. When the low-power mode ends, the data sent to
  // the host PC over USB serial no longer arrives at the CLI. There may be a
  // more elegant solution. I tried dropping re-establishing the HCOM serial
  // connections to the CLI but that didn't solve the problem.
  regval = getreg32(STM32_OTG_GCCFG);
  regval &= ~(OTG_GCCFG_PWRDWN);
  putreg32(regval, STM32_OTG_GCCFG);

  // Unlock the locked RTC registers
  pwrmgmt_rtc_wprunlock();

  regval  = getreg32(STM32_PWR_CR1);

  // Clear the bits used to control the various power levels
  regval &= ~(PWR_CR1_LPDS);        // Bit 0:0=Main regulator vs Low-power
  regval &= ~(PWR_CR1_PDDS);        // Bit 1:0=Enter Stop, 1=Enter Standby
  regval &= ~(PWR_CR1_FPDS);        // Bit 9:1=Flash power off in Stop mode
  regval &= ~(PWR_CR1_LPUDS);       // Bit 10:1=Low-power regulator in under-drive
  regval &= ~(PWR_CR1_MRUDS);       // Bit 11:Main regulator in under-drive
  regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19:11=Under-drive, 00=disable
 
  // Setting the following seems to be the highest power savings for the stop
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

  // Relock the RTC registers
  pwrmgmt_rtc_wprlock();

  // Setup the ISR for the RTC wakeup timer counting down to 0. Every time
  // it reaches 0 an interrupt is generated.
  irq_attach(STM32_IRQ_RTC_WKUP, meadow_rtc_wakeup_isr, NULL);
  up_enable_irq(STM32_IRQ_RTC_WKUP);
  
#if MEADOW_PWRMGMT_SHOW_EXTRA_DEBUG_MSG > 0
  struct timespec abstime;
  struct tm tmNowOs;
  struct tm tmNowRtc;

  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowOs);

  syslog(2, "Before Stop:%4d-%02d-%02dT%02d:%02d:%02d RTC - %4d-%02d-%02dT%02d:%02d:%02d OS\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
            tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);
#endif

// #if defined (USE_MEADOW_DEBUG_HELPERS)
//   MEADOW_TRACE_DEBUG("====> Calling WFE -> Entering Stop-mode\n");
//   MEADOW_TRACE_DEBUG("------------------------------\n");
//   usleep(20 * 1000);
// #endif

  // syslog(1, "====> Calling WFE -> Entering Stop-mode\n");
  syslog(1, "------------------------------\n");
  usleep(20 * 1000);

  // // Calculate the CRC across some of the SDRAM. It will be checked after the
  // // stop-mode ends
  // regval = getreg32(STM32_CRC_CR);
  // regval |= CRC_CR_RESET;   // Setting resets the CRC hardware
  // putreg32(regval, STM32_CRC_CR);

  // // Run across some of the SDRAMs data
  // // 0xc0000000
  // for()
  // {
  //   regval = 
  //   putreg32(regval, STM32_CRC_DR);
  // }
  


  // Disabled Systick (it's re-enabled in ISR)
  up_disable_irq(STM32_IRQ_SYSTICK);

  // Force memory sync before wfe, thus ensuring that all instructions done
  // before entering STOP mode Data synchronous Barrier (DSB) just after the
  // write operation. This will force the CPU to respect the sequence of
  // instructions (no optimization).
  asm volatile ("dsb");
  asm volatile ("isb");

  // Put SDRAM into self-refresh mode so data isn't lost (saves current too).
  // This must follow all other activities because once in the self-refresh
  // mode, *ANY* SDRAM access will return the SDRAM to normal mode. This
  // includes function calls as these put the return address on the stack.
  putreg32(FMC_SDRAM_MODE_CMD_SELF_REFRESH | FMC_SDRAM_CMD_BANK_1, STM32_FMC_SDCMR);
  
  // Wait till busy flag is cleared
  // while ((getreg32(STM32_FMC_SDSR) & 0x00000020) != 0);
  putreg32(0x0000ffff, HCOM_NX_BATTERY_BACKED_REG_GP);
  while ((regval != 0) && (getreg32(HCOM_NX_BATTERY_BACKED_REG_GP)--) > 0)

  // Request Wait For Event
  asm volatile ("sev");    // Set event
  asm volatile ("wfe");    // Clear just set Event, we know our state now
  asm volatile ("wfe");    // This is the wait that "waits"

  // We are back from Stop-mode

  // SysTick was enabled in ISR. We won't need anymore wakeup interrupts.
  up_disable_irq(STM32_IRQ_RTC_WKUP);
  irq_detach(STM32_IRQ_RTC_WKUP);
  
  // MEADOW_TRACE_DEBUG("====> Running after being in Stop mode\n");
  syslog(1, "====> Running after Stop mode.\n");

  // Clear sleep control bits
  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_LPDS | PWR_CR1_PDDS);
  regval &= ~(PWR_CR1_UDEN_ENABLE | PWR_CR1_MRUDS | PWR_CR1_LPUDS);
  putreg32(regval, STM32_PWR_CR1);

  // Clear SLEEPDEEP bit of Cortex System Control Register. Otherwise any
  // WFI or WFE will become a SLEEPDEEP event. And most of the time WFI/WFE
  // are used to sleep the MCU core till the next interrupt.
  regval  = getreg32(NVIC_SYSCON);
  regval &= ~NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // Clear Wakeup timer flag
  pwrmgmt_rtc_wprunlock();
  regval = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_WUTF;
  putreg32(regval, STM32_RTC_ISR);
  pwrmgmt_rtc_wprlock();

  // Synch Nuttx clock with RTC hardware which, maintained time while stopped
  clock_synchronize();

  // Disable wakeup timer, therewise the wakeup timer will repeatedly timeout.
  meadow_pwr_mgmt_disable_wakeup_timer();

  // Turn on USB OTG's power to its transceiver to re-enable communications
  regval = getreg32(STM32_OTG_GCCFG);
  regval |= (OTG_GCCFG_PWRDWN);
  putreg32(regval, STM32_OTG_GCCFG);

#if MEADOW_PWRMGMT_SHOW_EXTRA_DEBUG_MSG > 0
  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowOs);

  syslog(2, "After Stop:%4d-%02d-%02dT%02d:%02d:%02d RTC - %4d-%02d-%02dT%02d:%02d:%02d OS\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
            tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);
#endif

  return OK;
}

#endif // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
