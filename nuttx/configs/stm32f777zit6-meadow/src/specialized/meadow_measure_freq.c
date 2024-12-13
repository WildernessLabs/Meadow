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
// This module, uses timers to calculate frequency

// The Problem with 16-bit timers.
// At certain input frequencies the CNT being cleared and the CNT overflow
// are reported in the same interrupt. However, there is only 1 bit available
// to indicate both conditions. This makes it impossible to tell which it is.
// The highest frequency this occurs at is TimerClock/65536, which is
// 1,464.844 Hz with a timer clock of 96 MHz. This reoccurs at the intervals
// (TimerClock/65536)/2, (TimerClock/65536)/3 etc.
//
// ToDo List
// x1. Add a running average feature. It would be the average since the last
//  reading.
// x2. Add count of the input GPIO trailing edges since last reading.
// x3. Add CCM support. This requires changes to the configuration and adding,
//  modifying or replacing existing tables to support more or all Timers
//  and their associated GPIOs.
// x4. Add Duty Cycle and Frequency average support
// 5. Add multi-channel support. Support all timer channels for input.
// 6. 
// 5. Add syscalls as needed (probably 2 maybe 3)
// 6. For 16-bit timers, allow with configuration to include SLOW, MED and
//  FAST options to reduce the effects of the 65,536 count rollover?
// 7. Write and test unconfigure code (need unique syscall?)
// 7a 
// 8. Support Tim1 and Tim8? These have more complex IRQ requirements.
// 9. Clean up code, remove unneeded header includes and retest

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

#include "specialized/meadow_measure_freq.h"

// Diagnostic
#pragma GCC optimize("O0")    // Prevent compiler from changing the code
#pragma message "(--) meadow_measure_freq.c"
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
#define MEADOW_FREQ_MAX_TIMER_CHANNELS     (4)
#define MEADOW_FREQ_TIMER_WIDTH_16         (0)
#define MEADOW_FREQ_TIMER_WIDTH_32         (1)
#define MEADOW_FREQ_16_BIT_OVERFLOW_COUNT  (65536)
#define MEADOW_FREQ_32_BIT_OVERFLOW_COUNT  (4294967296)

// To configure a GPIO as an input to a timer it, needs to contain the how it
// will be used (input with pulldown), Pin and Port, the Timer defined
// alternate function value plus the Nuttx GPIO_ALT value.
#define MEADOW_TIMER_GPIO_CONST (GPIO_ALT | GPIO_INPUT | GPIO_PULLDOWN)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static uint64_t startCaptureTime;

// This array contains timer information most of which is fixed by the STM32F7
// hardware. It contains each F7 timer and a flag for useability (TIM1 and
// TIM8 are not usable). The 'Acv' (i.e. Active channels) byte contains 4 bits
// that represent active/inuse channels within the timer.
static mdwFreqTimerInfo_t mdwFreqTimerInfoArray[] =
{
            //   |--- bit-field---|---------------------- Fixed by hardware -----------------------|--------- Runtime Data ---------|
            //   #  wid max apb use    Base Addr       Clk Timer Enable      IRQ Vector    Alt Func OvF Acv  Chan1 Chan2 Chan3 Chan4
  /* TIM1   */  {1 , 0,  1,  0, 0, STM32_TIM1_BASE,  0,                   0,               GPIO_AF1, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM2   */  {2 , 1,  0,  0, 1, STM32_TIM2_BASE,  RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2,  GPIO_AF1, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM3   */  {3 , 0,  0,  0, 1, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM4   */  {4 , 0,  0,  0, 1, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM5   */  {5 , 1,  0,  0, 1, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM8   */  {8 , 0,  1,  0, 0, STM32_TIM8_BASE,  0,                   0,               GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM9   */  {9 , 0,  1,  1, 1, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9,  GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM10  */  {10, 0,  1,  1, 1, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM11  */  {11, 0,  1,  1, 1, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM12  */  {12, 0,  0,  0, 1, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, GPIO_AF9, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM13  */  {13, 0,  0,  0, 1, STM32_TIM13_BASE, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM14  */  {14, 0,  0,  0, 1, STM32_TIM14_BASE, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14, GPIO_AF9, 0,  0, {NULL, NULL, NULL, NULL}}
};

#define MEADOW_FREQ_TOTAL_TIMERS_AVAILABLE (sizeof(mdwFreqTimerInfoArray)/sizeof(mdwFreqTimerInfo_t))

//----------------------------------------------------------------------------
// This table contains all of the STM32F7's timers and their valid GPIOs and
// channel. It is used for F7v1 and F7v2 and CCM. It should be able to verify
// any timer GPIO combination with an STM32F7 MCU.
mdwFreqChanPortPin_t validStm32F7GpioArray[][11] = 
{
// Tim
//  1   PA8         PE9        PA9        PE11      PA10,      PE13,      PA11,      PE14
      {{0x08, 1}, {0x49, 1}, {0x09, 2}, {0x4b, 2}, {0x0a, 3}, {0x4d, 3}, {0x0b, 4}, {0x4e, 4}, {0xff, 0}},
//  2    PA0,      PA15,       PA1,       PB3,       PA2,      PB10,       PA3,      PB11
      {{0x00, 1}, {0x0f, 1}, {0x01, 2}, {0x13, 2}, {0x02, 3}, {0x1a, 3}, {0x03, 4}, {0x1b, 4}, {0xff, 0}},
//  3    PA6        PC6        PB4,       PA7        PC7,       PB5,       PB0        PC8,       PB1        PC9
      {{0x06, 1}, {0x26, 1}, {0x14, 1}, {0x07, 2}, {0x27, 2}, {0x15, 2}, {0x10, 3}, {0x28, 3}, {0x11, 4}, {0x29, 4}, {0xff, 0}},
//  4    PD12       PB6        PD13       PB7        PD14,      PB8        PD15       PB9
      {{0x3c, 1}, {0x16, 1}, {0x3d, 2}, {0x17, 2}, {0x3e, 3}, {0x18, 3}, {0x3f, 4}, {0x19, 4}, {0xff, 0}},
//  5    PA0        PH10       PA1        PH11       PA2        PH12,      PA3,       PI0},
      {{0x00, 1}, {0x7a, 1}, {0x01, 2}, {0x7b, 2}, {0x02, 3}, {0x7c, 3}, {0x03, 4}, {0x80, 4}, {0xff, 0}},
//  6  No GPIO
      {{0xff, 0}},
//  7  No GPIO
      {{0xff, 0}},
//  8    PC6        PI5        PC7        PI6        PC8        PI7        PC9        PI2
      {{0x26, 1}, {0x85, 1}, {0x27, 2}, {0x86, 2}, {0x28, 3}, {0x87, 3}, {0x29, 4}, {0x82, 4}, {0xff, 0}},
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

// Used to verify pin and port availability on F7v1
static uint8_t validF7v1GpioArray[][5] =
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

// Used to verify pin and port availability on F7v2
static uint8_t validF7v2GpioArray[][5] =
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

static int meadow_measure_freq_cfg_timer_hardware(mdwFreqTimerInfo_t *mdwFreqTimerInfo,
            int inputTimerChan, bool configure);
static int meadow_measure_freq_isr(int irq, void *context, void *arg);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR is called for all frequency measurement interrupts.
// There is a unique ISR vector for each timer. But, each timer must process
// 1 - 4 inputs. On entry we don't know which input(s) is/are involved.
//
// Note: a 16-bit register at 96 MHz will overflow every 683 microseconds.
int meadow_measure_freq_isr(int irq, void *context, void *arg)
{
  mdwFreqTimerInfo_t *mdwFreqTimerInfo = (mdwFreqTimerInfo_t *)arg;
  uint32_t timerBase = mdwFreqTimerInfo->timerBase;

  // Get the current status to determine why ISR called
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  //----------------------------------------------------------
  // Check UIF (Update Interrupt Flag), which indicates CNT changed.
  // This is used to maintain the overflow count. The overflow count
  // is not a problem for 32-bit timers but for 16-bit it is.
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;

    // We need to account for the timer's CNT overflow and use it in the
    // individual input calculations.
    // Note: with a 96 MHz clock a 16-bit timer will rollover every 683
    // microseconds and at 960 kHz it will rollover every 68.3 milliseconds,
    // about 14.6 times/second.
    //
    // What is done here is to have a 32-bit value for the timer count and
    // another for the overflow value. This approach will effectively add
    // 32-bits to each timers size.
    mdwFreqTimerInfo->timerOverflow++;
  }

  //----------------------------------------------------------
  // Channel 1
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    // (--) Diag
    stm32_gpiowrite(DEBUG_PIN_V2_D06, true);
    
    timStatusReg &= ~GTIM_SR_CC1IF;

    // Ignore if channel not configured
    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_1)
    {
      // Channel data pointer
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_RT_DATA_OFFSET_CHAN_1];
      if(mdwFreqChanData == NULL)
      {
        syslog(1, "%s@%d-Channel 1-mdwFreqChanData is NULL in ISR\n", __FILE__, __LINE__);
        return -ERROR;  // -1
      }

      // From this point the code is identical for all channels
      // Read the captured value. This resets reg value to 0.
      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);

      // Read the input GPIO state to determine raising or falling edge.
      bool inputState = stm32_gpioread(mdwFreqChanData->inputConfig);
      if(inputState)
      {
        // Raising edge is end of previous capture and the beginning of a
        // new capture
        mdwFreqChanData->bgnResltCnt  = mdwFreqChanData->endResltCnt;
        mdwFreqChanData->bgnResltOFlo = mdwFreqChanData->endResltOFlo;
        mdwFreqChanData->midResltCnt  = mdwFreqChanData->midCaptrCnt;
        mdwFreqChanData->midResltOFlo = mdwFreqChanData->midCaptrOFlo;

        // End is now
        mdwFreqChanData->endResltCnt  = capturedCount;
        mdwFreqChanData->endResltOFlo = mdwFreqTimerInfo->timerOverflow;

        // The following is for finding average frequency
        mdwFreqChanData->totalGpioPulses++;
      }
      else
      {
        // Falling edge
        mdwFreqChanData->midCaptrCnt  = capturedCount;
        mdwFreqChanData->midCaptrOFlo = mdwFreqTimerInfo->timerOverflow;
      }
    }

    // (--) Diag
    stm32_gpiowrite(DEBUG_PIN_V2_D06, false);
  }

  //----------------------------------------------------------
  // Channel 2
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    // Ignore if channel not configured
    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_2)
    {
      // Channel data pointer
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_RT_DATA_OFFSET_CHAN_2];
      if(mdwFreqChanData == NULL)
      {
        syslog(1, "%s@%d-Channel 2-mdwFreqChanData is NULL in ISR\n", __FILE__, __LINE__);
        return -ERROR;  // -1
      }

      // Read the captured/control reg value. This resets reg value to 0.
      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);

      // Read the input GPIO state to determine raising or falling edge.
      bool inputState = stm32_gpioread(mdwFreqChanData->inputConfig);
      if(inputState)
      {
        // Raising edge is end of previous capture and the beginning of a
        // new capture
        mdwFreqChanData->bgnResltCnt  = mdwFreqChanData->endResltCnt;
        mdwFreqChanData->bgnResltOFlo = mdwFreqChanData->endResltOFlo;
        mdwFreqChanData->midResltCnt  = mdwFreqChanData->midCaptrCnt;
        mdwFreqChanData->midResltOFlo = mdwFreqChanData->midCaptrOFlo;

        // End is now
        mdwFreqChanData->endResltCnt  = capturedCount;
        mdwFreqChanData->endResltOFlo = mdwFreqTimerInfo->timerOverflow;

        // The following is for finding average frequency
        mdwFreqChanData->totalGpioPulses++;
      }
      else
      {
        // Falling edge
        mdwFreqChanData->midCaptrCnt  = capturedCount;
        mdwFreqChanData->midCaptrOFlo = mdwFreqTimerInfo->timerOverflow;
      }
    }
  }

  //----------------------------------------------------------
  // Channel 3
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    timStatusReg &= ~GTIM_SR_CC3IF;
    // Ignore if channel not configured
    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_3)
    {
      // Channel data pointer
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_RT_DATA_OFFSET_CHAN_3];
      if(mdwFreqChanData == NULL)
      {
        syslog(1, "%s@%d-Channel 3-mdwFreqChanData is NULL in ISR\n", __FILE__, __LINE__);
        return -ERROR;  // -1
      }

      // Read the captured/control reg value. This resets reg value to 0.
      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR3_OFFSET);

      // Read the input GPIO state to determine raising or falling edge.
      bool inputState = stm32_gpioread(mdwFreqChanData->inputConfig);
      if(inputState)
      {
        // Raising edge is end of previous capture and the beginning of a
        // new capture
        mdwFreqChanData->bgnResltCnt  = mdwFreqChanData->endResltCnt;
        mdwFreqChanData->bgnResltOFlo = mdwFreqChanData->endResltOFlo;
        mdwFreqChanData->midResltCnt  = mdwFreqChanData->midCaptrCnt;
        mdwFreqChanData->midResltOFlo = mdwFreqChanData->midCaptrOFlo;

        // End is now
        mdwFreqChanData->endResltCnt  = capturedCount;
        mdwFreqChanData->endResltOFlo = mdwFreqTimerInfo->timerOverflow;

        // The following is for finding average frequency
        mdwFreqChanData->totalGpioPulses++;
      }
      else
      {
        // Falling edge
        mdwFreqChanData->midCaptrCnt  = capturedCount;
        mdwFreqChanData->midCaptrOFlo = mdwFreqTimerInfo->timerOverflow;
      }
    }
  }

  //----------------------------------------------------------
  // Channel 4
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;

    // Ignore if channel not configured
    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_4)
    {
      // Channel data pointer
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_RT_DATA_OFFSET_CHAN_4];
      if(mdwFreqChanData == NULL)
      {
        syslog(1, "%s@%d-Channel 4-mdwFreqChanData is NULL in ISR\n", __FILE__, __LINE__);
        return -ERROR;  // -1
      }

      // Read the captured/control reg value. This resets reg value to 0.
      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR4_OFFSET);

      // Read the input GPIO state to determine raising or falling edge.
      bool inputState = stm32_gpioread(mdwFreqChanData->inputConfig);
      if(inputState)
      {
        // Raising edge is end of previous capture and the beginning of a
        // new capture
        mdwFreqChanData->bgnResltCnt  = mdwFreqChanData->endResltCnt;
        mdwFreqChanData->bgnResltOFlo = mdwFreqChanData->endResltOFlo;
        mdwFreqChanData->midResltCnt  = mdwFreqChanData->midCaptrCnt;
        mdwFreqChanData->midResltOFlo = mdwFreqChanData->midCaptrOFlo;

        // End is now
        mdwFreqChanData->endResltCnt  = capturedCount;
        mdwFreqChanData->endResltOFlo = mdwFreqTimerInfo->timerOverflow;

        // The following is for finding average frequency
        mdwFreqChanData->totalGpioPulses++;
      }
      else
      {
        // Falling edge
        mdwFreqChanData->midCaptrCnt  = capturedCount;
        mdwFreqChanData->midCaptrOFlo = mdwFreqTimerInfo->timerOverflow;
      }
    }
  }



  // (--) Diag
  stm32_gpiowrite(DEBUG_PIN_V2_D06, false);
  // Clear the timer's status register
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  return OK;
}

//=============================================================
// Find the RCC clock to enable for the selected timer
static uint32_t meadow_measure_freq_get_apb_clock(
          mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  if(mdwFreqTimerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
static uint32_t meadow_measure_freq_get_max_clock(
          const mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  if(mdwFreqTimerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
// Disable the selected timer
static void meadow_measure_freq_disable(const uint32_t timerBase)
{
  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
}

//=============================================================
// Enable the selected timer
static void meadow_measure_freq_enable(const uint32_t timerBase)
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
// The following finds the specified timer's information defined here
static mdwFreqTimerInfo_t *meadow_measure_freq_chk_get_timer_info(
          const int timerNumb)
{
  mdwFreqTimerInfo_t *timerInfo = &(mdwFreqTimerInfoArray[timerNumb - 1]);
  if(timerInfo->timerUsable)
    return timerInfo;

  return NULL;
}

//=====================================================================
// Returns the correct bit field definition based on the timer's channel
static uint8_t meadow_measure_freq_get_chan_bit_set(const int timerChan)
{
  switch(timerChan)
  {
    case 1:
      return ACTIVE_CHAN_BITFIELD_1;
      break;
    case 2:
      return ACTIVE_CHAN_BITFIELD_2;
      break;
    case 3:
      return ACTIVE_CHAN_BITFIELD_3;
      break;
    case 4:
      return ACTIVE_CHAN_BITFIELD_4;
      break;
    default:
      return 0;
  }
}

//==================================================================
// Get the current time in nanoseconds. This is used for the average
// frequency calculations.
static uint64_t meadow_measure_freq_get_current_time(void)
{
  int ret;
  uint64_t returnTime;
  struct tm rtcTime;
  long nsecs;
  long prevNsecs;

  // This Nuttx function reads the date, time and sub-seconds from the MCU's
  // hardware into a struct tm. However, the STM32F77X Errata warns about a
  // possible problem in ES0334-Rev 9 2.12.1 related to the RTC calendar
  // register not locked properly. Therefore, we'll read nsec twice and
  // compare.
#ifdef CONFIG_STM32F7_HAVE_RTC_SUBSECONDS
do
  {
    ret = up_rtc_getdatetime_with_subseconds(&rtcTime, &prevNsecs);
    if(ret < 0)
    {
      return ret;
    }

    // Read a second time per Errata
    ret = up_rtc_getdatetime_with_subseconds(&rtcTime, &nsecs);
    if(ret < 0)
    {
      return ret;
    }

    // If they match we have good values
    if(prevNsecs == nsecs)
      break;
      
  } while (1);
#else
  // This function is used if no sub-seconds.
  ret = up_rtc_getdatetime(&rtcTime)
  nsecs = 0;
#endif

  returnTime = mktime(&rtcTime);
  returnTime *= 1000 * 1000 * 1000;
  returnTime += nsecs;

  return returnTime;
}

//=============================================================
// This function will evaluate the GPIO based on 3 tables containing the
// valid GPIOs for the CCM (all F7 GPIOs checked not just CCM exposed)
// and for F7v1 and F7v2.
//
// Returns the channel, 1-4 unless not found, then returns 0.
static uint8_t meadow_measure_freq_get_chan_tim_port_pin(
          const int timerNumb, uint8_t portAndPin)
{
  int entry;
  int timerOffset = timerNumb - 1;

  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    // Verify pin & port are valid for FeatherV1 hardware
    entry = 0;
    while(validF7v1GpioArray[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validF7v1GpioArray[timerOffset][entry])
      {
        syslog(1, "%s@%d- V1 so far good\n", __FILE__, __LINE__);
      }
      entry++;
    }

    if(validF7v1GpioArray[timerOffset][entry] == 0xff)
     return 0;
  }
  else if (meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2)
  {
    // Verify pin & port are valid for FeatherV2 hardware
    entry = 0;
    while(validF7v2GpioArray[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validF7v2GpioArray[timerOffset][entry])
      {
        break;    // So far good
      }
      entry++;
    }
    
    if(validF7v2GpioArray[timerOffset][entry] == 0xff)
      return 0;
  }
  else if (meadow_hw_version_get() != MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    // Unsupported device type
    syslog(1, "%s@%d- Unknown device type\n", __FILE__, __LINE__);
    return 0;
  }

  // All hardware types reach here to find the timer channel and verify the
  // pin and port are legal.
  entry = 0;
  while(validStm32F7GpioArray[timerOffset][entry].portPin != 0xff)
  {
    if(portAndPin == validStm32F7GpioArray[timerOffset][entry].portPin)
    {
      // Found valid channel
      return validStm32F7GpioArray[timerOffset][entry].chan;
    }
    entry++;
  }

  return 0; // Channel not found
}

/****************************************************************************
 * Public Function
 ****************************************************************************/
// Called by Meadow.Core to configure a timer channel
// Timer numbers range from 1 - 14. However, some are not defined.
int meadow_measure_freq_configure(const int timerNumber, int channelNumber,
          const uint8_t portAndPin)
{
  int ret;
  static bool isFirstTime = true;
  int channelOffset = channelNumber - 1;
  uint32_t inputGpioConfig;

  // WIP - Used for not reconfiguring timer for additional GPIO inputs
  bool isTimerConfigured;

  if(isFirstTime)
  {
    isFirstTime = false;

    // (--) Diag config LED
    stm32_configgpio(DEBUG_PIN_V2_D06);
    // (--) Diag LED
  }

  if(timerNumber > 14 || timerNumber < 1)
  {
    syslog(2, "%s@%d-Timer must be 1 - 14\n", __FILE__, __LINE__);
    return -ENOTSUP;
  }
  
  if(channelNumber > 4 || channelNumber < 1)
  {
    syslog(2, "%s@%d-Channel must be 1 - 4\n", __FILE__, __LINE__);
    return -ENOTSUP;
  }

  // Insure a correct timer / pin+port combination was supplied.
  // Timers have, at most, 1-4 channels, each representing 1 GPIO. For the
  // specified timer we need to verify a proper port and pin.
  uint8_t validatedTimerChan = meadow_measure_freq_get_chan_tim_port_pin(
            timerNumber, portAndPin);
  if(validatedTimerChan == 0)
  {
    syslog(2, "%s@%d-The Port and Pin, not valid for timer %d\n",
              __FILE__, __LINE__, timerNumber);
    return -ENOTSUP;
  }

  // Is this the channel the user wanted?
  if(channelNumber != validatedTimerChan)
  {
    syslog(2, "%s@%d-The Port/Pin/Timer/Channel combination, not valid\n",
              __FILE__, __LINE__);
    return -ENOTSUP;
  }

  // Check if this timer is useable and get a pointer if it is
  mdwFreqTimerInfo_t *mdwFreqTimerInfo =
            meadow_measure_freq_chk_get_timer_info(timerNumber);
  if(mdwFreqTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Timer %ld cannot be used\n",
              __FILE__, __LINE__, timerNumber);
    return -ENOTSUP;
  }

  // Has this timer been initialized for a channel?
  if(mdwFreqTimerInfo->chanActiveBits != 0)
    isTimerConfigured = true;
  else
    isTimerConfigured = false;

  // This timer is usable, but is this channel already being used?
  uint8_t chanBit = meadow_measure_freq_get_chan_bit_set(channelNumber);
  if(mdwFreqTimerInfo->chanActiveBits & chanBit)
  {
    syslog(LOG_ERR, "%s@%d-Timer %d, channel %d in use\n",
              __FILE__, __LINE__, timerNumber, channelNumber);
    return -EADDRINUSE;
  }

  // Build the GPIO input configuration for Nuttx GPIO processing
  inputGpioConfig = MEADOW_TIMER_GPIO_CONST | portAndPin | \
            mdwFreqTimerInfo->timerAltFunc;

  // Diagnostic
  // syslog(1, "%s@%d-input Pin defn:0x%02x (P%c%d), Pin defn + AF:0x%08lx\n",
  //           __FILE__, __LINE__, portAndPin,
  //           ((portAndPin) >> 4) + 'A', portAndPin & 0x0f, inputGpioConfig);
  // Diagnostic

  // Allocate a struct for each new channel on a timer
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset] = (mdwFreqChanData_t*)zalloc(sizeof(mdwFreqChanData_t));
  if(mdwFreqTimerInfo->mdwFreqChanData[channelOffset] == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Allocation for mdwFreqChanData_s NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // Valid GPIO so configure input point for timer.
  ret = stm32_configgpio(inputGpioConfig);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:stm32_configgpio() returned:%ld\n",
              __FILE__, __LINE__, ret);
    free(mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
    return -ENOTSUP;   // Not supported
  }

  // Initialized the F7's timer hardware
  ret = meadow_measure_freq_cfg_timer_hardware(mdwFreqTimerInfo, validatedTimerChan,
            true);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow frequency init failed:%d\n",
              __FILE__, __LINE__, ret);
    free(mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
    return ret;
  }

  // Start filling the channel structure
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->inputConfig     = inputGpioConfig;
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->inputTimerChan  = validatedTimerChan;
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->totalGpioPulses = 0;
  mdwFreqTimerInfo->chanActiveBits |= chanBit;

  // This time is used to calculate average frequency over time
  startCaptureTime = meadow_measure_freq_get_current_time();
  return OK;
}

//=============================================================
// Frequency (and duty cycle) configuration of timer registers.
int meadow_measure_freq_cfg_timer_hardware(
          mdwFreqTimerInfo_t *mdwFreqTimerInfo,
          int inputTimerChan,
          bool isTimerConfigured)
{
  int ret;
  uint16_t regVal16;
  uint32_t regVal32;

  if(mdwFreqTimerInfo == NULL)
    return -ENXIO;

  uint32_t timerBase = mdwFreqTimerInfo->timerBase;
  
  // Before starting disable capture/control for all channels. Ref Man (26.4.7 at
  // end) "Note: CC1S bits are writable only when the channel is OFF (i.e.
  // CC1E = 0 in TIMx_CCER)."
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  // clear CCxE bits 0, 4, 8 & 12
  regVal16 &= ~(GTIM_CCER_CC1E | GTIM_CCER_CC2E | GTIM_CCER_CC3E | GTIM_CCER_CC4E);
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Associate each input to it's capture/compare register
  uint16_t dierBits = 0;
  dierBits |= GTIM_DIER_UIE;    // Enable timer overrun

  switch(inputTimerChan)
  {
    case 1:
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
      regVal32 |= 0x00000001;       // 1 = 01, set bits 1:0
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

      // Set the input triggers, rising, falling or both for channel
      regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
      // regVal16 &= ~(GTIM_CCER_CC1P | GTIM_CCER_CC1NP); // 00 rising only
      regVal16 |= (GTIM_CCER_CC1P | GTIM_CCER_CC1NP);     // 11 both edges
      putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

      dierBits |= GTIM_DIER_CC1IE;
      break;

    case 2:
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
      regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

      regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
      // regVal16 &= ~(GTIM_CCER_CC2P | GTIM_CCER_CC2NP); // 00 rising only
      regVal16 |= (GTIM_CCER_CC2P | GTIM_CCER_CC2NP);     // 11 both edges
      putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

      dierBits |= GTIM_DIER_CC2IE;
      break;

    case 3:
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
      regVal32 |= 0x00000001;   // 1 = 01, set bits 1:0
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

      regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
      // regVal16 &= ~(GTIM_CCER_CC3P | GTIM_CCER_CC3NP); // 00 rising only
      regVal16 |= (GTIM_CCER_CC3P | GTIM_CCER_CC3NP);     // 11 both edges
      putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
      dierBits |= GTIM_DIER_CC3IE;
      break;

    case 4:
      // 01: IC4 is mapped on TI4
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
      regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

      regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
      // regVal16 &= ~(GTIM_CCER_CC4P | GTIM_CCER_CC4NP); // 00 rising only
      regVal16 |= (GTIM_CCER_CC4P | GTIM_CCER_CC4NP);     // 11 both edges
      putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
      dierBits |= GTIM_DIER_CC4IE;
      break;

    default:
      return -ENODEV;   // No such device
  }

  // Slave mode control register
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= ~(GTIM_SMCR_ECE | GTIM_SMCR_DISAB | GTIM_SMCR_SMS);
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // To enable the timer we needed to know which clock enable register to use.
  // And we need to know which bit to set in the register
  uint32_t apbClock = meadow_measure_freq_get_apb_clock(mdwFreqTimerInfo);
  modifyreg32(apbClock, 0, mdwFreqTimerInfo->timerClkEn);

  // (--) NEW FEATURE IMPLEMENTED HERE? ALLOW USER TO SELECT FREQUENCY VIA
  // SLOW, MEDIUM AND FAST SELECTION? BUT NOT PER CHANNEL, PER TIMER
  // ?? Find proper pre-scaler value so all timers run at the same speed, no
  // ?? matter which clock line they are connected to.
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed, allowed by MEADOW_FREQ_CLOCK_FREQ.
  // A prescaler value of 1 will divide the clock by 2.
  uint16_t prescaler = (meadow_measure_freq_get_max_clock(mdwFreqTimerInfo)/ \
            MEADOW_FREQ_CLOCK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum allowed for the timer. Either
  // 32-bit or 16-bit ARR register.
  uint32_t maxARRValue = mdwFreqTimerInfo->timerWidth ==
            MEADOW_FREQ_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  // Control Register 1
  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // Enable the timer input capture for this channel (it was disabled earlier)
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  switch(inputTimerChan)
  {
    case 1:
      regVal16 |= GTIM_CCER_CC1E;
      break;

    case 2:
      regVal16 |= GTIM_CCER_CC2E;
      break;

    case 3:
      regVal16 |= GTIM_CCER_CC3E;
      break;

    case 4:
      regVal16 |= GTIM_CCER_CC4E;
      break;
  }
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Clear all interrupt sources and set the ones we need in the DMA/Interrupt
  // enable register (DIER).
  // Note: Advanced timers 1 & 8 add ATIM_DIER_COMIE, ATIM_DIER_BIE and
  // ATIM_DIER_COMDE
  putreg16(dierBits, timerBase + STM32_GTIM_DIER_OFFSET);

  // All interrupts are handled by the same ISR code, but each timer has a
  // different interrupt vector.
  ret = irq_attach(mdwFreqTimerInfo->timerIrqVec,
            meadow_measure_freq_isr,  // ISR address
            mdwFreqTimerInfo);        // Argument to ISR
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Enable timer
  meadow_measure_freq_enable(timerBase);

  // Lastly enable IRQ
  up_enable_irq(mdwFreqTimerInfo->timerIrqVec);

  return OK;
}

//=============================================================
// Called to unconfigure a timer
int meadow_measure_freq_unconfigure(const uint32_t timerNumber)
{
  // mdwFreqTimerInfo_t *mdwFreqTimerInfo =
  //           meadow_measure_freq_chk_get_timer_info(timerNumber);

  // // Is this slot being used?
  // if(mdwFreqTimerInfo->mdwFreqChanData == NULL)
  // {
  //   syslog(LOG_ERR, "%s@%d-Can't unconfigure, this timer %lu not configured.\n",
  //             __FILE__, __LINE__, timerNumber);
  //   return -EBADSLT;    // Invalid slot
  // }

  // // Stop interrupts
  // up_disable_irq(mdwFreqTimerInfo->timerIrqVec);

  // // Stop timer
  // meadow_measure_freq_disable(mdwFreqTimerInfo->timerBase);

  // // Unconfigure GPIO
  // stm32_unconfiggpio(mdwFreqTimerInfo->mdwFreqChanData->inputConfig);

  // // Free runtime memory
  // free(mdwFreqTimerInfo->mdwFreqChanData);
  // mdwFreqTimerInfo->mdwFreqChanData = NULL;
  return OK;
}

//================================================================
// Return Frequency and Duty Cycle information to caller.
// Moved as much processing here as apposed to the ISR.
int meadow_measure_freq_return_freq_info(mdwFreqReturnData_t
          *returnData)
{
  double dblFrequency;
  double dblAverageFreq;
  double dblDutyCycle;
  double dblTotalInputCount;
  uint64_t halfCycle;
  uint64_t fullCycle;
  uint64_t regOvrFloTimSize;
  uint64_t beginCaptrUi64;
  uint64_t midCaptrUi64;
  uint64_t endCaptrUi64;
  uint64_t endCaptureTime = meadow_measure_freq_get_current_time();

  // Just feed pulse train into appropriate GPIO
  mdwFreqTimerInfo_t *mdwFreqTimerInfo =
            meadow_measure_freq_chk_get_timer_info(returnData->timerNumber);
  if(mdwFreqTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Couldn't get TimerInfo from timer number:%lu\n",
          __FILE__, __LINE__, returnData->timerNumber);
    return -1;
  }

  mdwFreqChanData_t *mdwFreqChanData =
            mdwFreqTimerInfo->mdwFreqChanData[returnData->channelNumber - 1];
  if(mdwFreqChanData == NULL)
  {
    syslog(1, "%s@%d-Unconfigured channel accessed\n", __FILE__, __LINE__);
    return -ERROR;  // -1
  }

  if(mdwFreqTimerInfo->timerWidth)
    regOvrFloTimSize = MEADOW_FREQ_32_BIT_OVERFLOW_COUNT;
  else
    regOvrFloTimSize = MEADOW_FREQ_16_BIT_OVERFLOW_COUNT;

  uint64_t ovrFloAtStart = mdwFreqChanData->bgnResltOFlo;

  // Add in the overflow counts
  beginCaptrUi64 = mdwFreqChanData->bgnResltCnt + ((mdwFreqChanData->bgnResltOFlo - ovrFloAtStart) * regOvrFloTimSize);
  midCaptrUi64   = mdwFreqChanData->midResltCnt + ((mdwFreqChanData->midResltOFlo - ovrFloAtStart) * regOvrFloTimSize);
  endCaptrUi64   = mdwFreqChanData->endResltCnt + ((mdwFreqChanData->endResltOFlo - ovrFloAtStart) * regOvrFloTimSize);

  // Use normalize the values, set begin at 0
  fullCycle = endCaptrUi64 - beginCaptrUi64;
  halfCycle = midCaptrUi64 - beginCaptrUi64;

  // To help preserve resolution, use floating point math.
  syslog(1, "%s@%d-Snapshot-full Count:%llu, half Count:%llu, inputTotal:%llu\n",
            __FILE__, __LINE__,
            fullCycle, halfCycle,
            mdwFreqChanData->totalGpioPulses);

  // Duty Cycle is the ratio of the full cycle count and the cycle count
  // before the trailing edge was detected.
  dblDutyCycle = (((double)halfCycle) * 100.0) / ((double)fullCycle);

  // The dblFrequency is the timer's clock divided by the cycle count.
  dblFrequency = ((double)MEADOW_FREQ_CLOCK_FREQ) / ((double)fullCycle);

  // Average frequency since last read calculations
  uint64_t totalCaptureTime = endCaptureTime - startCaptureTime;
  // Convert nanoseconds to fractional seconds
  double dblTotalCaptureTime = ((double) totalCaptureTime) / (1000.0 * 1000.0 * 1000.0);
  dblTotalInputCount = (double)mdwFreqChanData->totalGpioPulses;
  dblAverageFreq = ((double)dblTotalInputCount) / dblTotalCaptureTime;

  syslog(2, "AvgFreq:%6.2f, Since last read, GpioCnt:%llu\n",
            dblAverageFreq,
            mdwFreqChanData->totalGpioPulses);

  syslog(2, "In Code-Freq:%06.2fHz, DC:%02.2f%%, AvgFreq:%06.8f, Count:%lu\n",
            dblFrequency, dblDutyCycle, dblAverageFreq,
            (uint32_t)dblTotalInputCount);

  returnData->frequencyX1000  = (dblFrequency * 1000.0);
  returnData->dutyCycleX1000  = (dblDutyCycle * 1000.0);
  returnData->avgFreqX1000    = (dblAverageFreq * 1000.0);
  returnData->totalGpioPulses = (uint32_t)dblTotalInputCount;

  // Reset capture counts for new average calc
  mdwFreqChanData->totalGpioPulses = 0;
  startCaptureTime = meadow_measure_freq_get_current_time();

  return OK;
}
#endif    // #if defined(MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD)
