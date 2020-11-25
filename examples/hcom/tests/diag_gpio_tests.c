/****************************************************************************
 * \apps\examples\hcom\tests\developer_tests.c
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

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_gpio_defn_diag.h>


/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
// static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
void diag_gpio_tests_basic_gpio_tests(uint32_t userData);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void diag_gpio_tests_basic_gpio_tests(uint32_t userData)
{
  int ret;
  if(userData == 500)
  {
    // This will create something like '#define THE_GPIO_THAT_IS_DEFINED_HERE (0X01234345)'
    ret = hcom_via_nx_diag_gpio_make_defns(hcom_via_nx_get_fd());
    if(ret < 0)
    {
      syslog(2, "hcom_via_nx_diag_gpio_make_defns() value of:%d\n", ret);
    }
  }
  else
  {
    ret = hcom_via_nx_diag_gpio_write_byte(hcom_via_nx_get_fd(),
          (uint8_t) userData, 1);
    if(ret < 0)
    {
      syslog(2, "hcom_via_nx_diag_gpio_write_byte() value of:%d\n", ret);
    }
  }
}
// These tests were spread over different locations. No effort has been made to
// put this into service at this time.
// More GPIO Test Code
  // int ret;

  // for(int cnt = 0; cnt < 255; cnt++)
  // {
  //   hcom_diag_gpio_write_byte(cnt, 1);
  //   usleep(50 * 1000);
  // }

  // // static bool isInitialized = false;

  // // Step 1 configure gpios for output
  // if(!isInitialized)
  // {
  //   // Configure gpios in array offsets 2 - 10 whicn are only used for testing
  //   for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  //   {
  //     ret = hcom_via_nx_gpio_config(hcom_via_nx_get_fd(), gpioOffset, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  //     if(ret < 0)
  //     {
  //       syslog(2, "hcom_via_nx_gpio_config value of:%d\n", gpioOffset);
  //     }
  //   }
  //   isInitialized = true;
  // }
  
  // // TEST ESP32 GPIOs
  // ret = hcom_via_nx_gpio_config(hcom_via_nx_get_fd(), 0, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  // if(ret < 0)
  // {
  //   syslog(2, "hcom_via_nx_gpio_config value of:%d\n", 0);
  // }

  // ret = hcom_via_nx_gpio_config(hcom_via_nx_get_fd(), 1, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  // if(ret < 0)
  // {
  //   syslog(2, "hcom_via_nx_gpio_config value of:%d\n", 1);
  // }
  // // Just toggle 2 GPIOS
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 0, 1);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 0, 0);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 0, 1);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 0, 0);
  // sleep(1);

  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 1, 1);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 1, 0);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 1, 1);
  // sleep(1);
  // hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), 1, 0);
  // sleep(1);

  // // uint32_t gpioOffset4 = 4;
  // // for(int cnt = 0; cnt < userData; cnt++)
  // // {
  // //     ret = hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), gpioOffset4, 1);
  // //     ret = hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), gpioOffset4, 0);
  // // }
  // // return;

  // // P.S it's too fast to see
  // for(int cnt = 0; cnt < userData; cnt++)
  // {
  //   // Turn all on
  //   for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  //   {
  //     ret = hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), gpioOffset, 1);
  //     if(ret < 0)
  //     {
  //       syslog(2, "hcom_via_nx_gpio_config value of:%d\n", gpioOffset);
  //     }
  //   }

  //   // Turn all off
  //   for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
  //   {
  //     ret = hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), gpioOffset, 0);
  //     if(ret < 0)
  //     {
  //       syslog(2, "hcom_via_nx_gpio_config value of:%d\n", gpioOffset);
  //     }
  //   }
  //   //usleep(1);    // Adjust slow down for testing
  // }

  // This will control gpio A0-MISO ports based on userData
  // userData = 1 to 9 turns on gpios
  // userData = -1 to -9 turns off gpios

//   static bool isInitialized = false;
//   uint8_t gpioHcomId;
//   uint8_t cmdValue;
//   int32_t signedUserData = (int32_t)userData;

//   if(!isInitialized)
//   {
//     // Configure gpios in array offsets 2 - 10
//     for(int gpioOffset = 2; gpioOffset < 11; gpioOffset++)
//     {
//       ret = hcom_via_nx_gpio_config(hcom_via_nx_get_fd(), gpioOffset, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
//       if(ret < 0)
//       {
//         syslog(2, "hcom_via_nx_gpio_config value of:%d\n", gpioOffset);
//       }
//     }
//     isInitialized = true;
//   }

//   if(signedUserData == 0 || signedUserData > 9 || signedUserData < -9)
//   {
//     syslog(2, "userData of:%d is not valid\n", signedUserData);
//     return;
//   }

//   if(signedUserData > 0)
//   {
//     gpioHcomId = userData + 1;    // userData of 1 is offset of 2
//     cmdValue = HCOM_NX_GPIO_DIGITAL_CMD_VALUE_HIGH;
//   }
//   else
//   {
//     gpioHcomId = (signedUserData * -1) + 1;
//     cmdValue = HCOM_NX_GPIO_DIGITAL_CMD_VALUE_LOW;
//   }

//   ret = hcom_via_nx_gpio_write(hcom_via_nx_get_fd(), gpioHcomId, cmdValue);
//   if(ret < 0)
//   {
//     syslog(2, "hcom_via_nx_gpio_write error:%d gpio:%d, value:%d\n", ret, gpioHcomId,cmdValue);
//   }
