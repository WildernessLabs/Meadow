/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_interrupt.h
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
 *  ****************************************************************************/
#ifndef __CONFIGS_MEADOW_SRC_MEADOW_INTERRUPT__H
#define __CONFIGS_MEADOW_SRC_MEADOW_INTERRUPT__H

#define MINT_MSG_QUEUE_NAME           "/mdw_int"
#define MINT_MSG_QUEUE_MAX_MSGS       16

// This #define was added so timestamp code could be added without changing
// the current behavior and do refactoring.
// ONLY SET THIS TO 1 IF MEADOW.CORE HAS BEEN MODIFIED TO EXPECT TIME
// Remove after Managed code has been updated`
#define MEADOW_INTERRUPT_INCLUDE_TIME_STAMP (0)

// This struct is sent from Meadow.Core to configure or remove an interrupt.
struct mint_gpio_int_config
{
  // Must match ...\Meadow\Meadow.Core\source\Meadow.Core\Interop\Interop.upd.cs
  uint32_t port;                // 0 - 15 (A-K)
  uint32_t pin;                 // 0 - 15
  uint32_t configType;          // 0=remove, 1=new, 2=lp wakeup
  uint32_t risingEdge;          // 1 = enable
  uint32_t fallingEdge;         // 1 = enable
  uint32_t resistorMode;        // 0 = float, 1 = pull up, 2 = pull down
  uint32_t debounceDuration;    // millisec * 10
  uint32_t glitchDuration;      // millisec * 10
};

// This struct is sent to Meadow.Core when an interrupt occurs
struct mint_send_int_core_s
{
  uint8_t gpioPinId;
  uint8_t gpioState;
#if (MEADOW_INTERRUPT_INCLUDE_TIME_STAMP > 0)
  clock_t interruptTicks;
#endif
} __attribute__((__packed__));
typedef struct mint_send_int_core_s mint_send_int_core_t;

#define MEADOW_INTERRUPT_MQ_MSG_SIZE sizeof(mint_send_int_core_t)

enum GPIOInterruptCfgType_e
{
  gpio_intrpt_cfg_type_remove = 0,
  gpio_intrpt_cfg_type_new = 1,
  gpio_intrpt_cfg_type_wakeup = 2
};

int meadow_interrupt_setup(void);

// Called from meadow-upd.c
int mint_config_interrupt(struct mint_gpio_int_config* cfg);

#endif // __CONFIGS_MEADOW_SRC_MEADOW_INTERRUPT__H