/****************************************************************************
 * hcom_user_space_config_manager.c
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static meadow_configuration_t *user_space_meadow_configuration = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_user_space_config_lock
 *
 * Description:
 *  Lock the specified configuration object.
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
void hcom_user_space_config_lock(meadow_configuration_t *config)
{
    sem_wait(&config->lock);
}

/****************************************************************************
 * Name: hcom_user_space_config_unlock
 *
 * Description:
 *  Unlock the specified configuration object.
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
void hcom_user_space_config_unlock(meadow_configuration_t *config)
{
    sem_post(&config->lock);
}

/****************************************************************************
 * Name: hcom_user_space_get_configuration
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
meadow_configuration_t *hcom_user_space_get_configuration(void)
{
    return user_space_meadow_configuration;
}

/****************************************************************************
 * Name: hcom_user_space_get_string_from_kernel
 *
 * Description:
 *  Copy a string value from the kernel.
 *
 * Input Parameters:
 *  source - address of the string in kernel space.
 *  request - pointer to a hcom_nx_get_string_t object to use for the
 *            request.  Please see the assumptions below.
 *
 * Returned Value:
 *  Pointer to a copy of the string that can be used in user space.
 *
 * Assumptions/Limitations:
 *  The hcom_nx_get_string_t request object must be set up and have a
 *  destination buffer allocated / available along with the length field
 *  correctly populated.
 *
 ****************************************************************************/
char *hcom_user_space_get_string_from_kernel(char *source, hcom_nx_get_string_t *request)
{
    char *result = NULL;

    if (source != NULL)
    {
        request->source = source;
        hcom_via_nx_copy_string(request);
        if (request->destination[0] != 0)
        {
            result = strdup(request->destination);
        }
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_user_space_refresh_configuration
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
meadow_configuration_t *hcom_user_space_refresh_configuration(void)
{
    meadow_configuration_t *config = hcom_user_space_get_configuration();
    sem_t lock;
    if (config == NULL)
    {
        sem_init(&lock, 0, 1);
        sem_setprotocol(&lock, SEM_PRIO_NONE);
        user_space_meadow_configuration = (meadow_configuration_t *) malloc(sizeof(meadow_configuration_t));
        if (user_space_meadow_configuration == NULL)
        {
            return(NULL);
        }
        config = user_space_meadow_configuration;
    }
    else
    {
        if (config->esp_software_version != NULL)
        {
            free(config->esp_software_version);
        }
        if (config->device_name != NULL)
        {
            free(config->device_name);
        }
        if (config->mono_trace != NULL)
        {
            free(config->mono_trace);
        }
        lock = config->lock;
    }

    memset(config, 0, sizeof(meadow_configuration_t));
    hcom_via_nx_copy_config(config);
    config->lock = lock;

    hcom_nx_get_string_t request;
    request.length = 100;
    request.destination = (char *) malloc(request.length);
    if (request.destination != NULL)
    {
        config->esp_software_version = hcom_user_space_get_string_from_kernel(config->esp_software_version, &request);
        config->device_name = hcom_user_space_get_string_from_kernel(config->device_name, &request);
        config->mono_trace = hcom_user_space_get_string_from_kernel(config->mono_trace, &request);
        free(request.destination);
    }
    else
    {
        //
        //  Not enough memory to get the values from kernel space to mark them
        //  as unpopulated to prevent attempts to access them.
        //
        config->esp_software_version = NULL;
        config->device_name = NULL;             // Maybe MeadowF7 ?
        config->mono_trace = NULL;
    }
    return(config);
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
int hcom_user_space_config_init(void)
{
    int result = OK;

    if (user_space_meadow_configuration == NULL)
    {
        user_space_meadow_configuration = hcom_user_space_refresh_configuration();
        if (user_space_meadow_configuration == NULL)
        {
            result = ERROR;
        }
    }

    return(result);
}

