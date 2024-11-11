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

#if MEADOW_INCLUDE_FREQ_DUTY_CYCLE_TESTS_IN_BUILD > 0

#include <nuttx/config.h>
#include <string.h>
#include <stdint.h>
#include "stm32_gpio.h"   // stm32_configgpio

int meadow_timer_freq_duty_config(uint32_t timerNumber, uint32_t gpioPort,
          uint32_t gpioPin, uint32_t gpioPolarity);

// Test functions
void meadow_kt_frequency_dutycycle_tests(uint32_t userData);

#endif      // #if MEADOW_INCLUDE_FREQ_DUTY_CYCLE_TESTS_IN_BUILD > 0

#endif      // __CONFIGS_MEADOW_SPEC_MEADOW_FREQ_DC__H
