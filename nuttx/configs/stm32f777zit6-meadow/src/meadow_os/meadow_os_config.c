/****************************************************************************
 * meadow_os_config.c
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

#include <nuttx/config.h>

#include <stdlib.h>
#include <nuttx/kmalloc.h>

#include <meadow/meadow_os.h>

#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_copy_string
 *
 * Description:
 *  Copy the source string into a new string is user space memory.
 *
 * Input Parameters:
 *  source - String to be copied (this can be NULL).
 *
 * Returned Value:
 *  Pointer to a copy of the string in user space memory or NULL if the
 *  original string was NULL or there was a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static char *meadow_os_copy_string(char *source)
{
    char *result = source;

    if (source != NULL)
    {
        result = kumm_malloc(strlen(source) + 1);
        if (result != NULL)
        {
            memcpy(result, source, strlen(source) + 1);
        }
    }

    return(result);
}

/****************************************************************************
 * Name: meadow_os_copy_version_strings
 *
 * Description:
 *  Copy the strings from a meadow_version_number_t object
 * 
 * Input Parameters:
 *  source - Original version number object being copied.
 *  destination - Destination of the copies.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void meadow_os_copy_version_strings(meadow_version_number_t *source, meadow_version_number_t *destination)
{
    destination->branch_name = meadow_os_copy_string(source->branch_name);
    destination->short_string = meadow_os_copy_string(source->short_string);
    destination->long_string = meadow_os_copy_string(source->short_string);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_get_config
 *
 * Description:
 *  Get a copy of the system config and place the copy in user space memory.
 * 
 *  Note that the release method must be called to free all of the memory
 *  allocated here.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Pointer to the copy of the config.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
meadow_configuration_t *meadow_os_deep_copy_config(void)
{
    meadow_configuration_t *result = kumm_malloc(sizeof(meadow_configuration_t));

    if (result != NULL)
    {
        hcom_nx_config_lock();

        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        memcpy(result, config, sizeof(meadow_configuration_t));
        //
        //  All of the components like integers etc will be OK to use without
        //  modification, we must however replace any strings or pointers
        //  with user space copies.
        //
        //  Note that it is important that any copies of structures, strings
        //  or any objects copied on the heap must have a corresponding free
        //  operation in the meadow_os_config_free_resources method.
        //
        result->device_name = meadow_os_copy_string(config->device_name);
        result->reserved_pins = meadow_os_copy_string(config->reserved_pins);
        result->mono_options = meadow_os_copy_string(config->mono_options);
        result->hardware_version_text = meadow_os_copy_string(config->hardware_version_text);
        meadow_os_copy_version_strings(&config->os_version, &result->os_version);
        meadow_os_copy_version_strings(&config->mono_version, &result->mono_version);
        meadow_os_copy_version_strings(&config->esp_version, &result->esp_version);
        if (config->default_interface != NULL)
        {
            result->default_interface = kumm_malloc(sizeof(meadow_network_interface_t));
            if (result->default_interface != NULL)
            {
                memcpy(result->default_interface, config->default_interface, sizeof(meadow_network_interface_t));
                result->default_interface->name = meadow_os_copy_string(config->default_interface->name);
                result->default_interface->psock_methods = NULL;
            }
        }
        else
        {
            result->default_interface = NULL;
        }

        if (config->default_cell_settings != NULL)
        {
            result->default_cell_settings = kumm_malloc(sizeof(cell_settings_t));
            if (result->default_cell_settings != NULL)
            {
                result->default_cell_settings->apn = meadow_os_copy_string(config->default_cell_settings->apn);
                result->default_cell_settings->operator = meadow_os_copy_string(config->default_cell_settings->operator);
                result->default_cell_settings->pap_user = meadow_os_copy_string(config->default_cell_settings->pap_user);
                result->default_cell_settings->pap_password = meadow_os_copy_string(config->default_cell_settings->pap_password);
                result->default_cell_settings->timeout = meadow_os_copy_string(config->default_cell_settings->timeout);
                result->default_cell_settings->ttyname = meadow_os_copy_string(config->default_cell_settings->ttyname);
                result->default_cell_settings->mode = meadow_os_copy_string(config->default_cell_settings->mode);
                result->default_cell_settings->module = meadow_os_copy_string(config->default_cell_settings->module); 
                result->default_cell_settings->module_id = config->default_cell_settings->module_id;
                result->default_cell_settings->scan_mode = config->default_cell_settings->scan_mode;
            }
        }
        else
        {
            result->default_cell_settings = NULL;
        }

        if (config->ntp_servers_count > 0)
        {
            result->ntp_servers = kumm_zalloc(config->ntp_servers_count * sizeof(char *));
            for (int index = 0; index < config->ntp_servers_count; index++)
            {
                config->ntp_servers[index] = meadow_os_copy_string(config->ntp_servers[index]);
            }
        }
        hcom_nx_config_unlock();
    }

    return(result);
}

/****************************************************************************
 * Name: meadow_os_config_free_resources
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
void meadow_os_config_free_resources(meadow_configuration_t *config)
{
    if (config != NULL)
    {
        kumm_free(config->mono_options);
        kumm_free(config->device_name);
        kumm_free(config->reserved_pins);
        kumm_free(config->hardware_version_text);
        kumm_free(config->os_version.short_string);
        kumm_free(config->os_version.long_string);
        kumm_free(config->os_version.branch_name);
        kumm_free(config->mono_version.short_string);
        kumm_free(config->mono_version.long_string);
        kumm_free(config->mono_version.branch_name);
        kumm_free(config->esp_version.short_string);
        kumm_free(config->esp_version.long_string);
        kumm_free(config->esp_version.branch_name);
        if (config->default_interface != NULL)
        {
            kumm_free(config->default_interface->name);
            kumm_free(config->default_interface);
        }
        if (config->default_cell_settings != NULL)
        {
            kumm_free(config->default_cell_settings->apn);
            kumm_free(config->default_cell_settings->operator);
            kumm_free(config->default_cell_settings->pap_user);
            kumm_free(config->default_cell_settings->pap_password);
            kumm_free(config->default_cell_settings->timeout);
            kumm_free(config->default_cell_settings->ttyname);
            kumm_free(config->default_cell_settings->mode);
            kumm_free(config->default_cell_settings->module);
            kumm_free(config->default_cell_settings);
        }
        if (config->ntp_servers_count > 0)
        {
            for (int index = 0; index < config->ntp_servers_count; index++)
            {
                kumm_free(config->ntp_servers[index]);
            }
            kumm_free(config->ntp_servers);
        }
        kumm_free(config);
    }
}