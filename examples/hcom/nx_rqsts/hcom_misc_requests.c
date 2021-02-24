/****************************************************************************
 * \apps\examples\hcom\os_rqsts\hcom_misc_requests.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_nuttx_shared.h>
#include "misc/hcom_userspace_config_manager.h"

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
int hcom_misc_rqst_setup()
{
  return OK;
}

//======================================================================================
// Most of the device information is available from here (apps) but not
// the MCU unique identifier, for this we must access the nuttx side.
void hcom_misc_rqst_get_device_info(uint32_t userData)
{
  int ret;
  char *csvDevInfo;
  int stringLen;
  char mcuSerNumb[16];
  uint8_t uniqueId[12];  // 96 bit unique chip id as 12 bytes
  char deviceNameBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  csvDevInfo = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  if(csvDevInfo == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Alloc failed\n", thisFile, __LINE__);
    stringLen = snprintf(csvDevInfo, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Memory allocation error. No results can be sent");
    
    DEBUGASSERT(stringLen < HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            csvDevInfo, thisFile, __LINE__);
    return;
  }

  // nuttx access
  ret = hcom_via_nx_get_mcu_id(uniqueId);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_NOTICE, "%s@%d-Get device info error:%d\n", thisFile, __LINE__, ret);
  }

  char strChipId[64];
  snprintf(strChipId, 64, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
    uniqueId[0], uniqueId[1], uniqueId[2], uniqueId[3], uniqueId[4], uniqueId[5],
    uniqueId[6], uniqueId[7], uniqueId[8], uniqueId[9], uniqueId[10], uniqueId[11]);

  // nuttx access
  ret = hcom_via_nx_get_mcu_ser_numb(mcuSerNumb);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_NOTICE, "%s@%d-Get device info error:%d\n", thisFile, __LINE__, ret);
    return;
  }

  ret = hcom_via_nx_ini_cfg_get_value(NULL, MEADOW_INI_CFG_OPERATION_SECTION,
                    MEADOW_INI_CFG_DEV_NAME_KEY, deviceNameBuf, MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  if(ret != OK)
  {
    // Substitute the default device name on error
    strcpy(deviceNameBuf, MEADOW_INI_CFG_DEFAULT_DEV_NAME);
  }

  char *coprocessor_version = "Not available";
  meadow_configuration_t *config = hcom_user_space_get_configuration();
  if (config != NULL)
  {
    if (config->esp_software_version == NULL)
    {
      config = hcom_user_space_refresh_configuration();
      if (config->esp_software_version != NULL)
      {
        coprocessor_version = config->esp_software_version;
      }
    }
  }

  // Meadow by Wilderness Labs, Model: F7Micro, MeadowOS Version: 0.4.0 (Dec  5 2020 09:04:51),
  // Processor: STM32F777IIK6, Processor Id: 19-00-27-00-0e-51-38-32-37-35-36-30,
  // Serial Number: 305D355A3238, CoProcessor: ESP32, CoProcessor OS Version: 0.0.1
  stringLen = snprintf(csvDevInfo, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
          "%s, Model: %s, MeadowOS Version: %s (%s %s), Processor: %s, Processor Id: %s, "
          "Serial Number: %s, CoProcessor: %s, CoProcessor OS Version: %s, "
          "Mono Version: %s, Device Name: %s",
          HCOM_DEVICE_INFO_PRODUCT, HCOM_DEVICE_INFO_MODEL,
          HCOM_DEVICE_INFO_MEADOW_OS_VERSION, __DATE__, __TIME__,
          HCOM_DEVICE_INFO_PROCESSOR_TYPE, strChipId, mcuSerNumb,
          HCOM_DEVICE_INFO_COPROCESSOR_TYPE, coprocessor_version,
          HCOM_DEVICE_INFO_MONO_VERSION, deviceNameBuf);

  DEBUGASSERT(stringLen < HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0,
          csvDevInfo, thisFile, __LINE__);
    
  free(csvDevInfo);
}

//======================================================================================
// The device name  comes from the configuration file, meadow.cfg
void hcom_misc_rqst_get_device_name(uint32_t userData)
{
  int ret;
  int stringLen;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  // On error the call returns a text error message in the return buffer
  // if it's large enough
  ret = hcom_via_nx_ini_cfg_get_value(NULL, MEADOW_INI_CFG_OPERATION_SECTION,
              MEADOW_INI_CFG_DEV_NAME_KEY, returnValueBuf, HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  if(ret != OK)
  {
    // Substitute the default device name on error
    strcpy(returnValueBuf, MEADOW_INI_CFG_DEFAULT_DEV_NAME);
  }

  // Pass device name to CLI
  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, returnValueBuf);
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0,
          hostMsg, thisFile, __LINE__);
}

//======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_misc_rqst_enter_dfu_mode(uint32_t userData)
{
  // Cannot write to memory from app land.
  hcom_via_nx_put_meadow_into_dfu_mode();
}
