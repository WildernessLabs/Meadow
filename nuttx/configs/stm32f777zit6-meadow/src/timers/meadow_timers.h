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


// TO BE REMOVED ONCE GPIO INPUTS ARE DEFINED IN EACH FEATURE
#define MEADOW_TIMER_NUMBER_EXPERIMENTAL (4)

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
