/****************************************************************************
 * configs/stm32f777zit6-meadow/src/specialized/meadow_rotary_encoder.h
 * 
 *   Copyright (C) 2024-2025 Wilderness Labs. All rights reserved.
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

// Caller must populate this struct
struct rotenc_config_parms
{
  uint32_t encoderNumb;       // 0 - 7 to identify for collections
  bool isAddEncoder;          // true=add, false=remove
  uint32_t portA;             // 0 - 15 (A-K)
  uint32_t pinA;              // 0 - 15
  uint32_t portB;             // 0 - 15 (A-K)
  uint32_t pinB;              // 0 - 15
  uint32_t resistorMode;      // 0 = float, 1 = pull up, 2 = pull down
};

struct rotaryEncoderInfo_s
{
  uint8_t EncoderNumb;        // Encoder number

  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinInfoA;           // Supplied by configuration
  uint8_t PinInfoB;           // Supplied by configuration

  // Address of correct "Input Data Register" which holds GPIO's hardware port
  // state bits
  uint32_t IDRAddressA;       // Calculated during configuration
  uint32_t IDRAddressB;       // Calculated during configuration

  uint32_t prevCondBits;
  uint32_t prevOff;
  int abEdgeCount;            // Count of A and B, both edges
  bool rotClockWise;          // Current direction
};
typedef struct rotaryEncoderInfo_s rotaryEncoderInfo_t;

// Public functions
int meadow_config_rotary_encoder(struct rotenc_config_parms* rotencCfg);
int meadow_rotary_encoder_read_count(uint8_t encoderNumb, int *encoderCount, bool *rotClockWise);
int meadow_rotary_encoder_set_count(uint8_t encoderNumb, int encoderCount);

// Test function follow
void rotary_encoder_test_exercise_test(uint32_t userData);

#endif      // #if MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0

#endif      // __CONFIGS_MEADOW_SRC_MEADOW_ROTENC__H
