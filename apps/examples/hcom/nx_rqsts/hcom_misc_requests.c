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
#include <meadow/meadow_os.h>

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
  char *device_info = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  if (device_info == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Alloc failed\n", thisFile, __LINE__);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            "Memory allocation error. No results can be sent", thisFile, __LINE__);
    return;
  }

  *device_info = 0;
  int buffer_length = 256;
  char *buffer = (char *) malloc(buffer_length);
  if (buffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Alloc failed\n", thisFile, __LINE__);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            "Memory allocation error. No results can be sent", thisFile, __LINE__);
    free(device_info);
    return;
  }

  snprintf(buffer, buffer_length, "Product|%s~", HCOM_DEVICE_INFO_PRODUCT);
  strcat(device_info, buffer);

  snprintf(buffer, buffer_length, "Model|%s~", HCOM_DEVICE_INFO_MODEL);
  strcat(device_info, buffer);

  snprintf(buffer, buffer_length, "ProcessorType|%s~", HCOM_DEVICE_INFO_PROCESSOR_TYPE);
  strcat(device_info, buffer);

  snprintf(buffer, buffer_length, "CoprocessorType|%s~", HCOM_DEVICE_INFO_COPROCESSOR_TYPE);
  strcat(device_info, buffer);

  meadow_configuration_t *config = meadow_os_deep_copy_config();
  if (config != NULL)
  {
    char *version = (g_current_hcom_protocol_version > HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER) ? config->os_version.long_string : config->os_version.short_string;
    snprintf(buffer, buffer_length, "OSVersion|%s~", version);
    strcat(device_info, buffer);

    version = (g_current_hcom_protocol_version > HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER) ? config->esp_version.long_string : config->esp_version.short_string;
    if (version != NULL)
    {
      snprintf(buffer, buffer_length, "CoprocessorVersion|%s~", version);
      strcat(device_info, buffer);
    }
    if ((config->mono_version.major != 0) || (config->mono_version.minor != 0) || (config->mono_version.revision != 0) || (config->mono_version.build != 0))
    {
      version = (g_current_hcom_protocol_version > HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER) ? config->mono_version.long_string : config->mono_version.short_string;
      snprintf(buffer, buffer_length, "MonoVersion|%s~", version);
      strcat(device_info, buffer);
    }

    snprintf(buffer, buffer_length, "ProcessorId|%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X~",
      config->serial_number[0], config->serial_number[1], config->serial_number[2], config->serial_number[3],
      config->serial_number[4], config->serial_number[5], config->serial_number[6], config->serial_number[7],
      config->serial_number[8], config->serial_number[9], config->serial_number[10], config->serial_number[11]);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "Hardware|%s~", config->hardware_version_text);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "DeviceName|%s~", config->device_name);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "SerialNo|%02X%02X%02X%02X%02X%02X~", config->chip_id[0], config->chip_id[1], config->chip_id[2], config->chip_id[3], config->chip_id[4], config->chip_id[5]);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "WiFiMAC|%02X:%02X:%02X:%02X:%02X:%02X~", config->board_mac_address[0], config->board_mac_address[1], config->board_mac_address[2], config->board_mac_address[3], config->board_mac_address[4], config->board_mac_address[5]);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "SoftAPMac|%02X:%02X:%02X:%02X:%02X:%02X~", config->soft_ap_mac_address[0], config->soft_ap_mac_address[1], config->soft_ap_mac_address[2], config->soft_ap_mac_address[3], config->soft_ap_mac_address[4], config->soft_ap_mac_address[5]);
    strcat(device_info, buffer);

    snprintf(buffer, buffer_length, "BtMAC|%02X:%02X:%02X:%02X:%02X:%02X~", config->bluetooth_mac_address[0], config->bluetooth_mac_address[1], config->bluetooth_mac_address[2], config->bluetooth_mac_address[3], config->bluetooth_mac_address[4], config->bluetooth_mac_address[5]);
    strcat(device_info, buffer);

    meadow_os_config_free_resources(config);
  }
  strcat(device_info, "\n");
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0, device_info, thisFile, __LINE__);

  free(buffer);
  free(device_info);
}

//======================================================================================
// The device name  comes from the configuration file, meadow.cfg
void hcom_misc_rqst_get_device_name(uint32_t userData)
{
  // char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  meadow_configuration_t *config = meadow_os_deep_copy_config();
  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, config->device_name);
  meadow_os_config_free_resources(config);
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