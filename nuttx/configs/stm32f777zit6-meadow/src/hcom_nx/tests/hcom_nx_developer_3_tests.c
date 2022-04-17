/****************************************************************************
 * configs/stm32f777zit6-meadow/src/hcom_nx/tests/hcom_nx_developer_3_tests.c
 * 
 *   Copyright (C) 2019 - 2021 Wilderness Labs. All rights reserved.
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

// Available tests based on provided user data

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

  // This is an indicator that this is temporary or needs work for CCM
#if MEADOW_ETHERNET_INCLUDE_TEMP_WIFI_SWITCH > 0 
#include <meadow/hcom_bbreg_defn.h>
#include <arch/board/board.h>
#include "stm32_gpio.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

// How often to output syslog information?

/****************************************************************************
 * Private Data
 ****************************************************************************/

// static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_exec_developer_3_tests(struct hcom_nx_cmd_data *cmdData)
{
  int userData = (int)cmdData->userData;
  UNUSED(userData);
  
  // The struct hcom_nx_cmd_data fields are:
  // uint16_t hcomCmd;   // The orginal host command
  // uint32_t userData;
  // uint8_t logLevel;
  // uint8_t logLen;
  // char logMsg[HCOM_NX_CMD_LOG_MSG_SIZE + 1];
  // void (* send_host_msg)(uint16_t, uint32_t, char *, char *, int);

  // This is an indicator that this is temporary or needs work for CCM
#if MEADOW_ETHERNET_INCLUDE_TEMP_WIFI_SWITCH > 0
  if(userData == 1)
  {
    syslog(1, "CLI requests Ethernet to be enabled and WiFi disabled\n");

    // Set the ethernet flag
    modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER, 0, HCOM_BBREG_ETHERNET_WIFI_TEMP_CTRL_BIT);
    
    hcom_nx_common_utils_only_restart_meadow();
  }
  else if (userData == 2)
  {
    syslog(1, "CLI requests Ethernet to be disabled and WiFi enabled\n");

    // Clear the ethernet flag
    modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER, HCOM_BBREG_ETHERNET_WIFI_TEMP_CTRL_BIT, 0);

    hcom_nx_common_utils_only_restart_meadow();
  }
#endif

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
  // 50 - 69
  if(userData > 49 && userData < 60)
  {
    return hcom_nx_exec_power_mgmt_tests(cmdData);
  }
#endif

#if HCOM_INCLUDE_ISO8601_PARSING_TESTS_IN_BUILD > 0
  // 60 - 69
  if(userData > 59 && userData < 70)
  {
    return hcom_nx_exec_iso8601_parsing_tests(cmdData);
  }
#endif

#if HCOM_INCLUDE_SD_CARD_TESTS_IN_BUILD > 0
  // 100 - 124
  if(userData > 99 && userData < 125)
  {
    return hcom_nx_exec_sdcard_tests(cmdData);
  }
#endif

  return OK;
}
