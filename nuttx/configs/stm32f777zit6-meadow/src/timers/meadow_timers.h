/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_timer_support.c
 * 
 *   Copyright (C) 2020, 2021 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

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

// PeterM - still needed?
#include <nuttx/kthread.h>
#include <meadow/meadow_hw_version.h>

//===================================================================
#ifndef __INCLUDE_MEADOW_TIMER__H
#define __INCLUDE_MEADOW_TIMER__H

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

#define MEADOW_TIMER_EXPERIMENT_THREAD_NAME "TimerTest"
#define MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY 120
#define MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE 2048

// For reasons I don't fully understand the counts are always 2 less that they
// should be. This may be first and last counts or something like this.
#define MEADOW_TIMER_CORRECTION_COUNT (0)

// This determines the timers clock speed
#define MEADOW_TIMER_PRESCALER_CLK_DIV (1)

#define MEADOW_TIMERS_MINIMUM_USABLE_CNT (180)

#define MEADOW_TIMER_16_BIT_OVERFLOW (65536)

// There are 14 timers in the stm32f777
#define MEADOW_TIMERS_NUMB_OF_TIMERS (14)

//--------------------------------------------------------------------------
// ONLY F7v2 for testing
#define MEADOW_TIMER_TEST_GPIO_D14_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTB | GPIO_PIN12)
#define MEADOW_TIMER_TEST_GPIO_D15_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTG | GPIO_PIN12)

// Input points to TIMx_CHx
// Note the alternate function entries are non-optional and vary with different timer/channels
// #define MEADOW_F7V1_TIM5_CH1_PH10_D10  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTH | GPIO_PIN10)
#define MEADOW_F7VX_TIM4_CH1_PB6_D08  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN6)
#define MEADOW_F7V2_TIM5_CH1_PH10_D02  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTH | GPIO_PIN10)

// #define MEADOW_F7V1_TIM8_CH1_PC6_D02   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTC | GPIO_PIN6)
#define MEADOW_F7V2_TIM8_CH1_PC6_D09   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTC | GPIO_PIN6)

#define MEADOW_F7VX_TIM10_CH1_PB8_D03   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN8)
#define MEADOW_F7VX_TIM11_CH1_PB9_D04   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN9)

// DURING DEVELOPMENT ONLY F7v2 is supported
// This is used for testing. Make sure MEADOW_TIMER_CHANNEL_IN_USE matches the
// the timer channel we expect to use, based on the GPIO selected

#define MEADOW_TIMER_CHANNEL_IN_USE (1)
#define MEADOW_TIMER_NUMBER_EXPERIMENTAL (4)

#if MEADOW_TIMER_NUMBER_EXPERIMENTAL == 4
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM4_CH1_PB6_D08)
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 5
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM5_CH1_PH10_D02)
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 8
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM8_CH1_PC6_D09)
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 10
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM10_CH1_PB8_D03)
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 11
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM11_CH1_PB9_D04)
#else
#error Unsupported Timer Number
#endif

// Meadow A0-A5 configured as digital output ports for DEBUGGING
#define MEADOW_DEBUG_PIN_V2_A0   (0x00040c04)
#define MEADOW_DEBUG_PIN_V2_A1   (0x00040c05)
#define MEADOW_DEBUG_PIN_V2_A2   (0x00040c03)
#define MEADOW_DEBUG_PIN_V2_A3   (0x00040c10)
#define MEADOW_DEBUG_PIN_V2_A4   (0x00040c11)
#define MEADOW_DEBUG_PIN_V2_A5   (0x00040c20)

//=====================================================================
// PeterM - There are static and dynamic fields can the static ones be removed
// from the dyanamic ones?
struct timerInfo_s
{
  // Timer base address
  uint8_t timerNumb;                  // For diagnostics
  volatile uint8_t timerWidth;        // Either 16 or 32 bit wide (replace with func bit)
  volatile uint8_t timerDectSync;     // FDc - CCR1 interrupt missing
  volatile uint8_t timerDEBUG;      // FDc - TBD
  volatile bool timerCapCCR2OvFl;     // FDc - Should CCR2 collect the CNT overflows
  volatile uint32_t timerCount1;      // Primary value of the count
  volatile uint32_t timerCount2;      // Secondary value of the count
  volatile uint32_t timerExtra1;      // Extra information 1
  volatile uint32_t timerExtra2;      // Extra information 2
  volatile uint32_t timerFreq;        // Running timer clock frequency (could be prescaler value)
  uint32_t timerFunc;                 // Bit fields with the functions this timer has and can perform
  uint32_t timerBase;                 // Unique for each timer
  uint32_t timerMaxClk;               // Either 192MHz or 96MHz (replace with func bit)
  uint32_t timerAPBClk;               // Proper APB clock register for timer enable bit field (replace with func bit)
  uint32_t timerClkEn;                // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;               // Interrupt vector
};

//=====================================================================
// Public functions
int meadow_timer_setup_pulse_width(struct timerInfo_s *timerData);
int meadow_timer_setup_freq_duty(struct timerInfo_s *timerData);
int meadow_timer_setup_idle_detect(struct timerInfo_s *timerData);

void meadow_timer_enable(struct timerInfo_s *timerInfo);
void meadow_timer_disable(struct timerInfo_s *timerInfo);

int meadow_timer_isr_pulse_width(int irq, void *context, void *arg);
int meadow_timer_init_gated_pulse_width(struct timerInfo_s *timerInfo);
int meadow_timer_test_gated_pulse_width(struct timerInfo_s *timerInfo);

int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg);
int meadow_timer_init_freq_and_dutycycle(struct timerInfo_s *timerInfo);
int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo);

int meadow_timer_isr_idle_measure(int irq, void *context, void *arg);
int meadow_timer_init_idle_measure(struct timerInfo_s *timerInfo);
int meadow_timer_test_idle_measure(struct timerInfo_s *timerInfo);

#endif // __INCLUDE_MEADOW_TIMER__H