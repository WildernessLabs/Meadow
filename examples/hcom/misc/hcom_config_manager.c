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

static meadow_configuration_t *user_space_meadow_configuration = NULL;

static sem_t config_lock;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_config_lock
 *
 * Description:
 *  Lock the configuration object.
 *
 * Input Parameters:
 *  config - Pointer to an meadow_configuration_t object to be locked.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_config_lock(void)
{
    sem_wait(&config_lock);
}

/****************************************************************************
 * Name: hcom_config_unlock
 *
 * Description:
 *  Unlock the configuration object.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_config_unlock(void)
{
    sem_post(&config_lock);
}

/****************************************************************************
 * Name: hcom_config_get_pointer
 *
 * Description:
 *  Get a pointer to the current configuration.
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
    return user_space_meadow_configuration;
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
meadow_configuration_t *hcom_refresh_configuration_from_kernel(void)
{
    uint8_t *buffer = (uint8_t *) malloc(256);

    if (buffer == NULL)
    {
        return(NULL);
    }
    int32_t *ip = (int32_t *) buffer;
    *ip = 256;
    hcom_via_nx_copy_config(buffer);

    hcom_config_lock();
    if (user_space_meadow_configuration == NULL)
    {
        user_space_meadow_configuration = (meadow_configuration_t *) malloc(sizeof(meadow_configuration_t));
        if (user_space_meadow_configuration == NULL)
        {
            hcom_config_unlock();
            return(NULL);
        }
    }
    else
    {
        if (user_space_meadow_configuration->meadow_software_version != NULL)
        {
            free(user_space_meadow_configuration->meadow_software_version);
        }
        if (user_space_meadow_configuration->esp_software_version != NULL)
        {
            free(user_space_meadow_configuration->esp_software_version);
        }
        if (user_space_meadow_configuration->device_name != NULL)
        {
            free(user_space_meadow_configuration->device_name);
        }
        if (user_space_meadow_configuration->mono_trace != NULL)
        {
            free(user_space_meadow_configuration->mono_trace);
        }
    }
    //
    //  These strings must be deserialised in the same order as the serialsed in hcom_nx_copy_config_for_user_mode.
    //
    memcpy(user_space_meadow_configuration, buffer, sizeof(meadow_configuration_t));
    char *ptr = (char *) (buffer + sizeof(meadow_configuration_t));
    user_space_meadow_configuration->mono_trace = (*ptr == 0) ? NULL : strdup(ptr);
    ptr += strlen(ptr) + 1;
    user_space_meadow_configuration->meadow_software_version = (*ptr == 0) ? NULL : strdup(ptr);
    ptr += strlen(ptr) + 1;
    user_space_meadow_configuration->esp_software_version = (*ptr == 0) ? NULL : strdup(ptr);
    ptr += strlen(ptr) + 1;
    user_space_meadow_configuration->device_name = (*ptr == 0) ? NULL : strdup(ptr);
    hcom_config_unlock();

    free(buffer);

    return(user_space_meadow_configuration);
}

/****************************************************************************
 * Name: hcom_user_space_config_init
 *
 * Description:
 *  Initialise the user space configuration system and get a copy of the 
 *  configuration from the kernel.
 *
 * Input Parameters:
 *  none.
 *
 * Returned Value:
 *  OK if the configuration was copied, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  none.
 *
 ****************************************************************************/
int hcom_config_init(void)
{
    int result = OK;

    sem_init(&config_lock, 0, 1);
    sem_setprotocol(&config_lock, SEM_PRIO_NONE);
    if (user_space_meadow_configuration == NULL)
    {
        user_space_meadow_configuration = hcom_refresh_configuration_from_kernel();
        if (user_space_meadow_configuration == NULL)
        {
            result = ERROR;
        }
    }

    return(result);
}

//======================================================================================
// Get the version information for esp32, meadow OS and mono 
int hcom_config_get_software_versions(hcom_config_version_numbers_t *version_numbs)
{
  int stringLen;

  memset((void *)version_numbs, 0, sizeof(hcom_config_version_numbers_t));

  hcom_config_lock();
  meadow_configuration_t *config = hcom_config_get_pointer();

  if (config != NULL)
  {
    if (config->esp_software_version == NULL || config->meadow_software_version == NULL)
    {
      hcom_config_unlock();
      config = hcom_refresh_configuration_from_kernel();
      hcom_config_lock();
    }

    if (config->meadow_software_version != NULL)
    {
      stringLen = strlen(config->meadow_software_version);
      DEBUGASSERT(stringLen < HCOM_VERSION_NUMBER_MAX_LENGTH);
      strncpy(version_numbs->meadow_version, config->meadow_software_version, HCOM_VERSION_NUMBER_MAX_LENGTH);
    }
    else
    {
      version_numbs->meadow_version[0]= '\0';
    }

    if (config->esp_software_version != NULL)
    {
      stringLen = strlen(config->esp_software_version);
      DEBUGASSERT(stringLen < HCOM_VERSION_NUMBER_MAX_LENGTH);
      strncpy(version_numbs->esp32_version, config->esp_software_version, HCOM_VERSION_NUMBER_MAX_LENGTH);
    }
    else
    {
      version_numbs->esp32_version[0]= '\0';
    }

    // Need to convert the mono's uint32_t serial number to a string
    stringLen = sprintf(version_numbs->mono_version, "%d.%d.%d.%d",
          config->mono_version >> 24,
          (config->mono_version >> 16) & 0xff,
          (config->mono_version >> 8) & 0xff,
          config->mono_version & 0xff);
    
    DEBUGASSERT(stringLen < HCOM_VERSION_NUMBER_MAX_LENGTH);
  }
  
  hcom_config_unlock();
  return OK;
}


