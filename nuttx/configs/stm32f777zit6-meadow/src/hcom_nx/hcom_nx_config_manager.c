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
#include <strings.h>
#include <time.h>
#include <nuttx/semaphore.h>
#include <nuttx/kstring.h>

#include "hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/meadow_hw_version.h>
#include "../espcp/espcp_coprocessor.h"
#include "../espcp/espcp_message_dispatcher.h"
#include "../espcp/espcp_shared_enums.h"
#include "../espcp/espcp_usrsock.h"
#include "../misc/meadow_logging.h"
#include "stm32_uid.h" // stm32_get_uniqueid()

#include "hcom_nx_common.h"

#include "hcom_nx_config_manager.h"
#include "hcom_nx_config_manager_yaml.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Default entry in the network_interfaces array to be used if no interface
 *         is selected in the config file.
 */
#define MEADOW_DEFAULT_NETWORK_INTERFACE    MEADOW_IFT_ESP32

/**
 * @brief String used for version numbers when the value is not available.
 */
#define UNKNOWN_VERSION_STRING              "Not available"

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
 *  @brief Array of network interfaces available.
 */
static meadow_network_interface_t network_interfaces[] = 
{
    {
        .interface_type = MEADOW_IFT_ESP32,
        .name = MEADOW_IFT_ESP32_NAME,
        .use_dhcp = 1,
        .ip_address = 0,
        .netmask = 0,
        .gateway = 0,
        .psock_methods = &g_usrsock_sockif_esp32
    },
    {
        .interface_type = MEADOW_IFT_ETHERNET,
        .name = MEADOW_IFT_ETHERNET_NAME,
        .use_dhcp = 1,
        .ip_address = 0,
        .netmask = 0,
        .gateway = 0,
        .psock_methods = NULL
    },
    {
        .interface_type = MEADOW_IFT_CELL,
        .name = MEADOW_IFT_CELL_NAME,
        .use_dhcp = 1,
        .ip_address = 0,
        .netmask = 0,
        .gateway = 0,
        .psock_methods = NULL
    }
};

/**
 *  Mutex to be used by any code that wants access to the configuration.
 */
static sem_t config_lock = { };

/**
 *  Structure to hold the F7MicroV2 pin mappings
 */
struct f7_micro_v2_pin_mapping_s {
    const char* pin_name;
    int pin_value;
};
typedef struct f7_micro_v2_pin_mapping_s f7_micro_v2_pin_mapping_t;

/**
 *  Define the pin mappings as an array of structures.
 */
const f7_micro_v2_pin_mapping_t f7_micro_v2_pin_mappings[] = {
    { F7_MICRO_V2_A00_PIN_NAME, F7_MICRO_V2_A00_PIN },
    { F7_MICRO_V2_A01_PIN_NAME, F7_MICRO_V2_A01_PIN },
    { F7_MICRO_V2_A02_PIN_NAME, F7_MICRO_V2_A02_PIN },
    { F7_MICRO_V2_A03_PIN_NAME, F7_MICRO_V2_A03_PIN },
    { F7_MICRO_V2_A04_PIN_NAME, F7_MICRO_V2_A04_PIN },
    { F7_MICRO_V2_A05_PIN_NAME, F7_MICRO_V2_A05_PIN },
    { F7_MICRO_V2_D00_PIN_NAME, F7_MICRO_V2_D00_PIN },
    { F7_MICRO_V2_D01_PIN_NAME, F7_MICRO_V2_D01_PIN },
    { F7_MICRO_V2_D02_PIN_NAME, F7_MICRO_V2_D02_PIN },
    { F7_MICRO_V2_D03_PIN_NAME, F7_MICRO_V2_D03_PIN },
    { F7_MICRO_V2_D04_PIN_NAME, F7_MICRO_V2_D04_PIN },
    { F7_MICRO_V2_D05_PIN_NAME, F7_MICRO_V2_D05_PIN },
    { F7_MICRO_V2_D06_PIN_NAME, F7_MICRO_V2_D06_PIN },
    { F7_MICRO_V2_D07_PIN_NAME, F7_MICRO_V2_D07_PIN },
    { F7_MICRO_V2_D08_PIN_NAME, F7_MICRO_V2_D08_PIN },
    { F7_MICRO_V2_D09_PIN_NAME, F7_MICRO_V2_D09_PIN },
    { F7_MICRO_V2_D10_PIN_NAME, F7_MICRO_V2_D10_PIN },
    { F7_MICRO_V2_D11_PIN_NAME, F7_MICRO_V2_D11_PIN },
    { F7_MICRO_V2_D12_PIN_NAME, F7_MICRO_V2_D12_PIN },
    { F7_MICRO_V2_D13_PIN_NAME, F7_MICRO_V2_D13_PIN },
    { F7_MICRO_V2_D14_PIN_NAME, F7_MICRO_V2_D14_PIN },
    { F7_MICRO_V2_D15_PIN_NAME, F7_MICRO_V2_D15_PIN },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_nx_config_populate_cell_module_id
 *
 * Description:
 *  Populate the cell module id according to the cell module name set in
 *  the cell.config.yaml
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_populate_cell_module_id(meadow_configuration_t *config)
{
    if ((config == NULL) || (config->default_cell_settings == NULL) || (config->default_cell_settings->module == NULL))
    {
        syslog(LOG_INFO, "Failed getting cell default settings");
        return;
    }

    if (strcasecmp(config->default_cell_settings->module, CELL_BG770A_MODULE_NAME) == 0)
    {
        config->default_cell_settings->module_id = CELL_BG770A_MODULE;
    }
    else if (strcasecmp(config->default_cell_settings->module, CELL_M95_MODULE_NAME) == 0)
    {
        config->default_cell_settings->module_id = CELL_M95_MODULE;
    }
    else if (strcasecmp(config->default_cell_settings->module, CELL_BG95M3_MODULE_NAME) == 0)
    {
        config->default_cell_settings->module_id = CELL_BG95M3_MODULE;
    }
    else
    {
        syslog(LOG_INFO, "Failed populating cell module id");
        config->default_cell_settings->module_id = CELL_UNKNOWN_MODULE;
    }
}

/****************************************************************************
 * Name: hcom_nx_config_populate_cell_network_mode_id
 *
 * Description:
 *  Populate the cell network mode id according to the mode set
 *  in the cell.config.yaml
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_populate_cell_network_mode_id(meadow_configuration_t *config)
{
    if ((config == NULL) || (config->default_cell_settings == NULL) || (config->default_cell_settings->mode == NULL))
    {
        syslog(LOG_INFO, "Failed getting default cell settings");
        return;
    }

    if (strcasecmp(config->default_cell_settings->mode, CELL_CATM1_MODE_NAME) == 0)
    {
        config->default_cell_settings->mode_id = CELL_CATM1_MODE;
    }
    else if (strcasecmp(config->default_cell_settings->mode, CELL_NBIOT_MODE_NAME) == 0)
    {
        config->default_cell_settings->mode_id = CELL_NBIOT_MODE;
    }
    else if (strcasecmp(config->default_cell_settings->mode, CELL_GSM_MODE_NAME) == 0)
    {
        config->default_cell_settings->mode_id = CELL_GSM_MODE;
    }
    else
    {
        syslog(LOG_INFO, "Failed populating cell network mode id");
        config->default_cell_settings->mode_id = CELL_UNKNOWN_MODE;
    }
}

/****************************************************************************
 * Name: hcom_nx_config_map_cell_network_mode
 *
 * Description:
 *  Map the cell network mode according to the Mode defined in
 *  the cell.config.yaml, since different modules may use distinct integers 
 *  to reference network modes (e.g., Cat-M1 is 8 for Quectel BG95-M3, 
 *  but 7 for Quectel BG770A).
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_map_cell_network_mode(meadow_configuration_t *config)
{
    if ((config == NULL) || (config->default_cell_settings == NULL))
    {
        syslog(LOG_INFO, "Failed getting default cell settings");
        return;
    }

    uint32_t module = config->default_cell_settings->module_id;
    uint32_t mode = config->default_cell_settings->mode_id;

    switch (module)
    {
    case CELL_BG770A_MODULE:
        switch (mode)
        {
        case CELL_CATM1_MODE:
            strcpy(config->default_cell_settings->mode, "7");
            break;
        case CELL_NBIOT_MODE:
            strcpy(config->default_cell_settings->mode, "9");
            break;
        default:
            syslog(LOG_INFO, "Mode %u not supported on BG770A module", mode);
            strcpy(config->default_cell_settings->mode, "");
            break;
        }
        break;

    case CELL_BG95M3_MODULE:
        switch (mode)
        {
        case CELL_CATM1_MODE:
            strcpy(config->default_cell_settings->mode, "8");
            break;
        case CELL_NBIOT_MODE:
            strcpy(config->default_cell_settings->mode, "9");
            break;
        case CELL_GSM_MODE:
            strcpy(config->default_cell_settings->mode, "0");
            break;
        default:
            syslog(LOG_INFO, "Mode %u not supported on BG95-M3 module", mode);
            strcpy(config->default_cell_settings->mode, "");
            break;
        }
        break;

    case CELL_M95_MODULE:
        switch (mode)
        {
        case CELL_GSM_MODE:
            strcpy(config->default_cell_settings->mode, "0");
            break;
        default:
            syslog(LOG_INFO, "Mode %u not supported on M95 module", mode);
            strcpy(config->default_cell_settings->mode, "");
            break;
        }
        break;

    default:
        syslog(LOG_INFO, "Failed to map cell network mode name to the equivalent integer");
        strcpy(config->default_cell_settings->mode, "");
        break;
    }
}

/****************************************************************************
 * Name: hcom_nx_config_get_turn_on_pin
 *
 * Description:
 *  Function to get the turn-on pin value from the pin name using a mapping array.
 *
 * Input Parameters:
 *  pin_name - F7v2 pin name (e.g. D10)
 *
 * Returned Value:
 *  The correspondent MCU pin value associated to the F7v2 pin name.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t hcom_nx_config_get_turn_on_pin(const char* pin_name) {
    for (size_t i = 0; i < sizeof(f7_micro_v2_pin_mappings) / sizeof(f7_micro_v2_pin_mappings[0]); ++i) {
        if (strcmp(f7_micro_v2_pin_mappings[i].pin_name, pin_name) == 0) {
            return f7_micro_v2_pin_mappings[i].pin_value;
        }
    }
    return -1;
}

/****************************************************************************
 * Name: hcom_nx_config_populate_cell_turn_on_pin
 *
 * Description:
 *  Convert the Meadow device pin name to the correspondent binary value.
 *
 * Input Parameters:
 *  config - Pointer to the system config object
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_map_cell_turn_on_pin(meadow_configuration_t *config)
{
    if ((config == NULL) || (config->default_cell_settings == NULL))
    {
        syslog(LOG_INFO, "Failed getting default cell settings");
        return;
    }

    char* turn_on_pin_name = config->default_cell_settings->turn_on_pin_name;

    uint32_t pin_value = hcom_nx_config_get_turn_on_pin(turn_on_pin_name);
    if (pin_value != -1) {
        config->default_cell_settings->turn_on_pin = pin_value;
    } else {
        syslog(LOG_INFO, "Failed populating cell turn-on pin");
        config->default_cell_settings->turn_on_pin = F7_MICRO_V2_D10_PIN;
    }
}

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
 * Name: hcom_nx_config_manager_logger
 *
 * Description:
 *  Process log requests from the cyaml library.
 *
 * Input Parameters:
 *  level - Logging level.
 *  context - Context.
 *  format - Format string for the log message.
 *  args - Arguments matching the format starting.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_nx_config_manager_logger(cyaml_log_t level, void *context, const char *format, va_list args)
{
    if (format != NULL)
    {
        char *buffer = malloc(MEADOW_FILE_LOG_LINE_LENGTH);
        if (buffer != NULL)
        {
            meadow_file_logging_level_t log_level = (level == CYAML_LOG_ERROR) ? mfl_error : mfl_info;
            vsnprintf(buffer, MEADOW_FILE_LOG_LINE_LENGTH - 1, format, args);
            char *log = malloc(MEADOW_FILE_LOG_LINE_LENGTH);
            if (log != NULL)
            {
                snprintf(log, MEADOW_FILE_LOG_LINE_LENGTH - 1, "CONFIG: %s", buffer);
                meadow_logging_write(log_level, log);
                free(log);
            }
            MEADOW_TRACE_INFORMATION(buffer);
            free(buffer);
        }
    }
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
 * Name: hcom_nx_config_get_long_version_string
 *
 * Description:
 *  Get the version information as a long string.
 *
 * Input Parameters:
 *  config - Version information.
 *
 * Returned Value:
 *  Pointer to a block of kernel memory containing the version string.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static char *hcom_nx_config_get_long_version_string(meadow_version_number_t *version)
{
    char *result = NULL;
    
    if ((version->major != 0) || (version->minor != 0) || (version->revision != 0) || (version->build != 0))
    {
        char *storage = (char *) kmm_zalloc(150);
        if (storage != NULL)
        {
            char *branch_name = (char *) kmm_zalloc(66);  // 64 characters for branch + ':' + terminator.
            if (branch_name != NULL)
            {
                if (version->branch_name == NULL)
                {
                    branch_name[0] = 0;
                }
                else
                {
                    snprintf(branch_name, 66, ":%s", version->branch_name);
                }
                snprintf_chk(storage, 150, "%d.%d.%d.%d built %02d %s 20%02d %02d:%02d:%02d UTC (%08x%s)", 
                    version->major, version->minor, version->revision, version->build, version->day, 
                    version->month_text, version->year, version->hour, version->minute, version->second,
                    version->hash, branch_name);
                result = kmm_strdup(storage);
                kmm_free(branch_name);
            }
            kmm_free(storage);
        }
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_short_version_string
 *
 * Description:
 *  Get the version information as a short string (a.b.c.d).
 *
 * Input Parameters:
 *  config - Version information.
 *
 * Returned Value:
 *  Pointer to a block of kernel memory containing the version string.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static char *hcom_nx_config_get_short_version_string(meadow_version_number_t *version)
{
    char *result = NULL;

    if (version != NULL)
    {
        char version_string[45];    // Long enough for 4294967295.4294967295.4294967295.4294967295
        
        if ((version->major != 0) || (version->minor != 0) || (version->revision != 0) || (version->build != 0))
        {
            snprintf(version_string, 45, "%d.%d.%d.%d", version->major, version->minor, version->revision, version->build);
        }
        result = kmm_strdup(version_string);
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_set_month_text
 *
 * Description:
 *  Convert the integer month value into the three character month name in
 *  a version structure.
 *
 * Input Parameters:
 *  version - Pointer to a version structure.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The month number is in the range 1 - 12 as returned by the system date
 *  application.
 *
 ****************************************************************************/
static void hcom_nx_config_set_month_text(meadow_version_number_t *version)
{
    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    t.tm_mon = version->month - 1;
    strftime(version->month_text, 4, "%b", &t);
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
        new_name = kmm_strdup(MEADOW_CONFIG_DEFAULT_DEVICE_NAME);
    }
    else
    {
        new_name = kmm_strdup(device_name);
    }
    kmm_free(config->device_name);
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
            kmm_free(config->device_name);
            config->device_name = kmm_strdup((char *) buffer);
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
static uint8_t hcom_nx_config_parse_boolean(const char *config_value, uint8_t default_value)
{
    uint8_t value = default_value;

    if (config_value != NULL)
    {
        char *lowercase = kmm_zalloc(strlen(config_value) + 1);

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
        kmm_free(lowercase);
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
 * Name: hcom_nx_config_parse_ip_address
 *
 * Description:
 *  Parse the string and determine if it is a valid address and return a
 *  uint32_t value for the IP address.
 *
 * Input Parameters:
 *  address - address to be parsed
 *
 * Returned Value:
 *  uint32_t value representing the IP address or 0 if the address is
 *  invalid.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static uint32_t hcom_nx_config_parse_ip_address(const char *address)
{
    uint32_t ip = 0;
    if (address != NULL)
    {
        struct sockaddr_in sa;
        if (inet_pton(AF_INET, address, &(sa.sin_addr)) == 1)
        {
            ip = (uint32_t ) sa.sin_addr.s_addr;
        }
    }
    return(ip);
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
    config->ntp_servers = kmm_zalloc(4 * sizeof(char *));
    config->ntp_servers[0] = kmm_strdup(NTP_DEFAULT_SERVER0);
    config->ntp_servers[1] = kmm_strdup(NTP_DEFAULT_SERVER1);
    config->ntp_servers[2] = kmm_strdup(NTP_DEFAULT_SERVER2);
    config->ntp_servers[3] = kmm_strdup(NTP_DEFAULT_SERVER3);
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
 * Name: hcom_nx_config_find_interface_by_name
 *
 * Description:
 *  Find the specified interface in the list of registered (possible) interfaces.
 *
 * Input Parameters:
 *  name - Name of the interface to find.
 *
 * Returned Value:
 *  Pointer to the interface requested, NULL if the interface cannot be found.
 *
 * Assumptions/Limitations:
 *  The configuration structure has been locked by the caller.
 *
 ****************************************************************************/
static meadow_network_interface_t *hcom_nx_config_find_interface_by_name(const char *name)
{
    meadow_network_interface_t *interface = NULL;
    if (name != NULL)
    {
        for (int index = 0; index < sizeof(network_interfaces) / sizeof(meadow_network_interface_t); index++)
        {
            if (strcasecmp(network_interfaces[index].name, name) == 0)
            {
                interface = &network_interfaces[index];
                break;
            }
        }
    }
    return(interface);
}

/****************************************************************************
 * Name: hcom_nx_config_process_interface_section
 *
 * Description:
 *  Process a network interface definition from the meadow.config.yaml file.
 *
 * Input Parameters:
 *  yaml_interface - Pointer to information about a network interface.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The configuration structure has been locked by the caller.
 *
 ****************************************************************************/
static void hcom_nx_config_process_interface_section(yaml_network_interface_t *yaml_interface)
{
    if (yaml_interface != NULL)
    {
        meadow_network_interface_t *interface = hcom_nx_config_find_interface_by_name(yaml_interface->name);
        if (interface != NULL)
        {
            interface->ip_address = hcom_nx_config_parse_ip_address(yaml_interface->ip_address);
            interface->netmask = hcom_nx_config_parse_ip_address(yaml_interface->netmask);
            interface->gateway = hcom_nx_config_parse_ip_address(yaml_interface->gateway);
            interface->use_dhcp = hcom_nx_config_parse_boolean(yaml_interface->use_dhcp, 1);
        }
    }
}

/****************************************************************************
 * Name: hcom_nx_config_process_network_section
 *
 * Description:
 *  Process the network section from the meadow.config.yaml file.
 *
 * Input Parameters:
 *  network_config - Pointer the to yaml_network_t object containing the
 *                   network configuration from the config file.
 *  meadow_configuration - Pointer to the configuration information being
 *                         assembled from the config file.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The configuration structure has been locked by the caller.
 *
 ****************************************************************************/
static void hcom_nx_config_process_network_section(yaml_network_t *network_config, meadow_configuration_t *config)
{
    if (network_config != NULL)
    {
        config->get_network_time_at_startup = hcom_nx_config_parse_boolean(network_config->get_network_time_at_startup, 0);
        if (network_config->ntp_servers_count > 0)
        {
            config->ntp_servers_count = network_config->ntp_servers_count;
            config->ntp_servers = kmm_zalloc(meadow_configuration->ntp_servers_count * sizeof(char *));
            for (int index = 0; index < meadow_configuration->ntp_servers_count; index++)
            {
                config->ntp_servers[index] = kmm_strdup(network_config->ntp_servers[index]);
            }
        }
        else
        {
            hcom_nx_config_setup_default_ntp_servers(meadow_configuration);
        }
        config->ntp_refresh_period_seconds = hcom_nx_config_parse_unsigned_integer(network_config->ntp_refresh_period_seconds, NTP_DEFAULT_REFRESH_PERIOD);
        if (config->ntp_refresh_period_seconds < NTP_MINIMUM_REFRESH_PERIOD)
        {
            config->ntp_refresh_period_seconds = NTP_MINIMUM_REFRESH_PERIOD;
        }
        bool create_default_dns_resolver_file = true;
        if (network_config->dns_servers_count > 0)
        {
            for (int index = 0; index < network_config->dns_servers_count; index++)
            {
                if (hcom_nx_config_is_valid_ip_address(network_config->dns_servers[index]))
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
            hcom_nx_config_create_dns_resolver_file(network_config->dns_servers, network_config->dns_servers_count);
        }
        //
        //  Now work out the network interface / adapter details.
        //
        if (network_config->interfaces != NULL)
        {
            for (int index = 0; index < network_config->interfaces_count; index++)
            {
                hcom_nx_config_process_interface_section(&network_config->interfaces[index]);
            }
        }
        //
        //  Now work out which adapter should be used.
        //
        config->default_interface = hcom_nx_config_find_interface_by_name(network_config->default_interface);
        if (config->default_interface == NULL)
        {
            config->default_interface = hcom_nx_config_find_interface_by_name(MEADOW_IFT_ESP32_NAME);
        }
    }
    else
    {
        hcom_nx_config_setup_default_ntp_servers(config);
        hcom_nx_config_setup_default_dns_servers();
        config->default_interface = &network_interfaces[MEADOW_DEFAULT_NETWORK_INTERFACE];
    }
}

/****************************************************************************
 * Name: hcom_nx_config_process_meadow_config_file
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
static meadow_configuration_t *hcom_nx_config_process_meadow_config_file(void)
{
    hcom_nx_config_lock();
    if (meadow_configuration == NULL)
    {
        meadow_configuration = (meadow_configuration_t *) kmm_zalloc(sizeof(meadow_configuration_t));
        if (meadow_configuration != NULL)
        {
        	yaml_configuration_t *configuration;

            cyaml_err_t err = cyaml_load_file(MEADOW_CONFIG_DEFAULT_FILE_NAME, &cyaml_config, &configuration_schema, (void **) &configuration, NULL);
            if ((err != CYAML_OK) || (configuration == NULL))
            {
                if (err != CYAML_OK)
                {
                    meadow_logging_write(mfl_error, "Error processing config file, using default config");
                }
                //
                //  Add any default settings here.
                //
                meadow_configuration->using_default_configuration = 1;
                meadow_configuration->reboot_on_unhandled_exceptions = 1;
                meadow_configuration->initialisation_timeout_seconds = DEFAULT_INITIALISATION_TIMEOUT_SECONDS;
                meadow_configuration->reset_esp32_at_startup = 1;
                meadow_configuration->esp_spi_speed_hz = DEFAULT_STM_ESP_SPI_SPEED;
                meadow_configuration->maximum_retry_count = 3;
                hcom_nx_config_setup_default_dns_servers();                
                hcom_nx_config_setup_default_ntp_servers(meadow_configuration);
                meadow_configuration->ntp_refresh_period_seconds = NTP_DEFAULT_REFRESH_PERIOD;
                meadow_configuration->default_interface = &network_interfaces[MEADOW_DEFAULT_NETWORK_INTERFACE];
            }
            else
            {
                meadow_configuration->using_default_configuration = 0;
                if (configuration->mono_control != NULL)
                {
                    meadow_configuration->mono_options = kmm_strdup(configuration->mono_control->options);
                }
                //
                if (configuration->coprocessor != NULL)
                {
                    uint32_t speed = hcom_nx_config_parse_unsigned_integer(configuration->coprocessor->spi_speed_hz, DEFAULT_STM_ESP_SPI_SPEED);
                    meadow_configuration->esp_spi_speed_hz = (speed < 100000) ? 100000 : speed;
                    meadow_configuration->automatically_reconnect = hcom_nx_config_parse_boolean(configuration->coprocessor->automatically_reconnect, false);
                    meadow_configuration->automatically_start_network = hcom_nx_config_parse_boolean(configuration->coprocessor->automatically_start_network, false);
                    meadow_configuration->maximum_retry_count = hcom_nx_config_parse_unsigned_integer(configuration->coprocessor->maximum_retry_count, 3);
                }
                else
                {
                    meadow_configuration->esp_spi_speed_hz = DEFAULT_STM_ESP_SPI_SPEED;
                }
                hcom_nx_config_process_network_section(configuration->network, meadow_configuration);
                if (configuration->internal_debug != NULL)
                {
                    meadow_configuration->trace_level = hcom_nx_config_parse_unsigned_integer(configuration->internal_debug->trace_level, 0);
                    if (meadow_configuration->trace_level > 4)
                    {
                        meadow_configuration->trace_level = 0;
                    }
                    meadow_configuration->use_uart1_for_trace = (strcmp(configuration->internal_debug->uart1_use, "trace") == 0);
                    meadow_configuration->reset_esp32_at_startup = !hcom_nx_config_parse_boolean(configuration->internal_debug->debugger_attached_to_esp, false);
                }
                else
                {
                    meadow_configuration->reset_esp32_at_startup = 1;
                }
                //
                if (configuration->device != NULL)
                {
                    if (configuration->device->name == NULL)
                    {
                        meadow_configuration->device_name = kmm_strdup(MEADOW_CONFIG_DEFAULT_DEVICE_NAME);
                    }
                    else
                    {
                        meadow_configuration->device_name = kmm_strdup(configuration->device->name);
                    }
                    if (configuration->device->reserved_pins != NULL)
                    {
                        meadow_configuration->reserved_pins = kmm_strdup(configuration->device->reserved_pins);
                    }
                    meadow_configuration->reboot_on_unhandled_exceptions = hcom_nx_config_parse_boolean(configuration->device->reboot_on_unhandled_exceptions, true);
                    meadow_configuration->initialisation_timeout_seconds = hcom_nx_config_parse_unsigned_integer(configuration->device->initialisation_timeout_seconds, DEFAULT_INITIALISATION_TIMEOUT_SECONDS);
                    meadow_configuration->sd_storage_supported = hcom_nx_config_parse_boolean(configuration->device->sd_storage_supported, false)
                                                                 && (meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2);
                }

                cyaml_free(&cyaml_config, &configuration_schema, configuration, 0);
                meadow_logging_write(mfl_info, "Config file successfully processed");
            }
        }
    }
    hcom_nx_config_unlock();

#if defined(USE_MEADOW_DEBUG_HELPERS)
    MEADOW_TRACE_INFORMATION("Using %s configuration\n", (meadow_configuration->using_default_configuration == 1) ? "default" : "user");
    MEADOW_TRACE_INFORMATION("Device Information:\n");
    MEADOW_TRACE_INFORMATION("    Device name: %s\n", meadow_configuration->device_name);
    MEADOW_TRACE_INFORMATION("    Reboot on unhandled exception: %d\n", meadow_configuration->reboot_on_unhandled_exceptions);
    MEADOW_TRACE_INFORMATION("    Initialisation timeout: %d seconds\n", meadow_configuration->initialisation_timeout_seconds);
    MEADOW_TRACE_INFORMATION("    SD storage enabled: %d\n", meadow_configuration->sd_storage_supported);
    MEADOW_TRACE_INFORMATION("Mono Control:\n");
    MEADOW_TRACE_INFORMATION("    Options: %s\n", (meadow_configuration->mono_options == NULL) ? "None configured" : meadow_configuration->mono_options);
    MEADOW_TRACE_INFORMATION("Coprocessor:\n");
    MEADOW_TRACE_INFORMATION("    SPI speed: %d Hz\n", meadow_configuration->esp_spi_speed_hz);
    MEADOW_TRACE_INFORMATION("    Automatically start network: %d\n", meadow_configuration->automatically_start_network);
    MEADOW_TRACE_INFORMATION("    Automatically reconnect: %d\n", meadow_configuration->automatically_reconnect);
    MEADOW_TRACE_INFORMATION("    Maximum retry count: %d\n", meadow_configuration->maximum_retry_count);
    char address[INET_ADDRSTRLEN];
    MEADOW_TRACE_INFORMATION("Network:\n");
    MEADOW_TRACE_INFORMATION("    Ethernet:\n");
    MEADOW_TRACE_INFORMATION("        Default: %d\n", meadow_configuration->default_interface == &network_interfaces[MEADOW_IFT_ETHERNET]);
    MEADOW_TRACE_INFORMATION("        Use DHCP: %d\n", network_interfaces[MEADOW_IFT_ETHERNET].use_dhcp);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ETHERNET].ip_address, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        IP Address: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ETHERNET].netmask, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Subnet mask: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ETHERNET].gateway, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Gateway: %s\n", address);
    MEADOW_TRACE_INFORMATION("    WiFi:\n");
    MEADOW_TRACE_INFORMATION("        Default: %d\n", meadow_configuration->default_interface == &network_interfaces[MEADOW_IFT_ESP32]);
    MEADOW_TRACE_INFORMATION("        Use DHCP: %d\n", network_interfaces[MEADOW_IFT_ESP32].use_dhcp);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ESP32].ip_address, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        IP Address: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ESP32].netmask, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Subnet mask: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_ESP32].gateway, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Gateway: %s\n", address);
    MEADOW_TRACE_INFORMATION("    BG770A:\n");
    MEADOW_TRACE_INFORMATION("        Default: %d\n", meadow_configuration->default_interface == &network_interfaces[MEADOW_IFT_CELL]);
    MEADOW_TRACE_INFORMATION("        Use DHCP: %d\n", network_interfaces[MEADOW_IFT_CELL].use_dhcp);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_CELL].ip_address, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        IP Address: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_CELL].netmask, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Subnet mask: %s\n", address);
    inet_ntop(AF_INET, &network_interfaces[MEADOW_IFT_CELL].gateway, address, INET_ADDRSTRLEN);
    MEADOW_TRACE_INFORMATION("        Gateway: %s\n", address);
    MEADOW_TRACE_INFORMATION("    Get network time at startup: %d\n", meadow_configuration->get_network_time_at_startup);
    MEADOW_TRACE_INFORMATION("    NTP refresh period: %d seconds\n", meadow_configuration->ntp_refresh_period_seconds);
    MEADOW_TRACE_INFORMATION("    NTP Servers (%d):\n", meadow_configuration->ntp_servers_count);
    int index = 0;
    for (index = 0; index < meadow_configuration->ntp_servers_count; index++)
    {
        inet_ntop(AF_INET, &meadow_configuration->ntp_servers[index], address, INET_ADDRSTRLEN);
        MEADOW_TRACE_INFORMATION("        - %s\n", address);
    }
    MEADOW_TRACE_INFORMATION("    DNS Servers:\n");
    FILE *dns_file = fopen(CONFIG_NETDB_RESOLVCONF_PATH, "rb");
    char buffer[32];
    while (fgets(buffer, 32, dns_file) != NULL)
    {
        MEADOW_TRACE_INFORMATION("        - %s", (buffer + 11));
    }
    fclose(dns_file);
    MEADOW_TRACE_INFORMATION("Internal Debug:\n");
    MEADOW_TRACE_INFORMATION("    Use UART for trace: %d\n", meadow_configuration->use_uart1_for_trace);
    MEADOW_TRACE_INFORMATION("    Trace level: %d\n", meadow_configuration->trace_level);
    MEADOW_TRACE_INFORMATION("    Debugger attached to ESP32: %d\n", (meadow_configuration->reset_esp32_at_startup == 1) ? 0 : 1);
#endif

    return(meadow_configuration);
}

/****************************************************************************
 * Name: hcom_nx_config_get_uint32_value
 *
 * Description:
 *  Get a uint32_t configuration value and copy it to the destination buffer.
 *
 * Input Parameters:
 *  source - configuration value to be copied.
 *  destination - destination buffer to hold the value.
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
        return(ERROR);
    }
    *((uint32_t *) destination) = source;
    return(sizeof(uint32_t));
}

/****************************************************************************
 * Name: hcom_nx_config_get_uint8_value
 *
 * Description:
 *  Get a uint8_t configuration value and copy it to the destination buffer.
 *
 * Input Parameters:
 *  source - configuration value to be copied.
 *  destination - destination buffer to hold the value.
 *  dest_length - length of the destination buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int hcom_nx_config_get_uint8_value(uint8_t source, uint8_t *destination, int destination_length)
{
    int result = ERROR;

    if (destination_length > 0)
    {
        *destination = source;
        result = 1;
    }

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
 * Name: hcom_nx_config_get_ip_address
 *
 * Description:
 *  Get an IP address or 0 if DHCP is enabled.
 *
 * Input Parameters:
 *  use_dhcp - use DHCP.
 *  ip_address - IP address to be used if DHCP is not enabled.
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
static int hcom_nx_config_get_ip_address(bool use_dhcp, uint32_t ip_address, uint8_t *destination, int destination_length)
{
    if (use_dhcp)
    {
        return(hcom_nx_config_get_uint32_value(0, destination, destination_length));
    }
    return(hcom_nx_config_get_uint32_value(ip_address, destination, destination_length));
}

/****************************************************************************
 * Name: hcom_nx_config_get_bytes
 *
 * Description:
 *  Get a byte array configuration value and copy it to the destination buffer.
 *
 * Input Parameters:
 *  source - configuration bytes to be copied.
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
 * Name: hcom_nx_config_set_maximum_retry_count
 *
 * Description:
 *  Set the MaximumRetryCount property passing the new value to the ESP32.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer holding the new value for the MaximumRetryCount
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
        result = hcom_nx_config_set_esp_integer_value(espcp_configuration_items_maximum_retry_count, retryCount);
        if (result == OK)
        {
            config->maximum_retry_count = retryCount;
        }
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
 * Name: hcom_nx_config_get_version_string
 *
 * Description:
 *  Get the version string (or string representing an unknown value).
 *
 * Input Parameters:
 *  version - Pointer to the version information.
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
int hcom_nx_config_get_version_string(meadow_version_number_t *version, uint8_t *buffer, int buffer_length)
{
    int result = 0;

    if (version->short_string == NULL)
    {
        result = hcom_nx_config_get_string_value(UNKNOWN_VERSION_STRING, buffer, buffer_length);
    }
    else
    {
        result = hcom_nx_config_get_string_value(version->short_string, buffer, buffer_length);
    }
    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_build_date
 *
 * Description:
 *  Get the OS build date.
 *
 * Input Parameters:
 *  version - Version information.
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
int hcom_nx_config_get_build_date(meadow_version_number_t *version, uint8_t *buffer, int buffer_length)
{
    int result = 0;

    if (buffer_length < (strlen(HCOM_DEVICE_INFO_DATE_FORMAT) + 1))
    {
        result = -1;
    }
    else
    {
        if (version->short_string == NULL)
        {
            result = hcom_nx_config_get_string_value(UNKNOWN_VERSION_STRING, buffer, buffer_length);
        }
        else
        {
            char date[32];
            result = snprintf(date, 32, HCOM_DEVICE_INFO_DATE_FORMAT, version->day, version->month_text,
                              version->year, version->hour, version->minute, version->second);
            result = hcom_nx_config_get_string_value(date, buffer, buffer_length);
        }
    }

    return(result);
}

/****************************************************************************
 * Name: hcom_nx_config_get_selected_network
 *
 * Description:
 *  Get the selected network.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
 *  buffer - Buffer to hold the value when reading, or holding the new value
 *           when writing.
 *  buffer_length - Length of the buffer.
 *
 * Returned Value:
 *  Amount of data copied or a negative number on error.
 *
 * Assumptions/Limitations:
 *  Configuration objet has been locked by the caller.
 *
 ****************************************************************************/
int hcom_nx_config_get_selected_network(meadow_configuration_t *config, uint8_t *buffer, int buffer_length)
{
    int result = ERROR;

    if (buffer_length > 0)
    {
        *buffer = config->default_interface->interface_type;
        result = 1;
    }

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
            case cv_reserved_pins:
                result = hcom_nx_config_get_string_value(config->reserved_pins, buffer, buffer_length);
                break;
            case cv_product:
                result = hcom_nx_config_get_uint32_value(config->hardware_version, buffer, buffer_length);
                break;
            case cv_model:
                result = hcom_nx_config_get_string_value(HCOM_DEVICE_INFO_MODEL, buffer, buffer_length);
                break;
            case cv_os_version:
                result = hcom_nx_config_get_version_string(&config->os_version, buffer, buffer_length);
                break;
            case cv_mono_version:
                result = hcom_nx_config_get_version_string(&config->mono_version, buffer, buffer_length);
                break;
            case cv_build_date:
                result = hcom_nx_config_get_build_date(&config->os_version, buffer, buffer_length);
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
                result = hcom_nx_config_get_version_string(&config->esp_version, buffer, buffer_length);
                break;
            case cv_automatically_start_network:
                result = hcom_nx_config_get_uint8_value(config->automatically_start_network, buffer, buffer_length);
                break;
            case cv_automatically_reconnect:
                result = hcom_nx_config_get_uint8_value(config->automatically_reconnect, buffer, buffer_length);
                break;
            case cv_maximum_network_retry_count:
                result = hcom_nx_config_get_uint32_value(config->maximum_retry_count, buffer, buffer_length);
                break;
            case cv_get_time_at_startup:
                result = hcom_nx_config_get_uint8_value(config->get_network_time_at_startup, buffer, buffer_length);
                break;
            case cv_mac_address:
                result = hcom_nx_config_get_bytes(config->board_mac_address, sizeof(config->board_mac_address), buffer, buffer_length);
                break;
            case cv_soft_ap_mac_address:
                result = hcom_nx_config_get_bytes(config->soft_ap_mac_address, sizeof(config->soft_ap_mac_address), buffer, buffer_length);
                break;
            case cv_default_access_point:
                result = hcom_nx_config_get_string_value(config->default_access_point, buffer, buffer_length);
                break;
            case cv_reset_reason:
                result = hcom_nx_config_get_bytes(&config->esp32_reset_reason, 1, buffer, buffer_length);
                break;
            case cv_selected_network:
                result = hcom_nx_config_get_selected_network(config, buffer, buffer_length);
                break;
            case cv_static_ip_address:
                result = hcom_nx_config_get_ip_address(config->default_interface->use_dhcp == 1, config->default_interface->ip_address, buffer, buffer_length);
                break;
            case cv_default_gateway:
                result = hcom_nx_config_get_ip_address(config->default_interface->use_dhcp == 1, config->default_interface->gateway, buffer, buffer_length);
                break;
            case cv_subnet_mask:
                result = hcom_nx_config_get_ip_address(config->default_interface->use_dhcp == 1, config->default_interface->netmask, buffer, buffer_length);
                break;
            case cv_sd_storage_supported:
                result = hcom_nx_config_get_uint8_value(config->sd_storage_supported, buffer, buffer_length);
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
            case cv_automatically_reconnect:
                result = hcom_nx_config_set_automatically_reconnect(config, buffer, buffer_length);
                break;
            case cv_maximum_network_retry_count:
                result = hcom_nx_config_set_maximum_retry_count(config, buffer, buffer_length);
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
 *  Compare the incoming ESP configuration with the current configuration
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
        hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_automatically_reconnect, configuration->automatically_reconnect == 1);
        //
        if ((configuration->maximum_retry_count >= 3) && (configuration->maximum_retry_count != esp_config->maximum_retry_count))
        {
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_maximum_retry_count, configuration->maximum_retry_count);
        }
        //
        if ((esp_config->device_name != NULL) && (strcmp(configuration->device_name, esp_config->device_name) != 0))
        {
            hcom_nx_config_set_esp_string_value(espcp_configuration_items_device_name, configuration->device_name);
        }
        //
        if (esp_config->default_access_point != NULL)
        {
            configuration->default_access_point = kmm_strdup(esp_config->default_access_point);
        }
        else
        {
            configuration->default_access_point = NULL;
        }
        if ((!configuration->default_interface->use_dhcp) && (configuration->default_interface->interface_type == MEADOW_IFT_ESP32))
        {
            //
            //  Using the ESP32 and static IP address so let the ESP32 know about this.
            //
            hcom_nx_config_set_esp_boolean_value(espcp_configuration_items_use_dhcp, configuration->default_interface->use_dhcp);
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_static_ip_address, configuration->default_interface->ip_address);
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_subnet_mask, configuration->default_interface->netmask);
            hcom_nx_config_set_esp_integer_value(espcp_configuration_items_default_gateway, configuration->default_interface->gateway);
        }
        //
        configuration->esp_version.major = esp_config->version_major;
        configuration->esp_version.minor = esp_config->version_minor;
        configuration->esp_version.revision = esp_config->version_revision;
        configuration->esp_version.build = esp_config->version_build;
        configuration->esp_version.day = esp_config->build_day;
        configuration->esp_version.month = esp_config->build_month;
        hcom_nx_config_set_month_text(&configuration->esp_version);
        configuration->esp_version.year = esp_config->build_year;
        configuration->esp_version.hour = esp_config->build_hour;
        configuration->esp_version.minute = esp_config->build_minute;
        configuration->esp_version.second = esp_config->build_second;
        configuration->esp_version.hash = esp_config->build_hash;
        kmm_free(configuration->esp_version.branch_name);
        configuration->esp_version.branch_name = kmm_strdup(esp_config->build_branch_name);
        kmm_free(configuration->esp_version.short_string);
        configuration->esp_version.short_string = hcom_nx_config_get_short_version_string(&configuration->esp_version);
        kmm_free(configuration->esp_version.long_string);
        configuration->esp_version.long_string = hcom_nx_config_get_long_version_string(&configuration->esp_version);
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
    yaml_wifi_credentials_t *credentials;

    cyaml_err_t err = cyaml_load_file(MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME, &cyaml_config, &wifi_credentials_schema, (void **) &credentials, NULL);
    if ((err == CYAML_OK) && (credentials != NULL))
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
            uint8_t *buffer = kmm_zalloc(size);
            if (buffer != NULL)
            {
                hcom_nx_config_lock();
                meadow_configuration_t *config = hcom_nx_config_get_pointer();
                kmm_free(config->default_access_point);
                config->default_access_point = kmm_strdup(credentials->credentials->ssid);
                hcom_nx_config_unlock();
                strcpy((char *) buffer, credentials->credentials->ssid);
                strcpy((char *) (buffer + strlen(credentials->credentials->ssid) + 1), password);
                hcom_nx_config_set_esp_value(espcp_configuration_items_default_ap_and_password, buffer, size);
                kmm_free(buffer);
            }
        }
        cyaml_free(&cyaml_config, &wifi_credentials_schema, credentials, 0);
    }
    //
    //  Now we can delete the file.
    //
    FILE *file = fopen(MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME, "r");
    if (file)
    {
        fclose(file);
        unlink(MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME);
    }
}

/****************************************************************************
 * Name: hcom_nx_config_get_cell_module_id
 *
 * Description:
 *  Get the cell module model id based on the module name set on the
 *  cell.settings.yaml.
 *
 * Input Parameters:
 *  None.
 * 
 * Returned Value:
 *  Correspondent module id for the cell module model defined
 *  by the user.
 * 
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_config_get_cell_module_id()
{
    uint32_t module_id;
    hcom_nx_config_lock();
    meadow_configuration_t *config;
    config = hcom_nx_config_get_pointer();

    if ((config != NULL) && (config->default_cell_settings != NULL))
    {
        module_id = config->default_cell_settings->module_id;
    }
    else
    {
        module_id = CELL_UNKNOWN_MODULE;
    }

    hcom_nx_config_unlock();
    syslog(LOG_INFO, "Cell module id: %u\n", module_id);

    return module_id;
}

/****************************************************************************
 * Name: hcom_nx_config_get_cell_turn_on_pin
 *
 * Description:
 *  Get the Meadow device pin used to turn on the cell module.
 *
 * Input Parameters:
 *  None.
 * 
 * Returned Value:
 *  Correspondent device pin for the turn-on pin defined
 *  by the user.
 * 
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_config_get_cell_turn_on_pin()
{
    uint32_t turn_on_pin;
    hcom_nx_config_lock();
    meadow_configuration_t *config;
    config = hcom_nx_config_get_pointer();

    if ((config != NULL) && (config->default_cell_settings != NULL))
    {
        turn_on_pin = config->default_cell_settings->turn_on_pin;
    }

    hcom_nx_config_unlock();
    syslog(LOG_INFO, "Cell turn-on pin: %u\n", turn_on_pin);

    return turn_on_pin;
}

/****************************************************************************
 * Name: hcom_nx_config_process_cell_config_file
 *
 * Description:
 *  Check to see if a cell.config.yaml file exists and use the settings
 *  if it exists and contains valid data.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 * 
 ****************************************************************************/
void hcom_nx_config_process_cell_config_file(void)
{
    yaml_cell_config_t *settings;
    cyaml_err_t err = cyaml_load_file(MEADOW_CELL_CONFIG_DEFAULT_FILE_NAME, &cyaml_config, &cell_settings_schema, (void **) &settings, NULL);
    syslog(LOG_INFO, "Cell config file load status: %d\n", err);
    if ((err == CYAML_OK) && (settings != NULL))
    {
        syslog(LOG_INFO, "Cell settings found\n");

        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();

        config->default_cell_settings = (cell_settings_t*) malloc(sizeof(cell_settings_t));

        if ((config->default_cell_settings != NULL) &&
            (settings->settings->apn != NULL) &&
            (strlen(settings->settings->apn) <= MAXIMUM_APN_LENGTH) &&
            (strlen(settings->settings->apn) > 0))
        {
            config->default_cell_settings->apn = kmm_strdup(settings->settings->apn);
            syslog(LOG_INFO, "Default cell APN loaded: %s\n", config->default_cell_settings->apn);

            config->default_cell_settings->timeout = ((settings->settings->timeout != NULL) && 
                                                    (strlen(settings->settings->timeout) <= MAXIMUM_TIMEOUT_LENGTH) && 
                                                    (strlen(settings->settings->timeout) > 0)) ? 
                                                    kmm_strdup(settings->settings->timeout) : 
                                                    kmm_strdup(DEFAULT_CELL_PPPD_TIMEOUT);

            syslog(LOG_INFO, "Default cell PPPD timeout loaded: %s\n", config->default_cell_settings->timeout);

            config->default_cell_settings->pap_user = ((settings->settings->user != NULL) && 
                                                        (strlen(settings->settings->user) <= MAXIMUM_USER_LENGTH) && 
                                                        (strlen(settings->settings->user) > 0)) ? 
                                                        kmm_strdup(settings->settings->user) : 
                                                        kmm_strdup(DEFAULT_CELL_PAP_USER);

            syslog(LOG_INFO, "Default cell PAP username loaded: %s\n", config->default_cell_settings->pap_user);

            config->default_cell_settings->pap_password = ((settings->settings->password != NULL) && 
                                                            (strlen(settings->settings->password) <= MAXIMUM_PASSWORD_LENGTH) && 
                                                            (strlen(settings->settings->password) > 0)) ? 
                                                            kmm_strdup(settings->settings->password) : 
                                                            kmm_strdup(DEFAULT_CELL_PAP_PASSWORD);

            syslog(LOG_INFO, "Default cell PAP password loaded: %s\n", config->default_cell_settings->pap_password);

            config->default_cell_settings->ttyname = ((settings->settings->ttyname != NULL) && 
                                                        (strlen(settings->settings->ttyname) <= MAXIMUM_INTERFACE_LENGTH) && 
                                                        (strlen(settings->settings->ttyname) > 0)) ? 
                                                        kmm_strdup(settings->settings->ttyname) : 
                                                        kmm_strdup(DEFAULT_CELL_INTERFACE);

            syslog(LOG_INFO, "Default cell interface name loaded: %s\n", config->default_cell_settings->ttyname);

            config->default_cell_settings->turn_on_pin_name = ((settings->settings->turn_on_pin_name != NULL) && 
                                                        (strlen(settings->settings->turn_on_pin_name) <= MAXIMUM_TURN_ON_PIN_LENGTH) && 
                                                        (strlen(settings->settings->turn_on_pin_name) > 0)) ? 
                                                        kmm_strdup(settings->settings->turn_on_pin_name) : 
                                                        kmm_strdup(DEFAULT_CELL_TURN_ON_PIN);

            syslog(LOG_INFO, "Default cell turn-on pin name loaded: %s\n", config->default_cell_settings->turn_on_pin_name);

            config->default_cell_settings->mode = ((settings->settings->mode != NULL) && 
                                                    (strlen(settings->settings->mode) <= MAXIMUM_MODE_LENTGH) && 
                                                    (strlen(settings->settings->mode) > 0)) ? 
                                                    kmm_strdup(settings->settings->mode) : 
                                                    kmm_strdup(DEFAULT_CELL_MODE);

            syslog(LOG_INFO, "Default cell operation mode loaded: %s\n", config->default_cell_settings->mode);

            config->default_cell_settings->operator = ((settings->settings->operator != NULL) && 
                                                        (strlen(settings->settings->operator) <= MAXIMUM_OPERATOR_LENGTH) && 
                                                        (strlen(settings->settings->operator) > 0)) ? 
                                                        kmm_strdup(settings->settings->operator) : 
                                                        kmm_strdup(DEFAULT_CELL_OPERATOR);

            syslog(LOG_INFO, "Default cell operator loaded: %s\n", config->default_cell_settings->operator);

            config->default_cell_settings->module = ((settings->settings->module != NULL) && 
                                                        (strlen(settings->settings->module) <= MAXIMUM_MODULE_LENGTH) && 
                                                        (strlen(settings->settings->module) > 0)) ? 
                                                        kmm_strdup(settings->settings->module) :
                                                        kmm_strdup(CELL_UNKNOWN_MODULE_NAME);
            
            syslog(LOG_INFO, "Default cell scan mode: %s\n", config->default_cell_settings->scan_mode);
            
            config->default_cell_settings->scan_mode = hcom_nx_config_parse_boolean(settings->settings->scan_mode, 0);

            syslog(LOG_INFO, "Default cell module loaded: %s\n", config->default_cell_settings->module);

            hcom_nx_config_populate_cell_module_id(config);

            syslog(LOG_INFO, "Default cell module id populated: %u\n", config->default_cell_settings->module_id);

            hcom_nx_config_populate_cell_network_mode_id(config);

            syslog(LOG_INFO, "Default cell network mode id populated: %u\n", config->default_cell_settings->mode_id);

            hcom_nx_config_map_cell_network_mode(config);

            syslog(LOG_INFO, "Default cell operation mode updated after mapping: %s\n", config->default_cell_settings->mode);

            hcom_nx_config_map_cell_turn_on_pin(config);

            syslog(LOG_INFO, "Default cell turn-on pin mapped: %u\n", config->default_cell_settings->turn_on_pin);
        }
        else
        {
            if (config->default_cell_settings != NULL) 
            {
                free(config->default_cell_settings);
                config->default_cell_settings = NULL;
            }
            syslog(LOG_ERR, "Failed to get default cell settings\n");
        }

        hcom_nx_config_unlock();
        cyaml_free(&cyaml_config, &cell_settings_schema, settings, 0);
        syslog(LOG_INFO, "Cyaml free\n");
    }
        
}

/****************************************************************************
 * Name: hcom_nx_config_set_time_to_os_build_time
 *
 * Description:
 *  Use the OS build time as the minimum initial value for the system clock.
 * 
 *  SSL certificate validation requires the clock to be set to a recent time.
 *  The board must be operating after the OS build time so using this gives
 *  the board a starting point.  A more accurate clock can be set later
 *  using NTP.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void hcom_nx_config_set_time_to_os_build_time(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    if (tp.tv_sec < HCOM_DEVICE_INFO_BUILD_EPOCH_TIME)
    {
        tp.tv_sec = HCOM_DEVICE_INFO_BUILD_EPOCH_TIME;
        tp.tv_nsec = 0;
        clock_settime(CLOCK_REALTIME, &tp);
    }
}

/****************************************************************************
 * Name: hcom_nx_config_turn_on_the_cell_module
 *
 * Description:
 *  Function to turn on the cell module, which can vary according to
 *  the meadow device pinout and modem model used.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void hcom_nx_config_turn_on_the_cell_module()
{
    uint32_t module_id; 
    uint32_t turn_on_pin;
    module_id = hcom_nx_config_get_cell_module_id();
    turn_on_pin = hcom_nx_config_get_cell_turn_on_pin();

    switch(module_id)
    {
        case CELL_BG770A_MODULE:
            // Low pulse for 3 seconds to turn on the Quectel BG770A-GL cell module
            syslog(LOG_INFO, "Turning on BG770A module");
            stm32_configgpio(GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | turn_on_pin); 
            stm32_gpiowrite(turn_on_pin, false);
            usleep(3000000);
            stm32_gpiowrite(turn_on_pin, true);
            stm32_gpiowrite(turn_on_pin, false);
        break;

        case CELL_M95_MODULE:
            syslog(LOG_INFO, "Turning on M95 module");
            stm32_configgpio(GPIO_OUTPUT | turn_on_pin);
            stm32_gpiowrite(turn_on_pin, true);
        break;

        case CELL_BG95M3_MODULE:
            syslog(LOG_INFO, "Turning on BG95-M3 module");
            stm32_configgpio(GPIO_OUTPUT | turn_on_pin);
            stm32_gpiowrite(turn_on_pin, true);
            usleep(3000000);
            stm32_gpiowrite(turn_on_pin, false);
        break;

        default:
            syslog(LOG_INFO, "Failed to identify and turn on the cell module");
        break;

    }
    
    // TODO: Add support to turn on the BG770A-GL on the Project Lab and for
    // Meadow F7v1 Feather
}

/****************************************************************************
 * Name: hcom_nx_config_refresh_mono_version
 *
 * Description:
 *  Refresh the mono version in the specified configuration object.
 *
 * Input Parameters:
 *  config - Pointer to the system configuration object.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The configuration object must be locked before this method is called.
 *
 ****************************************************************************/
void hcom_nx_config_refresh_mono_version(meadow_configuration_t *config)
{
    uint32_t block_size = hcom_nx_exec_ex_flash_get_block_size();

    mono_signature_t *mono_signature = (mono_signature_t *) kmm_zalloc(block_size);
    if (mono_signature != NULL)
    {
        hcom_nx_exec_ex_flash_read_absolute_block(0, (void *) mono_signature);
        memset(&config->mono_version, 0, sizeof(meadow_version_number_t));
        if (mono_signature->signature != 0xDDCCBBAA)
        {
            syslog(LOG_ERR, "Mono runtime was not found flashed in external flash.\n");
        }
        else
        {
            if ((mono_signature->build & 0x00ffff00) != 0)
            {
                //
                //  For older versions of Meadow.OS.Runtime.bin the version number
                //  was encoded as a single uint32_t.
                //
                //  TODO: Change this after version 1.0 and before version 256.0.
                //
                config->mono_version.revision = (mono_signature->build >> 8) & 0xff;
                config->mono_version.minor = (mono_signature->build >> 16) & 0xff;
                config->mono_version.major = (mono_signature->build  >> 24) & 0xff;
                config->mono_version.build = mono_signature->build & 0xff;
            }
            else
            {
                config->mono_version.build = mono_signature->build;
                config->mono_version.revision = mono_signature->revision;
                config->mono_version.minor = mono_signature->minor;
                config->mono_version.major = mono_signature->major;
                config->mono_version.day = mono_signature->day;
                config->mono_version.month = mono_signature->month;
                hcom_nx_config_set_month_text(&config->mono_version);
                config->mono_version.year = mono_signature->year;
                config->mono_version.hour = mono_signature->hour;
                config->mono_version.minute = mono_signature->minute;
                config->mono_version.second = mono_signature->second;
                config->mono_version.hash = mono_signature->hash;
                config->mono_version.branch_name = kmm_strdup((char *) &mono_signature->start_of_branch_string);
                config->mono_version.short_string = hcom_nx_config_get_short_version_string(&config->mono_version);
                config->mono_version.long_string = hcom_nx_config_get_long_version_string(&config->mono_version);
            }
        }
        kmm_free(mono_signature);
    }
}

/****************************************************************************
 * Name: hcom_nx_config_init
 *
 * Description:
 *  Setup the configuration system and populate the configuration structure
 *  with data from the configuration file.
 * 
 *  Note that one of the side effects of this method is to copy the Mono
 *  runtime into RAM making it ready for use (assuming the runtime file is 
 *  validated OK).
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
    if (meadow_configuration == NULL)
    {
        sem_init(&config_lock, 0, 1);                   // Create the config lock.
        sem_setprotocol(&config_lock, SEM_PRIO_NONE);
        hcom_nx_config_process_meadow_config_file();

        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        hcom_nx_config_set_host_name(config, config->device_name);
        config->hardware_version = meadow_hw_version_get();
        hcom_nx_config_refresh_mono_version(config);
        config->os_version.major = HCOM_DEVICE_INFO_MAJOR;
        config->os_version.minor = HCOM_DEVICE_INFO_MINOR;
        config->os_version.revision = HCOM_DEVICE_INFO_REVISION;
        config->os_version.build = HCOM_DEVICE_INFO_BUILD;
        config->os_version.day = HCOM_DEVICE_INFO_BUILD_DAY;
        config->os_version.month = HCOM_DEVICE_INFO_BUILD_MONTH;
        hcom_nx_config_set_month_text(&config->os_version);
        config->os_version.year = HCOM_DEVICE_INFO_BUILD_YEAR;
        config->os_version.hour = HCOM_DEVICE_INFO_BUILD_HOUR;
        config->os_version.minute = HCOM_DEVICE_INFO_BUILD_MINUTE;
        config->os_version.second = HCOM_DEVICE_INFO_BUILD_SECOND;
        config->os_version.hash = HCOM_DEVICE_INFO_BUILD_HASH_NUMBER;
        config->os_version.branch_name = HCOM_DEVICE_INFO_GIT_REF;
        config->os_version.short_string = hcom_nx_config_get_short_version_string(&config->os_version);
        config->os_version.long_string = hcom_nx_config_get_long_version_string(&config->os_version);
        config->hardware_version_text = meadow_hw_version_string_return();
        stm32_get_uniqueid(config->serial_number);                           // Convert chip Id to serial number
        config->chip_id[0] = config->serial_number[11];                      // 95-88
        config->chip_id[1] = config->serial_number[10] + config->serial_number[2];        // 87-80 + 23-16
        config->chip_id[2] = config->serial_number[9];                       // 79-72
        config->chip_id[3] = config->serial_number[8] + config->serial_number[0] + 10;    // 71-64 + 7-0 + magic 10
        config->chip_id[4] = config->serial_number[7];                       // 63-56
        config->chip_id[5] = config->serial_number[6];                       // 55-48

        hcom_nx_config_unlock();
    }
    else
    {
        syslog(LOG_ERR, "Configuration should only be initialised once.\n");
    }
}
