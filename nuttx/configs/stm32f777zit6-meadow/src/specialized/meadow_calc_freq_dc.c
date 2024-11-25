/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_calc_freq_dc.c
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

// This module, uses timers to calculate frequency and duty.

// ToDo List
// Many of this are optional or future
// x1. Add a running average feature. It would be the average since the last
//  reading.
// x2. Add count of the input GPIO trailing edges since last reading.
// 3. Add CCM support! This requires changes to the configuration and adding,
//  modifying or replacing existing tables to support more or all Timers
//  and their associated GPIOs.
// 4. For 16-bit timers, allow with configuration to include SLOW, MED and
//  FAST options to reduce the effects of the 65,536 count rollover glitch.
// 5. Add syscalls as needed (probably 2 maybe 3)
// 6. Test unconfigure (need syscall?)
// 7. Clean up code, remove unneeded header includes

/****************************************************************************
 * Included Files
 ****************************************************************************/
// Consider removing this and always build
#define MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD (1)

#if MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD > 0

// WHAT HEADER FILES ARE REALLY NEEDED?
#include <nuttx/config.h>
#include <arch/board/board.h>

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include <nuttx/semaphore.h>
#include <nuttx/arch.h>

#include "stm32f777zit6-meadow.h"

#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"
#include "stm32_gpio.h"
#include <meadow/meadow_hw_version.h>
#include <meadow/hcom_shared_common.h>
#include <stdlib.h>

#include "specialized/meadow_calc_freq_dc.h"

// Diagnostic
#pragma GCC optimize("O0")    // Prevent compiler from changing the code
#pragma message "(--) meadow_calc_freq_dc.c"
// Diagnostic

//=====================================================
// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
// #include <meadow/meadow_debug_helpers.h>
#define DEBUG_PIN_V2_D05  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN4)
#define DEBUG_PIN_V2_D06  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN13)
// Diagnostic only

//=====================================================
#define MEADOW_FREQ_DC_MAX_TIMER_CHANNELS     (4)
#define MEADOW_FREQ_DC_VALID_DATA_ATTEMPTS    (5)
#define MEADOW_FREQ_DC_BAD_GPIO_VALUE         (0xffffffff)
#define MEADOW_FREQ_DC_TIMER_WIDTH_16         (0)
#define MEADOW_FREQ_DC_TIMER_WIDTH_32         (1)
#define MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT  (65536)

#define MEADOW_FREQ_DC_FREQ_DC_SYNC_UNKNOWN   (100)
#define MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR     (101)
#define MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING   (102)
#define MEADOW_FREQ_DC_FREQ_DC_SYNC_TRAILING  (103)

// Timer counts below this value are not valid because they would represent
// short pulses, to short to measure.
#define MEADOW_FREQ_DC_MINIMUM_USABLE_CNT (180)

// To configure a GPIO as an input to a timer it, needs to contain the how it
// will be used (input with pulldown), Pin and Port, the Timer defined
// alternate function value plus the Nuttx GPIO_ALT value.
#define MEADOW_TIMER_GPIO_CONST (GPIO_ALT | GPIO_INPUT | GPIO_PULLDOWN)

/****************************************************************************
 * Private Data
 ****************************************************************************/
// Notes: related to GPIO Input implementation
// Only timers with GPIO are considered this removes TIM6 and TIM7
// Advanced timer (TIM1 and TIM8) are not included because they have a more
//  complex interrupt structure. This simplified implementation, plus TIM1
//  has no GPIO exposed on F7FeatherV1 or F7FeatherV2, only TIM8.
// TIM2 is hard wired to the tri-color LEDs on F7FeatherV1 and F7FeatherV2.
//   But, included for possible CCM use.
// TIM13 has no GPIO exposed on F7FeatherV1 or F7FeatherV2. But, included for
//   possible CCM use.
// TIM14 only has one GPIO exposed on F7FeatherV1. But, included for possible
//   CCM use.

// This array contains timer information that is fixed by the STM32F7. It
// contains the timers that are currently available and useable. It also
// defines which timers can be used and invariant characteristics. Several
// of these values have be reduced to a bit-field simple to save space.
static struct freqDcTimerInfo_s freqTimerInfoArray[] = 
{
            //   |--- bit-field---|
            //   #  wid max apb fut    Base Addr       Clk Timer Enable      IRQ Vector     Ptr
  /* TIM2   */  {2 , 1,  0,  0, 0, STM32_TIM2_BASE,  RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2 , NULL},
  /* TIM3   */  {3 , 0,  0,  0, 0, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3 , NULL},
  /* TIM4   */  {4 , 0,  0,  0, 0, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4 , NULL},
  /* TIM5   */  {5 , 1,  0,  0, 0, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5 , NULL},
  /* TIM9   */  {9 , 0,  1,  1, 0, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9 , NULL},
  /* TIM10  */  {10, 0,  1,  1, 0, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, NULL},
  /* TIM11  */  {11, 0,  1,  1, 0, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, NULL},
  /* TIM12  */  {12, 0,  0,  0, 0, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, NULL},
  /* TIM13  */  {13, 0,  0,  0, 0, STM32_TIM13_BASE, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13, NULL},
  /* TIM14  */  {14, 0,  0,  0, 0, STM32_TIM14_BASE, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14, NULL},
};

#define MEADOW_FREQ_DC_TOTAL_TIMERS_AVAILABLE (sizeof(freqTimerInfoArray)/sizeof(struct freqDcTimerInfo_s))

//----------------------------------------------------------------------------
// GPIOs definitions are in their own table due to the GPIO being based on
// the F7 type.
struct timerGpioInfo_s
{
  // In Nuttx pin is bits 3:0, port bits 7:4 (one byte) and Alt Func 15:12
  uint8_t timerF7v1Gpio[4];  // GPIO for each timer channel
  uint8_t timerF7v2Gpio[4];  // GPIO for each timer channel
  uint16_t timerAltFunc;     // GPIO Alternate Function for each timer
};

// This array defines the GPIO values that must be used by the various timers.
// There can be up to 4 channels per timer. Notice that this array contains
// F7v1 and F7v2 values as well as the alternate function for each timer. It
// should be obvious but, this table must line up with the previous
// freqTimerInfoArray table.
// The '*' below indicates that this GPIO is used by more than 1 timer.
static struct timerGpioInfo_s timerGpioInfoArray[] =
{
  //                            F7v1                                              F7v2                       Alt Func
  /* TIM2  GPIO NOT EXPOSED     */ {{0xff,0xff,0xff,0xff}, /* GPIO NOT EXPOSED     */ {0xff,0xff,0xff,0xff}, GPIO_AF1},
  /* TIM3  D02, D05, D06, D09   */ {{0x26,0x27,0x10,0x11}, /* D05, D10, A03, A04   */ {0x14,0x27,0x10,0x11}, GPIO_AF2},
  /* TIM4  D08, D07, D03*, D04* */ {{0x16,0x17,0x18,0x19}, /* D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19}, GPIO_AF2},
  /* TIM5  D10,                 */ {{0x7a,0xff,0xff,0xff}, /* D02,            A02  */ {0x7a,0xff,0xff,0x03}, GPIO_AF2},
  /* TIM9  A02,                 */ {{0x03,0xff,0xff,0xff}, /* A02                  */ {0x03,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM10 D03*,                */ {{0x18,0xff,0xff,0xff}, /* D03*                 */ {0x18,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM11 D04*,                */ {{0x19,0xff,0xff,0xff}, /* D04*                 */ {0x19,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM12 D12, D13             */ {{0x1e,0x1e,0xff,0xff}, /* D12, D13             */ {0x1e,0x1f,0xff,0xff}, GPIO_AF9},
  /* TIM13 GPIO NOT EXPOSED     */ {{0xff,0xff,0xff,0xff}, /* GPIO NOT EXPOSED     */ {0xff,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM14 A03,                 */ {{0x07,0xff,0xff,0xff}, /* GPIO NOT EXPOSED     */ {0xff,0xff,0xff,0xff}, GPIO_AF9},
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_calc_freq_dc_init(struct freqDcTimerInfo_s *freqDcTimerInfo);
static int meadow_calc_freq_dc_isr(int irq, void *context, void *arg);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR is called for all frequency with duty cycle interrupts.
// Note: I've used the terms rising and falling here, instead of leading and
// falling to simplify the concept behind the operation.
//
// On the first rising edge of the input, the timer clears the CNT count and
// CNT begins counting up.
// On the following falling edge, the timer copies the CNT value into CCR2.
// On the next rising edge, the timer copies the CNT value into CCR1 and CNT
// is again cleared to zero and the process repeats.
// This means that we must save the CCR1 and CCR2 timer values between the
// rising edge and the falling edge.
// For 16-bit timers, this is more complex because we must maintain a count
// for each CNT overflow interrupt. This allows us to maintain a 32-bit value.
// For CCR1 overflow for the entire period but for CCR2 only between the
// rising edge and the falling edge.
// Note: a 16-bit register at 96 MHz will overflow every 683 microseconds.
int meadow_calc_freq_dc_isr(int irq, void *context, void *arg)
{
  struct freqDcTimerInfo_s *freqDcTimerInfo = (struct freqDcTimerInfo_s *)arg;
  struct freqDcRtData_s *freqDcRtData =
            (struct freqDcRtData_s *)freqDcTimerInfo->freqDcRtData;

  uint32_t timerBase = freqDcTimerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  if(freqDcRtData == NULL)
  {
    syslog(1, "%s@%d-freqDcRtData is NULL in ISR\n", __FILE__, __LINE__);
    return -1;
  }

  // (--) Diag
  stm32_gpiowrite(DEBUG_PIN_V2_D06, true);

  // The Problem with 16-bit timers.
  // At certain input frequencies the CNT being cleared and the CNT overflow
  // are reported in the same interrupt. However, there is only 1 bit available
  // to indicate both conditions.
  // The highest frequency this occurs at is TimerClock/65536, which is
  // 1,464.844 Hz with a timer clock of 96 MHz. This reoccurs at the intervals
  // (TimerClock/65536)/2, (TimerClock/65536)/3 etc.
  // More details.
  // For most input frequencies, on a leading (e.g. rising) or trailing (e.g.
  // falling) edge an interrupt is generated containing the CC1IF (leading) or
  // CC2IF (trailing) flags set. The UIF flag is always set with the CC1IF
  // flag, to indicate that the CNT register has been cleared. However, at
  // some frequencies, the overflow and the CNT reset occur at the same moment.
  // The UIF is set but it cannot be determined if it indicates CNT overflow or
  // CNT reset.
  switch(timStatusReg & 0x0007)
  {
    case 0x00:    // Nothing happened, just ignore
      break;

    //------------------------------------------------------------
    case 0x01:    // Lone UIF flag. CNT register changed, either reset or overflow)
      timStatusReg &= ~GTIM_SR_UIF;

      // Add to Leading to leading overflow
      freqDcRtData->LeadToLeadOverFlow++;
      break;

    //------------------------------------------------------------
    // Leading Edge
    case 0x02:    // Lone leading edge, never expected
      timStatusReg &= ~GTIM_SR_CC1IF;
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;
      break;

    case 0x03:     // Leading edge + UIF (Normal for End/Start of capture)
      timStatusReg &= ~GTIM_SR_UIF;   // Could be overflow or CNT reset
      timStatusReg &= ~GTIM_SR_CC1IF; // Leading edge should be CNT reset

      if(freqDcTimerInfo->timerWidth == MEADOW_FREQ_DC_TIMER_WIDTH_32)
      {
        // At this point we expect to have seen a trailing edge and no errors.
        // Verify that we are expecting this leading edge
        if(freqDcRtData->activeState == MEADOW_FREQ_DC_FREQ_DC_SYNC_TRAILING)
        {
          // Save new values for user access
          freqDcRtData->countLeadToLead  = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
          freqDcRtData->countLeadToTrail = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);

          // Add current to total timer count for freq average and
          // increment the GPIO input count
          freqDcRtData->totalTimerCount += freqDcRtData->countLeadToLead;
          freqDcRtData->gpioInputCount++;
        }
        else
        {
          freqDcRtData->countLeadToLead = 0;
          freqDcRtData->countLeadToTrail = 0;
        }

        freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING;
        break;
      }

      // Must be 16-bit timer
      uint32_t count1;
      uint32_t count2;

      if(freqDcRtData->activeState == MEADOW_FREQ_DC_FREQ_DC_SYNC_TRAILING)
      {
        // Read current 16-bit values
        count1 = getreg16(timerBase + STM32_GTIM_CCR1_OFFSET);
        count2 = getreg16(timerBase + STM32_GTIM_CCR2_OFFSET);

        // Add each 16-bit CNT overflow to counts
        count1 += (freqDcRtData->LeadToLeadOverFlow * MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT);
        count2 += (freqDcRtData->LeadToTrailOverFlow * MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT);

        // Check for various detectable errors. There are some that cannot
        // be detected.

        // Since there's a limit to the highest frequency we can detect, we
        // need to check if we've gone beyond a reasonable frequency.
        if(count1 < MEADOW_FREQ_DC_MINIMUM_USABLE_CNT)
        {
          count1 = 0;
          count2 = 0;
        }
        else if(count1 < count2)
        {
          // This works in many cases, one is the initial frequency
          // that causes trouble (i.e. TimerClock/65536). However, it may be
          // that multiple overflow interrupts have been missed.
          count1 += MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT;
        }
        else
        {
          // Reasonable Duty Cycle test, must be > 1.0% and < 99.0%
          uint32_t dutyCycle = (count2 * 1000)/count1;
          if(dutyCycle < 10 || dutyCycle > 990)
          {
            count1 = 0;
            count2 = 0;
          }
        }
      }
      else
      {
        count1 = 0;
        count2 = 0;
      }

      // Provide consumer with values
      freqDcRtData->countLeadToLead = count1;
      freqDcRtData->countLeadToTrail = count2;

      // Add current to total timer count for freq average
      // and maintain the input count.
      freqDcRtData->totalTimerCount += count1;
      freqDcRtData->gpioInputCount++;
      
      // Clear previous overflow
      freqDcRtData->LeadToLeadOverFlow = 0;
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING;
      break;

    // Trailing/Falling Edge
    case 0x05:    // Falling edge with UIF (i.e. assume Falling Edge + Overflow)
      timStatusReg &= ~GTIM_SR_UIF;

    case 0x04:    // Falling edge alone. End of CCR2 capture.
      timStatusReg &= ~GTIM_SR_CC2IF;

      // Falling edge check if there has been a valid leading edge
      if(freqDcRtData->activeState != MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING)
      {
        freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;
        break;  // And quit
      }
      
      // Falling edge means we're done with CCR2's value. We don't need
      // to worry about CNT overflow with respect to CCR2 either.
      if(timStatusReg & GTIM_SR_UIF)
        freqDcRtData->LeadToLeadOverFlow++;    // Adjust overflow count

      // Time to capture the first half overflow
      freqDcRtData->LeadToTrailOverFlow = freqDcRtData->LeadToLeadOverFlow;

      // This value will be tested when the leading edge arrives
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_TRAILING;
      break;

    //------------------------------------------------------------
    case 0x06:    // (illegal) Rising and Falling together, no way
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;
      break;

    case 0x07:    // (illegal) Rising and Falling plus UIF
      // This case exists when the duty cycle is very small (< 0.5%) or very
      // large (> 99.5%)
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timStatusReg &= ~GTIM_SR_UIF;
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;
      break;

    // There are only 3 bits to check, so this is a not needed. We've checked
    // all possible combinations
    default:
      break;
  }
  // (--) Diag
  stm32_gpiowrite(DEBUG_PIN_V2_D06, false);

  // Clear status register as needed
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  return OK;
}

//=============================================================
static uint32_t meadow_calc_freq_dc_get_apb_clock(
          struct freqDcTimerInfo_s *freqDcTimerInfo)
{
  if(freqDcTimerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
// Uses the bit-field to determine the Timer clock
static uint32_t meadow_calc_freq_dc_get_max_clock(
          const struct freqDcTimerInfo_s *freqDcTimerInfo)
{
  if(freqDcTimerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
static void meadow_calc_freq_dc_disable(const uint32_t timerBase)
{
  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
}

//=============================================================
static void meadow_calc_freq_dc_enable(const uint32_t timerBase)
{
  // Enable timer Counter
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  
  // Re-initialize the counter and generates an update of the registers
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);
  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
}

//=====================================================================
// The following function finds the specified timer's built-in information.
// It should only return NULL if the timerNumb is not valid.
struct freqDcTimerInfo_s *meadow_calc_freq_dc_get_timer_info_pointer(const int timerNumb)
{
  for (int offset = 0; offset < MEADOW_FREQ_DC_TOTAL_TIMERS_AVAILABLE; offset++)
  {
    // Find the offset for the timer number
    if(freqTimerInfoArray[offset].timerNumb == timerNumb)
    {
      // Found the offset, return a pointer to the entry
      return ( &(freqTimerInfoArray[offset]));
    }
  }

  return NULL;
}

//=====================================================================
// From the timer number find the correct GPIO table entry
static struct timerGpioInfo_s *meadow_calc_freq_dc_get_timer_gpio_pointer(const int timerNumb)
{
  // Look through all the times and find the matching one 
  for (int offset = 0; offset < MEADOW_FREQ_DC_TOTAL_TIMERS_AVAILABLE; offset++)
  {
    if(freqTimerInfoArray[offset].timerNumb == timerNumb)
    {
      // Found the offset, return a pointer from other array
      return ( &(timerGpioInfoArray[offset]));
    }
  }

  return NULL;
}

//=====================================================================
// This function looks up the correct alternate function for the input
// GPIO on a specific timer. This is limited to inputs that can be used.
static uint32_t meadow_calc_freq_dc_get_af_for_input(const int timerNumb)
{
  // Get the pointer to the GPIO information specified by the timerNumb.
  struct timerGpioInfo_s *timerGpioInfo =
            meadow_calc_freq_dc_get_timer_gpio_pointer(timerNumb);
  
  // The alternate function is Meadow version and input independent
  return timerGpioInfo->timerAltFunc;
}

//=============================================================
// Using the Meadow device version, lookup the GPIO Port and Pin combination.
// Returns channel, either 1-4 or MEADOW_FREQ_DC_BAD_GPIO_VALUE.
static uint32_t meadow_calc_freq_dc_get_ver_based_gpio_chan(const int timerNumb,
          uint8_t portAndPin)
{
  uint32_t chan;

  // Get the pointer to the structure containing all the GPIO information.
  struct timerGpioInfo_s *timerGpioInfo =
            meadow_calc_freq_dc_get_timer_gpio_pointer(timerNumb);

  // Depending on the device version find specified GPIO
  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    for(chan = 0; chan < MEADOW_FREQ_DC_MAX_TIMER_CHANNELS; chan++)
    {
      if(timerGpioInfo->timerF7v1Gpio[chan] == portAndPin)
        return chan + 1;
    }
    return MEADOW_FREQ_DC_BAD_GPIO_VALUE;    // Not found
  }
  else if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2 ||
          meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    for(chan = 0; chan < MEADOW_FREQ_DC_MAX_TIMER_CHANNELS; chan++)
    {
      if(timerGpioInfo->timerF7v2Gpio[chan] == portAndPin)
        return chan + 1;
    }
    return MEADOW_FREQ_DC_BAD_GPIO_VALUE;    // Not found
  }
  else
  {
    return MEADOW_FREQ_DC_BAD_GPIO_VALUE;   // Unknown version
  }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called by Meadow.Core to configure
// Timer numbers range from 1 - 14. However, some are not defined.
int meadow_calc_freq_dc_freq_duty_config(const int timerNumber,
          const uint8_t portAndPin, const uint8_t gpioPolarity)
{
  int ret;
  uint32_t inputGpioConfig;
  static bool firstTime = true;

  if(firstTime)
  {
    firstTime = false;

    // (--) Diag config LED
    stm32_configgpio(DEBUG_PIN_V2_D06);
    // (--) Diag LED
  }

  // Check if there's already an object in this slot. The
  struct freqDcTimerInfo_s *freqDcTimerInfo =
            meadow_calc_freq_dc_get_timer_info_pointer(timerNumber);
  if(freqDcTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Timer %ld not defined\n",
              __FILE__, __LINE__, timerNumber);
    return -ERROR;
  }

  if(freqDcTimerInfo->freqDcRtData != NULL)
  {
    syslog(LOG_ERR, "%s@%d-Already timer defined in slot.\n", __FILE__, __LINE__);
    return -ERROR;
  }

  // Get the timer dependent alternate function
  uint32_t altFunction = meadow_calc_freq_dc_get_af_for_input(timerNumber);
  if(altFunction == MEADOW_FREQ_DC_BAD_GPIO_VALUE)
  {
    syslog(LOG_ERR, "%s@%d-Timer%d has no alternate function defined\n", __FILE__, __LINE__);
    return -ENOTSUP;   // Not supported
  }

  // Insure a correct timer / pin+port combination was supplied.
  // Timers have, at most, 1-4 channels, each representing 1 GPIOs. For the
  // specified timer we need to verify a proper port and pin. The channelFound
  // value isn't used, it's only used as a test.
  uint32_t channelFound =
            meadow_calc_freq_dc_get_ver_based_gpio_chan(timerNumber,
            portAndPin);
  if(channelFound == MEADOW_FREQ_DC_BAD_GPIO_VALUE)
  {
    syslog(2, "The requested GPIO and Timer combination are not supported\n");
    return -ENOTSUP;
  }

  // This is enough to build the input configuration for the a GPIO
  inputGpioConfig = MEADOW_TIMER_GPIO_CONST | portAndPin | altFunction;

  // Diagnostic
  // syslog(1, "%s@%d-input Pin defn:0x%02x (P%c%d), Pin defn + AF:0x%08lx\n",
  //           __FILE__, __LINE__, portAndPin,
  //           ((portAndPin) >> 4) + 'A', portAndPin & 0x0f, inputGpioConfig);
  // Diagnostic

  // Valid GPIO so configure input point for timer.
  ret = stm32_configgpio(inputGpioConfig);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:stm32_configgpio() returned:%ld\n",
              __FILE__, __LINE__, ret);
    return -ENOTSUP;   // Not supported
  }

  // Allocate a new struct for each new timer to contain runtime data.
  freqDcTimerInfo->freqDcRtData = zalloc(sizeof(struct freqDcRtData_s));
  if(freqDcTimerInfo->freqDcRtData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Allocation for freqDcRtData_s NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // Initialized the timer hardware
  ret = meadow_calc_freq_dc_init(freqDcTimerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow freq+dc init failed:%d\n", __FILE__, __LINE__, ret);
    free(freqDcTimerInfo->freqDcRtData);
    return ret;
  }

  // With a place to put the information, we can start filling the structure.
  freqDcTimerInfo->freqDcRtData->activeState     = MEADOW_FREQ_DC_FREQ_DC_SYNC_UNKNOWN;
  freqDcTimerInfo->freqDcRtData->inputPolarity   = gpioPolarity;
  freqDcTimerInfo->freqDcRtData->inputConfig     = inputGpioConfig;
  freqDcTimerInfo->freqDcRtData->gpioInputCount  = 0;
  freqDcTimerInfo->freqDcRtData->totalTimerCount = 0;

  return OK;
}

//=============================================================
// Frequency and duty cycle measurement.
int meadow_calc_freq_dc_init(struct freqDcTimerInfo_s *freqDcTimerInfo)
{
  // See RM0410 Reference manual for STM32F76xxx and STM32F77xxx section 26.3.6
  // for original concept.

  // A single input (T1) is used. It is configured as input to Compare/Capture
  // registers 1 and 2. For CCR1 it is configured to for leading edge interrupt
  // and trailing edge for CCR2. By using the count between interrupts for one
  // CCR's (i.e. leading to leading edges) the frequency can be found by
  // counting between interrupts from leading to trailing the duty cycle can be
  // found.

  int ret;
  uint16_t regVal16;
  uint32_t regVal32;
  uint32_t timerBase;

  timerBase = freqDcTimerInfo->timerBase;

  // Before starting disable capture/control for input 1 and 2 by setting CC1E
  // and CC2E to 0. Ref Man (26.4.7 at end) "Note: CC1S bits are writable only
  // when the channel is OFF (CC1E = 0 in TIMx_CCER)."
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xffcc;   // c = 1110, clear CC1E and CC2E bits 0 & 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // 1. Select the active input for TIMx_CCR1: write the CC1S bits to 01 in
  // the TIMx_CCMR1 register (TI1 selected).
  // Ref Man "01: CC1 channel is configured as input, IC1 is mapped on TI1."
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffffc;   // c = 1100, clear CC1S bits 1:0
  regVal32 |= 0x00000001;   // 1 = 0001, set '01'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // 2. Select the active polarity for TI1FP1 (used both for capture in
  // TIMx_CCR1 and counter clear): write the CC1P to ‘0’ and the CC1NP bit to
  // ‘0’ (active on rising edge).
  // Note: the CCR1 register is readonly so the above configuration
  // instructions are wrong.
  // Capture/Compare Enable Register (CCER) is were the polarity is set by
  // CC1P & CC1NP. In Ref Man the CC1P for input describes both the CC1P and
  // CC1NP bit as if a 2 bit field. But they are actually bit 1 and bit 3.
  // Ref Man "00: non-inverted/rising edge, 01: inverted/falling edge
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xfff5;   // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)

  if(freqDcTimerInfo->freqDcRtData->inputPolarity) // 0 = leading is rising, 1 = leading is falling
    regVal16 |= 0x0002;         // Set bit 1 to change 00 to 01 (inverted/falling)

  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // 3. Select the active input for TIMx_CCR2: write the CC2S bits to 10 in the TIMx_CCMR1
  // register (TI1 selected).
  // Ref Man "10: CC2 channel is configured as input, IC2 is mapped on TI1"
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffcff;   // c = 1100, clear CC2S bits 9:8
  regVal32 |= 0x00000200;   // 2 = 0010, set to '10'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // 4. Select the active polarity for TI1FP2 (used for capture in TIMx_CCR2): write the CC2P
  // bit to ‘1’ and the CC2NP bit to ’0’ (active on falling edge).
  // Ref Man "01: inverted/falling edge
  //  Circuit is sensitive to TIxFP1 falling edge (capture, trigger in reset,
  //  external clock or trigger mode), TIxFP1 is inverted (trigger in gated
  //  mode, encoder mode)."
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xff5f;   // 5 = 0101, clear GTIM_CCER_CC2NP (bit 7) &
  //                                       GTIM_CCER_CC2P (bit 5).

  // CC2P & CC2NP must be opposite of CC1P & CC1NP
  // (--) SOMETHING SEEMS WRONG. ARE WE JUST CHECKING FOR '0'?
  // 0 = leading is rising, 1 = leading is falling
  if(!freqDcTimerInfo->freqDcRtData->inputPolarity)
    regVal16 |= 0x0020;           // 2 = 0010 set bit 5 and leave bit 7 clear

  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // 5. Select the valid trigger input: write the TS bits to 101 in the TIMx_SMCR
  // register (TI1FP1 selected). Ref Man "101: Filtered Timer Input 1 (TI1FP1)"
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= 0xffffff8f;   // 8 = 1000, clear TS bits 6:4
  regVal32 |= 0x00000050;   // 5 = 0101 sets '101'
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??

  // 6. Configure the slave mode controller in reset mode: write the SMS bits
  // to 100 in the TIMx_SMCR register. Reg Man "0100: Reset Mode - Rising edge
  // of the selected trigger input (TRGI) reinitializes the counter and
  // generates an update of the registers."
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??
  regVal32 &= 0xfffefff8;    // e = 1110, Clear SMS bit 16, 8 = 1000, clear 2:0
  regVal32 |= 0x00000004;    // 4 = 0100 sets 2:0 = '100', leave bit 16 = 0
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // (--) SOMETHING SEEMS WRONG. IS IT CC1E AND CC2E OR CC1E AND CC1P?
  // CODE IS CCIP COMMENTS ARE CC2E.
  // 7. Enable the captures: write the CC1E (bit 0) and CC2E (bit 4) bits to
  // ‘1' in the TIMx_CCER register.
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 |= 0x0011;   // set bit 0 and bit 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // To enable the timer we needed to know which clock enable register to use.
  uint32_t apbClock = meadow_calc_freq_dc_get_apb_clock(freqDcTimerInfo);

  // And we need to know which bit to set in the register, which is in th
  // array. 
  modifyreg32(apbClock, 0, freqDcTimerInfo->timerClkEn);

  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // (--) NEW FEATURE IMPLEMENTED HERE ALLOW USER TO SELECT FREQUENCY VIA
  // SLOW, MEDIUM AND FAST SELECTION?
  // Find proper pre-scaler value so all timers run at the same speed, no
  // matter which clock line they are connected to.
  uint16_t prescaler = (meadow_calc_freq_dc_get_max_clock(freqDcTimerInfo)/ \
            MEADOW_FREQ_DC_CLOCK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum allowed for the timer. Either
  // 32-bit or 16-bit ARR register.
  uint32_t maxARRValue = freqDcTimerInfo->timerWidth ==
            MEADOW_FREQ_DC_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // Clear all interrupt sources and set the ones we need. CC1IE is the rising
  // edge, CC2IE is the falling edge and UIE is whenever the CNT register is
  // cleared or overflows, DMA/Interrupt enable register (DIER)
  // Advanced timers 1 & 8 add ATIM_DIER_COMIE | ATIM_DIER_BIE | ATIM_DIER_COMDE
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
        GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
        GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
        GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
        GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_UIE);  

  // All Frequency/Duty Cycle interrupts are handled by same isr
  ret = irq_attach(freqDcTimerInfo->timerIrqVec,
            meadow_calc_freq_dc_isr,  // ISR address
            freqDcTimerInfo);         // Argument to ISR
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Get things ready
  freqDcTimerInfo->freqDcRtData->activeState =
            MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;

  // Enable IRQ
  up_enable_irq(freqDcTimerInfo->timerIrqVec);

  // Enable timer
  meadow_calc_freq_dc_enable(timerBase);
  return OK;
}

//=============================================================
// Called to unconfigure a timer
int meadow_calc_freq_dc_freq_duty_unconfig(const uint32_t timerNumber)
{
  struct freqDcTimerInfo_s *freqDcTimerInfo =
            meadow_calc_freq_dc_get_timer_info_pointer(timerNumber);

  // Is this slot being used?
  if(freqDcTimerInfo->freqDcRtData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Can't unconfigure, this timer %lu not configured.\n",
              __FILE__, __LINE__, timerNumber);
    return -EBADSLT;    // Invalid slot
  }

  // Stop interrupts
  up_disable_irq(freqDcTimerInfo->timerIrqVec);

  // Stop timer
  meadow_calc_freq_dc_disable(freqDcTimerInfo->timerBase);

  // Unconfigure GPIO
  stm32_unconfiggpio(freqDcTimerInfo->freqDcRtData->inputConfig);

  // Free runtime memory
  free(freqDcTimerInfo->freqDcRtData);
  freqDcTimerInfo->freqDcRtData = NULL;
  return OK;
}

//================================================================
// Return Frequency and Duty Cycle information to caller.
int meadow_calc_freq_dc_return_freq_info(struct freqDcReturnData_s *returnData)
{
  double freq;
  double averageFreq;
  double dutyCycle;

  // Just feed pulse train into appropriate GPIO
  struct freqDcTimerInfo_s *freqDcTimerInfo =
            meadow_calc_freq_dc_get_timer_info_pointer(returnData->timerNumber);
  struct freqDcRtData_s *freqDcRtData =
            (struct freqDcRtData_s *)freqDcTimerInfo->freqDcRtData;

  uint32_t validCheckCount = 0;

  // These are so once a valid value is found, a change in the timers data
  // structure won't affect the output.
  uint32_t fullCycle;
  uint32_t halfCycle;
  uint32_t inputCnt;
  uint32_t totalCnt;

  // Find valid data where both full cycle and the half cycle values are
  // available. This is only an issue at higher frequencies.
  for(validCheckCount = 0; validCheckCount < 5; validCheckCount++)
  {
    // Get all the values at one time
    // This would be nice if it was atomic but its not....
    fullCycle = freqDcRtData->countLeadToLead;
    halfCycle = freqDcRtData->countLeadToTrail;
    inputCnt = freqDcRtData->gpioInputCount;
    totalCnt = freqDcRtData->totalTimerCount;

    if(fullCycle > 0 && halfCycle > 0)
      break;

    usleep(1 * 1000);   // delay - wait for valid data
  }

  if(fullCycle > 0 && halfCycle > 0)
  {
    // Do floating point math and convert to integer times 1000. Duty Cycle
    // is the ratio of the full cycle count and the cycle count before the
    // trailing edge was detected.
    dutyCycle = (double)(halfCycle * 100.0) / (double)fullCycle;

    // The frequency is the timer's counting frequency divided by the
    // cycle count.
    freq = (double)(MEADOW_FREQ_DC_CLOCK_FREQ)/ \
              (double)fullCycle;

    // Average frequency since last read
    averageFreq = (double)(MEADOW_FREQ_DC_CLOCK_FREQ) / \
              (double)(totalCnt / inputCnt);

    syslog(2, "Freq:%06.2fHz, DC:%02.2f%%, Count:%lu, AvgFreq:%06.2f retries:%lu\n",
              freq, dutyCycle, inputCnt,
              averageFreq, validCheckCount);

    // These are returned to managed code
    returnData->frequencyX1000 = (freq * 1000.0);
    returnData->dutyCycleX1000 = (dutyCycle * 1000.0);
    returnData->avgFreqX1000   = (averageFreq * 1000);
    returnData->gpioInputCount = inputCnt;
  }
  else
  {
    // These are returned to managed code
    returnData->frequencyX1000 = 0;
    returnData->dutyCycleX1000 = 0;
    returnData->avgFreqX1000   = 0;
    returnData->gpioInputCount = 0;

    syslog(2, "Invalid data CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
              fullCycle, halfCycle, validCheckCount);
  }

  // Prevent counts from being used in the future.
  freqDcRtData->countLeadToLead   = 0;
  freqDcRtData->countLeadToTrail  = 0;
  freqDcRtData->gpioInputCount    = 0;
  freqDcRtData->totalTimerCount   = 0;

  return OK;
}

#endif    // #if defined(MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD)
