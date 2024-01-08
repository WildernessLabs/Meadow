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
// Set == 1 for I/O and config diagnostic output
// Set == 2 or > to output all diagnostic output
#define MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG (0)

// Defines the maximum number of gpios that can be monitored.
// Since the F7 + Nuttx only have 16-interrupt groups there's no point in
// having more that 8 retary encoders connected
#define MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED (8)

// FOR ROTARY ENCODER SPEED WILL NEED A TIMER UNLESS SOMEOTHER SCHEME IS USED
// THE TIMER CODE IS COPIED FROM 
// Timer 13 isn't much used in Meadow
// #define MEADOW_ROTENC_STM32F7_TIMER_NUMBER (13)

// Prescaler is be between 0 and 0xffff. A prescaler value of 0 to will
// not divide the input clock and a prescaler value of 1 will divide
// the clock by 2 etc.
// Timer 7's input clock is 96 MHz (1/2 of the STM32_SYSCLK_FREQUENCY speed).
// (prescaler + 1) * (auto reload register + 1) = TimerClock / frequency.
// The following values will give us a timer overflow interrupt every 0.1
// millisecond, which is the desired frequency
// #define MEADOW_ROTENC_RUNNING_PSC (959)
// #define MEADOW_ROTENC_RUNNING_ARR (9)

#if CONFIG_USEC_PER_TICK == 1000
#define MEADOW_ROTENC_TICK_MILLISEC_FACTOR (10)
#else
#define MEADOW_ROTENC_TICK_MILLISEC_FACTOR (1)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _firstTimeConfig = true;
// static struct stm32_tim_dev_s *_periodicTimer;

enum RequestedInterruptMode_e
{
  rqstdintmode_none,
  rqstdintmode_rising,
  rqstdintmode_falling,
  rqstdintmode_both
};

enum GPIOInterruptCfgType_e
{
  gpio_intrpt_cfg_type_remove   = 0,
  gpio_intrpt_cfg_type_rotenc_1 = 1,
  gpio_intrpt_cfg_type_rotenc_2 = 2,
  gpio_intrpt_cfg_type_rotenc_3 = 3,
  gpio_intrpt_cfg_type_rotenc_4 = 4
};

// All F7 possible input data registers addresses, used for ISR access to GPIO
// state value.
static uint32_t inputDataRegAddrs[] = 
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
#define MEADOW_HW_INPUT_DATA_REGS_TOTAL (sizeof(inputDataRegAddrs) / sizeof(uint32_t))

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
   
  // Get previous B state
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

  // Get previous A state
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

// //===============================================================
// // Read the pin that generated this interrupt. This assumes the pin's state
// // has not changed since the interrupt was generated.
// bool rotenc_get_current_gpio_state(rotaryEncoderInfo_t *rotaryEncoderAddr, bool isPinA)
// {
//   uint8_t pinNumb;
//   uint32_t idrRegister;

//   if(isPinA)
//   {
//     pinNumb = rotaryEncoderAddr->PinInfoA & 0x0f;
//     idrRegister = *((uint32_t *)(rotaryEncoderAddr->IDRAddressA));
//   }
//   else
//   {
//     pinNumb = rotaryEncoderAddr->PinInfoB & 0x0f;
//     idrRegister = *((uint32_t *)(rotaryEncoderAddr->IDRAddressB));
//   }
//   return (idrRegister & (1 << pinNumb)) > 0 ? true : false;
// }

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

//=============================================================================
// Timer setup is here. This should only be called once to prepare timer for
// for periodic operation.
// Note: Timers 6 & 7 are Basic Timers. Timer 6 can be used to measure CPU idle
// time and Timer 7 is used here. Since neither timer has any GPIO this means
// other timers can be used for GPIO related work.
// static int rotenc_config_interrupt_prep_timer(int stm32_timer_numb)
// {
//   // int ret;
  
//   // Setup the clock enable
//   modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIM7EN);
  
//   // Set prescaler and auto reload register to determine timer interrupt period
//   putreg16(MEADOW_ROTENC_RUNNING_PSC, STM32_TIM7_BASE + STM32_BTIM_PSC_OFFSET);
//   putreg16(MEADOW_ROTENC_RUNNING_ARR, STM32_TIM7_BASE + STM32_BTIM_ARR_OFFSET);

//   uint16_t regval = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
//   regval |= BTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
//   putreg16(regval, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);

//   // Timer 7 only supports UIE interrupt
//   putreg16(BTIM_DIER_UIE, STM32_TIM7_BASE + STM32_BTIM_DIER_OFFSET);

//   // The ISR for periodic interupt for timing, will roll-over every 65536
//   // counts.
// // REMOVED SO IT WOULD BUILD
// //   ret = irq_attach(STM32_IRQ_TIM7, mint_isr_periodic, NULL);
// //   if(ret < 0)
// //   {
// //     syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
// //           __FILE__, __LINE__, ret, errno);
// //     return ret;
// //   }

//   // Clear interrupt bit
//   uint16_t timStatusReg = getreg16(STM32_TIM7_BASE + STM32_GTIM_SR_OFFSET);
//   timStatusReg &= ~BTIM_SR_UIF;
//   putreg16(timStatusReg, STM32_TIM7_BASE + STM32_GTIM_SR_OFFSET);

//   up_enable_irq(STM32_IRQ_TIM7);

//   meadow_rotenc_timer_enable(STM32_TIM7_BASE);

//   return OK;
// }

//=============================================================
// Enables the timer
// void meadow_rotenc_timer_enable(uint32_t timerBase)
// {
//   // Why this order? tryed to copy the NUTTX order
//   uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
//   cr1Val |= GTIM_CR1_CEN;
  
//   uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
//   egrVal |= GTIM_EGR_UG;

//   putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);

//   putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
// }

// NOT USED YET
// //=====================================================================
// // Setting the Counter Enable bit
// static void rotenc_turn_periodic_timer_on(void)
// {
//   uint16_t cr1Val = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
//   cr1Val |= BTIM_CR1_CEN;   // counter enable
//   putreg16(cr1Val, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
// }

// //=====================================================================
// static void rotenc_turn_periodic_timer_off(void)
// {
//   uint16_t cr1Val = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
//   cr1Val &= ~BTIM_CR1_CEN;   // counter enable
//   putreg16(cr1Val, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
// }

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from meadow-upd.c to configure or remove a gpio for monitoring
// CALLED TWICE ONCE FOR EACH PIN A & B
int rotenc_config_interrupt(struct rotenc_config_parms* cfg)
{
  int ret = OK;
  int i;

  rotaryEncoderInfo_t *rotaryEncoderAddr;
  uint8_t pinDesignationA = cfg->portA << 4 | cfg->pinA;
  uint8_t pinDesignationB = cfg->portB << 4 | cfg->pinB;

  syslog(1, "Entered rotenc_config_interrupt\n");

  if(_firstTimeConfig)
  {
    // _allGpiosBeingTimedCnt = 0;
    _firstTimeConfig = false;

    // Empty list of all active rotary encoders
    for(i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
    {
      _allRotaryEncodersList[i] = NULL;
    }

    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A0);    // True while in periodic isr
    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A1);    // True while in no delay isr
    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A2);    // True while in delay isr
    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A3);
    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A4);
    // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A5);
    
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A0);
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A2);
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A3);
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A4);
    // DEBUG_SET_LOW(DEBUG_PIN_V2_A5);

    // // Setup the timer once, the first time
    // ret = rotenc_config_interrupt_prep_timer(MEADOW_ROTENC_STM32F7_TIMER_NUMBER);
    // if(ret < 0)
    // {
    //   syslog(LOG_ERR, "rotenc(cfg)-rotenc_config_interrupt_prep_timer failed, ret:%d\n", ret);
    //   return ret;
    // }
  }
  
  // Initialize all elements
  for(i = 0; i < MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED; i++)
  {
    if(_allRotaryEncodersList[i] == NULL)
      break;
  }

  if(i == MEADOW_ROTARY_ENCODERS_MAX_SUPPORTED)
  {
    // Handle error
  }
  
  rotaryEncoderAddr = zalloc(sizeof(rotaryEncoderInfo_t));
  if(rotaryEncoderAddr == NULL)
  {
    // Handle error
  }

  // Add allocated memory to array
  ret = rotenc_add_new_config_to_allocation_list(rotaryEncoderAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "No room for allocated memory, ret:%d\n", ret);
    return ret;
  }

  // Save the 2 GPIO inputs. These will be used to identify this encoder when deleted
  rotaryEncoderAddr->PinInfoA = pinDesignationA;
  rotaryEncoderAddr->PinInfoB = pinDesignationB;

  // Find the correct offset for both GPIOs
  rotaryEncoderAddr->IDRAddressA = inputDataRegAddrs[cfg->portA];
  rotaryEncoderAddr->IDRAddressB = inputDataRegAddrs[cfg->portB];

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
  syslog(LOG_INFO, "rotenc(cfg)- 0x%02x (P%c%d)-Cfg cfgset:0x%08x\n",
            rotaryEncoderAddr->PinId,
            ((rotaryEncoderAddr->PinId) >> 4) + 'A', rotaryEncoderAddr->PinId & 0x0f,
            cfgset);
#endif

  // Tell Nuttx about interrupt parameters
  if(cfg->configType == gpio_intrpt_cfg_type_remove)
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
  rotenc_gpio_rot_enc_isr_a,      // ISR A
  rotaryEncoderAddr);              // Encoder information address

  rotaryEncoderAddr->cfgIsRotEncA = false;
  ret = stm32_gpiosetevent(
  cfgsetB,                      // Nuttx cfgset
  1,                            // risingEdge,
  1,                            // fallingEdge,
  0,                            // event
  rotenc_gpio_rot_enc_isr_b,      // ISR B
  rotaryEncoderAddr);              // Encoder information address

  return ret;
}

//========================================================================
// Remove an existing interrupt entry
int rotenc_config_interrupt_remove(struct rotenc_config_parms* cfg,
          rotaryEncoderInfo_t *rotaryEncoderAddr)
{
  int ret;

  // Disable - remove a GPIO from being monitored
#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "rotenc(cfg)-0x%02x (P%c%d)--Removing GPIO\n", rotaryEncoderAddr->PinId,
              ((rotaryEncoderAddr->PinId) >> 4) + 'A', rotaryEncoderAddr->PinId & 0x0f);
#endif

  // Tell Nuttx to forget about these interrupts
  ret = stm32_gpiosetevent(rotaryEncoderAddr->PinInfoA, 0, 0, 0, NULL, NULL);
  ret = stm32_gpiosetevent(rotaryEncoderAddr->PinInfoB, 0, 0, 0, NULL, NULL);

  // This call will free the memory allocated
  ret = rotenc_free_config_in_allocation_list(rotaryEncoderAddr->PinInfoA);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-rotenc_free_gpio_in_allocation_list returned, ret:%d\n",
              __FILE__, __LINE__, ret);
    // Reported error might as well finish removing GPIO
  }

  free(rotaryEncoderAddr);
  return ret;
}

#endif      // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
