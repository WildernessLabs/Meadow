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
// x1. Add a running average feature. It would be the average since the last
//  reading.
// x2. Add count of the input GPIO trailing edges since last reading.
// x3. Add CCM support. This requires changes to the configuration and adding,
//  modifying or replacing existing tables to support more or all Timers
//  and their associated GPIOs.
// 4. Add multi-channel support. Allow all timer channels to be used for input.
// 5. Support Tim1 and Tim8? These have more complex IRQ requirements.
// 6. For 16-bit timers, allow with configuration to include SLOW, MED and
//  FAST options to reduce the effects of the 65,536 count rollover.
// 7. Add syscalls as needed (probably 2 maybe 3)
// 8. Test unconfigure code (need syscall?)
// 9. Clean up code, remove unneeded header includes and retest code

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
// defines which timers can be used and invariant characteristics. Some
// values have be reduced to a bit-field.
// NOTE:For now TIM1 and TIM8 are not supported
static struct freqDcTimerInfo_s freqTimerInfoArray[] = 
{
            //   |--- bit-field---|
            //   #  wid max apb ok    Base Addr       Clk Timer Enable      IRQ Vector    Alt Func  Ptr
  /* TIM1   */  {1 , 0,  1,  0, 0, STM32_TIM1_BASE,  0,                   0,               GPIO_AF1, NULL},
  /* TIM2   */  {2 , 1,  0,  0, 1, STM32_TIM2_BASE,  RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2,  GPIO_AF1, NULL},
  /* TIM3   */  {3 , 0,  0,  0, 1, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3,  GPIO_AF2, NULL},
  /* TIM4   */  {4 , 0,  0,  0, 1, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4,  GPIO_AF2, NULL},
  /* TIM5   */  {5 , 1,  0,  0, 1, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5,  GPIO_AF2, NULL},
  /* TIM8   */  {8 , 0,  1,  0, 0, STM32_TIM8_BASE,  0,                   0,               GPIO_AF3, NULL},
  /* TIM9   */  {9 , 0,  1,  1, 1, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9,  GPIO_AF3, NULL},
  /* TIM10  */  {10, 0,  1,  1, 1, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, GPIO_AF3, NULL},
  /* TIM11  */  {11, 0,  1,  1, 1, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, GPIO_AF3, NULL},
  /* TIM12  */  {12, 0,  0,  0, 1, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, GPIO_AF9, NULL},
  /* TIM13  */  {13, 0,  0,  0, 1, STM32_TIM13_BASE, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13, GPIO_AF3, NULL},
  /* TIM14  */  {14, 0,  0,  0, 1, STM32_TIM14_BASE, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14, GPIO_AF9, NULL},
};

#define MEADOW_FREQ_DC_TOTAL_TIMERS_AVAILABLE (sizeof(freqTimerInfoArray)/sizeof(struct freqDcTimerInfo_s))

//----------------------------------------------------------------------------
// This table contains all of the STM32F7's timers and their valid GPIOs with
// the currect channel. It is used for F7v1 and F7v2 and CCM. It should be
// useable for any STM32F7.
chanPortPin_t validStm32F7GpioArray[][11] = 
{
// Tim
//  1   PA8         PE9        PA9        PE11
      {{0x08, 1}, {0x49, 1}, {0x09, 2}, {0x4b, 2},
//      PA10,      PE13,      PA11,      PE14
       {0x0a, 3}, {0x4d, 3}, {0x0b, 4}, {0x4e, 4}, {0xff, 0}},
//  2    PA0,      PA15,       PA1,       PB3,
      {{0x00, 1}, {0x0f, 1}, {0x01, 2}, {0x13, 2},
//       PA2,      PB10,       PA3,      PB11
       {0x02, 3}, {0x1a, 3}, {0x03, 4}, {0x1b, 4}, {0xff, 0}},
//  3    PA6        PC6        PB4,       PA7
      {{0x06, 1}, {0x26, 1}, {0x14, 1}, {0x07, 2},
//       PC7,       PB5,       PB0        PC8,
       {0x27, 2}, {0x15, 2}, {0x10, 3}, {0x28, 3},
//       PB1        PC9
       {0x11, 4}, {0x29, 4}, {0xff, 0}},
//  4    PD12       PB6        PD13       PB7
      {{0x3c, 1}, {0x16, 1}, {0x3d, 2}, {0x17, 2},
//       PD14,      PB8        PD15       PB9
       {0x3e, 3}, {0x18, 3}, {0x3f, 4}, {0x19, 4}, {0xff, 0}},
//  5    PA0        PH10       PA1        PH11
      {{0x00, 1}, {0x7a, 1}, {0x01, 2}, {0x7b, 2},
//       PA2        PH12,      PA3,       PI0},
       {0x02, 3}, {0x7c, 3}, {0x03, 4}, {0x80, 4}, {0xff, 0}},
//  6  No GPIO
      {{0xff, 0}},
//  7  No GPIO
      {{0xff, 0}},
//  8    PC6        PI5        PC7        PI6
      {{0x26, 1}, {0x85, 1}, {0x27, 2}, {0x86, 2},
//       PC8        PI7        PC9        PI2
       {0x28, 3}, {0x87, 3}, {0x29, 4}, {0x82, 4}, {0xff, 0}},
//  9    PE5        PA2        PE6        PA3
      {{0x45, 1}, {0x02, 1}, {0x46, 2}, {0x03, 2}, {0xff, 0}},
// 10    PF6        PB8
      {{0x56, 1}, {0x18, 1}, {0xff, 0}},
// 11    PF7        PB9
      {{0x57, 1}, {0x19, 1}, {0xff, 0}},
// 12    PH6        PB14       PH9        PB15
      {{0x76, 1}, {0x1e, 1}, {0x79, 2}, {0x1f, 2}, {0xff, 0}},
// 13    PF8        PA6
      {{0x58, 1}, {0x06, 1}, {0xff, 0}},
// 14    PF9        PA7
      {{0x59, 1}, {0x07, 1}, {0xff, 0}},
};

// Used to verify pin and port of F7v1
static uint8_t validGpioF7v1Array[][5] =
{
  // F7v1
  /* TIM1  GPIO NOT EXPOSED     */ {0xff},
  /* TIM2  GPIO NOT EXPOSED     */ {0xff},
  /* TIM3  D02, D05, D06, D09   */ {0x26,0x27,0x10,0x11,0xff},
  /* TIM4  D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19,0xff},
  /* TIM5  D10,                 */ {0x7a,0xff},
  /* TIM6 GPIO NOT EXPOSED      */ {0xff},
  /* TIM7 GPIO NOT EXPOSED      */ {0xff},
  /* TIM8 GPIO NOT EXPOSED      */ {0xff,0xff},
  /* TIM9  A02,                 */ {0x03,0xff},
  /* TIM10 D03*,                */ {0x18,0xff},
  /* TIM11 D04*,                */ {0x19,0xff},
  /* TIM12 D12, D13             */ {0x1e,0x1e,0xff},
  /* TIM13 GPIO NOT EXPOSED     */ {0xff},
  /* TIM14 A03,                 */ {0x07,0xff}
};

// Used to verify pin and port of F7v2
static uint8_t validGpioF7v2Array[][5] =
{
  // F7v2
  /* TIM1  GPIO NOT EXPOSED     */ {0xff},
  /* TIM2  GPIO NOT EXPOSED     */ {0xff},
  /* TIM3  D05, D10, A03, A04   */ {0x14,0x27,0x10,0x11,0xff},
  /* TIM4  D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19,0xff},
  /* TIM5  D02,            A02  */ {0x7a,0x03,0xff},
  /* TIM6  GPIO NOT EXPOSED     */ {0xff},
  /* TIM7  GPIO NOT EXPOSED     */ {0xff},
  /* TIM8  GPIO NOT EXPOSED     */ {0xff},
  /* TIM9  GPIO NOT EXPOSED     */ {0xff},
  /* TIM10 D03*                 */ {0x18,0xff},
  /* TIM11 D04*                 */ {0x19,0xff},
  /* TIM12 D12, D13             */ {0x1e,0x1f,0xff},
  /* TIM13 GPIO NOT EXPOSED     */ {0xff},
  /* TIM14 GPIO NOT EXPOSED     */ {0xff}
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_calc_freq_dc_cfg_timer(struct freqDcTimerInfo_s *freqDcTimerInfo);
static int meadow_calc_freq_dc_isr(int irq, void *context, void *arg);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR is called for all frequency with duty cycle interrupts.
//
// On the first leading edge of the input, the timer clears the CNT count and
// CNT begins counting up.
// On the following trailing edge, the timer copies the CNT value into CCR2.
// On the next leading edge, the timer copies the CNT value into CCR1 and CNT
// is again cleared to zero and the process repeats.
// This means that we must save the CCR1 and CCR2 timer values between the
// leading edge and the trailing edge.
// For 16-bit timers, this is more complex because we must maintain a count
// for each CNT overflow interrupt. This allows us to maintain a 32-bit value.
// For CCR1 overflow for the entire period but for CCR2 only between the
// leading edge and the trailing edge.

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
    return ERROR;  // -1
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
      freqDcRtData->leadToLeadOverflow++;
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
        // At this point we've seen a trailing edge and no errors.
        // Verify that we are expecting this leading edge
        if(freqDcRtData->activeState == MEADOW_FREQ_DC_FREQ_DC_SYNC_TRAILING)
        {
          // A complete cycle has been seen. Save new values for user access
          freqDcRtData->countLeadToLead  = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
          freqDcRtData->countLeadToTrail = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);

          // Add current to total timer count for frequency average and
          // increment the GPIO input count.
          freqDcRtData->countTimerTotal += freqDcRtData->countLeadToLead;
          freqDcRtData->countInputTotal++;
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
        count1 += (freqDcRtData->leadToLeadOverflow * MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT);
        count2 += (freqDcRtData->leadToTrailOverflow * MEADOW_FREQ_DC_16_BIT_OVERFLOW_COUNT);

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

      // Add current to total timer count for frequency average
      // and maintain the input count.
      freqDcRtData->countTimerTotal += count1;
      freqDcRtData->countInputTotal++;
      
      // Clear previous overflow
      freqDcRtData->leadToLeadOverflow = 0;
      freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING;
      break;

    // Trailing Edge
    case 0x05:    // Trailing edge with UIF (i.e. Trailing Edge + Overflow)
      timStatusReg &= ~GTIM_SR_UIF;

    case 0x04:    // Trailing edge alone. End of CCR2 capture.
      timStatusReg &= ~GTIM_SR_CC2IF;

      // Trailing edge check if there has been a valid leading edge
      if(freqDcRtData->activeState != MEADOW_FREQ_DC_FREQ_DC_SYNC_LEADING)
      {
        freqDcRtData->activeState = MEADOW_FREQ_DC_FREQ_DC_SYNC_ERROR;
        break;  // And quit
      }
      
      // Trailing edge means we're done with CCR2's value. We don't need
      // to worry about CNT overflow with respect to CCR2 either.
      if(timStatusReg & GTIM_SR_UIF)
        freqDcRtData->leadToLeadOverflow++;    // Adjust overflow count

      // Time to capture the first half overflow
      freqDcRtData->leadToTrailOverflow = freqDcRtData->leadToLeadOverflow;

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

  // Clear status register
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

  // (--) Diag
  stm32_gpiowrite(DEBUG_PIN_V2_D06, false);
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
// The following finds the specified timer's built-in timer information.
struct freqDcTimerInfo_s *meadow_calc_freq_dc_get_timer_info(const int timerNumb)
{
  struct freqDcTimerInfo_s *timerInfo = &(freqTimerInfoArray[timerNumb - 1]);

  if(timerInfo->timerOkay)
    return timerInfo;

  return NULL;
}

//=============================================================
// This function will evaluate the GPIO based on 3 tables that contain the
// legal GPIOs for the CCM (all F7 GPIOs checked) and for F7v1 and F7v2.
// It returns the channel, 1-4 unless not found, then returns 0.
static uint8_t meadow_calc_freq_dc_from_pin_port_get_chan(const int timerNumb,
          uint8_t portAndPin)
{
  int entry;
  int timerOffset = timerNumb - 1;

  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    // Verify pin & port are valid
    entry = 0;
    while(validGpioF7v1Array[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validGpioF7v1Array[timerOffset][entry])
      {
        syslog(1, "%s@%d- V1 so far good\n", __FILE__, __LINE__);
      }
      entry++;
    }
    if(validGpioF7v1Array[timerOffset][entry] == 0xff)
     return 0;
  }
  else if (meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2)
  {
    // Verify pin & port are valid
    entry = 0;
    while(validGpioF7v2Array[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validGpioF7v2Array[timerOffset][entry])
      {
        break;    // So far good
      }
      entry++;
    }
    
    if(validGpioF7v2Array[timerOffset][entry] == 0xff)
      return 0;
  }
  else if (meadow_hw_version_get() != MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    // Unsupported device type
    syslog(1, "%s@%d- Unknown device type\n", __FILE__, __LINE__);
    return 0;
  }

  // All types are verified here too and pickup channel from this table
  entry = 0;
  while(validStm32F7GpioArray[timerOffset][entry].portPin != 0xff)
  {
    if(portAndPin == validStm32F7GpioArray[timerOffset][entry].portPin)
    {
      return validStm32F7GpioArray[timerOffset][entry].chan;
    }
    entry++;
  }

  return 0;
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

  // Insure a correct timer / pin+port combination was supplied.
  // Timers have, at most, 1-4 channels, each representing 1 GPIO. For the
  // specified timer we need to verify a proper port and pin.
  uint8_t timerChan = meadow_calc_freq_dc_from_pin_port_get_chan(timerNumber,
            portAndPin);
  if(timerChan == 0)
  {
    syslog(2, "%s@%d-The GPIO/Timer combination not valid\n",
              __FILE__, __LINE__);
    return -ENOTSUP;
  }

  // Check if there's already an object in this slot.
  struct freqDcTimerInfo_s *freqDcTimerInfo =
            meadow_calc_freq_dc_get_timer_info(timerNumber);
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

  // Enough to build the input configuration for the a GPIO
  inputGpioConfig = MEADOW_TIMER_GPIO_CONST | portAndPin | \
            freqDcTimerInfo->timerAltFunc;

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
  ret = meadow_calc_freq_dc_cfg_timer(freqDcTimerInfo, true);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow frequency+dc init failed:%d\n", __FILE__, __LINE__, ret);
    free(freqDcTimerInfo->freqDcRtData);
    return ret;
  }

  // With a place to put the information, we can start filling the structure.
  freqDcTimerInfo->freqDcRtData->activeState     = MEADOW_FREQ_DC_FREQ_DC_SYNC_UNKNOWN;
  freqDcTimerInfo->freqDcRtData->inputPolarity   = gpioPolarity;
  freqDcTimerInfo->freqDcRtData->inputConfig     = inputGpioConfig;
  freqDcTimerInfo->freqDcRtData->inputTimerChan  = timerChan;
  freqDcTimerInfo->freqDcRtData->countInputTotal = 0;
  freqDcTimerInfo->freqDcRtData->countTimerTotal = 0;

  return OK;
}

//=============================================================
// Frequency and duty cycle configuration of timer registers.
// It will configure and unconfigure 
int meadow_calc_freq_dc_cfg_timer(struct freqDcTimerInfo_s *freqDcTimerInfo,
          bool configure)
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

  //(++) Only disables channels 1 & 2. Disabling all 4 is probably okay as
  // as at the end of configuration all needed channels are correct.
  // Before starting disable capture/control for input 1 and 2 by setting CC1E
  // and CC2E to 0. Ref Man (26.4.7 at end) "Note: CC1S bits are writable only
  // when the channel is OFF (CC1E = 0 in TIMx_CCER)."
  //(++) Need to modify different bits (not 0xffcc) for channel 3 & 4
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xffcc;   // c = 1110, clear CC1E and CC2E bits 0 & 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  //(++) The following only configure inputs 1 & 2
  // 1. Select the active input for TIMx_CCR1: write the CC1S bits to 01 in
  // the TIMx_CCMR1 register (TI1 selected).
  // Ref Man "01: CC1 channel is configured as input, IC1 is mapped on TI1."
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffffc;   // c = 1100, clear CC1S bits 1:0
  regVal32 |= 0x00000001;   // 1 = 0001, set '01'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  //(++) only bits for input 1 are considered
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
  
  //(++) Only addresses channels 1 & 2
  // 3. Select the active input for TIMx_CCR2: write the CC2S bits to 10 in the TIMx_CCMR1
  // register (TI1 selected).
  // Ref Man "10: CC2 channel is configured as input, IC2 is mapped on TI1"
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffcff;   // c = 1100, clear CC2S bits 9:8
  regVal32 |= 0x00000200;   // 2 = 0010, set to '10'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // (--) Already modified but only considers channels. There are other bits for 3 & 4
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

  // (--) Only handles some channels not all
  // 5. Select the valid trigger input: write the TS bits to 101 in the TIMx_SMCR
  // register (TI1FP1 selected). Ref Man "101: Filtered Timer Input 1 (TI1FP1)"
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= 0xffffff8f;   // 8 = 1000, clear TS bits 6:4
  regVal32 |= 0x00000050;   // 5 = 0101 sets '101'
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??

  // (--) THIS REGISTER IS GENERIC, NOTHING RELATED TO CHANNELS
  // 6. Configure the slave mode controller in reset mode: write the SMS bits
  // to 100 in the TIMx_SMCR register. Reg Man "0100: Reset Mode - Rising edge
  // of the selected trigger input (TRGI) reinitializes the counter and
  // generates an update of the registers."
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??
  regVal32 &= 0xfffefff8;    // e = 1110, Clear SMS bit 16, 8 = 1000, clear 2:0
  regVal32 |= 0x00000004;    // 4 = 0100 sets 2:0 = '100', leave bit 16 = 0
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // (--) Only some channels are handled
  // (--) SOMETHING SEEMS WRONG. IS IT CC1E AND CC2E OR CC1E AND CC1P?
  // CODE IS CCIP COMMENTS ARE CC2E.
  // 7. Enable the captures: write the CC1E (bit 0) and CC2E (bit 4) bits to
  // ‘1' in the TIMx_CCER register.
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 |= 0x0011;   // set bit 0 and bit 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // To enable the timer we needed to know which clock enable register to use.
  uint32_t apbClock = meadow_calc_freq_dc_get_apb_clock(freqDcTimerInfo);

  // And we need to know which bit to set in the register, which is in the
  // array. 
  modifyreg32(apbClock, 0, freqDcTimerInfo->timerClkEn);

  // (--) Timer wide setting
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // (--) NEW FEATURE IMPLEMENTED HERE ALLOW USER TO SELECT FREQUENCY VIA
  // SLOW, MEDIUM AND FAST SELECTION? BUT NOT PER CHANNEL.
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

  // (--) Timer wide setting
  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // (--) Timer wide setting
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
            meadow_calc_freq_dc_get_timer_info(timerNumber);

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
int meadow_calc_freq_dc_return_freq_info(struct freqDcReturnData_s
          *returnData)
{
  double frequency;
  double averageFreq;
  double dutyCycle;
  double totalTimerCount;
  double inputTotalCount;
  uint32_t retryCount;
  uint32_t fullCycle;
  uint32_t halfCycle;

  // Just feed pulse train into appropriate GPIO
  struct freqDcTimerInfo_s *freqDcTimerInfo =
            meadow_calc_freq_dc_get_timer_info(returnData->timerNumber);
  struct freqDcRtData_s *freqDcRtData =
            (struct freqDcRtData_s *)freqDcTimerInfo->freqDcRtData;

  // Find valid data. That is, both full cycle and the half cycle values are
  // available. This is only an issue at higher frequencies.
  for(retryCount = 0; retryCount < 5; retryCount++)
  {
    // Get all the values at one time so once a valid value is found, a change
    // in the timer's data won't affect the output.
    // This would be nice if it was atomic
    fullCycle = freqDcRtData->countLeadToLead;
    halfCycle = freqDcRtData->countLeadToTrail;
    totalTimerCount = (double)freqDcRtData->countTimerTotal;
    inputTotalCount = (double)freqDcRtData->countInputTotal;

    if(fullCycle > 0 && halfCycle > 0)
      break;

    usleep(1 * 1000);   // delay for valid data
  }

  if(fullCycle > 0 && halfCycle > 0)
  {
    // Do floating point math and convert to integer times 1000.
    // Duty Cycle is the ratio of the full cycle count and the cycle count
    // before the trailing edge was detected.
    dutyCycle = (((double)halfCycle) * 100.0) / ((double)fullCycle);

    // The frequency is the timer's clock divided by the cycle count.
    frequency = ((double)MEADOW_FREQ_DC_CLOCK_FREQ) / ((double)fullCycle);

    // Average frequency since last read
    double averageCount = totalTimerCount / inputTotalCount;
    averageFreq = ((double)MEADOW_FREQ_DC_CLOCK_FREQ) / averageCount;

    syslog(2, "In Code-Freq:%06.2fHz, DC:%02.2f%%, AvgFreq:%06.2f, Count:%lu, retries:%lu\n",
              frequency, dutyCycle, averageFreq, inputTotalCount,
              retryCount);

    returnData->frequencyX1000  = (frequency * 1000.0);
    returnData->avgFreqX1000    = (averageFreq * 1000.0);
    returnData->dutyCycleX1000  = (dutyCycle * 1000.0);
    returnData->countInputTotal = (uint32_t)inputTotalCount;
  }
  else
  {
    syslog(2, "Invalid data CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
              fullCycle, halfCycle, retryCount);
  }

  // Prevent counts from being reused
  freqDcRtData->countLeadToLead   = 0;
  freqDcRtData->countLeadToTrail  = 0;
  freqDcRtData->countInputTotal   = 0;
  freqDcRtData->countTimerTotal   = 0;

  return OK;
}
#endif    // #if defined(MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD)
