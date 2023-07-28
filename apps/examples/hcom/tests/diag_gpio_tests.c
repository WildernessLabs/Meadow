/****************************************************************************
 * \apps\examples\hcom\tests\diag_gpio_tests.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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

#include <meadow/meadow_hw_version.h>
#include "../hcom_common.h"
#include "../diag/hcom_diag_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
// static char *thisFile = __FILE__;

  uint32_t f7v1GpioPinMap[] = 
  {
    DEBUG_PIN_V1_A0,
    DEBUG_PIN_V1_A1,
    DEBUG_PIN_V1_A2,
    DEBUG_PIN_V1_A3,
    DEBUG_PIN_V1_A4,
    DEBUG_PIN_V1_A5,
    DEBUG_PIN_V1_SCK,
    DEBUG_PIN_V1_COPI,
    DEBUG_PIN_V1_CIPO,
    DEBUG_PIN_V1_D00,
    DEBUG_PIN_V1_D01,
    DEBUG_PIN_V1_D02,
    DEBUG_PIN_V1_D03,
    DEBUG_PIN_V1_D04,
    DEBUG_PIN_V1_D05,
    DEBUG_PIN_V1_D06,
    DEBUG_PIN_V1_D07,
    DEBUG_PIN_V1_D08,
    DEBUG_PIN_V1_D09,
    DEBUG_PIN_V1_D10,
    DEBUG_PIN_V1_D11,
    DEBUG_PIN_V1_D12,
    DEBUG_PIN_V1_D13,
    DEBUG_PIN_V1_D14,
    DEBUG_PIN_V1_D15,
    DEBUG_PIN_V1_RED_LED,
    DEBUG_PIN_V1_GREEN_LED,
    DEBUG_PIN_V1_BLUE_LED,
  };

  uint32_t f7v2GpioPinMap[] = 
  {
    DEBUG_PIN_V2_A0,
    DEBUG_PIN_V2_A1,
    DEBUG_PIN_V2_A2,
    DEBUG_PIN_V2_A3,
    DEBUG_PIN_V2_A4,
    DEBUG_PIN_V2_A5,
    DEBUG_PIN_V2_SCK,
    DEBUG_PIN_V2_COPI,
    DEBUG_PIN_V2_CIPO,
    DEBUG_PIN_V2_D00,
    DEBUG_PIN_V2_D01,
    DEBUG_PIN_V2_D02,
    DEBUG_PIN_V2_D03,
    DEBUG_PIN_V2_D04,
    DEBUG_PIN_V2_D05,
    DEBUG_PIN_V2_D06,
    DEBUG_PIN_V2_D07,
    DEBUG_PIN_V2_D08,
    DEBUG_PIN_V2_D09,
    DEBUG_PIN_V2_D10,
    DEBUG_PIN_V2_D11,
    DEBUG_PIN_V2_D12,
    DEBUG_PIN_V2_D13,
    DEBUG_PIN_V2_D14,
    DEBUG_PIN_V2_D15,
    DEBUG_PIN_V2_RED_LED,
    DEBUG_PIN_V2_GREEN_LED,
    DEBUG_PIN_V2_BLUE_LED,
  };

#define HCOM_DIAG_GPIO_TESTS_GPIO_COUNT (sizeof(f7v2GpioPinMap) / sizeof(uint32_t))
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// userData contains the starting GPIO with A0 - D15 skipping D12 & D13 as this
// it used for syslog output.
void hcom_meadow_diag_gpio_tests(uint32_t userData)
{
  uint32_t gpioPinDefn;
  uint32_t hwVer;

  if(userData == 0 || userData > HCOM_DIAG_GPIO_TESTS_GPIO_COUNT)
  {
    syslog(2, "AP-userData:%d is out of range. Try 1-%d\n",
              userData, HCOM_DIAG_GPIO_TESTS_GPIO_COUNT);
    return;
  }

  hwVer = hcom_via_nx_get_hw_version();
  for(int led = userData - 1; led < userData + 9; led++)
  {
    if(hwVer == MEADOW_F7_HW_VERSION_NUMB_F7V1)
      gpioPinDefn = f7v1GpioPinMap[led];
    else
      gpioPinDefn = f7v2GpioPinMap[led];

    hcom_diag_gpio_set_high(gpioPinDefn);
    usleep(250 * 1000);
    hcom_diag_gpio_set_low(gpioPinDefn);
    usleep(250 * 1000);
  }
  
  // Test 10 leds
  for(int led = userData - 1; led < userData + 9; led++)
  {
    syslog(2, "AP-Testing GPIO:%d on F7v%d\n", led, hwVer);
    
    // Don't go past the last led
    if(led > HCOM_DIAG_GPIO_TESTS_GPIO_COUNT - 1)
      continue;
      
    if(hwVer == MEADOW_F7_HW_VERSION_NUMB_F7V1)
      gpioPinDefn = f7v1GpioPinMap[led];
    else
      gpioPinDefn = f7v2GpioPinMap[led];

    // Configure
    hcom_diag_gpio_config(gpioPinDefn);

    // Turn-on & off twice
    hcom_diag_gpio_set_high(gpioPinDefn);
    usleep(250 * 1000);
    hcom_diag_gpio_set_low(gpioPinDefn);
    usleep(250 * 1000);
    hcom_diag_gpio_set_high(gpioPinDefn);
    usleep(250 * 1000);
    hcom_diag_gpio_set_low(gpioPinDefn);
    usleep(250 * 1000);

    // pulse
    for(int i = 0; i < 30; i++)
    {
      hcom_diag_gpio_pulse(gpioPinDefn, 50 * 1000);
      usleep(50 * 1000);
    }
  }
}
