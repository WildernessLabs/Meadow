/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/meadow_power_mngt.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/power/pm.h>

#include "up_internal.h"
#include "stm32_pm.h"

#include <syslog.h>

// #include <meadow/meadow_hw_version.h>
// #include <nuttx/spi/qspi.h>

// #include <arch/stm32f7/chip.h>


// Diagnostic only
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
#include "stm32_gpio.h"

// CONFIG_PM is set at 'Device Drivers ->
//  [*] Power management (PM) driver interfaces --->'
#ifdef CONFIG_PM

#warning Experimental Meadow Power Management Code

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
// 
/****************************************************************************
 * Public Functions
 ****************************************************************************/
// When CONFIG_ARCH_CUSTOM_PMINIT=y is included this function is called instead
// of arch/arm/src/stm32/stm32_pminitialiaze.c.
//
// Within menuconfig, CONFIG_ARCH_CUSTOM_PMINIT is found at 'System Type ->
// [*] Custom PM Initialization'
void up_pminitialize(void)
{
  // Initialize Meadow specific needs
  syslog(1, "==>> PwrMngt Entered %s/%s()\n", __FILE__, __func__);

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_RED_LED);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_GREEN_LED);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_BLUE_LED);

  // High turns led off
  DEBUG_SET_HIGH(DEBUG_PIN_V2_RED_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_GREEN_LED);
  DEBUG_SET_HIGH(DEBUG_PIN_V2_BLUE_LED);

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D06);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D07);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D08);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D09);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D10);

  DEBUG_SET_LOW(DEBUG_PIN_V2_D06);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D07);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D08);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D09);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D10);

  /* Then initialize the NuttX power management subsystem proper */

  pm_initialize();
}

#endif /* CONFIG_PM */
