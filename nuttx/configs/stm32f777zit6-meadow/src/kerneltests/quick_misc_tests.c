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

// Only build if configured
#if defined(CONFIG_QUICK_MISC_TESTS)
#pragma message "(--) quick_misc_tests.c"

#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "stm32_rcc.h"    // stm32_clockenable
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access
#include "meadow-upd.h"   // mint_config_interrupt(cfg);

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// D05 (PB4) for Version 2 Feather or CCM V1
// For Testing wanted a pin that was Px0-4 to more easily figure out interrupts
// and because these are a high priority interrupts.
// #define QUICK_MISC_PIN_V2_D05  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void quick_misc_test_initialize_interrupt_for_wakeup(void);

int mint_config_interrupt(struct mint_gpio_int_config* cfg);    // This is a duplicate

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -d 10 come here
void meadow_kt_quick_misc_tests(uint32_t userData)
{
  static bool firstTime = true;

  syslog(2, "Quick and Misc tests received 'set developer -d 12 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
      if(firstTime)
      {
        firstTime = false;
        quick_misc_test_initialize_interrupt_for_wakeup();
      }
      else
      {
        syslog(2, "Only first time\n");
      }
      break;
    
    case 2:
      DEBUG_SET_HIGH(DEBUG_PIN_V2_D14);
      pwrmgmt_enter_stm32f7_stop_mode(10);    // Stop for 10 seconds
      DEBUG_SET_LOW(DEBUG_PIN_V2_D14);
      break;

    default:
      syslog(2, "Undefined test for meadow_kt_quick_misc_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// ============================================================================
// This test is used to determine if an interrupt can wakeup the F7 from a
// low-power mode. It simulates being configured via Meadow.Core.
static void quick_misc_test_initialize_interrupt_for_wakeup(void)
{
  // Setup a GPIO to generate an interrupt to wakeup
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D14);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D15);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D15);

  // Populate config structure for GPIO wakeup of PB4 (D05 in FeatherV2).
  // This is what Meadow.Core will do when it's been enhanced to support this
  // feature.
  // Note: cfg is not the Nuttx cfgset. The Nuttx cfgset is built by the call
  // to mint_config_interrupt().
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = 2;        // 2 = lp wakeup (gpio_int_cfg_type_wakeup)
  cfg->risingEdge = 1;
  cfg->fallingEdge = 0;
  cfg->resistorMode = 2;      // 2 = pull down
  cfg->debounceDuration = 0;  // Must be 0 for lp wakeup
  cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

  // Call public configuration function used by managed code
  ret = mint_config_interrupt(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
  }
  free (cfg);
}
#endif  // #if defined(CONFIG_QUICK_MISC_TESTS)
