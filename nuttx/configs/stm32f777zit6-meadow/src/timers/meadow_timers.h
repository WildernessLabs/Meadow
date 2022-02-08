/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/meadow_timers.h
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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

#define MEADOW_TIMER_16_BIT_OVERFLOW (65536)

#define MEADOW_TIMER_WIDTH_16 (0)
#define MEADOW_TIMER_WIDTH_32 (1)

//--------------------------------------------------------------------------
// ONLY F7v2 for testing
#define MEADOW_TIMER_TEST_GPIO_D14_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTB | GPIO_PIN12)
#define MEADOW_TIMER_TEST_GPIO_D15_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTG | GPIO_PIN12)


// Meadow A0-A5 configured as digital output ports for DEBUGGING
#define MEADOW_DEBUG_PIN_V2_A0   (0x00040c04)
#define MEADOW_DEBUG_PIN_V2_A1   (0x00040c05)
#define MEADOW_DEBUG_PIN_V2_A2   (0x00040c03)
#define MEADOW_DEBUG_PIN_V2_A3   (0x00040c10)
#define MEADOW_DEBUG_PIN_V2_A4   (0x00040c11)
#define MEADOW_DEBUG_PIN_V2_A5   (0x00040c20)

// Part of the GPIO input configuration, need Alt Func, Port and Pin
#define MEADOW_TIMER_GPIO_CONST (GPIO_ALT | GPIO_INPUT | GPIO_PULLDOWN)

// THESE NEED TO BE REMOVED ONCE GPIO INPUTS ARE DEFINED IN EACH FEATURE
// Input points to TIMx_CHx
// Note the alternate function entries are non-optional and vary with each
// timer/channels
// F7v1
#define MEADOW_F7V1_TIM5_CH1_PH10_D10  (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTH | GPIO_PIN10)
#define MEADOW_F7V1_TIM8_CH1_PC6_D02   (MEADOW_TIMER_GPIO_CONST | GPIO_AF3 | GPIO_PORTC | GPIO_PIN6)

// F7v2
#define MEADOW_F7V2_TIM5_CH1_PH10_D02  (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTH | GPIO_PIN10)
#define MEADOW_F7V2_TIM8_CH1_PC6_D09   (MEADOW_TIMER_GPIO_CONST | GPIO_AF3 | GPIO_PORTC | GPIO_PIN6)

// F7v1 & F7v2
#define MEADOW_F7vX_TIM10_CH1_PB8_D03  (MEADOW_TIMER_GPIO_CONST | GPIO_AF3 | GPIO_PORTB | GPIO_PIN8)
#define MEADOW_F7vX_TIM11_CH1_PB9_D04  (MEADOW_TIMER_GPIO_CONST | GPIO_AF3 | GPIO_PORTB | GPIO_PIN9)
// All Timer4 inputs
#define MEADOW_F7vX_TIM4_CH1_PB6_D08   (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTB | GPIO_PIN6)
#define MEADOW_F7vX_TIM4_CH2_PB7_D07   (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTB | GPIO_PIN7)
#define MEADOW_F7vX_TIM4_CH3_PB8_D03   (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTB | GPIO_PIN8)
#define MEADOW_F7vX_TIM4_CH4_PB9_D04   (MEADOW_TIMER_GPIO_CONST | GPIO_AF2 | GPIO_PORTB | GPIO_PIN9)

// DURING DEVELOPMENT ONLY F7v2 is supported
// This is used for testing. Make sure MEADOW_TIMER_CHANNEL_BEING_USED matches the
// the timer channel we expect to use, based on the GPIO selected

#define MEADOW_TIMER_CHANNEL_BEING_USED (1)

// Timers not listed:
// Timer 3 is 16-bit, 2 GPIO, 96MHz
// Timer 9 is 16-bit, 1 GPIO, 192MHz
// Timers 6 & 7 have not GPIO
// Timer 12 is 16-bit, 2 GPIO, 96MHz (GPIO pins used for syslog output)

// Pick from a timer from the following list
#define MEADOW_TIMER_NUMBER_EXPERIMENTAL (4)

/* Timer 4 is 16-bit, 4 GPIO, 96MHz */
#if MEADOW_TIMER_NUMBER_EXPERIMENTAL == 4
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7vX_TIM4_CH1_PB6_D08)

/* Timer 5 is the only 32-bit, 1 GPIO, 96MHz */
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 5
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM5_CH1_PH10_D02)

/* Timer 8 is 16-bit, 3 GPIO, 192MHz */
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 8
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM8_CH1_PC6_D09)

/* Timer 10 16-bit, 1 GPIO, 192MHz. Currently used for Glitch filtering */
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 10
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7vX_TIM10_CH1_PB8_D03)

/* Timer 11` 16-bit, 1 GPIO, 192MHz. */
#elif MEADOW_TIMER_NUMBER_EXPERIMENTAL == 11
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7vX_TIM11_CH1_PB9_D04)
#else
#error Unsupported Timer Number
#endif

//=====================================================================
// Public functions
int meadow_timer_setup_pulse_width(int timerNumber);
int meadow_timer_init_gated_pulse_width(int timerNumber);
int meadow_timer_test_gated_pulse_width(int timerNumber);

int meadow_timer_setup_freq_duty(int timerNumber);
int meadow_timer_init_freq_and_dutycycle(int timerNumber);
int meadow_timer_test_freq_and_dutycycle(int timerNumber);

int meadow_timer_setup_rc_servo_decode(int timerNumber);
int meadow_timer_init_rc_servo_decode(int timerNumber);
int meadow_timer_test_rc_servo_decode(int timerNumber);

int meadow_timer_setup_idle_detect(void);
int meadow_timer_init_idle_measure(void);
int meadow_timer_test_idle_measure(void);


#endif // __INCLUDE_MEADOW_TIMER__H
