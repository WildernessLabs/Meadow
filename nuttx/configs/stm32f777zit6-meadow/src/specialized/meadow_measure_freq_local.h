/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_measure_freq_local.h
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
#ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_LOCAL__H
#define __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_LOCAL__H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <meadow/hcom_shared_common.h>

#include <nuttx/config.h>
#include <string.h>
#include <stdint.h>
#include "stm32_gpio.h"   // stm32_configgpio

// 96 MHz is top speed. Since the interrupts are based on edges,
// the only reason for slowing the clock would be to slow down the
// number of overflows for a 16-bit timer (see comments below in ISR).
// The clock speed can only be even multiples of the system clock.
// See board.h for details.
// #define MEADOW_FREQ_CLOCK_FREQ (96000000) // 96 MHz target frequency
// For current application reduce clock speed by 100x
#define MEADOW_FREQ_CLOCK_FREQ (960000)       // 960kHz

//=====================================================
#define MEADOW_FREQ_MAX_TIMER_CHANNELS     (4)
#define MEADOW_FREQ_TIMER_WIDTH_16         (0)
#define MEADOW_FREQ_TIMER_WIDTH_32         (1)
#define MEADOW_FREQ_16_BIT_OVERFLOW_COUNT  (65536)
#define MEADOW_FREQ_32_BIT_OVERFLOW_COUNT  (4294967296)

// To configure a GPIO as an input to a timer it, needs to contain how it's
// to be used (floating input), plus Pin and Port and the Timer defined
// alternate function value plus the Nuttx GPIO_ALT value.
#define MEADOW_TIMER_GPIO_CONST (GPIO_ALT | GPIO_INPUT | GPIO_FLOAT)

//--------------------------------------------------------------------------
// This structure contains runtime data
struct mdwFreqChanData_s
{
  // Needed for duty cycle
  uint32_t midCaptureCnt;
  uint32_t midCaptureOvr;

  // Used to calculate frequency etc.
  uint32_t bgnResultCnt;
  uint32_t bgnResultOvr;
  uint32_t midResultCnt;
  uint32_t midResultOvr;
  uint32_t endResultCnt;
  uint32_t endResultOvr;

  bool useDutyCycle;            // Does channel need duty cycle?
  uint32_t inputConfig;         // Nuttx GPIO config for unconfig
  uint64_t gpioCountForAvg;     // GPIO input count for average
  uint64_t startTimeForAvg;     // Meadow start time for average
};
typedef struct mdwFreqChanData_s mdwFreqChanData_t;

#define ACTIVE_CHAN_BITFIELD_1 (0b00000001)
#define ACTIVE_CHAN_BITFIELD_2 (0b00000010)
#define ACTIVE_CHAN_BITFIELD_3 (0b00000100)
#define ACTIVE_CHAN_BITFIELD_4 (0b00001000)

// Offsets for channels in the mdwFreqChanData field
#define FREQ_CHANNEL_NUMBER_CHAN_1 (1)
#define FREQ_CHANNEL_NUMBER_CHAN_2 (2)
#define FREQ_CHANNEL_NUMBER_CHAN_3 (3)
#define FREQ_CHANNEL_NUMBER_CHAN_4 (4)

// Offsets for channels in the mdwFreqChanData field
#define FREQ_CHAN_DATA_OFFSET_CHAN_1 (0)
#define FREQ_CHAN_DATA_OFFSET_CHAN_2 (1)
#define FREQ_CHAN_DATA_OFFSET_CHAN_3 (2)
#define FREQ_CHAN_DATA_OFFSET_CHAN_4 (3)

// The 'freqTimerInfo_s' contains information that defines the selected
// timer's F7's internal hardware capabilities. Except for the 'freqDcRtData'
// element, each field is pre-defined from the 'struct freqTimerInfo_s array'
struct mdwFreqTimerInfo_s
{
  uint8_t  timerNumb   : 4; // ( # ) 1 - 14 timer number
  uint8_t  timerWidth  : 1; // (32b) 16-bit or 32-bit timer? 0=16-bits, 1=32-bits
  uint8_t  timerMaxClk : 1; // (216) 0=STM32_APB1_TIM2_CLKIN, 1=STM32_APB2_TIM1_CLKIN
  uint8_t  timerAPBClk : 1; // (apb) 0=STM32_RCC_APB1ENR, 1=STM32_RCC_APB2ENR
  uint8_t  timerUsable : 1; // (use) Is Timer useable? 0=No, 1=Yes
  uint32_t timerBase;       // Unique base address for each timer
  uint32_t timerClkEn;      // Offset for timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;     // Timer's Interrupt vector
  uint16_t timerAltFunc;    // Timer's GPIO alternate function
  uint64_t timerOverflow;   // Each CNT overflow, increment
  uint8_t  chanActiveBits;  // Channel active? (bit 0=chan1, bit 1=chan2....)
  mdwFreqChanData_t *mdwFreqChanData[4]; // One for each input channel
};
typedef struct mdwFreqTimerInfo_s mdwFreqTimerInfo_t;

struct mdwFreqChanPortPin_s
{
  uint8_t portPin;
  uint8_t chan;
};
typedef struct mdwFreqChanPortPin_s mdwFreqChanPortPin_t;

#endif      // __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_LOCAL__H
