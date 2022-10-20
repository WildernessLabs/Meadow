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
#include <meadow/hcom_nuttx_shared.h>
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
        if (config->mono_options != NULL)
        {
            free(config->mono_options);
        }
        if (config->device_name != NULL)
        {
            free(config->device_name);
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
    const int buffer_size = 4096;
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);

    if (buffer == NULL)
    {
        return(NULL);
    }
    int32_t *ip = (int32_t *) buffer;
    *ip = buffer_size;
    hcom_via_nx_copy_config(buffer);

    meadow_configuration_t *config = (meadow_configuration_t *) malloc(sizeof(meadow_configuration_t));
    if (config != NULL)
    {
        //
        //  These strings must be deserialised in the same order as the serialised in hcom_nx_copy_config_for_user_mode.
        //
        memcpy(config, buffer, sizeof(meadow_configuration_t));
        char *ptr = (char *) (buffer + sizeof(meadow_configuration_t));
        config->mono_options = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->meadow_hardware_version = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->esp_software_version = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
        config->device_name = (*ptr == 0) ? NULL : strdup(ptr);
        ptr += strlen(ptr) + 1;
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
      snprintf(version_info->meadow_version, HCOM_VERSION_NUMBER_MAX_LENGTH, "%d.%d.%d.%d",
        config->os_version.major, config->os_version.minor, config->os_version.revision,
        config->os_version.build);
      version_info->meadow_version_available = true;

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

    if ((config->mono_version.major != 0) || ((config->mono_version.minor != 0) && (config->mono_version.revision != 0)))
    {
      version_info->mono_version_available = true;
      stringLen = sprintf(version_info->mono_version, "%d.%d.%d.%d",
            config->mono_version.major,
            config->mono_version.minor,
            config->mono_version.revision,
            config->mono_version.build);
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

  hcom_config_free_resources(config);
  
  return OK;
}

//======================================================================================
// Translate the version information into a long format version string.
char *hcom_config_get_long_version_string(meadow_version_number_t *version)
{
  char *storage = (char *) malloc(150);
  char *result;

  if (storage != NULL)
  {
    snprintf_chk(storage, 150, HCOM_VERSION_FORMAT_STRING, version->major, version->minor,
        version->revision, version->build, version->day, version->month_text, version->year,
        version->hour, version->minute, version->second, version->hash,
        version->branch_name == NULL ? "Unknown" : version->branch_name);
    result = strdup(storage);
    free(storage);
  }
  else
  {
    result = NULL;
  }
  return(result);
}
