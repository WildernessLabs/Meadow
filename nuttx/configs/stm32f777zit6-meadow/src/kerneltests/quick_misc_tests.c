/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\quick_misc_tests.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx/hcom_nx_common.h"

#include <meadow/hcom_shared_common.h>

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#if defined(CONFIG_QUICK_MISC_TESTS)
#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// D05 (PB4) for Version 2 Feather or CCM V1
// For Testing wanted a pin that was Px0-4 to more easily figure out interrupts
// and because these are a high priority interrupts.
#define QUICK_MISC_PIN_V2_D05  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)

// It uses STM32_IRQ_EXTI4 (26) [position in Table 46 is 10 + 16 = 26]
// NVIC_IRQ0_31_PEND_OFFSET (IRQ 0 - 31 pending alarm register address is
// 0xe000e200).
// NVIC_IRQ0_31_CLRPEND_OFFSET (for IRQ 0 - 31 this is the offset to clear
// pending in the NVIC). And its register address is 0xe000e280. And the
// NVIC interrupt clear pending bit is 0x04000000 (bit 26)

// For any Px4 GPIO
// Find values in these locations
// nuttx/arch/arm/include/stm32f7/stm32f76xx77xx_irq.h
// nuttx/arch/arm/src/armv7-m/nvic.h
// nuttx/arch/arm/include/stm32f7/irq.h
#define QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT (1 << STM32_IRQ_EXTI4)

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

static void quick_misc_test_setup_interrupt_for_wakeup(void);

/************************************************************************************
 * Private Functions
 ************************************************************************************/

// Testing ISR
static int quick_misc_test_wakeup_stop_mode_isr(int irq, void *context, void *arg)
{
  DEBUG_SET_HIGH(DEBUG_PIN_V2_D15);

  // At this point the correct STM32_EXTI_PR bit has been cleared by Nuttx in
  // it's "first layer" ISR. According to the Ref Man we still need to set a
  // bit in the NVIC interrupt clear pending register

  // This is an experiment for PB4 (or any other Px4 GPIO)
  // syslog(1, "==> ISR Wakeup Misc test code\n");

// -----------------------------------------------------------
// START COPIED FROM NORMAL ISR
  // Reconfigure the internal clocks. Restarts the clocks as defined in
  // board.h. These clocks are what run the entire MCU.
  stm32_clockenable();

  // Restart Nuttx Systick
  up_enable_irq(STM32_IRQ_SYSTICK);

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Clear the EXTI Pending Register for the RTC Alarm
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Clear the EXTI Pending Register for the Wakeup Timer
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);
#else
  #error "Select Power Management Low-Power scheme"
#endif

  // Don't leave ISR until the above have finished
  asm volatile ("dsb");

// END COPIED FROM NORMAL ISR
// -----------------------------------------------------------
  // // Read NVIC interrupt active and pending register
  // uint32_t irq0_31Pend = getreg32(NVIC_IRQ0_31_PEND);
  // uint32_t irq0_31Actv = getreg32(NVIC_IRQ0_31_ACTIVE);
  // syslog(1, "==> ISR Before reset NVIC IRQ0_31, Active:0x%08x, Pending:0x%08x\n", irq0_31Actv, irq0_31Pend);
  
  // // NVIC interrupt clear pending bit-26
  // putreg32(QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT, NVIC_IRQ0_31_CLRPEND);
  
  // // Disable interrupt
  // putreg32(QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT, NVIC_IRQ0_31_CLEAR)  ;

  // irq0_31Pend = getreg32(NVIC_IRQ0_31_PEND);
  // syslog(1, "==> ISR After reset NVIC IRQ0_31_Pending:0x%08x\n", irq0_31Pend);

  DEBUG_SET_LOW(DEBUG_PIN_V2_D15);

  return OK;
}

// ============================================================================
// This test is used to determine if an interrupt can wakeup the F7 from a
// low-power mode. Specifically stop mode.
static void quick_misc_test_setup_interrupt_for_wakeup(void)
{
  // Setup D00 to generate an interrupt
  int ret;

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D14);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D15);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D15);

  // Configure input point
  ret = stm32_configgpio(QUICK_MISC_PIN_V2_D05);
  if(ret < 0)
  {
    syslog(1, "Error#1 in quick_misc_test_setup_interrupt_for_wakeup. ret:%d errno:%d\n", ret, errno);
  }

  // Setup for interrupts
  ret = stm32_gpiosetevent(
  QUICK_MISC_PIN_V2_D05,            // Nuttx cfgset
  true,                             // risingEdge,
  false,                            // fallingEdge,
  false,   // quick_misc_test_wakeup_stop_mode_isr,  // event    <-- ADDED THIS 2:11
  quick_misc_test_wakeup_stop_mode_isr,  // ISR 
  NULL);                            // arg for ISR
  if(ret < 0)
  {
    syslog(1, "Error#2 in quick_misc_test_setup_interrupt_for_wakeup. ret:%d errno:%d\n", ret, errno);
  }

  // Expect 0x04000000 bit 26
  // syslog(1, "==> Setting NVIC ENABLE. GPIO value:0x%08x\n", QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT);

  // // EXPERIMENT - TURN ON 'Send Event on Pending' bit
  // // uint32_t regval  = getreg32(NVIC_SYSCON);
  // // regval |= NVIC_SYSCON_SEVONPEND;
  // // putreg32(regval, NVIC_SYSCON);

  // // Setup NVIC for PB4 - EXTI4 - NVIC_IRQ0_31_ENABLE / NVIC_IRQ0_31_CLEAR
  // putreg32(QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT, NVIC_IRQ0_31_ENABLE);
  // uint32_t regval = getreg32(NVIC_IRQ0_31_ENABLE);
  // syslog(1, "==> Re-read NVIC ENABLE. GPIO value:0x%08x\n", regval);
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
void meadow_kt_quick_misc_tests(uint32_t userData)
{
  static bool onlyOnce = true;

  switch(userData)
  {
    case 1:
      syslog(1, "Received 'set developer -d 11 -v 1' \n");
      if(onlyOnce)
      {
        onlyOnce = false;
        quick_misc_test_setup_interrupt_for_wakeup();
      }
      else
      {
        syslog(1, "Only once\n");
      }
      break;
    
    case 2:
      syslog(1, "Received 'set developer -d 11 -v 2'- Sleep for 10 seconds\n");
      DEBUG_SET_HIGH(DEBUG_PIN_V2_D14);
      pwrmgmt_enter_stm32f7_stop_mode(10);    // Sleep 10 seconds
      DEBUG_SET_LOW(DEBUG_PIN_V2_D14);
      break;
      
    default:
      syslog(1, "Undefined test for meadow_kt_quick_misc_tests, userData:%lu\n", userData);
      break;
  }
}
#endif  // #if defined(CONFIG_QUICK_MISC_TESTS)
