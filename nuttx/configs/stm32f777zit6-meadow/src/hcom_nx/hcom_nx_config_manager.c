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
#include <nuttx/config.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <nuttx/semaphore.h>
#include <arch/board/boardctl.h>

#include "hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/meadow_hw_version.h>
#include "../espcp/espcp_coprocessor.h"
#include "../espcp/espcp_message_dispatcher.h"
#include "../espcp/espcp_shared_enums.h"
#include "stm32_uid.h" // stm32_get_uniqueid()

#include "hcom_nx_common.h"

#include "hcom_nx_config_manager.h"
#include "../libcyaml/cyaml.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 *  Local variable to hold a pointer to the configuration.
 */
static meadow_configuration_t *meadow_configuration = NULL;

/**
 *  Mutex to be used by any code that wants access to the configuration.
 */
static sem_t config_lock = { };

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
 *  Schema for string pointer values (used in sequences of strings).
 * 
 *  This is used in the DNS and NTP server sequences. 
 */
static const cyaml_schema_value_t string_ptr_schema =
{
	CYAML_VALUE_STRING(CYAML_FLAG_POINTER, char, 0, CYAML_UNLIMITED),
};

/**
 *  Device configuration options from the YAML file.
 */
struct yaml_device_s
{
    /**
     *  Name of the device.
     */
    char *name;
};
typedef struct yaml_device_s yaml_device_t;

/**
 *  Defintion of the fields in the yaml_debug_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_device_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Name", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, name, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  Mono startup configuration as defined in the YAML configuration file.
 */
struct yaml_mono_control_s
{
    /**
     *  Should mono be run at startup?
     */
    int disable;

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
    char *automatically_start_network;

    /**
     * Automatically reconnect to access point if the connection is lost.
     */
    char *automatically_reconnect;

    /**
     * Maximum number of retry attempts before the system should return an error condition.
     */
    char *maximum_retry_count;
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
    CYAML_FIELD_STRING_PTR("AutomaticallyStartNetwork", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_start_network, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("AutomaticallyReconnect", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_reconnect, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("MaximumRetryCount", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, maximum_retry_count, 0, CYAML_UNLIMITED),
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
    char *get_network_time_at_startup;

    /**
     * Indicate how often the time should be refreshed.
     */
    char *ntp_refresh_period;

    /**
     *  Name of the network time servers along with the number of NTP servers
     *  in the config file.
     */
    const char **ntp_servers;
    unsigned ntp_servers_count;

    /**
     *  IP addresses of the DNS servers along with the number of DNS servers
     *  in the config file.
     */
    const char **dns_servers;
    unsigned dns_servers_count;
};
typedef struct yaml_network_s yaml_network_t;

/**
 *  Defintion of the fields in the yaml_network_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_network_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("GetNetworkTimeAtStartup", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, get_network_time_at_startup, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("NtpRefreshPeriod", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, ntp_refresh_period, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE("NtpServers", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, ntp_servers, &string_ptr_schema, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE("DnsServers", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, dns_servers, &string_ptr_schema, 0, CYAML_UNLIMITED),
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
     *  Information about the device.
     */
    yaml_device_t *device;
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
};
typedef struct yaml_configuration_s yaml_configuration_t;

/**
 *  Definition of the fields in the struct configuration_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_fields_schema[] =
{
    CYAML_FIELD_MAPPING_PTR("Device", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, device, configuration_device_section_schema),
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

/**
 *  Device configuration options from the YAML file.
 */
struct yaml_credentials_s
{
    /**
     *  Name of the network access point to connect to.
     */
    char *ssid;

    /**
     *  Password for the network access point.
     */
    char *password;
};
typedef struct yaml_credentials_s yaml_credentials_t;

/**
 *  Defintion of the fields in the yaml_debug_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t wifi_credentials_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Ssid", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_credentials_t, ssid, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Password", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_credentials_t, password, 0, CYAML_UNLIMITED),
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
struct yaml_wifi_credentials_s
{
    /**
     *  Information about the device.
     */
    yaml_credentials_t *credentials;
};
typedef struct yaml_wifi_credentials_s yaml_wifi_credentials_t;

/**
 *  Definition of the fields in the struct configuration_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t wifi_credentials_fields_schema[] =
{
    CYAML_FIELD_MAPPING_PTR("Credentials", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_wifi_credentials_t, credentials, wifi_credentials_section_schema),
	CYAML_FIELD_END
};

/**
 *  Top level schema for the data from the YAML configuration file is a mapping.
 */
static const cyaml_schema_value_t wifi_credentials_schema =
{
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, yaml_wifi_credentials_t, wifi_credentials_fields_schema)
};

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
 * Name: hcom_nx_config_set_esp_string_value
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
 * Name: hcom_nx_config_set_host_name
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
void hcom_nx_config_set_host_name(meadow_configuration_t *config, const char *device_name)
{
    char *new_name = NULL;
    if (!hcom_nx_config_is_valid_host_name(device_name))
    {
        new_name = hcom_nx_common_utils_strdup(MEADOW_CONFIG_DEFAULT_DEVICE_NAME);
    }
    else
    {
        new_name = hcom_nx_common_utils_strdup(device_name);
    }
    if (config->device_name != NULL)
    {
        free(config->device_name);
    }
    config->device_name = new_name;
    sethostname(config->device_name, strlen(config->device_name));
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
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer to hold the NTP server name
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_set_device_name(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer[buffer_length] == 0)
    {
        result = hcom_nx_config_set_esp_string_value(espcp_configuration_items_device_name, (char *) buffer);
        if (result == OK)
        {
            if (config->device_name != NULL)
            {
                free(config->device_name);
            }
            config->device_name = hcom_nx_common_utils_strdup((char *) buffer);
        }
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_parse_boolean
 *
 * Description:
 *  Parse the string given and return 0 or 1 value (for false / true).
 * 
 *  The default_value is returned if an error occurs. 
 * 
 * Input Parameters:
 *  config_value - pointer to a string in the config file.
 *  default_value - Default value to be used
 *
 * Returned Value:
 *  0 - false
 *  1 - true
 *
 * Assumptions/Limitations:
 *  None.
 ****************************************************************************/
static uint8_t hcom_nx_config_parse_boolean(const char *config_value, uint32_t default_value)
{
    uint8_t value = default_value;

    if (config_value != NULL)
    {
        char *lowercase = malloc(strlen(config_value) + 1);

        for (int index = 0; index < strlen(config_value); index++)
        {
            lowercase[index] = tolower(config_value[index]);
        }
        lowercase[strlen(config_value)] = 0;
        if ((strcmp(lowercase, "true") == 0) || (strcmp(lowercase, "yes") == 0) || (config_value[0] == '1'))
        {
            value = 1;
        }
        else
        {
            if ((strcmp(lowercase, "false") == 0) || (strcmp(lowercase, "no") == 0) || (config_value[0] == '0'))
            {
                value = 0;
            }
        }
        free(lowercase);
    }

    return(value);
}

/****************************************************************************
 * Name: hcom_nx_config_parse_unsigned_integer
 *
 * Description:
 *  Safely convert the number represented as a string into an unsigned integer.
 *  object.
 * 
 * Input Parameters:
 *  number - String to be converted.
 *  default_value - Default value to be used
 *
 * Returned Value:
 *  Number as an unsigned integer if it can be safely converted or the
 *  default_value if there is a problem.
 *
 * Assumptions/Limitations:
 *  None.
 ****************************************************************************/
static uint32_t hcom_nx_config_parse_unsigned_integer(const char *config_value, uint32_t default_value)
{
    uint32_t value = default_value;

    if (config_value != NULL)
    {
        if (strspn(config_value, "0123456789") == strlen(config_value))
        {
            long l = atol(config_value);
            if (l <= UINT32_MAX)
            {
                value = (uint32_t) (l & 0xffffffff);
            }
        }
    }

    return(value);
}

/****************************************************************************
 * Name: hcom_nx_config_is_valid_ip_address
 *
 * Description:
 *  Determine if an IP address is valid or not.
 *
 * Input Parameters:
 *  address - address to be checked.
 *
 * Returned Value:
 *  true if the IP address is valid, false otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static bool hcom_nx_config_is_valid_ip_address(const char *address)
{
    struct sockaddr_in sa;
    return(inet_pton(AF_INET, address, &(sa.sin_addr)) == 1);
}

/****************************************************************************
 * Name: hcom_nx_config_setup_default_ntp_servers
 *
 * Description:
 *  Setup the default NTP servers.
 *
 * Input Parameters:
 *  config - pointer to the configuration object.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The configuration structure has been locked by the caller.
 *
 ****************************************************************************/
static void hcom_nx_config_setup_default_ntp_servers(meadow_configuration_t *config)
{
    config->ntp_servers_count = 4;
    config->ntp_servers = malloc(4 * sizeof(char *));
    config->ntp_servers[0] = hcom_nx_common_utils_strdup(NTP_DEFAULT_SERVER0);
    config->ntp_servers[1] = hcom_nx_common_utils_strdup(NTP_DEFAULT_SERVER1);
    config->ntp_servers[2] = hcom_nx_common_utils_strdup(NTP_DEFAULT_SERVER2);
    config->ntp_servers[3] = hcom_nx_common_utils_strdup(NTP_DEFAULT_SERVER3);
}

/****************************************************************************
 * Name: hcom_nx_config_create_dns_resolver_file
 *
 * Description:
 *  Create the DNS resolver file populated with the servers (where valid)
 *  specified.
 *
 * Input Parameters:
 *  servers - pointer to a list of DNS server IP addresses.
 *  server_count - number of servers in the list.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void hcom_nx_config_create_dns_resolver_file(const char **servers, uint32_t server_count)
{
    FILE *dns_file = fopen(CONFIG_NETDB_RESOLVCONF_PATH, "wb");
    for (int index = 0; index < server_count; index++)
    {
        if (hcom_nx_config_is_valid_ip_address(servers[index]))
        {
            fputs("nameserver ", dns_file);
            fputs(servers[index], dns_file);
            fputs("\n", dns_file);
        }
    }
    fclose(dns_file);
}

/****************************************************************************
 * Name: hcom_nx_config_setup_default_dns_servers
 *
 * Description:
 *  Create the DNS resolver file with a default DNS server entry.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The configuration structure has been locked by the caller.
 *
 ****************************************************************************/
static void hcom_nx_config_setup_default_dns_servers(void)
{
    char *servers = DNS_DEFAULT_SERVER;
    hcom_nx_config_create_dns_resolver_file((const char **) &servers, 1);
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
                //
                //  Add any default settings here.
                //
                meadow_configuration->using_default_configuration = 1;
                meadow_configuration->reset_esp32_at_startup = 1;
                meadow_configuration->esp_spi_speed = 8000000;
                meadow_configuration->maximum_retry_count = 3;
                hcom_nx_config_setup_default_dns_servers();                
                hcom_nx_config_setup_default_ntp_servers(meadow_configuration);
                meadow_configuration->ntp_refresh_period = NTP_DEFAULT_REFRESH_PERIOD;
            }
            else
            {
                meadow_configuration->using_default_configuration = 0;
                if (configuration->mono_control != NULL)
                {
                    meadow_configuration->disable_mono = configuration->mono_control->disable;
                    meadow_configuration->mono_options = hcom_nx_common_utils_strdup(configuration->mono_control->options);
                }
                //
                if (configuration->coprocessor != NULL)
                {
                    meadow_configuration->reset_esp32_at_startup = !configuration->coprocessor->debugger_attached;
                    meadow_configuration->esp_spi_speed = (configuration->coprocessor->spi_speed < 100000) ? 100000 : configuration->coprocessor->spi_speed;
                    meadow_configuration->automatically_reconnect = hcom_nx_config_parse_boolean(configuration->coprocessor->automatically_reconnect, 0);
                    meadow_configuration->automatically_start_network = hcom_nx_config_parse_boolean(configuration->coprocessor->automatically_start_network, 0);
                    meadow_configuration->maximum_retry_count = hcom_nx_config_parse_unsigned_integer(configuration->coprocessor->maximum_retry_count, 3);
                }
                else
                {
                    meadow_configuration->reset_esp32_at_startup = 1;
                    meadow_configuration->esp_spi_speed = 8000000;
                }
                if (configuration->network != NULL)
                {
                    meadow_configuration->get_network_time_at_startup = hcom_nx_config_parse_boolean(configuration->network->get_network_time_at_startup, 0);
                    if (configuration->network->ntp_servers_count > 0)
                    {
                        meadow_configuration->ntp_servers_count = configuration->network->ntp_servers_count;
                        meadow_configuration->ntp_servers = malloc(meadow_configuration->ntp_servers_count * sizeof(char *));
                        for (int index = 0; index < meadow_configuration->ntp_servers_count; index++)
                        {
                            meadow_configuration->ntp_servers[index] = hcom_nx_common_utils_strdup(configuration->network->ntp_servers[index]);
                        }
                    }
                    else
                    {
                        hcom_nx_config_setup_default_ntp_servers(meadow_configuration);
                    }
                    meadow_configuration->ntp_refresh_period = hcom_nx_config_parse_unsigned_integer(configuration->network->ntp_refresh_period, NTP_DEFAULT_REFRESH_PERIOD);
                    if (meadow_configuration->ntp_refresh_period < NTP_MINIMUM_REFRESH_PERIOD)
                    {
                        meadow_configuration->ntp_refresh_period = NTP_MINIMUM_REFRESH_PERIOD;
                    }
                    bool create_default_dns_resolver_file = true;
                    if (configuration->network->dns_servers_count > 0)
                    {
                        for (int index = 0; index < configuration->network->dns_servers_count; index++)
                        {
                            if (hcom_nx_config_is_valid_ip_address(configuration->network->dns_servers[index]))
                            {
                                create_default_dns_resolver_file = false;
                                break;
                            }
                        }
                    }
                    if (create_default_dns_resolver_file)
                    {
                        hcom_nx_config_setup_default_dns_servers();
                    }
                    else
                    {
                        hcom_nx_config_create_dns_resolver_file(configuration->network->dns_servers, configuration->network->dns_servers_count);
                    }
                }
                else
                {
                    hcom_nx_config_setup_default_ntp_servers(meadow_configuration);
                    hcom_nx_config_setup_default_dns_servers();
                }
                if (configuration->debug != NULL)
                {
                    meadow_configuration->trace_level = configuration->debug->trace_level;
                    meadow_configuration->use_uart1_for_trace = (strcmp(configuration->debug->uart1_use, "trace") == 0);
                }
                //
                if (configuration->device != NULL)
                {
                    if (configuration->device->name != NULL)
                    {
                        meadow_configuration->device_name = hcom_nx_common_utils_strdup(configuration->device->name);
                    }
                    else
                    {
                        meadow_configuration->device_name = hcom_nx_common_utils_strdup(MEADOW_CONFIG_DEFAULT_DEVICE_NAME);
                    }
                }
                //
                meadow_configuration->esp_software_version = NULL;

                cyaml_free(&cyaml_config, &configuration_schema, configuration, 0);
            }
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
    if (config->mono_options != NULL)
    {
        storage_required += strlen(config->mono_options) + 1;
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
        ptr += hcom_nx_config_copy_string(config->mono_options, ptr);
        new_config->meadow_software_version = ptr;
        ptr += hcom_nx_config_copy_string(config->meadow_software_version, ptr);
        new_config->meadow_hardware_version = ptr;
        ptr += hcom_nx_config_copy_string(config->meadow_hardware_version, ptr);
        new_config->esp_software_version = ptr;
        ptr += hcom_nx_config_copy_string(config->esp_software_version, ptr);
        new_config->device_name = ptr;
        ptr += hcom_nx_config_copy_string(config->device_name, ptr);
    }
    hcom_nx_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_uint32_value
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
static int hcom_nx_config_get_uint32_value(int source, uint8_t *destination, int destination_length)
{
    if (destination_length < sizeof(uint32_t))
    {
        return ERROR;
    }
    *destination = source;
    return(sizeof(uint32_t));
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
    int result = ERROR;
    
    if (source == NULL)
    {
        if ((destination != NULL) && (destination_length > 0))
        {
            *destination = 0;
            result = 0;
        }
    }
    else
    {
        if ((strlen(source) + 1) <= destination_length)
        {
            result = strlen(strcpy((char *) destination, source));
        }
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_strings
 *
 * Description:
 *  Copy a list of strings into the destination buffer.
 *
 * Input Parameters:
 *  source - configuration string(s) to be copied.
 *  destination - destination buffer to hold the strings.
 *  dest_length - length of the destination buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
// static int hcom_nx_config_get_strings(char **source, uint32_t number_of_entries, char *destination, int destination_length)
// {
//     int result = -1;
//     uint32_t storage_required = 0;
//     for (int index = 0; index < number_of_entries; index++)
//     {
//         storage_required += strlen(source[index]) + 1;
//     }

//     if (storage_required <= destination_length)
//     {
//         char *str = destination;
//         for (int index = 0; index < number_of_entries; index++)
//         {
//             strcpy(str, source);
//             str += (strlen(source[index]) + 1);
//         }
//         result = storage_required;
//     }
    
//     return(result);
// }

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
 *  config - Pointer to the system configuration object.
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
static int hcom_nx_config_get_automatically_connect_to_network(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        *buffer = config->automatically_start_network ? 1 : 0;
        result = 1;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_automatically_start_network
 *
 * Description:
 *  Set the automatically_start_network property and inform the ESP of the
 *  change.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
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
static int hcom_nx_config_set_automatically_start_network(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length == 1)
    {
        result = hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_start_network, *buffer);
        if (result == OK)
        {
            config->automatically_start_network = *buffer;
        }
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
 *  config - Pointer to the system configuration object.
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
static int hcom_nx_config_get_automatically_reconnect(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        *buffer = config->automatically_reconnect ? 1 : 0;
        result = 1;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_automatically_reconnect
 *
 * Description:
 *  Set the AutomaticallyReconnect property and inform the ESP of the change.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
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
static int hcom_nx_config_set_automatically_reconnect(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length == 1)
    {
        result = hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_reconnect, *buffer);
        if (result == OK)
        {
            config->automatically_reconnect = *buffer;
        }
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
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer to hold the current value of the GetNetworkTimeAtStartup
 *           property.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_get_get_time_at_startup(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        *buffer = config->get_network_time_at_startup ? 1 : 0;
        result = 1;
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_get_time_at_startup
 *
 * Description:
 *  Set the GetTimeAtStartup property passing the new value to the ESP32.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer holding the new value for the GetNetworkTimeAtStartup
 *           property.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_set_get_time_at_startup(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length == 1)
    {
        result = hcom_nx_config_set_esp_boolean_value(cv_get_time_at_startup, *buffer);
        if (result == OK)
        {
            config->get_network_time_at_startup = *buffer;
        }
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_maximum_retry_count
 *
 * Description:
 *  Get the maximum number of times a retry operation will be attempted.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
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
static int hcom_nx_config_get_maximum_retry_count(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    result = hcom_nx_config_get_bytes((uint8_t *) &config->maximum_retry_count, sizeof(int), buffer, buffer_length);

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_maximum_retry_count
 *
 * Description:
 *  Set the GetTimeAtStartup property passing the new value to the ESP32.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer holding the new value for the GetNetworkTimeAtStartup
 *           property.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_nx_config_set_maximum_retry_count(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length == 4)
    {
        int retryCount = *((int *) buffer);
        result = hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_maximum_retry_count, retryCount);
        if (result == OK)
        {
            config->maximum_retry_count = retryCount;
        }
    }
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
static int hcom_nx_config_get_board_mac_address(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;
    
    result = hcom_nx_config_get_bytes(config->board_mac_address, sizeof(config->board_mac_address), buffer, buffer_length);

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
static int hcom_nx_config_get_soft_ap_mac_address(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;
    
    result = hcom_nx_config_get_bytes(config->soft_ap_mac_address, sizeof(config->soft_ap_mac_address), buffer, buffer_length);

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
    if (direction == 0)
    {
        switch (item)
        {
            case cv_device_name:
                result = hcom_nx_config_get_string_value(config->device_name, buffer, buffer_length);
                break;
            case cv_product:
                result = hcom_nx_config_get_uint32_value(config->hardware_version, buffer, buffer_length);
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
                result = hcom_nx_config_get_automatically_connect_to_network(config, buffer, buffer_length);
                break;
            case cv_automatically_reconnect:
                result = hcom_nx_config_get_automatically_reconnect(config, buffer, buffer_length);
                break;
            case cv_maximum_network_retry_count:
                result = hcom_nx_config_get_maximum_retry_count(config, buffer, buffer_length);
                break;
            case cv_get_time_at_startup:
                result = hcom_nx_config_get_get_time_at_startup(config, buffer, buffer_length);
                break;
            case cv_mac_address:
                result = hcom_nx_config_get_board_mac_address(config, buffer, buffer_length);
                break;
            case cv_soft_ap_mac_address:
                result = hcom_nx_config_get_soft_ap_mac_address(config, buffer, buffer_length);
                break;
            case cv_default_access_point:
                result = hcom_nx_config_get_string_value(config->default_access_point, buffer, buffer_length);
                break;
            case cv_reset_reason:
            result = hcom_nx_config_get_bytes(&config->esp32_reset_reason, 1, buffer, buffer_length);
                break;
            default:
                result = ERROR;
                break;
        }
    }
    else
    {
        switch (item)
        {
            case cv_device_name:
                result = hcom_nx_config_set_device_name(config, buffer, buffer_length);
                break;
            case cv_automatically_start_network:
                result = hcom_nx_config_set_automatically_start_network(config, buffer, buffer_length);
                break;
            case cv_automatically_reconnect:
                result = hcom_nx_config_set_automatically_reconnect(config, buffer, buffer_length);
                break;
            case cv_maximum_network_retry_count:
                result = hcom_nx_config_set_maximum_retry_count(config, buffer, buffer_length);
                break;
            case cv_get_time_at_startup:
                result = hcom_nx_config_set_get_time_at_startup(config, buffer, buffer_length);
                break;
            default:
                result = ERROR;
                break;
        }
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
        //
        if (configuration->automatically_reconnect != esp_config->automatically_reconnect)
        {
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_reconnect, configuration->automatically_reconnect == 1);
        }
        //
        if (configuration->maximum_retry_count != esp_config->maximum_retry_count)
        {
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_maximum_retry_count, configuration->maximum_retry_count);
        }
        //
        if (configuration->get_network_time_at_startup != esp_config->get_time_at_startup)
        {
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_get_time_at_startup, configuration->get_network_time_at_startup == 1);
        }
        //
        if ((esp_config->device_name != NULL) && (strcmp(configuration->device_name, esp_config->device_name) != 0))
        {
            hcom_nx_config_set_esp_string_value(espcp_configuration_items_device_name, configuration->device_name);
        }
        //
        if (esp_config->default_access_point != NULL)
        {
            configuration->default_access_point = hcom_nx_common_utils_strdup(esp_config->default_access_point);
        }
        else
        {
            configuration->default_access_point = NULL;
        }
        //
        if (esp_config->software_version != NULL)
        {
            if (configuration->esp_software_version != NULL)
            {
                free(configuration->esp_software_version);
            }
            configuration->esp_software_version = hcom_nx_common_utils_strdup(esp_config->software_version);
        }
        else
        {
            configuration->esp_software_version = NULL;
        }
        //
        memcpy(configuration->board_mac_address, esp_config->board_mac_address, 6);
        memcpy(configuration->soft_ap_mac_address, esp_config->soft_ap_mac_address, 6);
        configuration->esp32_reset_reason = esp_config->reset_reason;
    }
    hcom_nx_config_unlock();
}

/****************************************************************************
 * Name: hcom_nx_config_process_wifi_credentials_file
 *
 * Description:
 *  Check to see if a wifi.yaml file exists and send the credentials to the
 *  ESP32 if it exists and contains valid data.
 *
 *  The wifi.yaml file will be deleted as a security measure to prevent
 *  the credentials from being downloaded using the CLI tool.
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
void hcom_nx_config_process_wifi_credentials_file(void)
{
    return;
    yaml_wifi_credentials_t *credentials;

    cyaml_err_t err = cyaml_load_file(MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME, &cyaml_config, &wifi_credentials_schema, (void **) &credentials, NULL);
    if (err == CYAML_OK)
    {
        if ((credentials->credentials->ssid != NULL) && (strlen(credentials->credentials->ssid) <= MAXIMUM_SSID_LENGTH) & (strlen(credentials->credentials->ssid) > 0))
        {
            char password[MAXIMUM_PASSWORD_LENGTH + 1];
            memset(password, 0, MAXIMUM_PASSWORD_LENGTH + 1);
            if ((credentials->credentials->password != NULL) && (strlen(credentials->credentials->password) <= MAXIMUM_PASSWORD_LENGTH))
            {
                strcpy(password, credentials->credentials->password);
            }
            uint32_t size = strlen(credentials->credentials->ssid) + strlen(password) + 2;
            uint8_t *buffer = malloc(size);
            if (buffer != NULL)
            {
                hcom_nx_config_lock();
                meadow_configuration_t *config = hcom_nx_config_get_pointer();
                if (config->default_access_point != NULL)
                {
                    free(config->default_access_point);
                }
                config->default_access_point = hcom_nx_common_utils_strdup(credentials->credentials->ssid);
                hcom_nx_config_unlock();
                strcpy((char *) buffer, credentials->credentials->ssid);
                strcpy((char *) (buffer + strlen(credentials->credentials->ssid) + 1), password);
                hcom_nx_config_set_esp_value(espcp_configuration_items_default_ap_and_password, buffer, size);
                free(buffer);
            }
        }
        cyaml_free(&cyaml_config, &wifi_credentials_schema, credentials, 0);
    }
    //
    //  Now we can delete the file.
    //
    unlink(MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME);
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
    hcom_nx_config_set_host_name(config, config->device_name);
    config->hardware_version = meadow_hw_version_get();
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
