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

// Not yet active in Meadow.OS until needed by .Net
#if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
#include "../hcom_nx/hcom_nx_common.h"
#include "meadow_rotary_encoder.h" 
#include "stm32_gpio.h"   // stm32_configgpio

// Only build if configured
#if defined(CONFIG_ROTARY_ENCODER_TESTS)
#pragma message "(--) rotary_encoder_tests.c"

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// D05 (PB4) for Version 2 Feather or CCM V1. Interrupt group isolation means
// that no pins can share the same Pin number for a rotary encoder
#define ENCODER_PIN_CCM_PB4_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)
#define ENCODER_PIN_CCM_PB13_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN13)
#define ENCODER_PIN_CCM_PB7_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN7)
#define ENCODER_PIN_CCM_PB6_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN6)
#define ENCODER_PIN_CCM_PI11_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTI | GPIO_PIN11)
#define ENCODER_PIN_CCM_PD5_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTD | GPIO_PIN5)

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
static void rotary_encoder_config_test_add_n(uint32_t userData);
static void rotary_encoder_config_test_remove_n(uint32_t userData);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// FUTURE-For when rotary encoder has it own unit test infrastructure
// // set developer -d 18 come here
void meadow_kt_rotary_encoder_tests(uint32_t userData)
{
  int ret;
  int currentCount;
  
  syslog(2, "Rotary Encoder tests received 'set developer -d 18 -v %lu'\n", userData);

  switch(userData)
  {
    case 1:
    case 2:
    case 3:
    case 4:
      rotary_encoder_config_test_add_n(userData);
      break;

    case 10:
    case 11:
    case 12:
    case 13:
      rotary_encoder_config_test_remove_n(userData);
      break;

    case 20:
      ret = meadow_rotary_encoder_read_count(0, &currentCount);
      if(ret < 0)
      {
        syslog(2, "Encoder 0 returned error:%d\n", ret);
        break;
      }
      syslog(2, "Encoder 0 count:%d\n", currentCount);
      break;

    case 21:
      ret = meadow_rotary_encoder_read_count(5, &currentCount);
      if(ret < 0)
      {
        syslog(2, "Encoder 5 returned error:%d\n", ret);
        break;
      }
      syslog(2, "Encoder 5 count:%d\n", currentCount);
      break;

    case 22:
      ret = meadow_rotary_encoder_read_count(7, &currentCount);
      if(ret < 0)
      {
        syslog(2, "Encoder 7 returned error:%d\n", ret);
        break;
      }
      syslog(2, "Encoder 7 count:%d\n", currentCount);
      break;

    default:
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
void rotary_encoder_config_test_add_n(uint32_t userData)
{
  // Setup a rotarty encoder
  int ret;
  uint32_t encoderNumb;
  uint32_t PinA;
  uint32_t PinB;

  struct rotenc_config_parms* cfg = malloc(sizeof(struct rotenc_config_parms));
  if(cfg == NULL)
  {
    syslog(LOG_ERR, "Memory allocation failed\n");
    return;
  }

  // Note: The input pins defined for F7FeatherV2 would be D05, D06, D07, D08
  // for the first 4 pins. The last 2 are not available on F7FeatherV2
  // Rotary Encoder testing. And becareful to not use the pins used by syslog,
  // D12 and D13.
  switch(userData)
  {
    case 1:
      encoderNumb = 0;
      PinA = ENCODER_PIN_CCM_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_PB13_INPUT;
      break;
    case 2:
      encoderNumb = 5;
      PinA = ENCODER_PIN_CCM_PB7_INPUT;
      PinB = ENCODER_PIN_CCM_PB6_INPUT;
      break;
    case 3:
      encoderNumb = 7;
      PinA = ENCODER_PIN_CCM_PI11_INPUT;
      PinB = ENCODER_PIN_CCM_PD5_INPUT;
      break;
    case 4:
      encoderNumb = 8;    // Invalid
      PinA = ENCODER_PIN_CCM_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_PB13_INPUT;
      break;
  }

  // For testing need to initialize 2 GPIOs as inputs
  stm32_configgpio(PinA);
  stm32_configgpio(PinB);
  
  cfg->encoderNumb = encoderNumb;   // Encoder number 0 - 7
  cfg->isAddEncoder = true;         // New rotary encoder
  cfg->portA = (PinA & 0xf0) >> 4;
  cfg->pinA =  (PinA & 0x0f);
  cfg->portB = (PinB & 0xf0) >> 4;
  cfg->pinB =  (PinB & 0x0f);
  cfg->resistorMode = 2;    // 2 = pull down

  // Call meadow_rotenc.c configuration function also used by managed code
  ret = meadow_config_rotary_encoder(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:meadow_config_rotary_encoder returned ret:%d\n", ret);
  }

  free (cfg);
}

//============================================================================
void rotary_encoder_config_test_remove_n(uint32_t userData)
{
  int ret;
  uint32_t encoderNumb;
  uint32_t PinA;
  uint32_t PinB;

  struct rotenc_config_parms* cfg = malloc(sizeof(struct rotenc_config_parms));
  if(cfg == NULL)
  {
    syslog(LOG_ERR, "Memory allocation failed\n");
    return;
  }

  switch(userData)
  {
    case 10:
      encoderNumb = 0;
      PinA = ENCODER_PIN_CCM_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_PB13_INPUT;
      break;
    case 11:
      encoderNumb = 5;
      PinA = ENCODER_PIN_CCM_PB7_INPUT;
      PinB = ENCODER_PIN_CCM_PB6_INPUT;
      break;
    case 12:
      encoderNumb = 7;
      PinA = ENCODER_PIN_CCM_PI11_INPUT;
      PinB = ENCODER_PIN_CCM_PD5_INPUT;
      break;
    case 13:
      encoderNumb = 8;    // Invalid
      PinA = ENCODER_PIN_CCM_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_PB13_INPUT;
      break;
  }

  // For testing need to unconfigure the 2 GPIOs used as inputs
  stm32_unconfiggpio(PinA);
  stm32_unconfiggpio(PinB);

  // Little configuration needed
  cfg->encoderNumb = encoderNumb;   // Encoder number 0 - 7
  cfg->isAddEncoder = false;        // Remove rotary encoder

  // Call meadow_rotenc.c configuration function also used by managed code
  ret = meadow_config_rotary_encoder(cfg);
  if(ret < 0)
  {
    syslog(2, "Error:meadow_config_rotary_encoder returned ret:%d\n", ret);
  }

  free (cfg);
}

#endif  // #if defined(CONFIG_ROTARY_ENCODER_TESTS)

#endif  // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
