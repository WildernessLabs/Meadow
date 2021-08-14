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
#include "misc/hcom_config_manager.h"

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
  char *csvDevInfo;
  char deviceNameBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  csvDevInfo = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  if(csvDevInfo == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Alloc failed\n", thisFile, __LINE__);
    snprintf_chk(csvDevInfo, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Memory allocation error. No results can be sent");
    
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            csvDevInfo, thisFile, __LINE__);
    return;
  }

  char *coprocessor_version = "Not available";
  char mono_version[20];
  char strChipId[64];

  meadow_configuration_t *config = hcom_config_get_pointer();
  if (config != NULL)
  {
    if (config->esp_software_version != NULL)
    {
      coprocessor_version = config->esp_software_version;
    }
    sprintf(deviceNameBuf, config->device_name);
    if (config->mono_version != 0)
    {
      sprintf(mono_version, "%d.%d.%d.%d", (config->mono_version >> 24) & 0xff, (config->mono_version >> 16) & 0xff,
          (config->mono_version >> 8) & 0xff, config->mono_version & 0xff);
    }
    else
    {
      sprintf(mono_version, "Not available");
    }
    snprintf_chk(strChipId, 64, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
      config->serial_number[0], config->serial_number[1], config->serial_number[2], config->serial_number[3],
      config->serial_number[4], config->serial_number[5], config->serial_number[6], config->serial_number[7],
      config->serial_number[8], config->serial_number[9], config->serial_number[10], config->serial_number[11]);
    // Meadow by Wilderness Labs, Model: F7Micro, H/W Version: F7v2, MeadowOS Version: 0.4.0 (Dec  5 2020 09:04:51),
    // Processor: STM32F777IIK6, Processor Id: 19-00-27-00-0e-51-38-32-37-35-36-30,
    // Serial Number: 305D355A3238, CoProcessor: ESP32, CoProcessor OS Version: 0.0.1
    snprintf_chk(csvDevInfo, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
            "%s, Model: %s, H/W Version: %s, MeadowOS Version: %s (%s %s), Processor: %s, Processor Id: %s, "
            "Serial Number: %02X%02X%02X%02X%02X%02X, CoProcessor: %s, CoProcessor OS Version: %s, "
            "Mono Version: %s, Device Name: %s",
            HCOM_DEVICE_INFO_PRODUCT, HCOM_DEVICE_INFO_MODEL,
            //
            //  Fix up the line below when hardware version integer is used.
            //
            config->meadow_hardware_version,
            HCOM_DEVICE_INFO_MEADOW_OS_VERSION, __DATE__, __TIME__,
            HCOM_DEVICE_INFO_PROCESSOR_TYPE, strChipId,
            config->chip_id[0], config->chip_id[1], config->chip_id[2], config->chip_id[3], config->chip_id[4], config->chip_id[5],
            HCOM_DEVICE_INFO_COPROCESSOR_TYPE, coprocessor_version,
            mono_version, deviceNameBuf);
    hcom_config_free_resources(config);
  }
  else
  {
    snprintf_chk(csvDevInfo, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "%s, Model: %s", HCOM_DEVICE_INFO_PRODUCT, HCOM_DEVICE_INFO_MODEL);
  }

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0,
          csvDevInfo, thisFile, __LINE__);
    
  free(csvDevInfo);
}

//======================================================================================
// The device name  comes from the configuration file, meadow.cfg
void hcom_misc_rqst_get_device_name(uint32_t userData)
{
  // char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  meadow_configuration_t *config = hcom_config_get_pointer();
  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, config->device_name);
  hcom_config_free_resources(config);
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
