/****************************************************************************
 * hcom_config_manager.c
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

//  The methods and data structures in this file provide access to the
//  configuration of the meadow board.

#include <stdlib.h>
#include <string.h>
#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <nuttx/semaphore.h>
#include <meadow/hcom_shared_common.h>
#include "hcom_config_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_config_free_resources
 *
 * Description:
 *  Free the resources (including the structure being pointed to) used
 *  by the configuration structure.
 *
 * Input Parameters:
 *  config - Pointer to the configuration structure to the released.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_config_free_resources(meadow_configuration_t *config)
{
    if (config != NULL)
    {
        if (config->mono_trace != NULL)
        {
            free(config->mono_trace);
        }
        if (config->device_name != NULL)
        {
            free(config->device_name);
        }
        if (config->meadow_software_version != NULL)
        {
            free(config->meadow_software_version);
        }
        if (config->meadow_hardware_version != NULL)
        {
            free(config->meadow_hardware_version);
        }
        if (config->esp_software_version != NULL)
        {
            free(config->esp_software_version);
        }
        free(config);
    }
}

/****************************************************************************
 * Name: hcom_refresh_configuration_from_kernel
 *
 * Description:
 *  Refresh the configuration by getting a fresh copy from NuttX.
 * 
 *  If the pointer to the configuration is NULL then a new copy will be
 *  created.
 * 
 *  If the pointer to the configuration is not NULL then the previous
 *  contents will be disposed of and the configuration refreshed.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  Pointer to the current configuration object.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
meadow_configuration_t *hcom_config_get_pointer(void)
{
    uint8_t *buffer = (uint8_t *) malloc(256);

    if (buffer == NULL)
    {
        return(NULL);
    }
    int32_t *ip = (int32_t *) buffer;
    *ip = 256;
    hcom_via_nx_copy_config(buffer);

    meadow_configuration_t *config = (meadow_configuration_t *) malloc(sizeof(meadow_configuration_t));
    if (config != NULL)
    {
        //
        //  These strings must be deserialised in the same order as the serialsed in hcom_nx_copy_config_for_user_mode.
        //
        memcpy(config, buffer, sizeof(meadow_configuration_t));
        char *ptr = (char *) (buffer + sizeof(meadow_configuration_t));
        config->mono_trace = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->meadow_software_version = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->meadow_hardware_version = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->esp_software_version = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->device_name = (*ptr == 0) ? NULL : strdup(ptr);
    }

    free(buffer);

    return(config);
}

//======================================================================================
// Get the version information for esp32, meadow OS and mono 
int hcom_get_software_version_info(hcom_config_version_information_t *version_info)
{
    int stringLen = 0;

    memset((void *)version_info, 0, sizeof(hcom_config_version_information_t));

    meadow_configuration_t *config = hcom_config_get_pointer();

    if (config != NULL)
    {
        if (config->meadow_software_version != NULL)
        {
            version_info->meadow_version_available = true;
            stringLen = strlen(config->meadow_software_version);
            if (stringLen >= HCOM_VERSION_NUMBER_MAX_LENGTH)
            {
                hcom_logging_syslog(LOG_WARNING, "%s@%d Buffer too small need:%d, have:%d\n",
                      __FILE__, __LINE__, stringLen + 1, HCOM_VERSION_NUMBER_MAX_LENGTH);
                return -ENAMETOOLONG;
            }
            strncpy(version_info->meadow_version, config->meadow_software_version, HCOM_VERSION_NUMBER_MAX_LENGTH);
        }
        else
        {
            version_info->meadow_version_available = false;
            strncpy(version_info->meadow_version, "Not available", HCOM_VERSION_NUMBER_MAX_LENGTH - 1);
        }

      if (config->meadow_hardware_version != NULL)
      {
          version_info->hardware_version_available = true;
          stringLen = strlen(config->meadow_hardware_version);
          if (stringLen >= HCOM_VERSION_NUMBER_MAX_LENGTH)
          {
              hcom_logging_syslog(LOG_WARNING, "%s@%d Buffer too small need:%d, have:%d\n",
                  __FILE__, __LINE__, stringLen + 1, HCOM_VERSION_NUMBER_MAX_LENGTH);
              return -ENAMETOOLONG;
        }
        strncpy(version_info->hardware_version, config->meadow_hardware_version, HCOM_VERSION_NUMBER_MAX_LENGTH);
      }
      else
    {
      version_info->hardware_version_available = false;
      strncpy(version_info->hardware_version, "Not available", HCOM_VERSION_NUMBER_MAX_LENGTH - 1);
    }

    if (config->esp_software_version != NULL)
    {
      version_info->esp32_version_available = true;
      stringLen = strlen(config->esp_software_version);
      if(stringLen >= HCOM_VERSION_NUMBER_MAX_LENGTH)
      {
        hcom_logging_syslog(LOG_WARNING, "%s@%d Buffer too small need:%d, have:%d\n",
                  __FILE__, __LINE__, stringLen + 1, HCOM_VERSION_NUMBER_MAX_LENGTH);
        return -ENAMETOOLONG;
      }
      strncpy(version_info->esp32_version, config->esp_software_version, HCOM_VERSION_NUMBER_MAX_LENGTH);
    }
    else
    {
      version_info->esp32_version_available = false;
      strncpy(version_info->esp32_version, "Not available", HCOM_VERSION_NUMBER_MAX_LENGTH - 1);
    }

    if(config->mono_version != 0x00000000)
    {
      // Need to convert the mono's uint32_t serial number to a string
      version_info->mono_version_available = true;
      stringLen = sprintf(version_info->mono_version, "%d.%d.%d.%d",
            config->mono_version >> 24,
            (config->mono_version >> 16) & 0xff,
            (config->mono_version >> 8) & 0xff,
            config->mono_version & 0xff);
    }
    else
    {
      version_info->mono_version_available = false;
      strncpy(version_info->mono_version, "Not available", HCOM_VERSION_NUMBER_MAX_LENGTH - 1);
    }
    
    if(stringLen >= HCOM_VERSION_NUMBER_MAX_LENGTH)
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d Buffer too small need:%d, have:%d\n",
                __FILE__, __LINE__, stringLen + 1, HCOM_VERSION_NUMBER_MAX_LENGTH);
      return -ENAMETOOLONG;
    }
  }
  
  return OK;
}


