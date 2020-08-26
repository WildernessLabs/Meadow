/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_gpio.c
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

// This module contains the code to allow GPIOs to be used for diagnistics
// This is only intended for low-level debugging

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_diag_gpio_setup()
{
  int ret;
  ret = hcom_diag_gpio_config_all_as_output();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_diag_gpio_config_all_as_output, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }
  return ret;
}

//================================================================
// Configure the diagnostic GPIOs
int hcom_diag_gpio_config_all_as_output()
{
  int ret;

  // Configure the first 9 GPIO as digital output.
  for(int gpioOffset = HCOM_DIAG_GPIO_A0;
    gpioOffset <= HCOM_DIAG_GPIO_D15; gpioOffset++)
  {
#if HCOM_DIAG_GPIO_DIAGNOSTIC_PERSERVE_UARTS > 0
    if(gpioOffset == HCOM_DIAG_GPIO_D00 || gpioOffset == HCOM_DIAG_GPIO_D01 ||
       gpioOffset == HCOM_DIAG_GPIO_D12 || gpioOffset == HCOM_DIAG_GPIO_D13)
      continue;
#endif
    ret = hcom_nx_diag_gpio_config(gpioOffset, HCOM_GPIO_DIGITAL_CONFIG_OUTPUT);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
      break;
    }
  }

  // // Turn all on
  // for(int gpioOffset = HCOM_DIAG_GPIO_A0;
  //   gpioOffset <= HCOM_DIAG_GPIO_D15; gpioOffset++)
  // {
// #if HCOM_DIAG_GPIO_DIAGNOSTIC_PERSERVE_UARTS > 0
//     if(gpioOffset == HCOM_DIAG_GPIO_D00 || gpioOffset == HCOM_DIAG_GPIO_D01 ||
//        gpioOffset == HCOM_DIAG_GPIO_D12 || gpioOffset == HCOM_DIAG_GPIO_D13)
//       continue;
// #endif
  //   ret = hcom_nx_diag_gpio_write(gpioOffset, 1);
  //   if(ret < 0)
  //   {
  //     syslog(1, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
  //     break;
  //   }
  //   usleep(10 * 1000);    // Adjust slow down for testing
  // }
 
  // for(uint8_t cnt = 0; cnt < 256; cnt++)
  // {
  //   hcom_diag_gpio_write_byte(cnt, 0);
  //   usleep(10 * 1000);
  // }

  // // Turn all off
  // for(int gpioOffset = HCOM_DIAG_GPIO_A0;
  //   gpioOffset <= HCOM_DIAG_GPIO_D15; gpioOffset++)
  // {
// #if HCOM_DIAG_GPIO_DIAGNOSTIC_PERSERVE_UARTS > 0
//     if(gpioOffset == HCOM_DIAG_GPIO_D00 || gpioOffset == HCOM_DIAG_GPIO_D01 ||
//        gpioOffset == HCOM_DIAG_GPIO_D12 || gpioOffset == HCOM_DIAG_GPIO_D13)
//       continue;
// #endif
  //   ret = hcom_nx_diag_gpio_write(gpioOffset, 0);
  //   if(ret < 0)
  //   {
  //     syslog(1, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
  //     break;
  //   }
  // }

  return ret;
}

//================================================================
// Configure one gpio as output
int hcom_diag_gpio_config_one_output(int gpioHcomId)
{
  int ret;
  
  DEBUGASSERT(gpioHcomId >= HCOM_DIAG_GPIO_A0 &&
              gpioHcomId <= HCOM_DIAG_GPIO_D15);
              
  ret = hcom_nx_diag_gpio_config(gpioHcomId, HCOM_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_nx_gpio_config ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }

  return ret;
}

//================================================================
// This simplified version allows 1 - 24 as ledNumber and true to make high
int hcom_diag_gpio_output_cmd_led(int ledNumber, bool turnOn)
{
  return hcom_nx_diag_gpio_write(ledNumber + HCOM_DIAG_GPIO_A0 - 1,
              turnOn ? HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH :
              HCOM_GPIO_DIGITAL_CMD_VALUE_LOW);
}

//================================================================
// This could be moved to the nuttx for much less overhead
int hcom_diag_gpio_write_byte(uint8_t byteValue, uint8_t rangeId)
{
  return hcom_nx_diag_gpio_write_byte(byteValue, rangeId);
}

#endif