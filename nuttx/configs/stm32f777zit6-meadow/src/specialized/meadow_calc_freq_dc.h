/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_calc_freq_dc.h
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
#ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_DC__H
#define __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_DC__H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <meadow/hcom_shared_common.h>

#include <nuttx/config.h>
#include <string.h>
#include <stdint.h>
#include "stm32_gpio.h"   // stm32_configgpio

// 96 MHz is top speed. Since the interrupts are based on leading and falling
// edges the only reason for slowing the clock would be to slow down the
// number of overflows for a 16-bit timer (see comments below in ISR).
#define MEADOW_FREQ_DC_CLOCK_FREQ (96000000) // 96 MHz target frequency

//--------------------------------------------------------------------------
// This internal structure contains the data that all timer applications
// require to operate. The data is primarily used by the ISR
//
// (--) THE FOLLOWING STRUCTURES ARE KIND OF MESSED UP. THE freqDcRtData_s
// CONTAINS ACTIVE RUNTIME DATA AND CONFIGURATION DATA. THE freqDcTimerInfo_s
// CONTAINS STATIC DATA PLUS A POINTER TO freqDcRtData_s. SHOULD THIS BE 3
// STRUCTURES, RUNTIME, CONFIG AND STATIC?
// This structure contains runtime data
struct freqDcRtData_s
{
  // In the following 'Lead' is the leading edge, which can be rising
  // or falling). It is the edge that begins the measurement cycle and
  // 'Trail' is the opposite edge.
  // The Lead to Lead count is the time for one full cycle, allowing
  // us to calculate the frequency. The Lead to Trail is the first half
  // of the cycle allowing us to calculate the duty cycle.
  volatile uint8_t activeState;           // State or Error of some type
  volatile uint32_t countLeadToLead;      // Count leading edge to next one
  volatile uint32_t countLeadToTrail;     // Count leading edge to 1/2 cycle
  volatile uint32_t LeadToLeadOverFlow;   // Leading to Leading overflow count
  volatile uint32_t LeadToTrailOverFlow;  // Leading to Trailing overflow count
  uint32_t inputConfig;                   // Nuttx style GPIO configuration
  uint8_t inputPolarity;       // 0=leading is rising, 1=leading is falling
};

// The 'freqDcTimerInfo_s' contains information that defines the selected
// timer's F7's internal hardware capabilities. Except for the 'freqDcRtData'
// element, each field is pre-defined from the 'struct freqDcTimerInfo_s array'.
struct freqDcTimerInfo_s
{
  uint8_t timerNumb   : 4;  // 0 - 15 timer number
  uint8_t timerWidth  : 1;  // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;  // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;  // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerFuture : 1;  // Future
  uint32_t timerBase;       // Unique for each timer
  uint32_t timerClkEn;      // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;     // Interrupt vector
  struct freqDcRtData_s *freqDcRtData;
};

struct freqDcReturnData_s
{
  uint32_t timerNumber;     // The timer number of the data
  uint32_t freqX1000;       // Frequency * 1000
  uint32_t dutyCycleX1000;  // Duty Cycle * 1000
};

//--------------------------------------------------------------------------
struct freqDcTimerInfo_s *meadow_calc_freq_dc_get_timer_info_pointer(int timerNumb);
int meadow_calc_freq_dc_mono_freq_duty_cycle(struct freqDcReturnData_s *returnData);

#if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)

int meadow_calc_freq_dc_freq_duty_config(int timerNumber,
          uint8_t pinDesignation, uint8_t gpioPolarity);
int meadow_calc_freq_dc_freq_duty_unconfig(uint32_t timerNumber);

void meadow_kt_frequency_dutycycle_tests(uint32_t userData);

#endif       // CONFIG_FREQUENCY_DUTYCYCLE_TESTS

#endif      // __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_DC__H
