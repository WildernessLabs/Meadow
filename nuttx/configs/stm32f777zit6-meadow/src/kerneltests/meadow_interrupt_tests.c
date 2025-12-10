/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\meadow_interrupt_tests.c
 * 
 *   Copyright (C) 2024-2025 Wilderness Labs. All rights reserved.
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
#include "meadow_interrupt.h"

// Only build if configured
#if defined(CONFIG_MEADOW_INTERRUPT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
#pragma message "(--) meadow_interrupt_tests.c"

#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "stm32_rcc.h"    // stm32_clockenable
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access
#include "clock/clock.h"
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

// D05 (PB4) for Version F7FeatherV2 or CCM V1
// For Testing wanted a pin that was Px0-4 to more easily figure out interrupts
// and because these are a high priority interrupts.
// #define QUICK_MISC_PIN_V2_D05  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)

/************************************************************************************
 * Private Data
 ************************************************************************************/
static char *thisFile = __FILE__;

static bool keepReading = true;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/
static void meadow_interrupt_test_test_mem_leak_fix_free_1(void);
static void meadow_interrupt_test_config_d05_gpio(void);
static void meadow_interrupt_test_test_mem_leak_fix_alloc_5(void);
static void meadow_interrupt_test_test_mem_leak_fix_free_5(void);
static void meadow_interrupt_test_initialize_interrupt_for_wakeup(void);
static void meadow_interrupt_test_initialize_wakeup_and_sleep(void);
static void meadow_interrupt_test_monitor_mint_mq(void);

int mint_config_interrupt(struct mint_gpio_int_config* cfg);    // This is a duplicate

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -p 16 comes here
void meadow_kt_meadow_interrupt_tests(uint32_t userData)
{
  static bool firstTime = true;

  syslog(LOG_MTEST, "Meadow interrupt tests received 'set developer -p 16 -v %lu'\n",
    userData);

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
        syslog(LOG_MTEST, "Only first time\n");
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
      // Use D05 as input to send mq message
      meadow_interrupt_test_config_d05_gpio();
      break;

    case 6:
      // Works with meadow_interrupt_test_monitor_mint_mq to stop reading mq.
      keepReading = false;
      meadow_interrupt_test_test_mem_leak_fix_free_1();
      break;
      
    case 7:
      meadow_interrupt_test_test_mem_leak_fix_alloc_5();
      break;

    case 8:
      meadow_interrupt_test_test_mem_leak_fix_free_5();
      break;

    case 9:
      meadow_interrupt_test_monitor_mint_mq();
      break;
      
    default:
      syslog(LOG_MTEST, "Undefined test for meadow_kt_meadow_interrupt_tests, userData:%lu\n",
        userData);
      break;
  }
}
 
/************************************************************************************
 * Private Functions
 ************************************************************************************/
// Meadow_Issue 346 was related to a memory leak found in meadow_interrupt.c.
// This leak would occur whenever a interrupt was configured as the memory
// allocated for the configuration would not be freed when the GPIO
// configuration was removed.
void meadow_interrupt_test_config_d05_gpio()
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n",
      __FILE__, __LINE__);
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
    syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
      ret);
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
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n",
      __FILE__, __LINE__);
    return;
  }
  syslog(LOG_MTEST, "TEST-Freeing memory - for 1 point 0x14\n");
  usleep(29 * 1000);

  // PB4
  cfg->port = 1;              // port B (D05 in FeatherV2)
  cfg->pin = 4;               // pin 4  (D05 in FeatherV2)
  cfg->configType = gpio_intrpt_cfg_type_remove;  // Delete
  cfg->risingEdge = 1;
  cfg->fallingEdge = 0;
  cfg->resistorMode = 2;      // 2 = pull down
  cfg->debounceDuration = 5;  // Must be 0 for lp wakeup
  cfg->glitchDuration = 0;    // Must be 0 for lp wakeup

  // Call public configuration function to dispose of interrupt GPIO
  ret = mint_config_interrupt(cfg);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
      ret);
  }

  free (cfg);
}

// ============================================================================
// These tests exercise the fix for the memory leak in meadow_interrupt.c
// Meadow_Issue 346
void meadow_interrupt_test_test_mem_leak_fix_alloc_5(void)
{
  // Setup a GPIO
  int ret;
  struct mint_gpio_int_config* cfg = malloc(sizeof(struct mint_gpio_int_config));
  if(cfg == NULL)
  {
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n", __FILE__, __LINE__);
    return;
  }
  syslog(LOG_MTEST, "TEST-Configuring 5 points 0x14\n"); usleep(29 * 1000);

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
    syslog(LOG_MTEST, "TEST-Config point:0x%02x\n", pinDesignation);
    usleep(29 * 1000);

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
      syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
        ret);
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
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n",
      __FILE__, __LINE__);
    return;
  }
  syslog(LOG_MTEST, "TEST-Freeing 5 points 0x14\n");
  usleep(29 * 1000);

  // PB7 - PB11 but remove in reverse order
  for(int i = 11; i >= 7; i--)
  {
    uint8_t pinDesignation = 0x10 | i;
    syslog(LOG_MTEST, "TEST-Free point:0x%02x\n", pinDesignation);
    usleep(29 * 1000);
    
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
      syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
        ret);
    }
  }

  syslog(LOG_MTEST, "EXITING meadow_interrupt_test_test_mem_leak_fix_free_5\n");
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
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n",
      __FILE__, __LINE__);
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
    syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
      ret);
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
    syslog(LOG_MTEST, "%s@%d-Error:malloc returned NULL\n",
      __FILE__, __LINE__);
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
    syslog(LOG_MTEST, "Error:mint_config_interrupt returned ret:%d\n",
      ret);
  }
  free (cfg);

  syslog(LOG_MTEST, "%s@%d - Going into Low-power sleep for 30 seconds unless interrupted.\n",
    __FILE__, __LINE__);
  // Need a bit of time to insure message is received before low-power mode
  usleep(50 * 1000);

  DEBUG_SET_HIGH(DEBUG_PIN_V2_D14);

  // Put Meadow to sleep for either time or till interrupt
  ret = pwrmgmt_enter_stm32f7_stop_mode(30);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "Error:pwrmgmt_enter_stm32f7_stop_mode returned ret:%d\n",
      ret);
  }

  DEBUG_SET_LOW(DEBUG_PIN_V2_D14);

  // Verify wakeup reason
  int wakeReason = pwrmgmt_most_recent_wakeup_reason();
  syslog(LOG_MTEST, "%s@%d - Low-power sleep ended, reason:%d\n",
    __FILE__, __LINE__, wakeReason);
  usleep(20 * 1000);
}

// ============================================================================
// This test reads the message queue into which interrupt's are placed.
// This simulates what Meadow.Core does.
// It displays the data of all queued messages, one-by-one and exits
// when no message are left.
//
// Execute 'developer -p 18 -v 4' configure D05 as DI no time
// Optional - Toggle D05, 1 or more times
// Execute -v 9 to begin reading mq to syslog
// Toggling D05 will now show messages at each toggle
// Reset button stop testing
void meadow_interrupt_test_monitor_mint_mq(void)
{
  int ret;
  mqd_t mint_test_mq_fd;
  mint_send_int_core_t mint_recvd_msg;
  ssize_t nbytes;

  // Open existing mq
  mint_test_mq_fd = mq_open(MINT_MSG_QUEUE_NAME, O_RDONLY);
  if (mint_test_mq_fd == (mqd_t)-1)
  {
    syslog(LOG_ERR, "Error:Mint test open mq:-1, errno:%d\n",errno);
    usleep(20 * 1000);
    return;
  }

  // Read and display all interrupt messages. 
  while(keepReading)
  {
    // syslog(LOG_MTEST, "----> %s@%d-Calling mq_receive, waiting for message\n",
    //   thisFile, __LINE__);
    // usleep(20 * 1000);
    nbytes = mq_receive(mint_test_mq_fd, (char *)&mint_recvd_msg,
      MEADOW_INTERRUPT_MQ_MSG_SIZE, NULL);
    if(nbytes < 0)
    {
      // Signal
      if(errno == EINTR)
      {
        continue;
      }

      syslog(LOG_ERR "---->%s@%d-Mint test - Error:mq_receive, ret:%d, errno:%d\n",
        thisFile, __LINE__, nbytes, errno);
        usleep(20 * 1000);
      break;
    }

    if(nbytes == 0)
    {
      syslog(LOG_ERR, "Mint test - read mq, 0-byte message?\n");
      usleep(20 * 1000);
      break;      // No message
    }

    // Display the mq's data.
    if(nbytes == MEADOW_INTERRUPT_MQ_MSG_SIZE)
    {
#if (MEADOW_INTERRUPT_INCLUDE_TIME_STAMP > 0)
      struct timespec mintTicks;

      // Convert Nuttx ticks to time
      (void)clock_ticks2time(mint_recvd_msg.interruptTicks, &mintTicks);

      syslog(LOG_MTEST, "Mint test - Received, PinId:0x%02x, State:0x%02x, Ticks:%lld (sec:%d, nsec:%09d)\n",
        mint_recvd_msg.gpioPinId,
        mint_recvd_msg.gpioState,
        mint_recvd_msg.interruptTicks,
        mintTicks.tv_sec,
        mintTicks.tv_nsec);
#else
      syslog(LOG_MTEST, "Mint test - Received, PinId:0x%02x, State:0x%02x\n",
        mint_recvd_msg.gpioPinId,
        mint_recvd_msg.gpioState);
#endif    
    }
    else
    {
      syslog(LOG_MTEST, "Mint test - UNKNOWN message received, size:%d\n", nbytes);
    }
  }   // while true

  // Close mq
  ret = mq_close(mint_test_mq_fd);
  if (ret < 0)
  {
    syslog(LOG_MTEST, "Mint test read mq, mq_close error, ret:%d\n", ret);
  }
}

#endif  // #if defined(CONFIG_MEADOW_INTERRUPT_TESTS)
