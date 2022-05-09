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

#include <nuttx/kthread.h>
// PeterM - still needed?
#include <meadow/meadow_hw_version.h>
#include <meadow/hcom_shared_common.h>

//===================================================================
#ifndef __INCLUDE_MEADOW_TIMER__H
#define __INCLUDE_MEADOW_TIMER__H

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)
//===================================================================

#define MEADOW_TIMER_EXPERIMENT_THREAD_NAME "TimerTest"
#define MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY 120
#define MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE 2048

#define MEADOW_TIMER_16_BIT_OVERFLOW (65536)

#define MEADOW_TIMER_WIDTH_16 (0)
#define MEADOW_TIMER_WIDTH_32 (1)

#define MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE (7)

#define MEADOW_TIMER_BAD_GPIO_VALUE (0xffffffff)

// Several timer features require a GPIO input configuration. This provides
// the basic information, only additional thing is which Alternate Function
// and this comes from the gpio table.
// Port and Pin as a minimum
#define MEADOW_TIMER_GPIO_CONST (GPIO_ALT | GPIO_INPUT | GPIO_PULLDOWN)

// There is a timer settup that is intended to test the frequency of the LSI
// internal clock. This may never be needed again but the code has been left
// in the event it is needed or the LSE clock needs to be tested.
#define MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD (0)

#if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0
  // Set to '1' to measure the internal LSI clock. Set to '0' to measure
  // a "clock" signal applied to GPIO F7v2's A02 pin (PA3). This is for
  // insuring that the LSI measurement code is working correctly.
  #define MEADOW_MEASURE_LSI_CLOCK_NOT_THE_GPIO_PA3_INPUT (1)
#endif

//=====================================================
// This enumeration and configuration structure is used to assign timers
// to features and specify the polarity of the input.
enum meadow_timer_usage_config
{
  Undefined = 0,
  PulseWidth = 1,
  FreqDutyCycle = 2,
  RcServoDecode = 3,
  MeadowOsTicks = 4,
  CpuLoadValue = 5,
#if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0
  LsiClkMeasure = 6   // Single purpose
#endif
};

//=====================================================
// The following structure is used to return data to the managed side
struct timerReturnData_s
{
  uint32_t timerNumber; // 1 - 14 timer number to use
  uint32_t timerUsage;  // 1=pulse width, 2=freq+duty cycle, 3=rc servo decode
  uint32_t dataField1;  // Pulse width, Frequency and RC Servo channel 1
  uint32_t dataField2;  // Duty Cycle and RC Servo channel 2
  uint32_t dataField3;  // RC Servo channel 3
  uint32_t dataField4;  // RC Servo channel 4
};

//=====================================================
// This is the structure that's used to define a configuration. Could use a
// union to reduce the size, but why?.
struct timerConfig_s
{
  uint8_t timerNumber;      // 1 - 14 timer number to use
  uint8_t timerUsage;       // 1=pulse width, 2=freq+duty cycle, 3=rc servo decode
  uint16_t pwTimeroutMs;    // Pulse Width only-How long to wait for pulse? Default 1000.
  uint8_t pwHCSR04Filter;   // Pulse Width only-0 = don't use HC-SR04 glitch filter, 1 = do use it
  uint8_t polarityChan1;    // All, 0 = leading is rising, 1 = leading is falling
  uint8_t polarityChan2;    // RC Servo only
  uint8_t polarityChan3;    // RC Servo only
  uint8_t polarityChan4;    // RC Servo only
};

//=====================================================
// This internal structure contains the data that all timer applications
// require to operate.
struct timerInfo_s
{
  uint8_t timerNumb   : 4;    // 0 - 15 timer number
  uint8_t timerWidth  : 1;    // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;    // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;    // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerFuture : 1;    // Future
  uint32_t timerBase;         // Unique for each timer
  uint32_t timerClkEn;        // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;       // Interrupt vector
  void *dataPtr;              // Points to usage specific data
};

//=====================================================
// GPIOs are in there own table due to the need to change GPIO definitions 
// based on the F7 version number. Hopefully, if there's additional versions
// this will simplify the effort
struct timerGpio_s
{
  // In Nuttx pin is bits 3:0, port bits 7:4 (one byte) and Alt Func 15:12
  uint8_t timerF7v1Gpio[4];  // GPIO for each timer channel
  uint8_t timerF7v2Gpio[4];  // GPIO for each timer channel
  uint16_t timerAltFunc;     // GPIO Alternate Function for each timer
};

//=====================================================================
// Public functions

// Used are by mono to get the results
int meadow_timer_configuration(struct timerConfig_s timerConfig);
int meadow_timer_mono_rc_servo_decode(struct timerReturnData_s *returnData);
int meadow_timer_mono_freq_duty_cycle(struct timerReturnData_s *returnData);
int meadow_timer_mono_pulse_width(struct timerReturnData_s *returnData);
int meadow_timer_mono_ticks_from_start(struct timerReturnData_s *returnData);
int meadow_timer_mono_current_cpu_load(struct timerReturnData_s *returnData);

// Used internally, implemented in timer_manager.c and shared by the timer
// features
struct timerInfo_s * meadow_timer_get_timer_info_pointer(int timerNumb);
uint32_t meadow_timer_get_ver_based_gpio_timer(int timerNumb);
uint32_t meadow_timer_get_ver_based_gpio_chan(int timerNumb, int channelOffset);
uint32_t meadow_timer_get_apb_clock(struct timerInfo_s *timerInfo);
uint32_t meadow_timer_get_max_clock(struct timerInfo_s *timerInfo);
void meadow_timer_disable(uint32_t timerBase);
void meadow_timer_enable(uint32_t timerBase);

// Called with configuration information from mono and timer_manager
// for testing
int meadow_timer_setup_pulse_width(struct timerConfig_s);
int meadow_timer_setup_freq_duty(struct timerConfig_s);
int meadow_timer_setup_rc_servo_decode(struct timerConfig_s);
#if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0
int meadow_timer_setup_lsi_clock(struct timerConfig_s);
#endif
// No configuration needed here.
int meadow_timer_cpu_measure_setup(void);

#if MEADOW_INCLUDE_TIMER_HARDWARE_TESTS_IN_BUILD > 0
// Only called from timer_manage for testing.
int meadow_timer_test_gated_pulse_width(int timerNumber);
int meadow_timer_test_freq_and_dutycycle(int timerNumber);
int meadow_timer_test_rc_servo_decode(int timerNumber);
#if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0
int meadow_timer_test_lsi_clock(int timerNumber);
#endif
int meadow_timer_test_cpu_measure_ticks(void);
int meadow_timer_test_cpu_cpu_load(void);
#endif  // #if MEADOW_INCLUDE_TIMER_HARDWARE_TESTS_IN_BUILD > 0

#endif  // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)

#endif // __INCLUDE_MEADOW_TIMER__H
