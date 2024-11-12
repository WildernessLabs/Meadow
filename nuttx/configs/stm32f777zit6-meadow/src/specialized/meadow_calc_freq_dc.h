/****************************************************************************
 * configs/stm32f777zit6-meadow/src/specialized/meadow_calc_freq_dc.h
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
// require to operate.

// This structure contains runtime data
struct freqDcData_s
{
  volatile uint8_t timerDectSync;     // Missing interrupt detection
  volatile uint32_t timerFullPeriod;  // Full period count
  volatile uint32_t timerPartPeriod;  // Part period count
  volatile uint32_t timerFullOvrFlo;  // Full overflow count
  volatile uint32_t timerPartOvrFlo;  // Part overflow count
  uint32_t gpioInputConfig;           // Nuttx style GPIO configuration
  uint8_t inputPolarity;              // 0 = leading is rising, 1 = leading is falling
};

// The freqDcTimerInfo_s contains information that is defined by the F7's internal
// hardware structure. Each field is populated at build time from the
// struct freqDcTimerInfo_s array.
struct freqDcTimerInfo_s
{
  uint8_t timerNumb   : 4;      // 0 - 15 timer number
  uint8_t timerWidth  : 1;      // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;      // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;      // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerFuture : 1;      // Future
  uint32_t timerBase;           // Unique for each timer
  uint32_t timerClkEn;          // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;         // Interrupt vector
  struct freqDcData_s *dataPtr; // Points to freqDcData_s for running timer
};

struct freqDcReturnData_s
{
  uint32_t timerNumber; // 1 - 14 timer number to use
  uint32_t dataField1;  // Frequency
  uint32_t dataField2;  // Duty Cycle
};

//--------------------------------------------------------------------------
struct freqDcTimerInfo_s *meadow_calc_freq_dc_get_timer_info_pointer(int timerNumb);

#if defined(CONFIG_FREQUENCY_DUTYCYCLE_TESTS)

int meadow_calc_freq_dc_freq_duty_config(uint32_t timerNumber,
          uint32_t pinDesignation, uint32_t gpioPolarity);
int meadow_calc_freq_dc_freq_duty_unconfig(uint32_t timerNumber);

void meadow_kt_frequency_dutycycle_tests(uint32_t userData);

#endif       // CONFIG_FREQUENCY_DUTYCYCLE_TESTS

#endif      // __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_DC__H
