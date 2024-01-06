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

// #if defined(CONFIG_QUICK_MISC_TESTS)
#pragma message "(--) meadow_rotary_encoder.c"
// #endif

// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//============================================================
// Set == 0 to disable diagnostic output via syslog
// Set == 1 for I/O and config diagnostic output
// Set == 2 or > to output all diagnostic output
#define MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG (0)

// Arbitrary, large value, defining the maximum number of gpios that can be
// monitored.
#define MEADOW_ROTENC_MAX_SUPPORTED_GPIOS (32)

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
  gpio_intrpt_cfg_type_remove,
  gpio_intrpt_cfg_type_rotenc_1a,
  gpio_intrpt_cfg_type_rotenc_1b,
  gpio_intrpt_cfg_type_rotenc_2a,
  gpio_intrpt_cfg_type_rotenc_2b
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

struct rotencGpioInfo
{
  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinId;              // Supplied by configuration

  // What is the configuration type for this?
  // 0 = remove, 1 = new, 2 = low-power sleep wakeup
  uint8_t gpioUsage;         // Supplied by configuration

  // Address of correct "Input Data Register" which holds GPIO's hardware port
  // state bits
  uint32_t IDRAddress;        // Calculated during configuration
};

// This is the list of the GPIOs currently being timed. The entries in this
// list are very short lived, begin added as soon as the GPIO ISR is called and
// removed as soon as the glitch or debounce period has elapsed.
// static struct rotencGpioInfo *_allGpiosBeingTimed[MEADOW_ROTENC_MAX_SUPPORTED_GPIOS];

// This list keeps the address of all the allocated structures so they can be
// removed when the GPIO is removed
static struct rotencGpioInfo *_allRotaryEncodedGpios[MEADOW_ROTENC_MAX_SUPPORTED_GPIOS];

// // Indicates the total number of GPIOs currently being timed.
// static volatile int _allGpiosBeingTimedCnt = 0;

// Incremented and decrementd during configuration to indicate how many GPIOs
// need to be timed (only glitch and debounce GPIOs need to be timed).
static volatile int _totalGpiosCanBeTimed = 0;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

// static int rotenc_config_interrupt_prep_timer(int stm32_timer_numb);
// static void meadow_rotenc_timer_enable(uint32_t timerBase);
static inline int rotenc_forward_interrupt_to_core(struct rotencGpioInfo *gpioInfoAddr, uint8_t state);
static void rotenc_remove_from_timed_list_and_decr(struct rotencGpioInfo *gpioInfoAddr);
static int rotenc_config_interrupt_remove(struct rotenc_gpio_config* cfg,
          struct rotencGpioInfo *gpioInfoAddr);

// Basic timers need to turn on/off the timer a different way 
// NOT USED YET
// static void rotenc_turn_periodic_timer_on(void);
// static void rotenc_turn_periodic_timer_off(void);

static bool rotenc_get_current_gpio_state(struct rotencGpioInfo *gpioInfoAddr);

int rotenc_config_rot_enc_new(struct rotenc_gpio_config* cfg,
          struct rotencGpioInfo *gpioInfoAddr, uint32_t cfgset,
          enum GPIOInterruptCfgType_e classDef);

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

static uint32_t _prevState1 = 0;
static uint32_t _prevOff1 = 0;
static int _activeCnt1 = 0;

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

static void FindDirection1(uint32_t newStateBits, char *pinUpDown)
{
  // Move 2 LS bits from previous to MS
  _prevOff1 <<= 2;           // Move previous A & B to bits 2 & 3
  _prevOff1 |= newStateBits; // Add the new A or B in bits 0 & 1
  _prevOff1 &= 0x0000000f;   // Save only lowest 4 bits

  // Use the new state bits to lookup direction
  int newDir = RotEncLookup[_prevOff1];
  switch (newDir)
  {
    case 0: // no change
    default:
      return;

    case 1: // CW
      _activeCnt1++;
      break;

    case -1: // CCW
      _activeCnt1--;
     break;
  }

// if((_activeCnt1 % 9973) == 0)  // Only output text every x counts
//   {
    syslog(2, "%s-(+1) Total:%08d, %s\n",
                    pinUpDown, _activeCnt1, newDir == 1 ? " CW" : "CCW");
//   }
}

//===========================================================================
// These ISRs the 2 inputs from the rotary encoders
int rotenc_gpio_rot_enc_isr_1a(int irq, void *context, void *arg)
{
  char *pinUpDown;
  struct rotencGpioInfo *gpioInfoAddr = (struct rotencGpioInfo *)arg;

  // Clear bit A, keep bit B
  uint32_t newABit = _prevState1 & 0x02;   // Save previous B state

  // GPIO now high?
  if (rotenc_get_current_gpio_state(gpioInfoAddr))
  {
    pinUpDown = "a^";
    newABit |= 0x01;    // Set bit A
  }
  else
  {
    pinUpDown = "av";
  }

  FindDirection1(newABit, pinUpDown);
  return OK;
}

//===========================================================================
int rotenc_gpio_rot_enc_isr_1b(int irq, void *context, void *arg)
{
  char *pinUpDown;
  struct rotencGpioInfo *gpioInfoAddr = (struct rotencGpioInfo *)arg;

  // Clear bit B bit, keep bit A
  uint32_t newBBit = _prevState1 & 0x01;   // Save previous A state

  // GPIO now high?
  if (rotenc_get_current_gpio_state(gpioInfoAddr))
  {
    pinUpDown = "b^";
    newBBit |= 0x02;              // Set bit B
  }
  else
  {
    pinUpDown = "bv";
  }

  FindDirection1(newBBit, pinUpDown);
  return OK;
}

//===============================================================
// Forward interrupt info to Meadow.Core
int rotenc_forward_interrupt_to_core(struct rotencGpioInfo *gpioInfoAddr, uint8_t state)
{
  int ret;
  extern mqd_t s_int_queue;
  
  // Timing must be finished
  DEBUG_SET_LOW(DEBUG_PIN_V2_A3);

  // Forward to Meadow.Core
  char queue_buffer[MINT_MSG_QUEUE_MSG_SIZE];
  queue_buffer[0] = gpioInfoAddr->PinId;
  queue_buffer[1] = state;

  // This message queue is opened in
  // /Meadow/nuttx/configs/stm32f777zit6-meadow/src/meadow-upd.c when the upd
  // is opened
  ret = mq_send(s_int_queue, queue_buffer, MINT_MSG_QUEUE_MSG_SIZE, 0);
  if(ret < 0)
  {
    if(errno == ENOMEM)
    {
      syslog(LOG_ERR, "0x%02x Queue overflow (too fast?)\n", gpioInfoAddr->PinId);
    }
    else
    {
      syslog(LOG_ERR, "%s@%d-0x%02x mq_send failed:%d, errno:%d\n", __FILE__, __LINE__, 
          gpioInfoAddr->PinId, ret, get_errno());
    }
  }

  return ret;
}

//===============================================================
bool rotenc_get_current_gpio_state(struct rotencGpioInfo *gpioInfoAddr)
{
  uint8_t pinNumb = gpioInfoAddr->PinId & 0x0f;
  uint32_t idrRegisterValues = *((uint32_t *)(gpioInfoAddr->IDRAddress));
  return (idrRegisterValues & (1 << pinNumb)) > 0 ? true : false;
}

//===============================================================
// We need to save the address of all allocated memory so it can be freed
// when the GPIO is removed.
static int rotenc_add_new_gpio_to_allocation_list(
            struct rotencGpioInfo *gpioInfoAddr)
{
  for(int i = 0; i < MEADOW_ROTENC_MAX_SUPPORTED_GPIOS; i++)
  {
    // Find unused slot
    if(_allRotaryEncodedGpios[i] == NULL)
    {
      _allRotaryEncodedGpios[i] = gpioInfoAddr;
      return OK;
    }
  }
  return -ENOSPC;   // No space
}

//===============================================================
// When removing a GPIO we must find it's memory so it can be freed
static int rotenc_free_gpio_in_allocation_list(uint8_t pinId)
{
  for(int i = 0; i < MEADOW_ROTENC_MAX_SUPPORTED_GPIOS; i++)
  {
    // If NULL continue
    if(_allRotaryEncodedGpios[i] == NULL)
      continue;

    // Match the allocation
    if(_allRotaryEncodedGpios[i]->PinId == pinId)
    {
      free(_allRotaryEncodedGpios[i]);
      _allRotaryEncodedGpios[i] = NULL;
      return OK;
    }
  }

  return -ENODATA;
}

//===============================================================
// Removes from the timer isr
void rotenc_remove_from_timed_list_and_decr(struct rotencGpioInfo *gpioInfoAddr)
{
//   int activeOffset;

  // Find the entry specified
//   for(activeOffset = 0; activeOffset < MEADOW_ROTENC_MAX_SUPPORTED_GPIOS; activeOffset++)
//   {
//     if(_allGpiosBeingTimed[activeOffset] == gpioInfoAddr)
//       break;
//   }

  // Was the item in the list?
//   if(activeOffset == MEADOW_ROTENC_MAX_SUPPORTED_GPIOS)
//     return;  // Not found

//   // Keep tally of entries
//   _allGpiosBeingTimedCnt--;

  // Is this the only entry?
//   if(activeOffset == _allGpiosBeingTimedCnt)
//   {
//     // Remove the last entry in the array
//     _allGpiosBeingTimed[activeOffset] = NULL;
//     return;
//   }
 
  // Compress the list by moving the last entry in the list to the slot we
  // are about to remove and then clearing the last slot.
//   _allGpiosBeingTimed[activeOffset] = _allGpiosBeingTimed[_allGpiosBeingTimedCnt];
//   _allGpiosBeingTimed[_allGpiosBeingTimedCnt] = NULL;
  return;
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
int rotenc_config_interrupt(struct rotenc_gpio_config* cfg)
{
  int ret = OK;

  struct rotencGpioInfo *gpioInfoAddr;
  uint8_t pinDesignation = cfg->port << 4 | cfg->pin;

  syslog(1, "Entered rotenc_config_interrupt\n");

  if(_firstTimeConfig)
  {
    // _allGpiosBeingTimedCnt = 0;
    _firstTimeConfig = false;

    for(int i = 0; i < MEADOW_ROTENC_MAX_SUPPORTED_GPIOS; i++)
    {
      _allRotaryEncodedGpios[i] = NULL;
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
    //   syslog(LOG_ERR, "mint-(cfg)-rotenc_config_interrupt_prep_timer failed, ret:%d\n", ret);
    //   return ret;
    // }
  }

  // Allocate memory for this GPIO's configuration and data storage needs
  gpioInfoAddr = (struct rotencGpioInfo *) zalloc(sizeof (struct rotencGpioInfo));
  if(gpioInfoAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }
  
  // We must set a few elements in the struct for this configuration
  gpioInfoAddr->PinId = pinDesignation;

  // Find the correct offset for this GPIO
  gpioInfoAddr->IDRAddress = inputDataRegAddrs[cfg->port];
  // gpioInfoAddr->LastKnownGpioState = 0xff;
  gpioInfoAddr->gpioUsage = cfg->configType;

  // Setup the Nuttx cfgset for this point to be configured by Nuttx
  uint32_t cfgset = (pinDesignation & 0x000000ff);   // Set Port and Pin and clear MM

  switch (cfg->configType)
  {
  case gpio_intrpt_cfg_type_remove:
    ret = rotenc_config_interrupt_remove(cfg, gpioInfoAddr);
    break;

  case gpio_intrpt_cfg_type_rotenc_1a:
    ret = rotenc_config_rot_enc_new(cfg, gpioInfoAddr, cfgset, gpio_intrpt_cfg_type_rotenc_1a);
    break;

  case gpio_intrpt_cfg_type_rotenc_1b:
    ret = rotenc_config_rot_enc_new(cfg, gpioInfoAddr, cfgset, gpio_intrpt_cfg_type_rotenc_1b);
    break;

  case gpio_intrpt_cfg_type_rotenc_2a:
    ret = rotenc_config_rot_enc_new(cfg, gpioInfoAddr, cfgset, gpio_intrpt_cfg_type_rotenc_2a);
    break;

  case gpio_intrpt_cfg_type_rotenc_2b:
    ret = rotenc_config_rot_enc_new(cfg, gpioInfoAddr, cfgset, gpio_intrpt_cfg_type_rotenc_2b);
    break;

  default:
    break;
  }

  return ret;
}

//========================================================================
// Remove an existing interrupt entry
int rotenc_config_interrupt_remove(struct rotenc_gpio_config* cfg,
          struct rotencGpioInfo *gpioInfoAddr)
{
  int ret;

  // Disable - remove a GPIO from being monitored
#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Removing GPIO\n", gpioInfoAddr->PinId,
              ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif

  // Small chance but it might be actively timing
  rotenc_remove_from_timed_list_and_decr(gpioInfoAddr);

  // Tell Nuttx to forget about this interrupt
  ret = stm32_gpiosetevent(gpioInfoAddr->PinId, 0, 0, 0, NULL, NULL);

  // This call will free the memory allocated when this GPIO was originally
  // configured
  ret = rotenc_free_gpio_in_allocation_list(gpioInfoAddr->PinId);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-rotenc_free_gpio_in_allocation_list returned, ret:%d\n",
              __FILE__, __LINE__, ret);
    // Reported error might as well finish removing GPIO
  }

  free(gpioInfoAddr);
  return ret;
}

//=================================================================
int rotenc_config_rot_enc_new(struct rotenc_gpio_config* cfg,
          struct rotencGpioInfo *gpioInfoAddr, uint32_t cfgset,
          enum GPIOInterruptCfgType_e cfgTypeDef)
{
  int ret;

  syslog(1, "Entered rotenc_config_rot_enc_new\n");

  // Save the allocated memory so it can be freed when/if GPIO interrupt is
  // disposed of.
  // ret = rotenc_add_new_gpio_to_allocation_list(gpioInfoAddr);
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "mint-(cfg)-rotenc_add_new_gpio_to_allocation_list, ret:%d\n", ret);
  //   return ret;
  // }

  // cfgset contains 20-bits of data. It is required by the Nuttx stm32_gpiosetevent
  // function. If the 20 bits of data are not correct, this Nuttx function will
  // reconfigure the GPIO based on whatever the data is in cfgset.
  // See stm32_gpio.h for more information.
  // Inputs: MMUU .... ...X PPPP BBBB
  // MM = Mode for input (this is 00)
  // UU = pull up, pull down or float
  // X  = configure as EXTI interrupt. Event or ISR set by stm32_gpiosetevent
  switch(cfg->resistorMode)
  {
    case 0: // Float
      cfgset |= GPIO_FLOAT;
      break;    // 0 = do nothing
    case 1: // Pull up
      cfgset |= GPIO_PULLUP;
      break;
    case 2: // Pull down
      cfgset |= GPIO_PULLDOWN;
      break;
  }

#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(cfg)- 0x%02x (P%c%d)-Cfg Enabled-LKS:%d, GLDuration:%d, DBDuration:%d, RiseFallValue:%d, cfgset:0x%08x\n",
            gpioInfoAddr->PinId,
            ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f,
            gpioInfoAddr->LastKnownGpioState,
            gpioInfoAddr->GlitchConfiguredDuration,
            gpioInfoAddr->DebounceConfiguredDuration,
            gpioInfoAddr->GpioRiseFallValue,
            cfgset);
#endif

  // Tell Nuttx about interrupt parameters
#if MEADOW_ROTENC_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Config Glitch\n", gpioInfoAddr->PinId,
              ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif

  // Rotary Encoder - we must receive both rising and falling to correctly
  // calculate the count and direction.
  // 0 = 1a, 1 = 1b, 2 = 2a and 3 = 2b
  switch((int)cfgTypeDef)
  {
    case gpio_intrpt_cfg_type_rotenc_1a:
      ret = stm32_gpiosetevent(
      cfgset,                     // Nuttx cfgset
      1,                          // risingEdge,
      1,                          // fallingEdge,
      0,                          // event
      rotenc_gpio_rot_enc_isr_1a,   // ISR
      gpioInfoAddr);              // GPIO information address
    break;

    case gpio_intrpt_cfg_type_rotenc_1b:
      ret = stm32_gpiosetevent(
      cfgset,                     // Nuttx cfgset
      1,                          // risingEdge,
      1,                          // fallingEdge,
      0,                          // event
      rotenc_gpio_rot_enc_isr_1b,   // ISR
      gpioInfoAddr);              // GPIO information address
    break;

    default:
      syslog(2, "Reached default of switch\n");
      break;

  }
  return ret;
}

#endif      // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0
