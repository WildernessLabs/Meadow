/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_rotary_encoder.c
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

// This module contains code to handle rotary encoders at a higher speed than
// can be achieved using C# and interrupts

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

#if defined(CONFIG_MEADOW_ROTARY_ENCODER)

#include <nuttx/config.h>
#include <string.h>
#include <stdbool.h>
#include <arch/board/board.h>
#include <nuttx/mqueue.h>
#include <errno.h>
#include "chip.h"
#include "stm32f777zit6-meadow.h"
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"
#include <chip/stm32f76xx77xx_rcc.h>
#include <nuttx/clock.h>    // for testing
#include <nuttx/arch.h>
#include "meadow-upd.h"
#include <meadow/meadow_hw_version.h>
#include <meadow/meadow_rotary_encoder.h>

#if defined(CONFIG_ROTARY_ENCODER_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
#pragma message "(--) meadow_rotary_encoder.c"
#endif

// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//============================================================
// Defines the maximum number of encoders that can be monitored.
// Since the F7 with Nuttx only have 16 "interrupt groups" there's no
// point in having more that 8 rotary encoders
#define MEADOW_ROTARY_ENC_MAX (8)

/****************************************************************************
 * Private Data
 ****************************************************************************/
// All F7 possible input data registers addresses, used for ISR access to read
// GPIO state value.
static uint32_t rotencInputDataReg[] = 
{
  STM32_GPIOA_IDR,
  STM32_GPIOB_IDR,
  STM32_GPIOC_IDR,
  STM32_GPIOD_IDR,
  STM32_GPIOE_IDR,
  STM32_GPIOF_IDR,
  STM32_GPIOG_IDR,
  STM32_GPIOH_IDR,
  STM32_GPIOI_IDR,
  STM32_GPIOJ_IDR,
  STM32_GPIOK_IDR,
};

// This is the list of the GPIOs currently configured.
static rotaryEncoderInfo_t *_allRotaryEncodersList[MEADOW_ROTARY_ENC_MAX]\
        = {NULL};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int rotenc_config_interrupt_remove(struct rotenc_config_parms* cfg);

// A rotary encoder has 2 digital outputs, called A and B. Therefore, 2 GPIO
// input are needed. The way a rotary encoder is designed, when rotated it
// toggles output A and output B 90 degrees out of phase. If A toggles
// followed by B it indicates clock-wise rotation, if B then A counter
// clock-wise. This is how rotational direction is determined and whether
// the count is incremented or decremented.
// For each transition we must consider the previous state of A and B and the
// current state of A and B. This can be represented by a 4-bit number.
// Bits 0 and 1 represent the current state of A and B and bits 2 and 3
// represent previous states of A and B.
// |old|new|
// |A|B|A|B|
//  3 2 1 0
//
// This 4-bit number creates 16 possible values (0-15). The following lookup
// table is used to determine if there is a change and if there is its
// direction.
// For example, the if bits 0-3 are all 0, this would mean that A
// is Low and was Low, and that B is Low and was Low (i.e. nothing changed.)

// The offsets yield 16 values which determine the direction, and how it
// effects the count. Either add 1 or subtract 1 or do nothing
static int rotEncLookup[] =
{
  // Notice the values forward and backward match
  0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This functions finds the direction (CW or CCW) and adds/subtracts the count.
// This counts rising and falling edges of interrupts, 'A' and 'B'.
static void rotenc_calc_dir_count(rotaryEncoderInfo_t *rotaryEncoderPtr,
          uint32_t newCondBits)
{
  uint32_t prevOff = rotaryEncoderPtr->prevOff;
  prevOff <<= 2;           // Move previous A & B to bits 2 & 3
  prevOff |= newCondBits;  // Add the new A or B in bits 0 & 1
  prevOff &= 0x0000000f;   // Save only lowest 4 bits
  rotaryEncoderPtr->prevOff = prevOff;

  // Use the new state bits to lookup direction (only 3 possible values)
  int newDir = rotEncLookup[prevOff];

  switch (newDir)
  {
    case 0: // no change
    default:
      return;

    case 1: // CW
      rotaryEncoderPtr->rotClockWise = 0;
      rotaryEncoderPtr->abEdgeCount++;
      rotaryEncoderPtr->abEdgeChange++;
      break;

    case -1: // CCW
      rotaryEncoderPtr->rotClockWise = 1;
      rotaryEncoderPtr->abEdgeCount--;
      rotaryEncoderPtr->abEdgeChange--;
      break;
  }

#if defined(CONFIG_ROTARY_ENCODER_TESTS)
  // For fast inputs, don't show every entry. The following was about right
  // for hall effect sensor on the shaft of a brushed motor
  // (48 edges/revolution).
  if((rotaryEncoderPtr->abEdgeCount % 9973) == 0)
  {
    syslog(2, "Encoder:%lu, Interrupts:%08ld, Direction:%s\n",
      rotaryEncoderPtr->EncoderNumb,
      rotaryEncoderPtr->abEdgeCount,
      rotaryEncoderPtr->rotClockWise == 0 ? "ClockWise" : "CounterClockWise");
  }
#endif
}

//===========================================================================
// Rotary encoder input A
// Note:may need to insure that both 'A' and 'B' are changing every other
// call, Possibly use int isrACount and isrBCount and if difference > than
// some small number, warn user (e.g. "Only GPIO is changing").
int rotenc_gpio_rot_enc_isr_a(int irq, void *context, void *arg)
{
  uint8_t pinNumb;
  uint32_t idrRegister;
  rotaryEncoderInfo_t *rotaryEncoderPtr = (rotaryEncoderInfo_t *)arg;
   
  // Save previous B state
  uint32_t newCondBits = (rotaryEncoderPtr->prevCondBits) & 0x02;

  // Find the current GPIO state 
  idrRegister = *((uint32_t *)(rotaryEncoderPtr->IDRAddressA));
  pinNumb = rotaryEncoderPtr->PinInfoA & 0x0f;
  bool gpioState = (idrRegister & (1 << pinNumb)) > 0 ? true : false;
  
  if(gpioState)
  {
    newCondBits |= 0x01;    // Set bit A if high
  }

  // Only saves 2 ls bits
  rotaryEncoderPtr->prevCondBits = newCondBits;

  rotenc_calc_dir_count(rotaryEncoderPtr, newCondBits);
  return OK;
}

// ===========================================================================
// Rotary encoder input B
int rotenc_gpio_rot_enc_isr_b(int irq, void *context, void *arg)
{
  uint8_t pinNumb;
  uint32_t idrRegister;
  
  rotaryEncoderInfo_t *rotaryEncoderPtr = (rotaryEncoderInfo_t *)arg;

  // Save previous A state
  uint32_t newCondBits = (rotaryEncoderPtr->prevCondBits) & 0x01;

  // Find the current GPIO state 
  idrRegister = *((uint32_t *)(rotaryEncoderPtr->IDRAddressB));
  pinNumb = rotaryEncoderPtr->PinInfoB & 0x0f;
  bool gpioState =  (idrRegister & (1 << pinNumb)) > 0 ? true : false;
  if(gpioState)
  {
    newCondBits |= 0x02;    // Set bit B
  }

  // Only saves 2 ls bits
  rotaryEncoderPtr->prevCondBits = newCondBits;

  rotenc_calc_dir_count(rotaryEncoderPtr, newCondBits);
  return OK;
}

//========================================================================
// Remove an existing rotary encoder entry
// Caller will need to handle GPIO configuration as needed.
int rotenc_config_interrupt_remove(struct rotenc_config_parms* cfg)
{
  int ret;
  uint8_t PinInfoA;
  uint8_t PinInfoB;
  rotaryEncoderInfo_t *rotaryEncoderPtr;

  // Requested to remove existing configuration
  rotaryEncoderPtr = _allRotaryEncodersList[cfg->encoderNumb];
  if(rotaryEncoderPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Slot %d is not configured\n",
              __FILE__, __LINE__, cfg->encoderNumb);
    return -ENODATA;   // No Data
  }

  // Get info from the configuration memory before freeing it
  PinInfoA = rotaryEncoderPtr->PinInfoA;
  PinInfoB = rotaryEncoderPtr->PinInfoB;

  // Tell Nuttx to forget about these GPIO interrupts
  // This call will also disables the interrupts
  ret = stm32_gpiosetevent(PinInfoA, 0, 0, 0, NULL, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error removing encoder:%d Pin A interrupt, ret:%d\n",
              __FILE__, __LINE__, cfg->encoderNumb, ret);
  }

  ret = stm32_gpiosetevent(PinInfoB, 0, 0, 0, NULL, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error removing encoder:%d Pin B interrupt, ret:%d\n",
              __FILE__, __LINE__, cfg->encoderNumb, ret);
  }

  free(rotaryEncoderPtr);
  _allRotaryEncodersList[cfg->encoderNumb] = NULL;

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// To be called from .Net app to configure rotary encoder
int meadow_rotary_encoder_config(struct rotenc_config_parms* cfg)
{
  int ret = OK;
  rotaryEncoderInfo_t *rotaryEncoderPtr;

  if(cfg == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Parameter cfg cannot be NULL\n",
              __FILE__, __LINE__);
    return -EINVAL;
  }

  uint8_t pinDesignationA = cfg->portA << 4 | cfg->pinA;
  uint8_t pinDesignationB = cfg->portB << 4 | cfg->pinB;

  // Request to remove or add an encoder?
  if(! cfg->isAddEncoder)
  {
    ret = rotenc_config_interrupt_remove(cfg);
    return ret;
  }

  // Is encoder number out of range?
  if(cfg->encoderNumb > (MEADOW_ROTARY_ENC_MAX - 1))
  {
    syslog(LOG_ERR, "%s@%d-Encoder %lu not available. Encoder range is 0 - %lu.\n",
              __FILE__, __LINE__, cfg->encoderNumb,
              (MEADOW_ROTARY_ENC_MAX - 1));
    return -ERANGE;    // Out of Range
  }

  // Is requested slot free?
  if(_allRotaryEncodersList[cfg->encoderNumb] != NULL)
  {
    syslog(LOG_ERR, "%s@%d-Slot %d is in use\n",
              __FILE__, __LINE__, cfg->encoderNumb);
    return -ENOTEMPTY;    // Not empty
  }

  rotaryEncoderPtr = zalloc(sizeof(rotaryEncoderInfo_t));
  if(rotaryEncoderPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Memory allocation failed\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // Add allocated memory to free array slot
  _allRotaryEncodersList[cfg->encoderNumb] = rotaryEncoderPtr;

  rotaryEncoderPtr->EncoderNumb = cfg->encoderNumb;

  // The 2 GPIO inputs are used to configure and remove interrupt handling
  rotaryEncoderPtr->PinInfoA = pinDesignationA;
  rotaryEncoderPtr->PinInfoB = pinDesignationB;

  // Find the correct offset for both GPIOs
  rotaryEncoderPtr->IDRAddressA = rotencInputDataReg[cfg->portA];
  rotaryEncoderPtr->IDRAddressB = rotencInputDataReg[cfg->portB];

  // cfgset contains 20-bits of data. It is required by the Nuttx stm32_gpiosetevent
  // function. If the 20 bits of data are not correct, this Nuttx function will
  // reconfigure the GPIO based on whatever the data is in cfgset.
  // See stm32_gpio.h for more information.
  // Inputs: MMUU .... ...X PPPP BBBB
  // MM = Mode for input (this is 00)
  // UU = pull up, pull down or float
  // X  = configure as EXTI interrupt. Event or ISR set by stm32_gpiosetevent
  
  // Setup the Nuttx cfgset for this point to be configured by Nuttx
  uint32_t cfgsetA = (pinDesignationA & 0x000000ff);   // Set Port and Pin and clear MM
  uint32_t cfgsetB = (pinDesignationB & 0x000000ff);   // Set Port and Pin and clear MM
  switch(cfg->resistorMode)
  {
    case 0: // Float
      cfgsetA |= GPIO_FLOAT;
      cfgsetB |= GPIO_FLOAT;
      break;    // 0 = do nothing
    case 1: // Pull up
      cfgsetA |= GPIO_PULLUP;
      cfgsetB |= GPIO_PULLUP;
      break;
    case 2: // Pull down
      cfgsetA |= GPIO_PULLDOWN;
      cfgsetB |= GPIO_PULLDOWN;
      break;
  }

  // Setup both input points to trigger isr
  // This call also enables the interrupts
  ret = stm32_gpiosetevent(
    cfgsetA,                      // Nuttx cfgset
    1,                            // rising edge,
    1,                            // falling edge,
    0,                            // event
    rotenc_gpio_rot_enc_isr_a,    // ISR A
    rotaryEncoderPtr);           // information address
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error encoder:%d Pin A interrupt, ret:%d\n",
              __FILE__, __LINE__, cfg->encoderNumb, ret);
  }
  
  ret = stm32_gpiosetevent(
    cfgsetB,                      // Nuttx cfgset
    1,                            // rising edge,
    1,                            // falling edge,
    0,                            // event
    rotenc_gpio_rot_enc_isr_b,    // ISR B
    rotaryEncoderPtr);           // information address
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error encoder:%d Pin B interrupt, ret:%d\n",
              __FILE__, __LINE__, cfg->encoderNumb, ret);
  }

  return ret;
}

//========================================================================
// Return the current rotary encoder
// Count change since count last read
// Current total count
// the current direction of counting.
int meadow_rotary_encoder_read_count(uint32_t encoderNumb,
  int32_t *encoderCount,      // rotary encoder count
  int32_t *encoderChanged,    // 0 = no changes, change count
  uint32_t *rotClockWise)     // 0 = Clockwise
{
  rotaryEncoderInfo_t *rotaryEncoderPtr;

  // Check for NULL
  if(encoderCount == NULL || encoderChanged == NULL || rotClockWise == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Input parameter cannot be NULL\n",
              __FILE__, __LINE__);
    return -EINVAL;
  }

  // Check for valid encoder number
  if(encoderNumb > (MEADOW_ROTARY_ENC_MAX - 1))
  {
    syslog(LOG_ERR, "%s@%d-Encoder %lu not available. Encoder range is 0 - %lu.\n",
              __FILE__, __LINE__, encoderNumb,
              (MEADOW_ROTARY_ENC_MAX - 1));
    return -ERANGE;    // Out of Range
  }

  rotaryEncoderPtr = _allRotaryEncodersList[encoderNumb];

  // Is requested slot valid?
  if(rotaryEncoderPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Slot %d is not configured\n",
              __FILE__, __LINE__, encoderNumb);
    return -ENODATA;    // No Data
  }

  // Get the encoders state and return
  sched_lock();

  *encoderChanged = rotaryEncoderPtr->abEdgeChange / MEADOW_ROTENC_GPIO_EDGES_PER_COUNT;
  rotaryEncoderPtr->abEdgeChange = 0;  // Clear count

  // Only return a full count
  *encoderCount = rotaryEncoderPtr->abEdgeCount / MEADOW_ROTENC_GPIO_EDGES_PER_COUNT;
  *rotClockWise = rotaryEncoderPtr->rotClockWise;

  sched_unlock();

  return OK;
}

//========================================================================
// Permit the user to set or clear the current rotary encoder count.
int meadow_rotary_encoder_set_count(uint32_t encoderNumb, int32_t encoderCount)
{
  rotaryEncoderInfo_t *rotaryEncoderPtr;
  rotaryEncoderPtr = _allRotaryEncodersList[encoderNumb];

  // Is requested slot valid?
  if(rotaryEncoderPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Slot %d is not configured\n",
              __FILE__, __LINE__, encoderNumb);
    return -ENODATA;    // No Data
  }

  sched_lock();
  rotaryEncoderPtr->abEdgeCount = encoderCount;
  sched_unlock();

  return OK;
}

#endif      // defined(CONFIG_MEADOW_ROTARY_ENCODER)
