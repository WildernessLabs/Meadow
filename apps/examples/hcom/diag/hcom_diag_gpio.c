/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_gpio.c
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

// This module contains the code to allow GPIOs to be used for diagnistics
// This is only intended for low-level debugging

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include "hcom_diag_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// The following provide a thin layer of abstraction and some features to
// functions existing in hcom_via_nx_gpio_xxx. Plus the names are easier
// to remember...
void hcom_diag_gpio_config(uint32_t pin)
{
  // stm32_unconfiggpio(pin);
  stm32_configgpio(pin);  
  stm32_gpiowrite(pin, false);
}

//==========================================================
void hcom_diag_gpio_set_high(uint32_t pin)
{
  stm32_gpiowrite(pin, true);
}

//==========================================================
void hcom_diag_gpio_set_low(uint32_t pin)
{
  stm32_gpiowrite(pin, false);
}

//==========================================================
void hcom_diag_gpio_pulse(uint32_t pin, uint32_t usec)
{
  stm32_gpiowrite(pin, true);
  usleep(usec);
  stm32_gpiowrite(pin, false);
}
