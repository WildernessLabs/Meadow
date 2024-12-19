/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_measure_freq_shared.h
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
#ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_SHARE__H
#define __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_SHARE__H

#include <nuttx/config.h>
#include <stdint.h>

// Configuration options
#define MEADOW_MEAS_FREQ_CONF_OPTION_NO_DC    (1)
#define MEADOW_MEAS_FREQ_CONF_OPTION_WITH_DC  (2)
#define MEADOW_MEAS_FREQ_CONF_OPTION_UNCFG    (3)

// Define error values returned
#define MEADOW_MEAS_FREQ_CONF_SUCCESSFUL                (0)
#define MEADOW_MEAS_FREQ_CONF_UNDEFINED_OPTION          (-1)
#define MEADOW_MEAS_FREQ_CONF_TIM_NUMB_ILLEGAL          (-2)
#define MEADOW_MEAS_FREQ_CONF_CHAN_NUMB_ILLEGAL         (-3)
#define MEADOW_MEAS_FREQ_CONF_PORT_PIN_NOT_FOR_TIM      (-4)
#define MEADOW_MEAS_FREQ_CONF_PORT_PIN_TIM_CHAN_INVALID (-5)
#define MEADOW_MEAS_FREQ_CONF_TIM_NOT_USABLE            (-6)
#define MEADOW_MEAS_FREQ_CONF_TIM_CHAN_IN_USE           (-7)
#define MEADOW_MEAS_FREQ_CONF_CHAN_MEM_ALLOC_FAILED     (-8)
#define MEADOW_MEAS_FREQ_CONF_CONFIGGPIO_ERR            (-9)
#define MEADOW_MEAS_FREQ_CONF_INIT_CHAN_HW_FAIL         (-10)
#define MEADOW_MEAS_FREQ_CONF_INIT_TIM_HW_FAIL          (-11)

#define MEADOW_MEAS_FREQ_READ_SUCCESSFUL                (0)
#define MEADOW_MEAS_FREQ_READ_NO_CHANS_ACTIVITY         (-21)
#define MEADOW_MEAS_FREQ_READ_INVALID_TIMER_NUMB        (-22)
#define MEADOW_MEAS_FREQ_READ_INVALID_CHANNEL_NUMB      (-23)
#define MEADOW_MEAS_FREQ_READ_TIMER_ACCESS_NULL         (-24)
#define MEADOW_MEAS_FREQ_READ_CHAN_NOT_CONFIG           (-25)
#define MEADOW_MEAS_FREQ_READ_CHANNEL_DATA_NULL         (-26)
#define MEADOW_MEAS_FREQ_READ_NO_CHANS_ACTIVE           (-27)
#define MEADOW_MEAS_FREQ_READ_NO_INPUT_DETECTED         (-28)

#define MEADOW_MEAS_FREQ_UNCFG_SUCCESSFUL               (0)
#define MEADOW_MEAS_FREQ_UNCFG_INVALID_TIMER_NUMB       (-31)
#define MEADOW_MEAS_FREQ_UNCFG_INVALID_CHANNEL_NUMB     (-32)
#define MEADOW_MEAS_FREQ_UNCFG_TIMER_ACCESS_NULL        (-33)
#define MEADOW_MEAS_FREQ_UNCFG_CHAN_NOT_CONFIG          (-34)
#define MEADOW_MEAS_FREQ_UNCFG_NO_CHANNEL               (-35)
#define MEADOW_MEAS_FREQ_UNCFG_IRQ_DETACH_ERR           (-36)

struct mdwFreqCfgTimer_s
{
  uint32_t timerNumber;   // 1-14 (1 & 8 not supported)
  uint32_t channelNumber; // 1-4
  uint32_t portAndPin;    // bits 7:4 port 0=A, 1=B & bits 3:0 pin 0-15
  // 0 = illegal
  // 1 = Configure without Duty Cycle,
  // 2 = Configure with Duty Cycle (twice the interrupts),
  // 3 = Unconfigure
  uint32_t configOption;    // See above
};
typedef struct mdwFreqCfgTimer_s mdwFreqCfgTimer_t;

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

//--------------------------------------------------------------------------
int meadow_measure_freq_configure(mdwFreqCfgTimer_t *mdwCfgTimerChan);
int meadow_measure_freq_return_freq_info(mdwFreqReturnData_t *mdwFreqReturnData);

#endif  // #ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_SHARE__H