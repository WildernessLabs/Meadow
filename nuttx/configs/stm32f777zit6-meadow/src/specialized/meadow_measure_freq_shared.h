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

// This structure defines the API for interacting with the Meadow Measure
// Frequency code.
// // The fields 'rqstAction', 'timerNumber' and 'channelNumber' are always
// //   required.
// // The 'rqstAction' field can have the following values
// //  1 = Configure with Duty Cycle (reduces interrupts by 50%)
// //  2 = Configure without Duty Cycle
// //  3 = Unconfigure
// //  4 = Return measurement information (i.e. last 4 fields)
// // 'portAndPin' are only required for configuration (#1 & #2)
// // Last 4 fields are readonly
// struct mdwMeasureFreqApi_s
// {
//   uint32_t rqstAction;        // What it the requested action
//   uint32_t timerNumber;       // 1-14 (1 & 8 not supported)
//   uint32_t channelNumber;     // 1-4
//   uint32_t portAndPin;        // bits 7:4 port 0=A, 1=B,... bits 3-0 pin 0-15
//   uint32_t frequencyX1000;    // Frequency * 1000
//   uint32_t dutyCycleX1000;    // Duty Cycle * 1000
//   uint32_t avgFreqX1000;      // Average frequency * 1000
//   uint32_t gpioCountForAvg;   // Number of transitions since list read
// };
// typedef struct mdwMeasureFreqApi_s mdwMeasureFreqApi_t;

struct mdwCfgTimerChan_s
{
  uint32_t timerNumber;   // 1-14 (1 & 8 not supported)
  uint32_t channelNumber; // 1-4
  uint32_t portAndPin;    // bits 7:4 port 0=A, 1=B & bits 3:0 pin 0-15
  // 0 = illegal
  // 1 = Configure with Duty Cycle,
  // 2 = Configure without Duty Cycle (reduces interrupts by 50%),
  // 3 = Unconfigure
  uint32_t configFreq;    // See above
};
typedef struct mdwCfgTimerChan_s mdwCfgTimerChan_t;

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

#endif  // #ifndef __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_SHARE__H