/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_measure_freq.h
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
#ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ__H
#define __CONFIGS_MEADOW_SPEC_MEADOW_FREQ__H

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
// For current application reduce clock speed
#define MEADOW_FREQ_CLOCK_FREQ (960000)       // 960kHz

//--------------------------------------------------------------------------
// This structure contains runtime data

// (--) mdwFreqRtData_s DOESN"T NEED TO BE IN THIS HEADER FILE
struct mdwFreqRtData_s
{
  uint32_t midCaptrCnt;
  uint32_t midCaptrOFlo;

  // Used to calculate frequency etc.
  uint32_t bgnResltCnt;
  uint32_t bgnResltOFlo;
  uint32_t midResltCnt;
  uint32_t midResltOFlo;
  uint32_t endResltCnt;
  uint32_t endResltOFlo;

  uint32_t inputConfig;         // Nuttx GPIO config for unconfig
  uint32_t inputTimerChan;      // 0=none used, 1-4 channel of timer input (--)NEEDED?
  uint64_t gpioCountForAvg;     // Count of GPIO inputs (average)
  uint64_t startTimeForAvg;     // Meadow start time for average
};
typedef struct mdwFreqRtData_s mdwFreqChanData_t;

#define ACTIVE_CHAN_BITFIELD_1 (0b00000001)
#define ACTIVE_CHAN_BITFIELD_2 (0b00000010)
#define ACTIVE_CHAN_BITFIELD_3 (0b00000100)
#define ACTIVE_CHAN_BITFIELD_4 (0b00001000)

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
  uint8_t  timerNumb   : 4; // 0 - 15 timer number
  uint8_t  timerWidth  : 1; // 16-bit or 32-bit timer? 0=16-bits, 1=32-bits
  uint8_t  timerMaxClk : 1; // 0=STM32_APB1_TIM2_CLKIN, 1=STM32_APB2_TIM1_CLKIN
  uint8_t  timerAPBClk : 1; // 0=STM32_RCC_APB1ENR, 1=STM32_RCC_APB2ENR
  uint8_t  timerUsable : 1; // Is Timer useable? 0=No, 1=Yes
  uint32_t timerBase;       // Unique base address for each timer
  uint32_t timerClkEn;      // Offset for timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;     // Timer's Interrupt vector
  uint16_t timerAltFunc;    // Timer's GPIO alternate function
  uint64_t timerOverflow;   // Each CNT overflow, increment
  uint8_t  chanActiveBits;  // Channel active? (bit 0=chan1, bit 1=chan2....)
  mdwFreqChanData_t *mdwFreqChanData[4]; // One for each input channel
};
typedef struct mdwFreqTimerInfo_s mdwFreqTimerInfo_t;

struct mdwFreqReturnData_s
{
  uint32_t timerNumber;       // The timer number 1-14
  uint32_t channelNumber;     // The channel number 1-4
  uint32_t frequencyX1000;    // Frequency * 1000
  uint32_t dutyCycleX1000;    // Duty Cycle * 1000
  uint32_t avgFreqX1000;      // Average frequency * 1000
  uint32_t gpioCountForAvg;   // Number of transitions since list read
};
typedef struct mdwFreqReturnData_s mdwFreqReturnData_t;

struct mdwFreqChanPortPin_s
{
  uint8_t portPin;
  uint8_t chan;
};
typedef struct mdwFreqChanPortPin_s mdwFreqChanPortPin_t;

//--------------------------------------------------------------------------
int meadow_measure_freq_configure(int timerNumber, int channelNumber,
          uint8_t pinDesignation);
int meadow_measure_freq_unconfigure(uint32_t timerNumber);
int meadow_measure_freq_return_freq_info(mdwFreqReturnData_t *mdwFreqReturnData);

#endif      // __CONFIGS_MEADOW_SPEC_MEADOW_FREQ__H
