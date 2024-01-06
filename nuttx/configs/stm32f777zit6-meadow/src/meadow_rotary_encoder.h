/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow_rotary_encoder.h
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
#ifndef __CONFIGS_MEADOW_SRC_MEADOW_ROTENC__H
#define __CONFIGS_MEADOW_SRC_MEADOW_ROTENC__H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <meadow/hcom_shared_common.h>

#if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0

#include <nuttx/config.h>
#include <string.h>
#include <stdint.h>
#include "stm32_gpio.h"   // stm32_configgpio

// Caller must populate this
struct rotenc_gpio_config
{
  uint32_t portA;                // 0 - 15 (A-K)
  uint32_t pinA;                 // 0 - 15
  uint32_t portB;                // 0 - 15 (A-K)
  uint32_t pinB;                 // 0 - 15
  uint32_t configType;          // 0=remove, 1-n=new
  uint32_t resistorMode;        // 0 = float, 1 = pull up, 2 = pull down
};

int rotenc_config_interrupt(struct rotenc_gpio_config* rotencCfg);

// DEFINE HERE UNTIL ROTARY ENCODER INFRASTRUCTOR IS COMPLETE
// THIS ALLOWS quick_misc_tests to call into rotenc test to execute the test.
void quick_misc_test_exercise_rotary_encoder_test(void);

#endif      // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0

#endif      // __CONFIGS_MEADOW_SRC_MEADOW_ROTENC__H
