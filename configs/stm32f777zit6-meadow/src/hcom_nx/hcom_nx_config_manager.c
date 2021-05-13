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

#include "hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_nuttx_shared.h>
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
};
typedef struct yaml_mono_control_s yaml_mono_control_t;

/**
 *  Defintion of the fields in the yaml_mono_control_t structure.
 * 
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_mono_control_section_schema[] =
{
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
 * Name: hcom_nx_get_configuration
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
meadow_configuration_t *hcom_nx_get_configuration(void)
{
    return meadow_configuration;
}

/****************************************************************************
 * Name: hcom_nx_read_configuration_file
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
static meadow_configuration_t *hcom_nx_read_configuration_file(void)
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
                meadow_configuration->reset_esp32_at_startup = 1;
                meadow_configuration->esp_spi_speed = 8000000;
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
                }
                //
                if (configuration->coprocessor != NULL)
                {
                    meadow_configuration->reset_esp32_at_startup = !configuration->coprocessor->debugger_attached;
                    meadow_configuration->esp_spi_speed = configuration->coprocessor->spi_speed;
                }
                else 
                {
                    meadow_configuration->reset_esp32_at_startup = 1;
                    meadow_configuration->esp_spi_speed = 8000000;
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
                meadow_configuration->esp_software_version = NULL;
            }
            cyaml_free(&cyaml_config, &configuration_schema, configuration, 0);

            if (meadow_configuration->device_name == NULL)
            {
                meadow_configuration->device_name = MEADOW_CONFIG_DEFAULT_DEVICE_NAME;
            }
        }
    }

    hcom_nx_config_unlock();
    return(meadow_configuration);
}

/****************************************************************************
 * Name: hcom_nx_copy_string
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
int hcom_nx_copy_string(char *source, char *destination)
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
 * Name: hcom_nx_copy_config
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
int hcom_nx_copy_config_for_user_mode(uint8_t *buffer, int length)
{
    if (buffer == NULL)
    {
        return ERROR;
    }

    int result = OK;
    hcom_nx_config_lock();

    meadow_configuration_t *config = hcom_nx_get_configuration();
    int storage_required = sizeof(meadow_configuration_t);
    if (config->mono_trace != NULL)
    {
        storage_required += strlen(config->mono_trace);
    }
    if (config->device_name != NULL)
    {
        storage_required += strlen(config->device_name);
    }
    if (config->meadow_software_version != NULL)
    {
        storage_required += strlen(config->meadow_software_version);
    }
    if (config->esp_software_version != NULL)
    {
        storage_required += strlen(config->esp_software_version);
    }
    storage_required += 4;          // Add on space for the terminating 0 in each of the strings.
    storage_required += sizeof(config->chip_id) + sizeof(config->serial_number);
    if (length < storage_required)
    {
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
        ptr += hcom_nx_copy_string(config->mono_trace, ptr);
        new_config->meadow_software_version = ptr;
        ptr += hcom_nx_copy_string(config->meadow_software_version, ptr);
        new_config->esp_software_version = ptr;
        ptr += hcom_nx_copy_string(config->esp_software_version, ptr);
        new_config->device_name = ptr;
        ptr += hcom_nx_copy_string(config->device_name, ptr);
    }
    
    hcom_nx_config_unlock();

    return(result);
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
    hcom_nx_read_configuration_file();

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
    meadow_configuration_t *config = hcom_nx_get_configuration();
    config->mono_version = mono_version;
    config->meadow_software_version = HCOM_DEVICE_INFO_MEADOW_OS_VERSION;

    stm32_get_uniqueid(config->serial_number);                           // Convert chip Id to serial number
    config->chip_id[0] = config->serial_number[11];                      // 95-88
    config->chip_id[1] = config->serial_number[10] + config->serial_number[2];        // 87-80 + 23-16
    config->chip_id[2] = config->serial_number[9];                       // 79-72
    config->chip_id[3] = config->serial_number[8] + config->serial_number[0] + 10;    // 71-64 + 7-0 + magic 10
    config->chip_id[4] = config->serial_number[7];                       // 63-56 
    config->chip_id[5] = config->serial_number[6];                       // 55-48

    hcom_nx_config_unlock();
}
