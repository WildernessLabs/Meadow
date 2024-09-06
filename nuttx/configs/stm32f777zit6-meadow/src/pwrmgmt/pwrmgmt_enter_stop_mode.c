/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_enter_stop_mode.c
 * 
 *   Copyright (C) 2022-2024 Wilderness Labs. All rights reserved.
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
// Meadow F7. It also calls a function that control the ESP32 sleep modes.

// Note: Nuttx has it's own power management implementation but after studying
// it, I decided to not use it because it made some assumptions about behavior
// that I thought were not in line with how Meadow was to operate. That said
// I did use the Nuttx implementation for "inspiration". Peter Moody 25Mar22

// The STM32F777 has 3 low power modes. This is their order, smallest power
// savings to largest. Meadow is currently using 'Stop' hence the name of
// this file.
// 1. Sleep
// 2. Stop
// 3. Standby

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>

#include "pwrmgmt_local.h"

// These 3 are needed for otg register access. This allows the USB transceiver
// to be turned off during low-power modes.
#include "chip/stm32f76xx77xx_memorymap.h"

// Need one line from here so that chip/stm32_otg.h will build
#include "stm32_otg.h"
#include "chip/stm32_otg.h"

#include "chip/stm32f76xx77xx_pwr.h"
#include "chip/stm32_exti.h"
#include "stm32_fmc.h"          // Needed for SDRAM access

#include "nvic.h"

#include <arch/board/board.h>
#include "stm32_gpio.h"

#include "stm32f777zit6-meadow.h"
#include "hcom_nx/hcom_nx_common.h"

#include "stm32_alarm.h"

#if defined (CONFIG_POWER_MANAGEMENT_TESTS)
#pragma message "(--) pwrmgmt_enter_stop_mode.c"
#endif

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#define MEADOW_PWRMGMT_SHOW_RTC_NUTTX_TIME (0)

// Diagnostic
#pragma message "(--) pwrmgmt_enter_stop_mode.c"

#define PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO (1)

#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)

// DIAGNOSTIC GPIO
#define TEST_PIN_V2_D03_STOP_TEST (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz \
          | GPIO_PORTB | GPIO_PIN8)
#endif

// These are the other interrupt sources besides Alarm A
#define PWRMGMT_ALL_UNUSED_RTC_INTERRUPT_SRCS (RTC_ISR_ALRBF | RTC_ISR_WUTF \
          | RTC_ISR_TSF | RTC_ISR_TSOVF | RTC_ISR_TAMP1F | RTC_ISR_TAMP2F)

/************************************************************************************
 * Private Data
 ************************************************************************************/
// static char *thisFile = __FILE__;

enum MeadowWakeupReason_e
{
  wake_reason_unknown             = 0,    // Wakeup reason not known
  wake_reason_wakeup_time_reached = 1,    // Wakeup time reached
  wake_reason_gpio_caused_wakeup  = 2,    // GPIO interrupt caused wakeup
};

static bool _firstTime = true;
static bool _isMeadowInStopMode = false;
static enum MeadowWakeupReason_e _wakeupReason = wake_reason_unknown;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/
static int pwrmgmt_isr_shared_wakeup_code(void);

//====================================================================
// ISR called when the RTC generates an alarm, or the wakeup timer expires,
// indicating time to exit low-power mode.
static int meadow_rtc_wakeup_isr_handler(int irq, FAR void *context,
          FAR void *arg)
{
  uint32_t regval;
  uint32_t rtcIsr;

#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
  stm32_gpiowrite(TEST_PIN_V2_D03_STOP_TEST, false);
#endif

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Clear the EXTI Pending Register bit for the RTC Alarm
  regval = getreg32(STM32_EXTI_PR);
  regval |= (EXTI_RTC_ALARM); // Writing '1' clears
  putreg32(regval, STM32_EXTI_PR);

  // Clear the Alarm A flag
  // Per ES0334 - Rev 9 - 2.12.2 implemented the following pattern to check
  // the significant interrupt twice.
  bool AlarmA = false;
 #if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  rtcIsr = getreg32(STM32_RTC_ISR);
  if((rtcIsr & RTC_ISR_ALRAF) != 0)
  {
    AlarmA = true;
    rtcIsr &= ~(RTC_ISR_ALRAF);
    putreg32(rtcIsr, STM32_RTC_ISR);
  }
 #endif

  // Clear any other interrupt source that shares the EXTI line
  rtcIsr = getreg32(STM32_RTC_ISR);
  if((rtcIsr & PWRMGMT_ALL_UNUSED_RTC_INTERRUPT_SRCS) != 0)
  {
    rtcIsr &= ~(PWRMGMT_ALL_UNUSED_RTC_INTERRUPT_SRCS);
    putreg32(rtcIsr, STM32_RTC_ISR);
  }

 #if (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 0)
  // Check the Alarm A flag again (per Errata)
  if(!AlarmA)
  {
    rtcIsr = getreg32(STM32_RTC_ISR);
    if((rtcIsr & RTC_ISR_ALRAF) != 0)
    {
      AlarmA = true;
      rtcIsr &= ~(RTC_ISR_ALRAF);
      putreg32(rtcIsr, STM32_RTC_ISR);
    }
  }

 #elif (PWRMGMT_LOW_PWR_0_USE_RTC_ALARM_A == 1)
    #error "Only Alarm A supported in module"
 #endif

#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Clear the EXTI Pending Register for the Wakeup Timer
  regval = getreg32(STM32_EXTI_PR);
  regval &= ~(EXTI_RTC_WAKEUP);
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);

  // NOTE: The following few lines of Alarm B code have never been tested.
  // Added when solving Issue #667 which only addressed waking up for RTC
  // Alarm.
  // Clear the Wakeup timer flag.
  rtcIsr = getreg32(STM32_RTC_ISR);
  if((rtcIsr & RTC_ISR_WUTF) != 0)
  {
    rtcIsr &= ~RTC_ISR_WUTF;
    putreg32(rtcIsr, STM32_RTC_ISR);
  }
#else
  #error "Select Power Management Low-Power scheme"
#endif

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // If Alarm A didn't caused interrupt exit
  if(!AlarmA)
    return OK;
#endif

  _wakeupReason = wake_reason_wakeup_time_reached;

  return pwrmgmt_isr_shared_wakeup_code();
}

//======================================================================
// This public function is called from Meadow GPIO interrupt handling code.
// This allows a GPIO interrupt to be configured for stop mode wakeup.
int pwrmgmt_isr_gpio_wakeup_code()
{
  // This check is for the case when a GPIO interrupt is received while
  // Meadow is not in stop mode and for when multiple interrupts may be
  // generated via switch bounce.
  if(!_isMeadowInStopMode)
  {
    // Meadow is not currently in stop mode. Either being put into stop mode
    // or being waken or no stop mode request was ever initiated.
    return OK;
  }

  _wakeupReason = wake_reason_gpio_caused_wakeup;

  return pwrmgmt_isr_shared_wakeup_code();
}

//==================================================================
// This bit of code is shared by both RTC Alarm wakeup and GPIO interrupt
// wakeup notifications.
int pwrmgmt_isr_shared_wakeup_code()
{
  // Reconfigure the internal clocks. Restarts the clocks as defined in
  // board.h. Starting these clocks, will allow the remaining wakeup code
  // to be executed.
  stm32_clockenable();

  // // Restart Nuttx Systick
  // up_enable_irq(STM32_IRQ_SYSTICK);

  // Don't leave ISR until the above have fully finished
  asm volatile ("dsb");

  return OK;
}

//=======================================================================
// This public function will put the F7 into stop mode
int pwrmgmt_enter_stop_mode(void)
{
  uint32_t regval;

  // One time initialization
  if(_firstTime)
  {
    _firstTime = false;

#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
    stm32_configgpio(TEST_PIN_V2_D03_STOP_TEST);
    stm32_gpiowrite(TEST_PIN_V2_D03_STOP_TEST, false);
#endif
  }

  // Reset the wakeup reason
  _wakeupReason = wake_reason_unknown;

  // ETHERNET POWERED DOWN
  // See Ref Man section 42.5.8, step-by-step at page bottom.
  // Might be clues in stmcube ETH_PhyEnterPowerDownMode. The main savings
  // would be the PHY chip, if Ethernet implemented.
  // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD) && defined(CONFIG_NETDEV_LATEINIT)
  //   if(meadow_hw_version_ethernet_supported())
  //   {
  //     // Ethernet Supported
  //   }
  //   #endif

  // SD-CARD POWER DOWN
  // See Ref Man section 39.8.1 SDMMC power control register and 39.8.2 SDMMC
  // clock control register bit 9. These may save only a bit of power, but we
  // don't want any on going SD Card activity to corrupt the SD Card's data.

  // ESP32 POWER DOWN
  // ToDo: espcp_low_power_sleep();

  //Disabled System tick early so the scheduler won't do any context switching
  up_disable_irq(STM32_IRQ_SYSTICK);

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

  // Clear the bits used to control the various power regulators
  regval &= ~(PWR_CR1_LPDS);        // Bit 0:0=Main regulator vs Low-power
  regval &= ~(PWR_CR1_PDDS);        // Bit 1:0=Enter Stop, 1=Enter Standby
  regval &= ~(PWR_CR1_FPDS);        // Bit 9:1=Flash power off in Stop mode
  regval &= ~(PWR_CR1_LPUDS);       // Bit 10:1=Low-power regulator in under-drive
  regval &= ~(PWR_CR1_MRUDS);       // Bit 11:Main regulator in under-drive
  regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19:11=Under-drive, 00=disable
 
  // Setting the following seems to be the highest power savings for the stop
  // mode. Without these the Meadow current drops to about 58 ma. With the
  // following settings added Meadow drops to about 52 ma.
  // See Table 19 in Ref Man for information
  regval |= PWR_CR1_LPDS;           // Low-power regulator on in Stop
  regval |= PWR_CR1_LPUDS;          // Low-power regulator in under-drive
  regval |= PWR_CR1_UDEN_ENABLE;    // Set both bits for under-drive
  putreg32(regval, STM32_PWR_CR1);

  // Set SLEEPDEEP bit of Cortex System Control Register. This is the same
  // setting for Stop or Standby. PWR_CR1_PDDS controls Stop or Standby. This
  // setting determines if Sleep or Stop/Standby when WFI or WFE is executed.
  // See PM0253 Programming manual for more details
  regval  = getreg32(NVIC_SYSCON);
  regval |= NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // Relock the RTC registers
  pwrmgmt_rtc_wprlock();

  // RTC Alarm and Wakeup Timer share the same ISR handler
#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Setup the ISR for the RTC alarm when date/time match. When the date and
  // time match an interrupt is generated.
  // Note: The same ISR is used for both RTC Alarm and Wakeup Timer.
  irq_attach(STM32_IRQ_RTCALRM, meadow_rtc_wakeup_isr_handler, NULL);
  up_enable_irq(STM32_IRQ_RTCALRM);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Setup the ISR for the RTC wakeup timer counting down to 0. Every time
  // it reaches 0 an interrupt is generated.
  irq_attach(STM32_IRQ_RTC_WKUP, meadow_rtc_wakeup_isr_handler, NULL);
  up_enable_irq(STM32_IRQ_RTC_WKUP);
#else
#error "Select Low-Power timing scheme"
#endif

#if MEADOW_PWRMGMT_SHOW_RTC_NUTTX_TIME > 0
  struct timespec abstime;
  struct tm tmNowOs;
  struct tm tmNowRtc;

  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowOs);

  syslog(2, "Before Stop:RTC-%4d-%02d-%02dT%02d:%02d:%02d, Nuttx-%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
            tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);
#endif

  // //Disabled Systick (it's re-enabled in ISR)
  // up_disable_irq(STM32_IRQ_SYSTICK);

  // Put SDRAM into self-refresh mode so data isn't lost (saves current).
  // This must follow all other activities because once in the self-refresh
  // mode, *ANY* SDRAM access will return the SDRAM to normal mode.
  // Wait for SDRAM to not be busy
  while ((getreg32(STM32_FMC_SDSR) & 0x00000020) != 0);

  putreg32(FMC_SDRAM_MODE_CMD_SELF_REFRESH | FMC_SDRAM_CMD_BANK_1, STM32_FMC_SDCMR);
  
  // Wait again till busy flag is cleared and SDRAM is fully in self-refresh
  while ((getreg32(STM32_FMC_SDSR) & 0x00000020) != 0);

  // DIAGNOSTIC-LED on only when sleeping the waking interrupt will turn this off
#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
  stm32_gpiowrite(TEST_PIN_V2_D03_STOP_TEST, true);
#endif

  _isMeadowInStopMode = true;

  // Force memory sync before wfe, thus ensuring that all instructions done
  // before entering the STOP mode Data Synchronous Barrier (DSB) just after
  // the write operation. This will force the CPU to respect the sequence of
  // instructions (no optimization).
  asm volatile ("dsb");   // All memory access needs to be completed
  asm volatile ("isb");   // Throw away prefetched instructions, execute in order

  // Put into stop-mode
  asm volatile ("sev");    // Set an event
  asm volatile ("wfe");    // Clear just set Event, we know our state now
  asm volatile ("wfe");    // This is the wait that forces low-power to begin

  //----------------------------------------------------------------------
  // The calling thread is stopped here while in Stop Mode
  //----------------------------------------------------------------------
  //
  // Meadow is running again. ISR has handled starting all the necessary
  // clocks, that must be in the ISR handler or things don't start
  // correctly.
  // Restore all the needed register values.
  _isMeadowInStopMode = false;

  // Clear power control bits in Power Controller register
  regval  = getreg32(STM32_PWR_CR1);
  regval &= ~(PWR_CR1_LPDS | PWR_CR1_PDDS);
  regval &= ~(PWR_CR1_UDEN_ENABLE | PWR_CR1_MRUDS | PWR_CR1_LPUDS);
  putreg32(regval, STM32_PWR_CR1);
  
  // Clear SLEEPDEEP bit of Cortex System Control Register. Otherwise any
  // WFI or WFE will become a SLEEPDEEP event. And normally WFI/WFE
  // are used to Sleep the MCU core (not Stop/Standby).
  regval  = getreg32(NVIC_SYSCON);
  regval &= ~NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // We won't need anymore waking up or interrupts
#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Disable RTC Alarm 
  pwrmgmt_disable_rtc_alarm_wakeup();

  up_disable_irq(STM32_IRQ_RTCALRM);
  irq_detach(STM32_IRQ_RTCALRM);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Disable Wakeup Timer
  pwrmgmt_disable_wakeup_timer_wakeup();

  up_disable_irq(STM32_IRQ_RTC_WKUP);
  irq_detach(STM32_IRQ_RTC_WKUP);
#else
#error "Select Low-Power timing scheme"
#endif

  // Synch Nuttx clock with RTC hardware, that keeps time while in stop
  // mode. The RTC clock may drift because the Meadow doesn't have a crystal
  // or resonator for the LSE clock. Therefore, we're using the LSI clock
  // which will drift.
  clock_synchronize();

  // Turn on USB OTG's power to its transceiver to re-enable communications
  regval = getreg32(STM32_OTG_GCCFG);
  regval |= (OTG_GCCFG_PWRDWN);
  putreg32(regval, STM32_OTG_GCCFG);

  // Put ESP32 into its normal running mode
  // ToDo: espcp_low_power_wakeup();

  // Restore Ethernet to operation

  // Restore SD Card to operation

  // Restart Nuttx Systick so the scheduler can switch to other threads
  up_enable_irq(STM32_IRQ_SYSTICK);

#if MEADOW_PWRMGMT_SHOW_RTC_NUTTX_TIME > 0
  struct timespec abstime2;
  struct tm tmNowOs2;
  struct tm tmNowRtc2;

  up_rtc_getdatetime(&tmNowRtc2);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime2);  // Nuttx internal time
  gmtime_r(&abstime2.tv_sec, &tmNowOs2);

  syslog(2, "Awake! - RTC-%4d-%02d-%02dT%02d:%02d:%02d, Nuttx-%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmNowRtc2.tm_year + 1900, tmNowRtc2.tm_mon + 1, tmNowRtc2.tm_mday,
            tmNowRtc2.tm_hour, tmNowRtc2.tm_min, tmNowRtc2.tm_sec,
            tmNowOs2.tm_year + 1900, tmNowOs2.tm_mon + 1, tmNowOs2.tm_mday,
            tmNowOs2.tm_hour, tmNowOs2.tm_min, tmNowOs2.tm_sec);
#endif

  return OK;
}

//================================================================
// This public function returns the wakeup reason to managed code or any
// other caller. It returns a simple integer.
// 0 = Wakeup reason not known
// 1 = Wakeup time reached
// 2 = GPIO interrupt caused wakeup
int pwrmgmt_most_recent_wakeup_reason(void)
{
  return (int)_wakeupReason;
}

#endif // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
