/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\meadow_rotary_encoder_tests.c
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

#include <meadow/hcom_shared_common.h>

#if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
#include "../hcom_nx/hcom_nx_common.h"

// Future define based on Unit Test infrastructure
#define CONFIG_ROTARY_ENCODER_TESTS

// Only build if configured
#if defined(CONFIG_ROTARY_ENCODER_TESTS)
#pragma message "(--) rotary_encoder_tests.c"

#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "stm32_rcc.h"    // stm32_clockenable
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access
// #include "meadow-upd.h"   // NEEDED UNTIL REPLACED WITH ROTENC TEST INFRASTRUCTURE
#include "meadow_rotary_encoder.h" 

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// D05 (PB4) for Version 2 Feather or CCM V1
#define QUICK_MISC_PIN_V2_D05_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)
#define QUICK_MISC_PIN_V2_D06_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN13)

/************************************************************************************
 * Private Data
 ************************************************************************************/
// Copied from meadow_interrupt.c

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// IF ROTARY ENCODER HAS ITS OWN TESTS CREATED
// // set developer -d ?? come here
// void meadow_kt_rotary_encoder_tests(uint32_t userData)
// {
//   syslog(2, "Rotary Encoder tests received 'set developer -d ?? -v %lu'\n", userData);

//   switch(userData)
//   {
//     case 1:
//       rotary_encoder_test_exercise_test();
//       break;
      
//     default:
//       syslog(2, "Undefined test for meadow_kt_quick_misc_tests, userData:%lu\n", userData);
//       break;
//   }
// }
 
/************************************************************************************
 * Private Functions
 ************************************************************************************/

//==================================================================

// Rotary Encoder PROTOTYPE testing
void rotary_encoder_test_exercise_test(void)
{
  // Setup a GPIO to generate an interrupt to wakeup
  int ret;

  syslog(1, "Entered rotary_encoder_tests\n");

  struct rotenc_config_parms* cfg = malloc(sizeof(struct rotenc_config_parms));

  stm32_configgpio(QUICK_MISC_PIN_V2_D05_INPUT);
  stm32_configgpio(QUICK_MISC_PIN_V2_D06_INPUT);

  // For Prototype testing need to initialize 2 GPIOs as inputs
  cfg->portA = 1;           // port B (D05 in FeatherV2)
  cfg->pinA = 4;            // pin 4  (D05 in FeatherV2)
  cfg->portB = 1;           // port B (D06 in FeatherV2)
  cfg->pinB = 13;           // pin 13  (D06 in FeatherV2)
  cfg->rotencConfig = true; // New rotary encoder
  cfg->resistorMode = 2;    // 2 = pull down

  // Call meadow_rotenc.c configuration function used by managed code
  ret = meadow_config_rotary_encoder(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:meadow_config_rotary_encoder returned ret:%d\n", ret);
  }

  free (cfg);
}

#endif  // #if defined(CONFIG_ROTARY_ENCODER_TESTS)

#endif  // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
