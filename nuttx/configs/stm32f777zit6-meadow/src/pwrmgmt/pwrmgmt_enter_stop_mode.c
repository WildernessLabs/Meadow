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
// I did use the Nuttx implemention for "inspirition". Peter Moody 25Mar22

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

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#define MEADOW_PWRMGMT_SHOW_RTC_NUTTX_TIME (0)

// Diagnostic
#pragma message "(--) pwrmgmt_enter_stop_mode.c"

#define PWRMGMT_BOOST_PRIORITY_OF_CALLER (1)
#define PWRMGMT_BOOST_PRIORITY_VALUE (253)

#define PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT (1)
#define PWRMGMT_ADD_SEMAPHORE_TO_CONTROL_ENTRY (1)
#define PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO (1)

#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
// DIAGNOSTIC GPIO
#define TEST_PIN_V2_D03_STOP_TEST (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_100MHz|GPIO_PORTB | GPIO_PIN8)
#endif

/************************************************************************************
 * Private Data
 ************************************************************************************/
// static char *thisFile = __FILE__;

#if (PWRMGMT_ADD_SEMAPHORE_TO_CONTROL_ENTRY > 0)
static sem_t _stopModeEntry;
#endif

#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
static irqstate_t _flags;
#endif

enum MeadowWakeupReason_e
{
  wake_reason_unknown             = 0,    // Wakeup reason not known
  wake_reason_wakeup_time_reached = 1,    // Wakeup time reached
  wake_reason_gpio_caused_wakeup  = 2,    // GPIO interrupt caused wakeup
};

static bool _firstTime = true;
static bool _meadowIsSleeping = false;
static enum MeadowWakeupReason_e _wakeupReason = wake_reason_unknown;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// ISR called when the RTC generates an alarm, or the wakeup timer expires,
// thus, indicating time to exit low-power mode.
static int meadow_rtc_wakeup_isr_handler(int irq, FAR void *context, FAR void *arg)
{
#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
  stm32_gpiowrite(TEST_PIN_V2_D03_STOP_TEST, false);
#endif

  return pwrmgmt_isr_shared_wakeup_code(false);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This public function is executed from the local ISR and from the Meadow
// interrupt handling code. Thus allowing a GPIO interrupt to be configured
// for wakeup.
// This function gets Nuttx started. Once that happens Nuttx will call code in
// this module that completes the wakeup sequence.
int pwrmgmt_isr_shared_wakeup_code(bool gpioWakeup)
{
  // Stop additional interrupts until fully up
#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
  _flags = enter_critical_section();
#endif

  if(gpioWakeup)
  {
    _wakeupReason = wake_reason_gpio_caused_wakeup;
  }
  else
  {
    _wakeupReason = wake_reason_wakeup_time_reached;
  }

  // This check is primarily for GPIO interrupt wakeup, in the case it is
  // interrupted while not sleeping, or generates multiple interrupts via
  // switch bounce.
  if(!_meadowIsSleeping)
  {
#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
    leave_critical_section(_flags);
#endif
    return OK;
  }

  // Reconfigure the internal clocks. Restarts the clocks as defined in
  // board.h. Starting these clocks, will allow the  remaining wakeup code
  // to be executed.
  stm32_clockenable();

  // Restart Nuttx Systick
  up_enable_irq(STM32_IRQ_SYSTICK);

  // If waking up from GPIO interrupt don't want to clear RTC register?
  if(! gpioWakeup)
  {
#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
    // Clear the EXTI Pending Register for the RTC Alarm
    putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
    // Clear the EXTI Pending Register for the Wakeup Timer
    putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);
#else
    #error "Select Power Management Low-Power scheme"
#endif
  }

  // Don't leave ISR until the above have fully finished
  asm volatile ("dsb");

  return OK;
}

// =======================================================================
// This call will put the F7 into stop mode
int pwrmgmt_enter_stop_mode(void)
{
  uint32_t regval;
#if (PWRMGMT_BOOST_PRIORITY_OF_CALLER > 0)
  struct sched_param schedParam;
  pthread_attr_t attr;
  int origThreadPri;
#endif

  // One time initialization
  if(_firstTime)
  {
    _firstTime = false;

    // Semaphore to only allow single thread here
#if (PWRMGMT_ADD_SEMAPHORE_TO_CONTROL_ENTRY > 0)
    sem_init(&_stopModeEntry, 0, 1);
#endif

#if (PWRMGMT_ADD_LEDS_FOR_DIAGNOSTIC_INFO > 0)
    // DIAGNOSTIC-Init diagnostic GPIO
    stm32_configgpio(TEST_PIN_V2_D03_STOP_TEST);
    stm32_gpiowrite(TEST_PIN_V2_D03_STOP_TEST, false);
#endif
  }

#if (PWRMGMT_ADD_SEMAPHORE_TO_CONTROL_ENTRY > 0)
  // Get the semaphore to insure only one caller at a time
  do
  {
    int ret;
    ret = sem_trywait(&_stopModeEntry);
    if(ret == OK)
      break;

    if(errno == -EINTR)
      continue;

    return -EALREADY;    // Error exit

  } while(true);
#endif

  // Stop all interrupts
#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
  _flags = enter_critical_section();
#endif

#if (PWRMGMT_BOOST_PRIORITY_OF_CALLER > 0)
// TODO: CHECK IF THIS IS A pthread. IF NOT EXIT OR SKIP BOOSTING PRIORITY CODE
  // Boost the priority of the calling pthread
  pthread_attr_init(&attr);
  (void)pthread_attr_getschedparam(&attr, &schedParam);
  origThreadPri = schedParam.sched_priority;
  schedParam.sched_priority = PWRMGMT_BOOST_PRIORITY_VALUE;
  (void)pthread_attr_setschedparam(&attr, &schedParam);
#endif

  // Reset the wakeup reason
  _wakeupReason = wake_reason_unknown;

  // ETHERNET POWERED DOWN
  // See Ref Man section 42.5.8, step-by-step in at the bottom.
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
  // setting determine to Sleep or Stop/Standby when WFI or WFE is executed.
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

  //Disabled Systick (it's re-enabled in ISR)
  up_disable_irq(STM32_IRQ_SYSTICK);

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

  _meadowIsSleeping = true;

  // Need an interrupt to wake from stop mode
#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
  leave_critical_section(_flags);
#endif

  // Force memory sync before wfe, thus ensuring that all instructions done
  // before entering the STOP mode Data synchronous Barrier (DSB) just after
  // the write operation. This will force the CPU to respect the sequence of
  // instructions (no optimization).
  asm volatile ("dsb");
  asm volatile ("isb");

  // Put into stop-mode
  asm volatile ("sev");    // Set an event
  asm volatile ("wfe");    // Clear just set Event, we know our state now
  asm volatile ("wfe");    // This is the wait that forces low-power to begin

  //----------------------------------------------------------------------
  // The calling thread is stopped here while in Stop Mode
  //----------------------------------------------------------------------

  // Meadow is running again. ISR has handled starting all the necessary
  // clocks These must be in the ISR handler or things don't start
  // correctly.
  // Restore all the needed register values.
  _meadowIsSleeping = false;

  // Clear sleep control bits in Power Controller registers
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

#if (PWRMGMT_ADD_CRITICAL_SECTION_SUPPORT > 0)
  // Okay to turn on interrupts again
  leave_critical_section(_flags);
#endif

  // Restore to original thread priority
#if (PWRMGMT_BOOST_PRIORITY_OF_CALLER > 0)
  (void)pthread_attr_getschedparam(&attr, &schedParam);
  schedParam.sched_priority = origThreadPri;
  (void)pthread_attr_setschedparam(&attr, &schedParam);
#endif

  // Turn on USB OTG's power to its transceiver to re-enable communications
  regval = getreg32(STM32_OTG_GCCFG);
  regval |= (OTG_GCCFG_PWRDWN);
  putreg32(regval, STM32_OTG_GCCFG);

  // Put ESP32 into its normal running mode
  // ToDo: espcp_low_power_wakeup();

  // Restore Ethernet to operation

  // Restore SD Card to operation

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

#if (PWRMGMT_ADD_SEMAPHORE_TO_CONTROL_ENTRY > 0)
  sem_post(&_stopModeEntry);
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
