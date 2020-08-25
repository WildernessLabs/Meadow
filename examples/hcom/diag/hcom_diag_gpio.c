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

#include "../hcom_common.h"
#include <meadow/hcom_udp_shared.h>
#include <meadow/hcom_shared_common.h>

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
#if HCOM_INCLUDE_DIAGNOSTIC_GPIO_CODE > 0
  int ret;
  ret = hcom_diag_gpio_config();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_diag_gpio_config, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }
  return ret;
#else
  return OK;
#endif
}

#if HCOM_INCLUDE_DIAGNOSTIC_GPIO_CODE > 0
//================================================================
// Configure the diagnostic GPIOs
int hcom_diag_gpio_config()
{
  int ret;

  // Configure the first 9 GPIO as digital output.
  for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  {
    ret = hcom_nx_gpio_config(gpioOffset, HCOM_GPIO_DIGITAL_CONFIG_OUTPUT);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
      break;
    }
  }

  // // Turn all on
  // for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  // {
  //   ret = hcom_nx_gpio_write(gpioOffset, 1);
  //   if(ret < 0)
  //   {
  //     syslog(1, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
  //     break;
  //   }
  //   usleep(10 * 1000);    // Adjust slow down for testing
  // }
 
  // for(uint8_t cnt = 0; cnt < 256; cnt++)
  // {
  //   hcom_diag_gpio_write_set_8bits(cnt);
  //   usleep(10 * 1000);
  // }

  // // Turn all off
  // for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  // {
  //   ret = hcom_nx_gpio_write(gpioOffset, 0);
  //   if(ret < 0)
  //   {
  //     syslog(1, "hcom_nx_gpio_config value of:%d\n", gpioOffset);
  //     break;
  //   }
  // }

  return ret;
}
#endif

//================================================================
// The caller provides the integer that represents the GPIO within
// hcom. The Meadow F7 GPIO A0 - MOSI have values of 2 - 10.
// cmdValue is either HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH (1) or
// HCOM_GPIO_DIGITAL_CMD_VALUE_LOW (0).
// Note: It seems to take about 5 microsec for this function to
// complete. While on the nuttx side it takes about 250 nanosec.
int hcom_diag_gpio_write(int gpioHcomId, uint8_t cmdValue)
{
#if HCOM_INCLUDE_DIAGNOSTIC_GPIO_CODE > 0
  int ret;
  
  DEBUGASSERT(cmdValue == HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH ||
              cmdValue == HCOM_GPIO_DIGITAL_CMD_VALUE_LOW);
  DEBUGASSERT(gpioHcomId >= HCOM_GPIO_DIG_NX_ID_A0___01 &&
              gpioHcomId <= HCOM_GPIO_DIG_NX_ID_MISO_09);

  ret = hcom_nx_gpio_write(gpioHcomId, cmdValue);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_nx_gpio_write, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }

  return ret;
#else
  return OK;
#endif
}

//================================================================
// This could be moved to the nuttx for much less overhead.
int hcom_diag_gpio_write_set_8bits(uint8_t setBits)
{  
  // Note: Low (false/0) turns led on for an open drain
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A0___01, (setBits & 0x01) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A1___02, (setBits & 0x02) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A2___03, (setBits & 0x04) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A3___04, (setBits & 0x08) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A4___05, (setBits & 0x10) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_A5___06, (setBits & 0x20) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_SCK__07, (setBits & 0x40) == 0 ? 0 : 1);
  hcom_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_MOSI_08, (setBits & 0x80) == 0 ? 0 : 1);

  return OK;
}
