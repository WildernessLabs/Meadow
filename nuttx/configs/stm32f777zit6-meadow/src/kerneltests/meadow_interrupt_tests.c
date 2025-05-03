/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\meadow_interrupt_tests.c
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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
#if defined(CONFIG_MEADOW_INTERRUPT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
#pragma message "(--) meadow_interrupt_tests.c"

#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "stm32_rcc.h"    // stm32_clockenable
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access
#include "meadow-upd.h"   // mint_config_interrupt(cfg);
#include "meadow_interrupt.h"
#include "pwrmgmt/pwrmgmt_local.h"
#include <meadow/meadow_syscall_support.h>

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
static void meadow_interrupt_test_test_mem_leak_fix_free_1(void);
static void meadow_interrupt_test_test_mem_leak_fix_alloc_1(void);
static void meadow_interrupt_test_test_mem_leak_fix_alloc_5(void);
static void meadow_interrupt_test_test_mem_leak_fix_free_5(void);
static void meadow_interrupt_test_initialize_interrupt_for_wakeup(void);
static void meadow_interrupt_test_initialize_wakeup_and_sleep(void);

int mint_config_interrupt(struct mint_gpio_int_config* cfg);    // This is a duplicate

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -d 16 comes here
void meadow_kt_meadow_interrupt_tests(uint32_t userData)
{
  static bool firstTime = true;

  syslog(2, "Meadow interrupt tests received 'set developer -d 16 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
      if(firstTime)
      {
        firstTime = false;
        meadow_interrupt_test_initialize_interrupt_for_wakeup();
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

    case 3:
      // Initialize wakeup pin then sleep
      meadow_interrupt_test_initialize_wakeup_and_sleep();
      break;

    case 4:
      meadow_interrupt_test_test_mem_leak_fix_alloc_1();
      break;

    case 5:
      meadow_interrupt_test_test_mem_leak_fix_free_1();
      break;
      
    case 6:
      meadow_interrupt_test_test_mem_leak_fix_alloc_5();
      break;

    case 7:
      meadow_interrupt_test_test_mem_leak_fix_free_5();
      break;
      
    default:
      syslog(2, "Undefined test for meadow_kt_meadow_interrupt_tests, userData:%lu\n", userData);
      break;
  }
}
 
/************************************************************************************
 * Private Functions
 ************************************************************************************/
// Meadow_Issue #346 was related to a memory leak found in meadow_interrupt.c.
// This leak would occure whenever a interrupt was configured as the memory
// allocated for the configuration would not be freed when the GPIO
// configuration was removed.
void meadow_interrupt_test_test_mem_leak_fix_alloc_1(void)
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }
  
  // We need some GPIOs to initialize and free
  // Populate config structure for GPIO wakeup of PB4 (D05 in FeatherV2).
  // This is what Meadow.Core will do when it's been enhanced to support this
  // feature.
  // Note: cfg is not the Nuttx cfgset. The Nuttx cfgset is built by the call
  // to mint_config_interrupt().
  // PB4
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = gpio_intrpt_cfg_type_new;   // New
  cfg->risingEdge = 1;
  cfg->fallingEdge = 0;
  cfg->resistorMode = 2;      // 2 = pull down
  cfg->debounceDuration = 5;  // Must be 0 for lp wakeup
  cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

  // Call public configuration function used by managed code
  ret = mint_config_interrupt(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
  }
  free (cfg);
}

// ============================================================================
// Delete one GPIO
void meadow_interrupt_test_test_mem_leak_fix_free_1(void)
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }
  syslog(2, "TEST-Freeing memory - for 1 point 0x14\n"); usleep(29 * 1000);

  // PB4
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = gpio_intrpt_cfg_type_remove;      // Delete
  cfg->risingEdge = 1;
  cfg->fallingEdge = 0;
  cfg->resistorMode = 2;      // 2 = pull down
  cfg->debounceDuration = 5;  // Must be 0 for lp wakeup
  cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

  // Call public configuration function to dispose of interrupt
  ret = mint_config_interrupt(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
  }

  free (cfg);
}

// ============================================================================
// These tests exercise the fix for the memory leak in meadow_interrupt.c
// Meadow_Issue #346
void meadow_interrupt_test_test_mem_leak_fix_alloc_5(void)
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }
  syslog(2, "TEST-Configuring 5 points 0x14\n"); usleep(29 * 1000);

  // We need some GPIOs to initialize and free
  // Populate config structure for GPIO wakeup of PB4 (D05 in FeatherV2).
  // This is what Meadow.Core will do when it's been enhanced to support this
  // feature.
  // Note: cfg is not the Nuttx cfgset. The Nuttx cfgset is built by the call
  // to mint_config_interrupt().
  // PB7 - PB11
  for(int i = 7; i <= 11; i++)
  {
    uint8_t pinDesignation = 0x10 | i;
    syslog(2, "TEST-Config point:0x%02x\n", pinDesignation); usleep(29 * 1000);

    cfg->port = 1;              // port B
    cfg->pin = i;               // pin 7-11
    cfg->configType = gpio_intrpt_cfg_type_new;   // New
    cfg->risingEdge = 1;
    cfg->fallingEdge = 0;
    cfg->resistorMode = 2;      // 2 = pull down
    cfg->debounceDuration = 5;  // Must be 0 for lp wakeup
    cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

    // Call public configuration function used by managed code
    ret = mint_config_interrupt(cfg);
    if(ret < 0)
    {
      syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
    }

  }
  free (cfg);
}

// ============================================================================
// Delete one GPIO
void meadow_interrupt_test_test_mem_leak_fix_free_5(void)
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }
  syslog(2, "TEST-Freeing 5 points 0x14\n"); usleep(29 * 1000);

  // PB7 - PB11 but remove in reverse order
  for(int i = 11; i >= 7; i--)
  {
    uint8_t pinDesignation = 0x10 | i;
    syslog(2, "TEST-Free point:0x%02x\n", pinDesignation); usleep(29 * 1000);
    
    cfg->port = 1;              // port B
    cfg->pin = i;               // pin 7-11
    cfg->configType = gpio_intrpt_cfg_type_remove;   // Delete
    cfg->risingEdge = 1;
    cfg->fallingEdge = 0;
    cfg->resistorMode = 2;      // 2 = pull down
    cfg->debounceDuration = 5;  // Must be 0 for lp wakeup
    cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

    // Call public configuration function used by managed code
    ret = mint_config_interrupt(cfg);
    if(ret < 0)
    {
      syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
    }
  }

  syslog(2, "EXITING meadow_interrupt_test_test_mem_leak_fix_free_5\n");
  free (cfg);
}

// ============================================================================
// This test is used to verify that an interrupt can wakeup the F7 from a
// low-power mode. It simulates being configured via Meadow.Core.
// It only configures the interrupt part. The input needs to be configured
// elsewhere before making this call
static void meadow_interrupt_test_initialize_interrupt_for_wakeup(void)
{
  // Setup a GPIO to generate an interrupt to wakeup
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D14); // On while sleeping
  DEBUG_SET_LOW(DEBUG_PIN_V2_D14);

  // Populate config structure for GPIO wakeup of PB4 (D05 in FeatherV2).
  // This is what Meadow.Core will do when it's been enhanced to support this
  // feature.
  // Note: cfg is not the Nuttx cfgset. The Nuttx cfgset is built by the call
  // to mint_config_interrupt().
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = gpio_intrpt_cfg_type_wakeup;
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

// ============================================================================
// Should this be in power_management_tests instead of here?
// This test is used to determine if an interrupt can wakeup the F7 from a
// low-power mode. It simulates being configured via Meadow.Core.
static void meadow_interrupt_test_initialize_wakeup_and_sleep(void)
{
  int ret;

  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(2, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }

  // Only used by this module
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D14); // On while sleeping
  DEBUG_SET_LOW(DEBUG_PIN_V2_D14);
  
  // D05 - PB4 Input for GPIO wakeup pin
  stm32_configgpio(GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4);
  
  // Populate config structure for GPIO wakeup of PB4 (D05 in FeatherV2).
  // This is what Meadow.Core will do when it's been enhanced to support this
  // feature.
  // Note: cfg is not the Nuttx cfgset. The Nuttx cfgset is built by the call
  // to mint_config_interrupt().
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = gpio_intrpt_cfg_type_wakeup;
  cfg->risingEdge = 1;
  cfg->fallingEdge = 0;
  cfg->resistorMode = 2;      // 2 = pull down
  cfg->debounceDuration = 0;  // Must be 0 for lp wakeup
  cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

  // Configure interrupt pin via public function used by managed code
  ret = mint_config_interrupt(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
  }
  free (cfg);

  syslog(2, "%s@%d - Going into Low-power sleep for 30 seconds unless interrupted.\n", __FILE__, __LINE__);
  // Need a bit of time to insure message is received before low-power mode
  usleep(50 * 1000);

  DEBUG_SET_HIGH(DEBUG_PIN_V2_D14);

  // Put Meadow to sleep for either time or till interrupt
  ret = pwrmgmt_enter_stm32f7_stop_mode(30);
  if(ret < 0)
  {
    syslog(2, "Error:mint_config_interrupt returned ret:%d\n", ret);
  }

  DEBUG_SET_LOW(DEBUG_PIN_V2_D14);

  // Verify wakeup reason
  int wakeReason = pwrmgmt_most_recent_wakeup_reason();
  syslog(2, "%s@%d - Low-power sleep ended, reason:%d\n", __FILE__, __LINE__, wakeReason);
  usleep(20 * 1000);
}

#endif  // #if defined(CONFIG_MEADOW_INTERRUPT_TESTS)
