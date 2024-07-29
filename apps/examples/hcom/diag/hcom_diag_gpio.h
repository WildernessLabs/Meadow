/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_gpio.h
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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
#if defined(CONFIG_GPIO_TESTS)

#ifndef __CONFIGS_MEADOW_SRC_HCOM_DIAG_GPIO__H
#define __CONFIGS_MEADOW_SRC_HCOM_DIAG_GPIO__H

#include <meadow/hcom_shared_common.h>

  // These functions provide access to the common gpio functions stm32_configgpio
  // and stm32_gpiowrite.
  void hcom_diag_gpio_config(uint32_t pin);
  void hcom_diag_gpio_set_high(uint32_t pin);
  void hcom_diag_gpio_set_low(uint32_t pin);  
  void hcom_diag_gpio_pulse(uint32_t pin, uint32_t usec);

  void hcom_diag_gpio_config_alt(int alt_access_fd, uint32_t pin);
  void hcom_diag_gpio_set_high_alt(int alt_access_fd, uint32_t pin);
  void hcom_diag_gpio_set_low_alt(int alt_access_fd, uint32_t pin);
  void hcom_diag_gpio_set_pulse_alt(int alt_access_fd, uint32_t pin, uint32_t usec);

// The following #defines are derived from #defines like
// #define DEBUG_PIN_V1_A0 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN4)
// and can therefore be used to configure and control gpio pines for diagnostic
// purposes. This is necessary because the C precompiler cannot access the
// needed STM32F7 header files for code within apps.
// Note:last 2 hex characters are port and ping (0 is port A, etc.)
#define DEBUG_PIN_V1_A0   (0x00040c04)
#define DEBUG_PIN_V1_A1   (0x00040c05)
#define DEBUG_PIN_V1_A2   (0x00040c03)
#define DEBUG_PIN_V1_A3   (0x00040c07)
#define DEBUG_PIN_V1_A4   (0x00040c20)
#define DEBUG_PIN_V1_A5   (0x00040c21)
#define DEBUG_PIN_V1_SCK  (0x00040c2a)
#define DEBUG_PIN_V1_COPI (0x00040c15)
#define DEBUG_PIN_V1_CIPO (0x00040c2b)
#define DEBUG_PIN_V1_D00  (0x00040c89)
#define DEBUG_PIN_V1_D01  (0x00040c7d)
#define DEBUG_PIN_V1_D02  (0x00040c26)
#define DEBUG_PIN_V1_D03  (0x00040c18)
#define DEBUG_PIN_V1_D04  (0x00040c19)
#define DEBUG_PIN_V1_D05  (0x00040c27)
#define DEBUG_PIN_V1_D06  (0x00040c10)
#define DEBUG_PIN_V1_D07  (0x00040c17)
#define DEBUG_PIN_V1_D08  (0x00040c16)
#define DEBUG_PIN_V1_D09  (0x00040c11)
#define DEBUG_PIN_V1_D10  (0x00040c7a)
#define DEBUG_PIN_V1_D11  (0x00040c29)
#define DEBUG_PIN_V1_D12  (0x00040c1e)
#define DEBUG_PIN_V1_D13  (0x00040c1f)
#define DEBUG_PIN_V1_D14  (0x00040c63)
#define DEBUG_PIN_V1_D15  (0x00040c43)
#define DEBUG_PIN_V1_RED_LED (0x00040c02)
#define DEBUG_PIN_V1_GREEN_LED (0x00040c01)
#define DEBUG_PIN_V1_BLUE_LED (0x00040c00)

#define DEBUG_PIN_V2_A0   (0x00040c04)
#define DEBUG_PIN_V2_A1   (0x00040c05)
#define DEBUG_PIN_V2_A2   (0x00040c03)
#define DEBUG_PIN_V2_A3   (0x00040c10)
#define DEBUG_PIN_V2_A4   (0x00040c11)
#define DEBUG_PIN_V2_A5   (0x00040c20)
#define DEBUG_PIN_V2_SCK  (0x00040c2a)
#define DEBUG_PIN_V2_COPI (0x00040c15)
#define DEBUG_PIN_V2_CIPO (0x00040c2b)
#define DEBUG_PIN_V2_D00  (0x00040c89)
#define DEBUG_PIN_V2_D01  (0x00040c7d)
#define DEBUG_PIN_V2_D02  (0x00040c7a)
#define DEBUG_PIN_V2_D03  (0x00040c18)
#define DEBUG_PIN_V2_D04  (0x00040c19)
#define DEBUG_PIN_V2_D05  (0x00040c14)
#define DEBUG_PIN_V2_D06  (0x00040c1d)
#define DEBUG_PIN_V2_D07  (0x00040c17)
#define DEBUG_PIN_V2_D08  (0x00040c16)
#define DEBUG_PIN_V2_D09  (0x00040c26)
#define DEBUG_PIN_V2_D10  (0x00040c27)
#define DEBUG_PIN_V2_D11  (0x00040c29)
#define DEBUG_PIN_V2_D12  (0x00040c1e)
#define DEBUG_PIN_V2_D13  (0x00040c1f)
#define DEBUG_PIN_V2_D14  (0x00040c1c)
#define DEBUG_PIN_V2_D15  (0x00040c6c)
#define DEBUG_PIN_V2_RED_LED (0x00040c02)
#define DEBUG_PIN_V2_GREEN_LED (0x00040c01)
#define DEBUG_PIN_V2_BLUE_LED (0x00040c00)

#endif //__CONFIGS_MEADOW_SRC_HCOM_DIAG_GPIO__H

#endif
