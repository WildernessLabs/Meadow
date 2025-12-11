/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\meadow_rotary_encoder_tests.c
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
#include <nuttx/config.h>

#include <meadow/hcom_shared_common.h>

#if defined(CONFIG_MEADOW_ROTARY_ENCODER)

#include "../hcom_nx/hcom_nx_common.h"
#include "stm32_gpio.h"   // stm32_configgpio
#include <meadow/meadow_rotary_encoder.h>

// Only build if configured
#if defined(CONFIG_ROTARY_ENCODER_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
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
#define ENCODER_PIN_CCM_D05_PB4_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)
#define ENCODER_PIN_CCM_D06_PB13_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN13)
#define ENCODER_PIN_CCM_D07_PB7_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN7)
#define ENCODER_PIN_CCM_D08_PB6_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN6)
#define ENCODER_PIN_CCM_D09_PI11_INPUT  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTI | GPIO_PIN11)
#define ENCODER_PIN_CCM_D10_PD5_INPUT   (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTD | GPIO_PIN5)

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
// set developer -p 18 come here
void meadow_kt_rotary_encoder_tests(uint32_t userData)
{
  int ret;
  int32_t currentCount;
  int32_t encoderChanged;
  uint32_t rotClockWise;

  syslog(LOG_MTEST,
    "Rotary Encoder tests received 'set developer -p 18 -v %lu'\n",
    userData);

  switch(userData)
  {
    // Test adding new configuration
    case 1:
    case 2:
    case 3:
    case 4:
      rotary_encoder_config_test_add_n(userData);
      break;

    // Test removing configuration
    case 11:
    case 12:
    case 13:
    case 14:
      rotary_encoder_config_test_remove_n(userData);
      break;

    // Test reading count
    case 21:
      syslog(LOG_MTEST, "Rotary Encoder tests - reading encoder 0 count (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_read_count(0, &currentCount,
        &encoderChanged, &rotClockWise);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 0 returned error:%d\n", ret);
        break;
      }

      if(encoderChanged == 0)
      {
        syslog(LOG_MTEST, "Encoder 0 - No change since last read, count:%ld, direction:%s\n",
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      else
      {
        syslog(LOG_MTEST, "Encoder 0 - Has changed by %ld since last read, count:%ld, direction:%s\n",
          encoderChanged,
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      break;

    case 22:
      syslog(LOG_MTEST, "Rotary Encoder tests - reading encoder 5 count (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_read_count(5, &currentCount,
        &encoderChanged, &rotClockWise);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 5 returned error:%d\n", ret);
        break;
      }
      if(encoderChanged == 0)
      {
        syslog(LOG_MTEST, "Encoder 5 - No since last read, count:%ld, direction:%s\n",
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      else
      {
        syslog(LOG_MTEST, "Encoder 5 - Has changed by %ld since last read, count:%ld, direction:%s\n",
          encoderChanged,
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      break;

    case 23:
      syslog(LOG_MTEST, "Rotary Encoder tests - reading encoder 7 count (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_read_count(7, &currentCount,
        &encoderChanged, &rotClockWise);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 7 returned error:%d\n", ret);
        break;
      }
      if(encoderChanged == 0)
      {
        syslog(LOG_MTEST, "Encoder 7 - No since last read, count:%ld, direction:%s\n",
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      else
      {
        syslog(LOG_MTEST,
          "Encoder 7 - Has changed by %ld since last read, count:%ld, direction:%s\n",
          encoderChanged,
          currentCount,
          rotClockWise == 0 ? "ClockWise" : "CounterClockWise") ;
      }
      break;

    // Test Setting count
    case 31:
      syslog(LOG_MTEST,
        "Rotary Encoder tests - setting count encoder 0 to 0 (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_set_count(0, 0);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 0 returned error:%d\n", ret);
        break;
      }
      syslog(LOG_MTEST, "Encoder 0 set to 0\n");
      break;

    case 32:
      syslog(LOG_MTEST, "Rotary Encoder tests - setting count encoder 5 to -999999 (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_set_count(5, -999999);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 5 returned error:%d\n", ret);
        break;
      }
      syslog(LOG_MTEST, "Encoder 5 set to -999999\n");
      break;

    case 33:
      syslog(LOG_MTEST, "Rotary Encoder tests - setting count encoder 7 to 0 (%lu)\n",
        userData);
      ret = meadow_rotary_encoder_set_count(7, 0);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Encoder 7 returned error:%d\n", ret);
        break;
      }
      syslog(LOG_MTEST, "Encoder 7 set to 0\n");
      break;
      
    default:
      syslog(LOG_MTEST, "Rotary Encoder tests - received unknown userData of %lu\n",
        userData);
      break;

    // Function parameter null tests
    case 41:
    case 42:
    case 43:
    case 44:
      rotary_encoder_config_test_null_parms(userData);
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
void rotary_encoder_config_test_add_n(uint32_t userData)
{
  // Setup a rotary encoder
  int ret;
  uint32_t encoderNumb;
  uint32_t PinA = 0;
  uint32_t PinB = 0;

  struct rotenc_config_parms* cfg = malloc(sizeof(struct rotenc_config_parms));
  if(cfg == NULL)
  {
    syslog(LOG_ERR, "Memory allocation failed\n");
    return;
  }

  // Note: The input pins defined for F7FeatherV2 would be D05, D06, D07, D08
  // for the first 4 pins. The last 2 are not available on F7FeatherV2
  // Rotary Encoder testing. And be careful to not use the pins used by syslog,
  // D12 and D13.
  switch(userData)
  {
    case 1:
      encoderNumb = 0;
      PinA = ENCODER_PIN_CCM_D05_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_D06_PB13_INPUT;
      break;
    case 2:
      encoderNumb = 5;
      PinA = ENCODER_PIN_CCM_D07_PB7_INPUT;
      PinB = ENCODER_PIN_CCM_D08_PB6_INPUT;
      break;
    case 3:
      encoderNumb = 7;
      PinA = ENCODER_PIN_CCM_D09_PI11_INPUT;
      PinB = ENCODER_PIN_CCM_D10_PD5_INPUT;
      break;
    case 4:
      encoderNumb = 8;    // Invalid-too big
      PinA = ENCODER_PIN_CCM_D05_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_D06_PB13_INPUT;
      break;
  }
  syslog(LOG_MTEST, "Rotary Encoder tests - configuring encoder %lu\n",
    encoderNumb);

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

  // Call configuration function also used by managed code
  ret = meadow_rotary_encoder_config(cfg);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "Error:meadow_rotary_encoder_config returned ret:%d\n",
      ret);
  }

  free (cfg);
}

//============================================================================
void rotary_encoder_config_test_remove_n(uint32_t userData)
{
  int ret;
  uint32_t encoderNumb = 9;  // Invalid
  uint32_t PinA = 0;
  uint32_t PinB = 0;

  struct rotenc_config_parms* cfg = malloc(sizeof(struct rotenc_config_parms));
  if(cfg == NULL)
  {
    syslog(LOG_ERR, "Memory allocation failed\n");
    return;
  }

  switch(userData)
  {
    case 11:
      encoderNumb = 0;
      PinA = ENCODER_PIN_CCM_D05_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_D06_PB13_INPUT;
      break;
    case 12:
      encoderNumb = 5;
      PinA = ENCODER_PIN_CCM_D07_PB7_INPUT;
      PinB = ENCODER_PIN_CCM_D08_PB6_INPUT;
      break;
    case 13:
      encoderNumb = 7;
      PinA = ENCODER_PIN_CCM_D09_PI11_INPUT;
      PinB = ENCODER_PIN_CCM_D10_PD5_INPUT;
      break;
    case 14:
      encoderNumb = 8;    // Invalid
      PinA = ENCODER_PIN_CCM_D05_PB4_INPUT;
      PinB = ENCODER_PIN_CCM_D06_PB13_INPUT;
      break;
  }
  syslog(LOG_MTEST, "Rotary Encoder tests - removing encoder %lu\n",
    encoderNumb);

  // Need to unconfigure the 2 GPIOs used as inputs
  stm32_unconfiggpio(PinA);
  stm32_unconfiggpio(PinB);

  // Setup to unconfigure as .Net will do
  cfg->encoderNumb = encoderNumb;   // Encoder number 0 - 7
  cfg->isAddEncoder = false;        // Remove rotary encoder

  // Call meadow_rotary_encoder.c's configuration function, used by
  // managed code
  ret = meadow_rotary_encoder_config(cfg);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "Error:meadow_rotary_encoder_config returned ret:%d\n",
      ret);
  }

  free (cfg);
}

//==================================================================
// Pass in bad parameters
void rotary_encoder_config_test_null_parms(uint32_t userData)
{
  // Setup a rotary encoder
  int ret;
  int32_t currentCount;
  int32_t encoderChanged;
  uint32_t rotClockWise;

  // All these tests use encoder 0 alone
  uint32_t encoderNumb = 0;

  switch(userData)
  {
    // NULL config test, Should fail
    case 41:
      
      // Call configuration function
      ret = meadow_rotary_encoder_config(NULL);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Error:meadow_rotary_encoder_config returned ret:%d\n",
          ret);
      }
      break;

    // There are 3 pointers needed for meadow_rotary_encoder_read_count()
    case 42:
      ret = meadow_rotary_encoder_read_count(0,
        NULL, &encoderChanged, &rotClockWise);
      break;
    case 43:
      ret = meadow_rotary_encoder_read_count(0,
        &currentCount, NULL, &rotClockWise);
      break;
    case 44:
      ret = meadow_rotary_encoder_read_count(0,
        &currentCount, &encoderChanged, NULL);
      break;
  }
  syslog(LOG_MTEST, "Rotary Encoder parameter test: encoder:%lu, ret:%d\n",
    encoderNumb, ret);
}

#endif  // #if defined(CONFIG_ROTARY_ENCODER_TESTS)

#endif  // defined(CONFIG_MEADOW_ROTARY_ENCODER)
