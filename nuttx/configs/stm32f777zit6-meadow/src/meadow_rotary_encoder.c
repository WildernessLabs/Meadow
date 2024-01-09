/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_rotary_encoder.c
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

// This module contains code to handle rotary encoders at a higher speed than
// can be achieved using C#

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

#if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0

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
#include "pwrmgmt/pwrmgmt_local.h"
#include <meadow/meadow_hw_version.h>

#include "meadow_rotary_encoder.h"

#if defined(CONFIG_QUICK_MISC_TESTS)
#pragma message "(--) meadow_rotary_encoder.c"
#endif

// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//============================================================
// Set == 0 to disable diagnostic output via syslog
// Set == 1 to enable some syslog output in build
#define MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG (0)

// Defines the maximum number of encoders that can be monitored.
// Since the F7 + Nuttx only have 16-interrupt groups there's no point
// in having more that 8 rotary encoders
#define MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED (8)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _firstTimeConfig = true;

// All F7 possible input data registers addresses, used for ISR access to GPIO
// state value.
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

struct rotaryEncoderInfo_s
{
  bool isBeingUsed;

  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinInfoA;              // Supplied by configuration
  uint8_t PinInfoB;              // Supplied by configuration

  // Address of correct "Input Data Register" which holds GPIO's hardware port
  // state bits
  uint32_t IDRAddressA;        // Calculated during configuration
  uint32_t IDRAddressB;        // Calculated during configuration

  // uint8_t cfgEncoderNumb;        // 1-n
  bool cfgIsRotEncA;             // Interrupt for GPIO A = true 
  uint32_t prevCondBits;
  uint32_t prevOff;
  int activeCnt;
};
typedef struct rotaryEncoderInfo_s rotaryEncoderInfo_t;

// This is the list of the GPIOs currently being timed. The entries in this
// list are very short lived, begin added as soon as the GPIO ISR is called and
// removed as soon as the glitch or debounce period has elapsed.
static rotaryEncoderInfo_t *_allRotaryEncodersList[MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

// Basic timers need to turn on/off the timer a different way 
// NOT USED YET
// static void rotenc_turn_periodic_timer_on(void);
// static void rotenc_turn_periodic_timer_off(void);

static int rotenc_config_interrupt_remove(struct rotenc_config_parms* cfg,
          rotaryEncoderInfo_t *rotaryEncoderAddr);
// static bool rotenc_get_current_gpio_state(rotaryEncoderInfo_t *rotaryEncoderAddr, bool isPinA);

// The rotary encoder has 2 inputs, called A and B. Because of its design
// either A or B changes but not both when the encoder is rotated. This is
// used to determine the direction. If A goes High before B then we are
// rotating one direction if B goes high before A we are rotating the other.
// For each change we must consider both the previous state of A and B and the
// current state of A and B. This can be used to represent 4-bit number.
// |old|new|
// |A|B|A|B|
//  3 2 1 0
// Bits 0 and 1 represent the current state of A and B and bits 2 and 3
// represent previous states of A and B. This 4-bit number yields 16 possible
// combination., however, there are combination that for which no change is
// represent. For example, the if bits 0-3 are all 0, this would mean that A
// was Low and is Low, and that B was Low and is Low (nothing changed.)

// This array of values determine the action to take with the current rotary
// count. Either add 1 or subtract 1 or do nothing.
static int RotEncLookup[] =
{
  // Notice the values forward and backward match
  0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

// This finds the direction (CW or CCW) and adds/subtracts the count.
static void LookupDir(rotaryEncoderInfo_t *rotaryEncoderAddr,
          uint32_t newCondBits)
{
  uint32_t prevOff = rotaryEncoderAddr->prevOff;
  prevOff <<= 2;           // Move previous A & B to bits 2 & 3
  prevOff |= newCondBits;  // Add the new A or B in bits 0 & 1
  prevOff &= 0x0000000f;   // Save only lowest 4 bits
  rotaryEncoderAddr->prevOff = prevOff;

  // Use the new state bits to lookup direction
  int newDir = RotEncLookup[prevOff];

  switch (newDir)
  {
    case 0: // no change
    default:
      return;

    case 1: // CW
      rotaryEncoderAddr->activeCnt++;
      break;

    case -1: // CCW
      rotaryEncoderAddr->activeCnt--;
      break;
  }

  // if((rotaryEncoderAddr->activeCnt % 9973) == 0)  // Only output text every x counts
  // {
    syslog(2, "Total:%08d\n", rotaryEncoderAddr->activeCnt);
  // }
}

//===========================================================================
// Rotary encoder input A
int rotenc_gpio_rot_enc_isr_a(int irq, void *context, void *arg)
{
  uint8_t pinNumb;
  uint32_t idrRegister;
  rotaryEncoderInfo_t *rotaryEncoderAddr = (rotaryEncoderInfo_t *)arg;
   
  // Save previous B state
  uint32_t newCondBits = (rotaryEncoderAddr->prevCondBits) & 0x02;

  // Find the current GPIO state 
  idrRegister = *((uint32_t *)(rotaryEncoderAddr->IDRAddressA));
  pinNumb = rotaryEncoderAddr->PinInfoA & 0x0f;
  bool gpioState = (idrRegister & (1 << pinNumb)) > 0 ? true : false;
  
  if(gpioState)
  {
    newCondBits |= 0x01;    // Set bit A if high
  }

  // Only saves 2 ls bits
  rotaryEncoderAddr->prevCondBits  = newCondBits;

  LookupDir(rotaryEncoderAddr, newCondBits);
  return OK;
}

// ===========================================================================
// Rotary encoder input B
int rotenc_gpio_rot_enc_isr_b(int irq, void *context, void *arg)
{
  uint8_t pinNumb;
  uint32_t idrRegister;
  
  rotaryEncoderInfo_t *rotaryEncoderAddr = (rotaryEncoderInfo_t *)arg;

  // Save previous A state
  uint32_t newCondBits = (rotaryEncoderAddr->prevCondBits) & 0x01;

  // Find the current GPIO state 
  idrRegister = *((uint32_t *)(rotaryEncoderAddr->IDRAddressB));
  pinNumb = rotaryEncoderAddr->PinInfoB & 0x0f;
  bool gpioState =  (idrRegister & (1 << pinNumb)) > 0 ? true : false;
  if(gpioState)
  {
    newCondBits |= 0x02;    // Set bit B
  }

  // Only saves 2 ls bits
  rotaryEncoderAddr->prevCondBits  = newCondBits;

  LookupDir(rotaryEncoderAddr, newCondBits);
  return OK;
}

//===============================================================
// We need to save the address of all allocated memory so it can be freed
// when the GPIO is removed.
static int rotenc_add_new_config_to_allocation_list(
            rotaryEncoderInfo_t *rotaryEncoderAddr)
{
  for(int i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
  {
    // Find unused slot
    if(_allRotaryEncodersList[i] == NULL)
    {
      _allRotaryEncodersList[i] = rotaryEncoderAddr;
      return OK;
    }
  }
  return -ENOSPC;   // No space
}

//===============================================================
// When removing a GPIO we must find it's memory so it can be freed
static int rotenc_free_config_in_allocation_list(uint8_t pinId)
{
  for(int i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
  {
    // If NULL continue
    if(_allRotaryEncodersList[i] == NULL)
      continue;

    // Match the allocation. Matching 1 GPIO should be good enough
    if(_allRotaryEncodersList[i]->PinInfoA == pinId)
    {
      free(_allRotaryEncodersList[i]);
      _allRotaryEncodersList[i] = NULL;
      return OK;
    }
  }

  return -ENODATA;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Will be called from .Net app to configure rotary encoder
int meadow_config_rotary_encoder(struct rotenc_config_parms* cfg)
{
  int ret = OK;
  int i;

  rotaryEncoderInfo_t *rotaryEncoderAddr;
  uint8_t pinDesignationA = cfg->portA << 4 | cfg->pinA;
  uint8_t pinDesignationB = cfg->portB << 4 | cfg->pinB;

  syslog(1, "Entered meadow_config_rotary_encoder\n");

  if(_firstTimeConfig)
  {
    // _allGpiosBeingTimedCnt = 0;
    _firstTimeConfig = false;

    // Empty list of all active rotary encoders
    for(i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
    {
      _allRotaryEncodersList[i] = NULL;
    }
  }
  
  // Initialize all elements
  for(i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
  {
    if(_allRotaryEncodersList[i] == NULL)
      break;
  }

  if(i == MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED)
  {
    syslog(LOG_ERR, "Reached to maximum number of rotary encoders\n");
    return -ENOSPC;   // No space
  }
  
  rotaryEncoderAddr = zalloc(sizeof(rotaryEncoderInfo_t));
  if(rotaryEncoderAddr == NULL)
  {
    syslog(LOG_ERR, "Memory allocation failed\n");
    return -ENOMEM;
  }

  // Add allocated memory to array
  ret = rotenc_add_new_config_to_allocation_list(rotaryEncoderAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "No room for config info, ret:%d\n", ret);
    return ret;
  }

  // Save the 2 GPIO inputs. These will be used to identify this encoder when deleted
  rotaryEncoderAddr->PinInfoA = pinDesignationA;
  rotaryEncoderAddr->PinInfoB = pinDesignationB;

  // Find the correct offset for both GPIOs
  rotaryEncoderAddr->IDRAddressA = rotencInputDataReg[cfg->portA];
  rotaryEncoderAddr->IDRAddressB = rotencInputDataReg[cfg->portB];

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

#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "rotenc(cfg)-0x%02x (P%c%d)-Cfg cfgsetA:0x%08x (B not shown)\n",
            rotaryEncoderAddr->PinInfoA,
            ((rotaryEncoderAddr->PinInfoA) >> 4) + 'A',
            rotaryEncoderAddr->PinInfoA & 0x0f,
            cfgsetA);
#endif

  if(cfg->rotencConfig == false)
  {
    ret = rotenc_config_interrupt_remove(cfg, rotaryEncoderAddr);
    return ret;
  }

  // Setup both input points to trigger isr
  rotaryEncoderAddr->cfgIsRotEncA = true;
  ret = stm32_gpiosetevent(
  cfgsetA,                      // Nuttx cfgset
  1,                            // risingEdge,
  1,                            // fallingEdge,
  0,                            // event
  rotenc_gpio_rot_enc_isr_a,    // ISR A
  rotaryEncoderAddr);           // Encoder information address

  rotaryEncoderAddr->cfgIsRotEncA = false;
  ret = stm32_gpiosetevent(
  cfgsetB,                      // Nuttx cfgset
  1,                            // risingEdge,
  1,                            // fallingEdge,
  0,                            // event
  rotenc_gpio_rot_enc_isr_b,    // ISR B
  rotaryEncoderAddr);           // Encoder information address

  return ret;
}

//========================================================================
// Remove an existing interrupt entry
int rotenc_config_interrupt_remove(struct rotenc_config_parms* cfg,
          rotaryEncoderInfo_t *rotaryEncoderAddr)
{
  int ret;
  uint8_t PinInfoA = rotaryEncoderAddr->PinInfoA;
  uint8_t PinInfoB = rotaryEncoderAddr->PinInfoB;

  // Disable - remove a GPIO from being monitored
#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "PinA P%c%d--Removing associated rotary encoder\n",
            (PinInfoA >> 4) + 'A', PinInfoA & 0x0f);
#endif

  // This call will insure at least Pin A is correct and free allocated
  // memory
  ret = rotenc_free_config_in_allocation_list(rotaryEncoderAddr->PinInfoA);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-rotenc_free_gpio_in_allocation_list returned, ret:%d\n",
              __FILE__, __LINE__, ret);
  }

  // Tell Nuttx to forget about these interrupts
  ret = stm32_gpiosetevent(PinInfoA, 0, 0, 0, NULL, NULL);
  ret = stm32_gpiosetevent(PinInfoB, 0, 0, 0, NULL, NULL);

  free(rotaryEncoderAddr);
  return ret;
}

#endif      // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
