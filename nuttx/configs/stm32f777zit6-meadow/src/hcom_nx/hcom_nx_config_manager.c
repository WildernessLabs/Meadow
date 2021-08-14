/****************************************************************************
 * hcom_nx_config_manager.c
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

#include <ctype.h>
#include "hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/meadow_hw_version.h>
#include "../espcp/espcp_coprocessor.h"
#include "../espcp/espcp_message_dispatcher.h"
#include "../espcp/espcp_shared_enums.h"
#include <nuttx/semaphore.h>
#include <arch/board/boardctl.h>
#include "stm32_uid.h" // stm32_get_uniqueid()

#include "hcom_nx_config_manager.h"
#include "../libcyaml/cyaml.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/**
 *  @brief Structure holding information about a valid option and an indictor
 *         to show it the option is present in the configuration file.
 */
struct valid_mono_options_s
{
    /**
     * @brief Valid Mono option name.
     */
    char *option;

    /**
     *  @brief Is the option present in the configuration file.
     */
    bool used;
};
typedef struct valid_mono_options_s valid_mono_options_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 *  Variable to hold a copy of the file name for logging and reporting.
 */
static char *thisFile = __FILE__;

/**
 *  Local variable to hold a pointer to the configuration.
 */
static meadow_configuration_t *meadow_configuration = NULL;

/**
 *  Mutex to be used by any code that wants access to the configuration.
 */
static sem_t config_lock = { };

/**
 *  @brief Table of the valid options that can be passed through to Mono.
 */
static valid_mono_options_t _valid_mono_options[] = 
{
    { "--optimize=", false },
    { "--gc-params=", false },
    { "--interp", false },
    { "--v" , false }
};

/**
 *  Configuration for the CYAML library.
 */
static const cyaml_config_t cyaml_config =
{
	.log_level = CYAML_LOG_WARNING, /* Logging errors and warnings only. */
	.log_fn = cyaml_log,            /* Use the default logging function. */
	.mem_fn = cyaml_mem,            /* Use the default memory allocator. */
    .flags = CYAML_CFG_IGNORE_UNKNOWN_KEYS | CYAML_CFG_CASE_INSENSITIVE
};

/**
 *  Mono startup configuration as defined in the YAML configuration file.
 */
struct yaml_mono_control_s
{
    /**
     *  Should Mono be run in debug mode?
     */
    int debug;

    /**
     *  Should mono be run at startup?
     */
    int disable;

    /**
     *  Pointer to a string that is used to control the tracing output from Mono.
     *  For more information see https://www.mono-project.com/docs/debug+profile/debug/
     *  This variable is used in the mono_main.c file.
     */
    char *trace;

    /**
     *  Pointer to a string containing the command line options that will be
     *  passed to Mono.
     */
    char *options;
};
typedef struct yaml_mono_control_s yaml_mono_control_t;

/**
 *  Defintion of the fields in the yaml_mono_control_t structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_mono_control_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Options", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_mono_control_t, options, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Trace", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_mono_control_t, trace, 0, CYAML_UNLIMITED),
    CYAML_FIELD_UINT("Debug", CYAML_FLAG_OPTIONAL, yaml_mono_control_t, debug),
	CYAML_FIELD_UINT("Disable", CYAML_FLAG_OPTIONAL, yaml_mono_control_t, disable),
	CYAML_FIELD_END
};

/**
 *  Configuration of the coprocessor from the YAML configuration file.
 */
struct yaml_coprocessor_s
{
    /**
     *  Is a debugger attached to the ESP32?
     *
     *  The ESP32 should not be reset at startup if a debugger is attached otherwise
     *  the connection between the debugger and the ESP32 will be broken.
     */
    int debugger_attached;

    /**
     *  Clock speed of the SPI interface between the STM32 and the ESP32.
     */
    int spi_speed;

    /**
     * Automatically start the WiFi adapter?
     */
    int automatically_start_network;

    /**
     * Automatically reconnect to access point if the connection is lost.
     */
    int automatically_reconnect;

    /**
     * Maximum number of retry attempts before the system should return an error condition.
     */
    int maximum_retry_count;
};
typedef struct yaml_coprocessor_s yaml_coprocessor_t;

/**
 *  Defintion of the fields in the yaml_coprocessor_s structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_coprocessor_section_schema[] =
{
	CYAML_FIELD_UINT("DebuggerAttached", CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, debugger_attached),
	CYAML_FIELD_UINT("SpiSpeed", CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, spi_speed),
	CYAML_FIELD_UINT("AutomaticallyStartNetwork", CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_start_network),
	CYAML_FIELD_UINT("AutomaticallyReconnect", CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_reconnect),
	CYAML_FIELD_UINT("MaximumRetryCount", CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, maximum_retry_count),
	CYAML_FIELD_END
};

/**
 * Network configuration section of the configuration file.
 */
struct yaml_network_s
{
    /**
     * Indicate if we should get the network time at startup.
     */
    int get_network_time_at_startup;

    /**
     * Name of the network time server.
     */
    char *ntp_server;
};
typedef struct yaml_network_s yaml_network_t;

/**
 *  Defintion of the fields in the yaml_network_s structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_network_section_schema[] =
{
	CYAML_FIELD_UINT("GetNetworkTimeAtStartup", CYAML_FLAG_OPTIONAL, yaml_network_t, get_network_time_at_startup),
    CYAML_FIELD_STRING_PTR("NtpServer", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, ntp_server, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  Debugging (internal) configuration options from the YAML file.
 */
struct yaml_debug_s
{
    /**
     *  Level of trace output to generate.
     */
    int trace_level;

    /**
     *  Should trace output be diverted to UART1?
     */
    char *uart1_use;
};
typedef struct yaml_debug_s yaml_debug_t;

/**
 *  Defintion of the fields in the yaml_debug_s structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_debug_section_schema[] =
{
	CYAML_FIELD_UINT("TraceLevel", CYAML_FLAG_OPTIONAL, yaml_debug_t, trace_level),
    CYAML_FIELD_STRING_PTR("Uart1Use", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_debug_t, uart1_use, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  This is a local definition of the configuration and it is aimed to be
 *  used by the CYAML library when reading the configuration data from the
 *  meadow.yaml configuration file.
 * 
 *  This additional structure is used as some of the configuration
 *  information in the globally available structure is derived from the
 *  chip / board.
 */
struct yaml_configuration_s
{
    /**
     *  Debug configuration options.
     */
    yaml_debug_t *debug;

    /**
     *  Coprocessor configuration.
     */
    yaml_coprocessor_t *coprocessor;

    /**
     *  Network configuration
     */
    yaml_network_t *network;

    /**
     *  Mono control configuration.
     */
    yaml_mono_control_t *mono_control;

    /*
     *  Name of the board.
     */
    char *device_name;
};
typedef struct yaml_configuration_s yaml_configuration_t;

/**
 *  Definition of the fields in the struct configuration_s structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_fields_schema[] =
{
    CYAML_FIELD_STRING_PTR("DeviceName", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, device_name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_MAPPING_PTR("Debug", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, debug, configuration_debug_section_schema),
    CYAML_FIELD_MAPPING_PTR("Coprocessor", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, coprocessor, configuration_coprocessor_section_schema),
    CYAML_FIELD_MAPPING_PTR("Network", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, network, configuration_network_section_schema),
    CYAML_FIELD_MAPPING_PTR("MonoControl", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, mono_control, configuration_mono_control_section_schema),
	CYAML_FIELD_END
};

/**
 *  Top level schema for the data from the YAML configuration file is a mapping.
 */
static const cyaml_schema_value_t configuration_schema =
{
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, yaml_configuration_t, configuration_fields_schema)
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static char **hcom_nx_config_extract_mono_options(char *);
static bool hcom_nx_config_string_starts_with(const char *, const char *);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_nx_config_lock
 *
 * Description:
 *  Lock the specified configuration object.
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
void hcom_nx_config_lock(void)
{
    sem_wait(&config_lock);
}

/****************************************************************************
 * Name: hcom_nx_config_unlock
 *
 * Description:
 *  Unlock the specified configuration object.
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
void hcom_nx_config_unlock(void)
{
    sem_post(&config_lock);
}

/****************************************************************************
 * Name: hcom_nx_config_get_pointer
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
meadow_configuration_t *hcom_nx_config_get_pointer(void)
{
    return meadow_configuration;
}

/****************************************************************************
 * Name: hcom_nx_config_is_valid_host_name
 *
 * Description:
 *  Validate the host name against the following rules:
 *  - Host name must be less than HOST_NAME_MAX characters.
 *  - Host name must start with a letter.
 *  - Host name must only contain the following characters: a-z A-Z 0-9 - _
 *
 * Input Parameters:
 *  host_name - The host name to be validated.
 *
 * Returned Value:
 *  1 if the host name is valid, 0 otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_is_valid_host_name(const char *host_name)
{
    bool result = 1;

    if ((host_name != NULL) && (strlen(host_name) <= HOST_NAME_MAX) && (strlen(host_name) > 0) && isalpha(host_name[0]))
    {
        for (const char *ch = host_name; *ch != 0; ch++)
        {
            if (!(isalnum(*ch) || (*ch == '-') || (*ch == '_')))
            {
                result = 0;
                break;
            }
        }
    }
    else
    {
        result = 0;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_esp_value
 *
 * Description:
 *  Set a value on the ESP32.
 * 
 *  This method will block until the ESP32 confirms that the value has been
 *  set correctly.
 *
 * Input Parameters:
 *  item - type of item to be set.
 *  value - pointer to the value to be set.
 *  value_size - size of the value.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_set_esp_value(espcp_configuration_items_t item, uint8_t *value, uint32_t value_size)
{
    espcp_configuration_value_t item_value = { };

    item_value.value = malloc(value_size);
    if (item_value.value == NULL)
    {
        return ERROR;        
    }
    memcpy(item_value.value, value, value_size);
    item_value.item = item;
    item_value.value_length = value_size;
    int payload_length = espcp_configuration_value_buffer_size(&item_value);
    uint8_t *payload = malloc(payload_length);
    if (payload == NULL)
    {
        free(item_value.value);
        return(ERROR);
    }
    espcp_encode_configuration_value(&item_value, payload);
    free(item_value.value);
    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system,
                                                            espcp_system_function_set_configuration_item, espcp_status_codes_completed_ok,
                                                            espcp_get_next_message_id(), payload, payload_length);
    if (message == NULL)
    {
        free(payload);
        return(ERROR);
    }

    int result = ERROR;
    if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
    {
        if (message->status_code == espcp_status_codes_completed_ok)
        {
            result = OK;
        }
    }
    espcp_delete_message_and_payload(message);
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_esp_integer_value
 *
 * Description:
 *  Set an integer value on the ESP32.
 * 
 * Input Parameters:
 *  item - type of item to be set.
 *  value - value to be set.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_set_esp_integer_value(espcp_configuration_items_t item, uint32_t value)
{
    return(hcom_nx_config_set_esp_value(item, (uint8_t *) &value, 4));
}

/****************************************************************************
 * Name: hcom_nx_config_set_esp_boolean_value
 *
 * Description:
 *  Set a boolean value on the ESP32.
 * 
 * Input Parameters:
 *  item - type of item to be set.
 *  value - value to be set.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_t item, uint8_t value)
{
    return(hcom_nx_config_set_esp_value(item, (uint8_t *) &value, 1));
}

/****************************************************************************
 * Name: hcom_nx_config_set_device_name
 *
 * Description:
 *  Set the a string configuration value on the ESP32.
 * 
 * Input Parameters:
 *  item - type of item to be set.
 *  value - value to be set.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_set_esp_string_value(espcp_configuration_items_t item, const char *value)
{
    int result;

    if (value != NULL)
    {
        result = hcom_nx_config_set_esp_value(item, (uint8_t *) value, strlen(value) + 1);
    }
    else
    {
        result = ERROR;
    }
    return(result);
}   

/****************************************************************************
 * Name: hcom_nx_config_set_device_name
 *
 * Description:
 *  Set the device name.
 * 
 *  If the device name is invalid then set the device name to the default
 *  value (MeadowF7).
 *
 * Input Parameters:
 *  config - pointer to the configuration structure.
 *  device_name - New name for the device.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_set_device_name(meadow_configuration_t *config, const char *device_name)
{
    char *new_name = NULL;
    if (!hcom_nx_config_is_valid_host_name(device_name))
    {
        new_name = strdup(MEADOW_CONFIG_DEFAULT_DEVICE_NAME);
    }
    else
    {
        new_name = strdup(device_name);
    }
    if (config->device_name != NULL)
    {
        free(config->device_name);
    }
    config->device_name = new_name;
    sethostname(config->device_name, strlen(config->device_name));
}

/****************************************************************************
 * Name: hcom_nx_config_read_file
 *
 * Description:
 *  Read the current configuration from flash and populate the configuration
 *  object.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  Pointer to the newly populated configuration object or NULL if there was
 *  a problem.
 *
 * Assumptions/Limitations:
 *  This method is only allowed to be called once to read the configuration
 *  at startup.  Duplicate calls to this method simply return the current
 *  value of the meadow_configuration pointer and take no other action.
 *
 ****************************************************************************/
static meadow_configuration_t *hcom_nx_config_read_file(void)
{
    hcom_nx_config_lock();
    if (meadow_configuration == NULL)
    {
        meadow_configuration = (meadow_configuration_t *) malloc(sizeof(meadow_configuration_t));
        if (meadow_configuration != NULL)
        {
        	yaml_configuration_t *configuration;

            memset(meadow_configuration, 0, sizeof(meadow_configuration_t));
            cyaml_err_t err = cyaml_load_file(MEADOW_CONFIG_DEFAULT_FILE_NAME, &cyaml_config, &configuration_schema, (void **) &configuration, NULL);
            if (err != CYAML_OK)
            {
                meadow_configuration->using_default_configuration = 1;
                meadow_configuration->reset_esp32_at_startup = 1;
                meadow_configuration->esp_spi_speed = 8000000;
                meadow_configuration->maximum_retry_count = 3;
                syslog(LOG_INFO, "%s@%d Unable to process configuration file, using system defaults.\n", thisFile, __LINE__);
            }
            else
            {
                if (configuration->mono_control != NULL)
                {
                    if (configuration->mono_control->trace != NULL)
                    {
                        meadow_configuration->mono_trace = strdup(configuration->mono_control->trace);
                    }
                    meadow_configuration->mono_debug = configuration->mono_control->debug;
                    meadow_configuration->disable_mono = configuration->mono_control->disable;
                    if (configuration->mono_control->options != NULL)
                    {
                        meadow_configuration->mono_options = hcom_nx_config_extract_mono_options(configuration->mono_control->options);
                    }
                }
                //
                if (configuration->coprocessor != NULL)
                {
                    meadow_configuration->reset_esp32_at_startup = !configuration->coprocessor->debugger_attached;
                    meadow_configuration->esp_spi_speed = configuration->coprocessor->spi_speed;
                    meadow_configuration->automatically_reconnect = configuration->coprocessor->automatically_reconnect;
                    meadow_configuration->automatically_start_network = configuration->coprocessor->automatically_start_network;
                }
                else 
                {
                    meadow_configuration->reset_esp32_at_startup = 1;
                    meadow_configuration->esp_spi_speed = 8000000;
                }
                if (configuration->network != NULL)
                {
                    meadow_configuration->get_network_time_at_startup = configuration->network->get_network_time_at_startup;
                    if (configuration->network->ntp_server != NULL)
                    {
                        meadow_configuration->ntp_server = strdup(configuration->network->ntp_server);
                    }
                }
                if (configuration->debug != NULL)
                {
                    meadow_configuration->trace_level = configuration->debug->trace_level;
                    meadow_configuration->use_uart1_for_trace = (strcmp(configuration->debug->uart1_use, "trace") == 0);
                }
                //
                if (configuration->device_name != NULL)
                {
                    meadow_configuration->device_name = strdup(configuration->device_name);
                }
                //
                meadow_configuration->esp_software_version = NULL;
            }

            cyaml_free(&cyaml_config, &configuration_schema, configuration, 0);
        }
    }

    hcom_nx_config_unlock();
    return(meadow_configuration);
}

/****************************************************************************
 * Name: hcom_nx_config_copy_string
 *
 * Description:
 *  Copy a string into the buffer and return the amount of storage used to
 *  store the string and its terminating 0.
 *
 * Input Parameters:
 *  source - String to be copied.
 *  destination - Memory to hold the copy of the string.
 *
 * Returned Value:
 *  Amount of memory consumed by the string and its terminating 0.
 *
 * Assumptions/Limitations:
 *  The destination buffer is large enough to hold tha copy of the string.
 *
 ****************************************************************************/
int hcom_nx_config_copy_string(char *source, char *destination)
{
    int length = 0;

    if (source == NULL)
    {
        *destination = 0;
    }
    else
    {
        length = strlen(source);
        strcpy(destination, source);
    }

    return(length + 1);
}

/****************************************************************************
 * Name: hcom_nx_config_copy_for_user_mode
 *
 * Description:
 *  Copy the configuration data into the specified location along with copies
 *  of any strings.
 *
 * Input Parameters:
 *  buffer - area of memory to hold the copy of the data in the configuration
 *           structure plus the string.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_copy_for_user_mode(uint8_t *buffer, int length)
{
    if (buffer == NULL)
    {
        return ERROR;
    }

    int result = OK;
    hcom_nx_config_lock();

    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    int storage_required = sizeof(meadow_configuration_t);
    if (config->mono_trace != NULL)
    {
        storage_required += strlen(config->mono_trace) + 1;
    }
    if (config->device_name != NULL)
    {
        storage_required += strlen(config->device_name) + 1;
    }
    if (config->meadow_software_version != NULL)
    {
        storage_required += strlen(config->meadow_software_version) + 1;
    }
    if (config->meadow_hardware_version != NULL)
    {
        storage_required += strlen(config->meadow_hardware_version) + 1;
    }
    if (config->esp_software_version != NULL)
    {
        storage_required += strlen(config->esp_software_version) + 1;
    }
    if (config->ntp_server != NULL)
    {
        storage_required += strlen(config->ntp_server) + 1;
    }

    storage_required += sizeof(config->chip_id) + sizeof(config->serial_number);
    if (length < storage_required)
    {
        hcom_nx_config_unlock();
        result = ERROR;
    }
    else
    {
        memset((void *) buffer, 0, length);
        meadow_configuration_t *new_config = (meadow_configuration_t *) buffer;

        memcpy((void *) new_config, (void *) config, sizeof(meadow_configuration_t));
        //
        //  Put the strings at the end of the configuration structure.
        //
        char *ptr = (char *) (buffer + sizeof(meadow_configuration_t));
        new_config->mono_trace = ptr;
        ptr += hcom_nx_config_copy_string(config->mono_trace, ptr);
        new_config->meadow_software_version = ptr;
        ptr += hcom_nx_config_copy_string(config->meadow_software_version, ptr);
        new_config->meadow_hardware_version = ptr;
        ptr += hcom_nx_config_copy_string(config->meadow_hardware_version, ptr);
        new_config->esp_software_version = ptr;
        ptr += hcom_nx_config_copy_string(config->esp_software_version, ptr);
        new_config->device_name = ptr;
        ptr += hcom_nx_config_copy_string(config->device_name, ptr);
        new_config->ntp_server = ptr;
        ptr += hcom_nx_config_copy_string(config->ntp_server, ptr);
    }    
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_string_value
 *
 * Description:
 *  Get a string configuration value and copy it to the destination buffer.
 *
 * Input Parameters:
 *  source - configuration string to be copied.
 *  destination - destination buffer to hold the string.
 *  dest_length - length of the destination buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_string_value(char *source, uint8_t *destination, int destination_length)
{
    if ((strlen(source) + 1) > destination_length)
    {
        return ERROR;
    }
    return(strlen(strcpy((char *) destination, source)));
}

/****************************************************************************
 * Name: hcom_nx_config_get_bytes
 *
 * Description:
 *  Get a string configuration value and copy it to the destination buffer.
 *
 * Input Parameters:
 *  source - configuration string to be copied.
 *  source_length - length of the source buffer.
 *  destination - destination buffer to hold the string.
 *  dest_length - length of the destination buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_bytes(uint8_t *source, int source_length, uint8_t *destination, int destination_length)
{
    int result;

    if (source_length > destination_length)
    {
        result = ERROR;
    }
    else
    {
        memcpy(destination, source, source_length);
        result = source_length;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_unique_id
 *
 * Description:
 *  get the unique ID from the config and format this into a string.
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *  buffer - Buffer to hold the unique ID string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  The config object is locked and released by the caller.
 *
 ****************************************************************************/
static int hcom_nx_config_get_unique_id(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result;

    if (buffer_length > 35)
    {
        result = snprintf((char *) buffer, buffer_length, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
                    config->serial_number[0], config->serial_number[1], config->serial_number[2],
                    config->serial_number[3], config->serial_number[4], config->serial_number[5], 
                    config->serial_number[6], config->serial_number[7], config->serial_number[8],
                    config->serial_number[9], config->serial_number[10], config->serial_number[11]);
    }   
    else
    {
        result = ERROR;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_get_serial_number
 *
 * Description:
 *  get the chip serial number from the config and format this into a string.
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *  buffer - Buffer to hold the serial number string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  The config object is locked and released by the caller.
 *
 ****************************************************************************/
int hcom_nx_config_get_serial_number(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result;

    if (buffer_length > 18)
    {
        result = snprintf((char *) buffer, buffer_length, "%02X%02X%02X%02X%02X%02X", config->chip_id[0], config->chip_id[1],
                            config->chip_id[2], config->chip_id[3], config->chip_id[4], config->chip_id[5]);
    }
    else
    {
        result = ERROR;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_get_config_coprocessor_firmware_version
 *
 * Description:
 *  Get the coprocessor firmware version from the config and format this
 *  into a string.
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *  buffer - Buffer to hold the coprocessor firmware version string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  The config object is locked and released by the caller.
 *
 ****************************************************************************/
static int hcom_nx_config_get_coprocessor_firmware_version(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result;

    if (config->esp_software_version != NULL)
    {
        result = hcom_nx_config_get_string_value(config->esp_software_version, buffer, buffer_length);
    }
    else
    {
        if (buffer_length > 7)
        {
            result = snprintf((char *) buffer, buffer_length, "Unknown");
        }
        else
        {
            result = ERROR;
        }
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_get_mono_version
 *
 * Description:
 *  Get the Mono version from the config and format this into a string.
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *  buffer - Buffer to hold the Mono version string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  The config object is locked and released by the caller.
 *
 ****************************************************************************/
static int hcom_nx_config_get_mono_version(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result;

    if (buffer_length > 16)
    {
        result = snprintf((char *) buffer, buffer_length, "%d.%d.%d.%d", (config->mono_version >> 24) & 0xff, (config->mono_version >> 16) & 0xff,
                            (config->mono_version >> 8) & 0xff, config->mono_version & 0xff);
    }
    else
    {
        result = ERROR;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_automatically_connect_to_network
 *
 * Description:
 *  Get the AutomaticallyConnectToNetwork property from the ESP configuration.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the Mono version string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_get_automatically_connect_to_network(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        if (config != NULL)
        {
            *buffer = config->automatically_start_network ? 1 : 0;
            result = 1;
        }
        hcom_nx_config_unlock();
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_automatically_reconnect
 *
 * Description:
 *  Get the AutomaticallyReconnect property from the ESP configuration.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the Mono version string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_get_automatically_reconnect(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        if (config != NULL)
        {
            *buffer = config->automatically_reconnect ? 1 : 0;
            result = 1;
        }
        hcom_nx_config_unlock();
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_get_time_at_startup
 *
 * Description:
 *  Get the GetTimeAtStartup property from the ESP configuration.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the Mono version string.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_get_get_time_at_startup(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        if (config != NULL)
        {
            *buffer = config->get_network_time_at_startup ? 1 : 0;
            result = 1;
        }
        hcom_nx_config_unlock();
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_ntp_server
 *
 * Description:
 *  Get the address of any configured NTP server.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the NTP server name
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_ntp_server(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if ((config != NULL) && (config->ntp_server != NULL))
    {
        result = hcom_nx_config_get_string_value(config->ntp_server, buffer, buffer_length);
    }
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_maximum_retry_count
 *
 * Description:
 *  Get the maximum number of times a retry operation will be attempted.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the maximum retry count.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_maximum_retry_count(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if (config != NULL)
    {
        result = hcom_nx_config_get_bytes((uint8_t *) &config->maximum_retry_count, sizeof(int), buffer, buffer_length);
    }
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hxom_nx_config_get_board_mac_address
 *
 * Description:
 *  Get the MAC address of the board (ESP32 MAC address).
 *
 * Input Parameters:
 *  buffer - Buffer to hold the MAC address
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_board_mac_address(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if (config != NULL)
    {
        result = hcom_nx_config_get_bytes(config->board_mac_address, sizeof(config->board_mac_address), buffer, buffer_length);
    }
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_soft_ap_mac_address
 *
 * Description:
 *  Get the soft access point MAC address of the ESP32 chip.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the soft access point MAC address
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_soft_ap_mac_address(uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if (config != NULL)
    {
        result = hcom_nx_config_get_bytes(config->soft_ap_mac_address, sizeof(config->soft_ap_mac_address), buffer, buffer_length);
    }
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_set_config_value
 *
 * Description:
 *  Read or write a configuration value.
 *
 * Input Parameters:
 *  item - Value to be accessed.
 *  direction - Read (0) or write (1).
 *  buffer - Buffer to hold the value when reading, or holding the new value
 *           when writing.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int hcom_nx_config_get_set_config_value(int item, uint8_t direction, uint8_t *buffer, int buffer_length)
{
    int result;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    switch (item)
    {
        case cv_device_name:
            result = hcom_nx_config_get_string_value(config->device_name, buffer, buffer_length);
            break;
        case cv_product:
            result = hcom_nx_config_get_string_value(config->meadow_hardware_version, buffer, buffer_length);
            break;
        case cv_model:
            result = hcom_nx_config_get_string_value(HCOM_DEVICE_INFO_MODEL, buffer, buffer_length);
            break;
        case cv_os_version:
            result = hcom_nx_config_get_string_value(config->meadow_software_version, buffer, buffer_length);
            break;
        case cv_build_date:
            result = hcom_nx_config_get_string_value(__DATE__ " " __TIME__, buffer, buffer_length);
            break;
        case cv_processor_type:
            result = hcom_nx_config_get_string_value(HCOM_DEVICE_INFO_PROCESSOR_TYPE, buffer, buffer_length);
            break;
        case cv_unique_id:
            result = hcom_nx_config_get_unique_id(config, buffer, buffer_length);
            break;
        case cv_serial_number:
            result = hcom_nx_config_get_serial_number(config, buffer, buffer_length);
            break;
        case cv_coprocessor_type:
            result = hcom_nx_config_get_string_value(HCOM_DEVICE_INFO_COPROCESSOR_TYPE, buffer, buffer_length);
            break;
        case cv_coprocessor_firmware_version:
            result = hcom_nx_config_get_coprocessor_firmware_version(config, buffer, buffer_length);
            break;
        case cv_mono_version:
            result = hcom_nx_config_get_mono_version(config, buffer, buffer_length);
            break;
        case cv_automatically_start_network:
            result = hcom_nx_config_get_automatically_connect_to_network(buffer, buffer_length);
            break;
        case cv_automatically_reconnect:
            result = hcom_nx_config_get_automatically_reconnect(buffer, buffer_length);
            break;
        case cv_maximum_network_retry_count:
            result = hcom_nx_config_get_maximum_retry_count(buffer, buffer_length);
            break;
        case cv_get_time_at_startup:
            result = hcom_nx_config_get_get_time_at_startup(buffer, buffer_length);
            break;
        case cv_ntp_server:
            result = hcom_nx_config_get_ntp_server(buffer, buffer_length);
            break;
        case cv_mac_address:
            result = hcom_nx_config_get_board_mac_address(buffer, buffer_length);
            break;
        case cv_soft_ap_mac_address:
            result = hcom_nx_config_get_soft_ap_mac_address(buffer, buffer_length);
            break;
        default:
            result = ERROR;
            break;
    }
    hcom_nx_config_unlock();
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_process_esp_configuration
 *
 * Description:
 *  Compare the incooming ESP configuration with the current configuration
 *  read from the configuration file.  The configuration file is considered
 *  to be the source of truth.
 * 
 *  Send any differences over to the ESP32.
 *
 * Input Parameters:
 *  esp_config - Configuration stored in the ESP32.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_process_esp_configuration(espcp_system_configuration_t *esp_config)
{
    hcom_nx_config_lock();
    meadow_configuration_t *configuration = hcom_nx_config_get_pointer();
    if (configuration != NULL)
    {
        if (configuration->automatically_start_network != esp_config->automatically_start_network)
        {
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_start_network, configuration->automatically_start_network == 1);
        }
        if (configuration->automatically_reconnect != esp_config->automatically_reconnect)
        {
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_reconnect, configuration->automatically_reconnect == 1);
        }
        if (configuration->maximum_retry_count != esp_config->maximum_retry_count)
        {
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_maximum_retry_count, configuration->maximum_retry_count);
        }
        if ((esp_config->device_name != NULL) && (strcmp(configuration->device_name, esp_config->device_name) != 0))
        {
            hcom_nx_config_set_esp_string_value(espcp_configuration_items_device_name, configuration->device_name);
        }
        if (configuration->get_network_time_at_startup != esp_config->get_time_at_startup)
        {
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_get_time_at_startup, configuration->get_network_time_at_startup == 1);
        }
        if (configuration->ntp_server != NULL)
        {
            if ((esp_config->ntp_server == NULL) || (strcmp(configuration->ntp_server, esp_config->ntp_server) != 0))
            {
                hcom_nx_config_set_esp_string_value(espcp_configuration_items_ntp_server, configuration->ntp_server);
            }
        }
        else
        {
            if ((esp_config->ntp_server != NULL) && (strlen(esp_config->ntp_server) != 0))
            {
                char null_str = '\0';
                hcom_nx_config_set_esp_string_value(espcp_configuration_items_ntp_server, &null_str);
            }
        }
        if (esp_config->software_version != NULL)
        {
            if (configuration->esp_software_version != NULL)
            {
                free(configuration->esp_software_version);
            }
            configuration->esp_software_version = strdup(esp_config->software_version);
        }
        else
        {
            configuration->esp_software_version = NULL;
        }
    }
    hcom_nx_config_unlock();
}

/****************************************************************************
 * Name: hcom_nx_config_init
 *
 * Description:
 *  Setup the configuration system and populate the configuration structure
 *  with data from the configuration file.
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
void hcom_nx_config_init(void)
{
    sem_init(&config_lock, 0, 1);                   // Create the config lock.
    sem_setprotocol(&config_lock, SEM_PRIO_NONE);
    hcom_nx_config_read_file();

    uint32_t mono_version = 0;

    boardctl(BIOC_ENTER_MEMMAP, 0);

    // Check if Meadow.OS runtime is flashed at external flash.
    #define STM32_FMCBANK4_BASE  0x90000000     /* 0x90000000-0x9fffffff: FMC bank 4 */
    uint32_t signature = *((uint32_t *) STM32_FMCBANK4_BASE);
    if (signature != 0xDDCCBBAA)
    {
        syslog(LOG_ERR, "Mono runtime was not found flashed in external flash.\n");
    }
    else
    {
        mono_version = *((uint32_t *) (STM32_FMCBANK4_BASE + 4));
    }
    boardctl(BIOC_EXIT_MEMMAP, 0);

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    hcom_nx_config_set_device_name(config, config->device_name);
    config->mono_version = mono_version;
    config->meadow_software_version = HCOM_DEVICE_INFO_MEADOW_OS_VERSION;
    config->meadow_hardware_version = meadow_hw_version_string_return();

    stm32_get_uniqueid(config->serial_number);                           // Convert chip Id to serial number
    config->chip_id[0] = config->serial_number[11];                      // 95-88
    config->chip_id[1] = config->serial_number[10] + config->serial_number[2];        // 87-80 + 23-16
    config->chip_id[2] = config->serial_number[9];                       // 79-72
    config->chip_id[3] = config->serial_number[8] + config->serial_number[0] + 10;    // 71-64 + 7-0 + magic 10
    config->chip_id[4] = config->serial_number[7];                       // 63-56 
    config->chip_id[5] = config->serial_number[6];                       // 55-48

    hcom_nx_config_unlock();
}

/****************************************************************************
 * Name: hcom_nx_config_string_starts_with
 *
 * Description:
 *  Check if a string starts with another specified string.
 * 
 * Input Parameters:
 *  string - String to be checked.
 *  prefix - Prefix to be match against.
 *
 * Returned Value:
 *  true if there is a match, false otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static bool hcom_nx_config_string_starts_with(const char *string, const char *prefix)
{
    bool result = false;
    if (string != NULL)
    {
        size_t prefix_len = strlen(prefix);
        if (strlen(string) >= prefix_len)
        {
            result = (strncmp(string, prefix, prefix_len) == 0);
        }
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_extract_options
 *
 * Description:
 *  Create an array of pointers to the options to be passed to mono.
 * 
 *  The options are validated against the list of allowed options and only
 *  those options in the valid list will be passed added to the 
 *
 * Input Parameters:
 *  options - string of options.
 *
 * Returned Value:
 *  Array of pointers to the individual options.  This may also be a NULL
 *  pointer.
 *
 * Assumptions/Limitations:
 *  List of valid option prefixes is configured (see the head of this file).
 *
 ****************************************************************************/
static char **hcom_nx_config_extract_mono_options(char *options)
{
    char **result = NULL;

    // if ((options != NULL) && (strlen(options) > 0) && (sizeof(_valid_mono_options) > 0))
    // {
    //     char *copy = strdup(options);
    //     if (copy != NULL)
    //     {
    //         char *start = copy;
    //         char *end = copy;
    //         bool found_last = false;
    //         int option_count = 0;
    //         while (!found_last)
    //         {
    //             while ((*end != ' ') && (*end != '\0'))
    //             {
    //                 end++;
    //             }
    //             if (*end == 0)
    //             {
    //                 found_last = true;
    //             }
    //             else
    //             {
    //                 *end = 0;
    //             }
    //             for (int index = 0; index < (sizeof(_valid_mono_options) / sizeof(_valid_mono_options[0])); index++)
    //             {
    //                 if (hcom_nx_config_string_starts_with(start, _valid_mono_options[index].option))
    //                 {
    //                     _valid_mono_options[index].used = true;
    //                     option_count++;
    //                     break;
    //                 }
    //             }
    //             start = end + 1;
    //         }
    //         free(copy);

    //         if (option_count > 0)
    //         {
    //             result = calloc(option_count + 1, sizeof(char *));
    //             if (result != NULL)
    //             {
    //                 int current_slot = 0;
    //                 for (int index = 0; index < (sizeof(_valid_mono_options) / sizeof(_valid_mono_options[0])); index++)
    //                 {
    //                     if (_valid_mono_options[index].used)
    //                     {
    //                         result[current_slot] = _valid_mono_options[index].option;
    //                         current_slot++;
    //                     }
    //                 }
    //                 result[option_count] = NULL;
    //             }
    //         }
    //     }
    // }

    return(result);
}
