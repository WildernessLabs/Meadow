/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_low_level.c
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

#include <syslog.h>

#include <meadow/hcom_shared_common.h>
#include "pwrmgmt_local.h"

#include "chip/stm32f76xx77xx_pwr.h"
#include "nvic.h"

#include <arch/board/board.h>
#include "stm32_gpio.h"

#include "stm32f777zit6-meadow.h"
#include "hcom_nx/hcom_nx_common.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

// Diagnostic only
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
#include "stm32_gpio.h"

#endif  // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_pwr_mgmt_enter_sleep(void);
static int meadow_pwr_mgmt_enter_stop(bool lowestPwr);
static int meadow_pwr_mgmt_enter_standby(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// 
int meadow_power_mgmt_initialize()
{
  int ret;

  // Initialize internal needs
  ret = pwrmgmt_lsi_cal_use_lsi_for_clock();

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_RED_LED);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_GREEN_LED);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_BLUE_LED);

  // High turns led off
  DEBUG_SET_HIGH(DEBUG_PIN_V2_RED_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_GREEN_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_BLUE_LED);

  DEBUG_SET_LOW(DEBUG_PIN_V2_RED_LED);

  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D06);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D07);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D08);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D09);
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D10);

  // DEBUG_SET_LOW(DEBUG_PIN_V2_D06);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D07);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D08);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D09);
  // DEBUG_SET_LOW(DEBUG_PIN_V2_D10);
#endif    // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

  return ret;
}

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
//===============================================================
// The RGB LEDs use power too
int meadow_pwr_mgmt_turn_off_leds()
{
  // Saves 0-6 ma
  DEBUG_SET_HIGH(DEBUG_PIN_V2_RED_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_GREEN_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_BLUE_LED);

  return OK;
}
#endif

// /****************************************************************************
//  * Public Functions
//  ****************************************************************************/
// Sleep mode saves little power but starts-up immediately
int meadow_pwr_mgmt_enter_sleep()
{
  uint32_t regval;

  regval = getreg32(NVIC_SYSCON);

  syslog(1, "==>>Entering Sleep mode. DeepSleep:0x%08x, SleepOnExit:%0x%08x \n",
            regval & NVIC_SYSCON_SLEEPDEEP, regval & NVIC_SYSCON_SLEEPONEXIT);
  sleep(1);

  // Clear SLEEPDEEP bit
  regval &= ~NVIC_SYSCON_SLEEPDEEP;
  
  // Clear SLEEPONEXIT bit
  regval &= ~NVIC_SYSCON_SLEEPONEXIT;

  putreg32(regval, NVIC_SYSCON);
  
  // Sleep till any interrupt
  asm volatile ("wfi");
  return OK;
}

//===============================================================
// Stop mode saves a moderate amount power but starts-up pretty fast
int meadow_pwr_mgmt_enter_stop(bool lowestPwr)
{
  uint32_t regval;

  regval  = getreg32(STM32_PWR_CR1);

  // The Power Down Deep Sleep (PDDS) bit determines if we enter Stop or
  // Standby modes. So, clear the bit for Stop mode.
  regval &= ~(PWR_CR1_PDDS);  // Clear the Power Down Deep Sleep (PDDS)

  // Clear all the bits used to control the power state
  regval &= ~(PWR_CR1_LPDS);        // Bit 0: Low-power deepsleep
  regval &= ~(PWR_CR1_FPDS);        // Bit 9: Flash power down in Stop mode
  regval &= ~(PWR_CR1_LPUDS);       // Bit 10: Low-power regulator in deepsleep under-drive mode
  regval &= ~(PWR_CR1_MRUDS);       // Bit 11: Main regulator in deepsleep under-drive mode
  regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19: Under-drive
 
  // The stop mode has a lot of optional power saving opportunites by using
  // the UDEN, MRUDS, LPUDS, LPDS and FPDS bits.
  // The followwing 2 options seem to be the highest and lowest power savings
  // options for the stop mode.
  if(lowestPwr)
  {
    // Meadow drops to about 52 ma
    // With the STOP ULP-FPD voltage Regulator mode saving the most power.
    // Save the most power
    // Set the Low Power Deep Sleep (LPDS) bit to keep stop the Main voltage
    // regulator and enable the Low-power voltage regulator.
    regval |= PWR_CR1_LPDS;         // Bit 9: Flash power down in Stop mode
    regval |= PWR_CR1_LPUDS;        // Bit 10: Low-power regulator in deepsleep under-drive mode
    regval |= PWR_CR1_UDEN_ENABLE;  // Bits 18-19: Under-drive enable
  }
  else
  {
    // Meadow drops to about 58 ma
    // Have the fastest startup clear by clearing these bit fields
    regval &= ~(PWR_CR1_MRUDS | PWR_CR1_LPDS | PWR_CR1_FPDS);
  }

  putreg32(regval, STM32_PWR_CR1);

  // Set SLEEPDEEP bit of Cortex System Control Register
  regval  = getreg32(NVIC_SYSCON);
  regval |= NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  // Stop till 
  asm volatile ("wfi");
  return OK;
}

//===============================================================
// Standby mode saves the most power but starts-up as if MCU reset
int meadow_pwr_mgmt_enter_standby()
{
  uint32_t regval;

  // If the Battery backup regulator is needed in standby mode then set
  // the BRE (backup regulator enable) bit.
  // From ref:  When set, the Backup regulator (used to maintain backup
  // SRAM content in Standby and VBAT modes) is enabled. If BRE is reset,
  // the backup regulator is switched off. The backup SRAM can still be
  // used but its content will be lost in the Standby and VBAT modes. Once
  // set, the application must wait that the Backup Regulator Ready flag (BRR)
  // is set to indicate that the data written into the RAM will be maintained
  // in the Standby and VBAT modes. Note: This bit is not reset when the device
  // wakes up from Standby mode, by a system reset, or by a power reset.
  // Power Control Status Register 1
  regval = getreg32(STM32_PWR_CSR1);
  regval &= ~PWR_CSR1_BRE;   // Clear BRE = Battery Backed RAM not used

  // Clear the Wake-Up Internal Flag. This bit is set when a wakeup is detected
  // on the internal wakeup line in standby mode. It is cleared when all
  // internal wakeup sources are cleared.
  // ** THIS MAY NOT NEED TO BE DONE. **
  // regval &= ~PWR_CSR1_WUIF;
  putreg32(regval, STM32_PWR_CSR1);

  // Power Control Register 1
  regval  = getreg32(STM32_PWR_CR1);

// DUPLICATE OF STOP MODE. HERE FOR TESTING POWER DIFFERENCE FOR STANDBY MODE.
//
  // // The Power Down Deep Sleep (PDDS) bit determines if we enter Stop or
  // // Standby modes. So, clear the bit for Stop mode.
  // regval &= ~(PWR_CR1_PDDS);  // Clear the Power Down Deep Sleep (PDDS)

  // // Clear all the bits used to control the power state
  // regval &= ~(PWR_CR1_LPDS);        // Bit 0: Low-power deepsleep
  // regval &= ~(PWR_CR1_FPDS);        // Bit 9: Flash power down in Stop mode
  // regval &= ~(PWR_CR1_LPUDS);       // Bit 10: Low-power regulator in deepsleep under-drive mode
  // regval &= ~(PWR_CR1_MRUDS);       // Bit 11: Main regulator in deepsleep under-drive mode
  // regval &= ~(PWR_CR1_UDEN_ENABLE); // Bits 18-19: Under-drive
 
  // // The stop mode has a lot of optional power saving opportunites by using
  // // the UDEN, MRUDS, LPUDS, LPDS and FPDS bits.

  // // With the STOP ULP-FPD voltage Regulator mode saving the most power.
  // // Save the most power
  // // Set the Low Power Deep Sleep (LPDS) bit to keep stop the Main voltage
  // // regulator and enable the Low-power voltage regulator.
  // regval |= PWR_CR1_LPDS;         // Bit 9: Flash power down in Stop mode
  // regval |= PWR_CR1_LPUDS;        // Bit 10: Low-power regulator in deepsleep under-drive mode
  // regval |= PWR_CR1_UDEN_ENABLE;  // Bits 18-19: Under-drive enable

  // The Power Down Deep Sleep (PDDS) bit determines if we enter Stop or
  // Standby modes. So, set the bit for Standby mode.
  regval |= PWR_CR1_PDDS;
  putreg32(regval, STM32_PWR_CR1);

  // Set SLEEPDEEP bit of Cortex System Control Register
  regval = getreg32(NVIC_SYSCON);
  regval |= NVIC_SYSCON_SLEEPDEEP;
  putreg32(regval, NVIC_SYSCON);

  asm volatile ("wfi");
  return OK;
}

//===============================================================
// This function will switch the power state of the STM32F7 to
// desired power state
int meadow_pwr_mgmt_change_state(enum mpm_state_e desiredState)
{
  static enum mpm_state_e prevState = mpm_state_run;
  irqstate_t flags;
  int ret = OK;

  // Is the requested state different?
  if(prevState == desiredState)
  {
    syslog(1, "No Power state mode change - already at requested state\n");
    return OK;
  }

  flags = enter_critical_section();

  switch(desiredState)
  {
    case mpm_state_run:
      break;

    case mpm_state_sleep:
      ret = meadow_pwr_mgmt_enter_sleep();
      break;

    case mpm_state_stop_save_min:
      ret = meadow_pwr_mgmt_enter_stop(true);
      break;

    case   mpm_state_stop_save_max:
      ret = meadow_pwr_mgmt_enter_stop(false);
      break;

    case mpm_state_standby:
      ret = meadow_pwr_mgmt_enter_standby();
      break;

    default:
      ret = ERROR;
      break;
  }

  prevState = desiredState;
  leave_critical_section(flags);

  return ret;
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
