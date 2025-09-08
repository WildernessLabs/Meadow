/****************************************************************************
 * espcp_core_dump.c
 *
 *   Copyright (C) 2025 Wilderness Labs. All rights reserved.
 *   Author: Mark Stevens
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

#include <nuttx/config.h>

#include <stdio.h>

#include <sys/stat.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mm/mm.h>
#include <nuttx/kstring.h>
#include <nuttx/wqueue.h>

#include "espcp_shared_enums.h"
#include "espcp_encoders.h"
#include "espcp_common.h"
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "meadow/hcom_shared_common.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Definitions.
 ****************************************************************************/

 /**
  * @brief Amount of data to be collected on each fragment request.
  */
 #define ESP32_CORE_DUMP_FRAGMENT_SIZE 4000

 /**
  * @brief Root file name for the ESP32 core dump.
  */
 #define ESP_CORE_DUMP_ROOT_FILE_NAME "esp-coredump-"


/****************************************************************************
 * Global variables.
 ****************************************************************************/
 
 /**
 * @brief Core dump work structure needed for NuttX work queues.
 */
struct work_s g_core_dump_work_struct = {};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

 /****************************************************************************
 * Name: espcp_create_core_dump_file_name
 *
 * Description:
 *  Create a file name for the core dump from the ESP32.
 *
 * Input Parameters:
 *  buffer - Pointer to the buffer to hold the file name.
 *  buffer_length - Length of the buffer.
 *  version - Pointer to a string holding the version information.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  The version string can be modified.
 *
 ****************************************************************************/
static void espcp_create_core_dump_file_name(char *buffer, size_t buffer_length, char *version)
 {
    if (version != NULL)
    {
        for (int index = 0; index < strlen(version); index++)
        {
            if (version[index] == '.')
            {
                version[index] = '-';
            }
        }
    }
    else
    {
        version = "unknown-version";
    }
    snprintf(buffer, buffer_length, CRASH_DIR "/" ESP_CORE_DUMP_ROOT_FILE_NAME "%s.bin", version);
    mkdir(CRASH_DIR, 0777);
 }

/****************************************************************************
 * Name: espcp_get_core_dump_information
 *
 * Description:
 *  Get information about the core dump on the ESP32.
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
 static espcp_core_dump_information_response_t *espcp_get_core_dump_information(void)
 {
    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system,
                                            espcp_system_function_get_core_dump_information, espcp_status_codes_completed_ok,
                                            espcp_get_next_message_id(), NULL, 0);
    if (message != NULL)
    {
        if (espcp_queue_message(message, true) != espcp_status_codes_completed_ok)
        {
            espcp_delete_message_payload(message);
            return(NULL);
        }
    }
    espcp_core_dump_information_response_t *information = espcp_extract_core_dump_information_response(message->payload);
    espcp_delete_message_and_payload(message);

    return(information);
}

/****************************************************************************
 * Name: espcp_get_core_dump_from_esp32
 *
 * Description:
 *  Get the core dump from the ESP32.
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
static uint8_t *espcp_get_core_dump_from_esp32(uint32_t size)
{
    uint8_t *core_dump = (uint8_t *) malloc(size);

    if (core_dump == NULL)
    {
        return(NULL);
    }

    uint32_t offset = 0;
    espcp_core_dump_fragment_request_t fragment_request = {};
    fragment_request.size = ESP32_CORE_DUMP_FRAGMENT_SIZE;
    uint32_t encoded_fragment_request_size = espcp_encoded_core_dump_fragment_request_buffer_size(&fragment_request);
    while (offset < size)
    {
        MEADOW_TRACE_INFORMATION("Requesting core dump fragment at offset %d\n", offset);
        uint8_t *encoded_fragment_request = (uint8_t *) malloc(encoded_fragment_request_size);
        if (encoded_fragment_request == NULL)
        {
            free(core_dump);
            return(NULL);
        }
        fragment_request.offset = offset;
        espcp_encode_core_dump_fragment_request(&fragment_request, encoded_fragment_request);

        espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system,
                                                espcp_system_function_core_dump_fragment, espcp_status_codes_completed_ok,
                                                espcp_get_next_message_id(), encoded_fragment_request, encoded_fragment_request_size);
        if (message != NULL)
        {
            if (espcp_queue_message(message, true) != espcp_status_codes_completed_ok)
            {
                espcp_delete_message_payload(message);
                free(core_dump);
                return(NULL);
            }
            espcp_core_dump_fragment_response_t *fragment_response = espcp_extract_core_dump_fragment_response(message->payload);
            if (fragment_response == NULL)
            {
                espcp_delete_message_and_payload(message);
                free(core_dump);
                return(NULL);
            }
            MEADOW_TRACE_INFORMATION("Received core dump fragment of size %d\n", fragment_response->size);
            memcpy(core_dump + offset, fragment_response->data, fragment_response->size);
            espcp_delete_message_and_payload(message);
        }
        else
        {
            free(encoded_fragment_request);
            free(core_dump);
            return(NULL);
        }
        offset += fragment_request.size;
    }

    return(core_dump);
}

/****************************************************************************
 * Name: espcp_generate_core_dump_file
 *
 * Description:
 *  Generate the core dump file from the ESP32.
 *
 * Input Parameters:
 *  version - The version string to include in the file name.
 *
 * Returned Value:
 *  0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int espcp_generate_core_dump_file(char *version)
{
    char file_name[64];
    espcp_create_core_dump_file_name(file_name, sizeof(file_name), version);
    espcp_core_dump_information_response_t *information = espcp_get_core_dump_information();
    if (information == NULL)
    {
        return(-1);
    }
    if (!information->is_valid)
    {
        free(information);
        return(-1);
    }
    MEADOW_TRACE_INFORMATION("Core dump size %d\n", information->core_dump_size);
    uint8_t *core_dump = espcp_get_core_dump_from_esp32(information->core_dump_size);
    if (core_dump != NULL)
    {
        MEADOW_TRACE_INFORMATION("Core dump retrieved successfully, writing to file %s\n", file_name);
        if (unlink(file_name) != 0)
        {
            MEADOW_TRACE_ERROR("Failed to delete existing core dump file %s (%s)\n", file_name, strerror(errno));
            if (errno != ENOENT)
            {
                free(core_dump);
                free(information);
                return(-1);
            }
        }

        FILE *file = fopen(file_name, "w");
        if (file != NULL)
        {
            int bytes_written = fwrite(core_dump, 1, information->core_dump_size, file);
            fflush(file);
            fclose(file);
            if (bytes_written != information->core_dump_size)
            {
                MEADOW_TRACE_ERROR("Failed to write complete core dump to file %s\n", file_name);
                free(core_dump);
                free(information);
                return(-1);
            }
        }
        else
        {
            MEADOW_TRACE_ERROR("Failed to open core dump file %s for writing\n", file_name);
            free(core_dump);
            free(information);
            return(-1);
        }
        free(core_dump);
    }
    free(information);

    return(0);
}

/****************************************************************************
 * Name: espcp_erase_core_dump_partition
 *
 * Description:
 *  Erase the core dump partition on the ESP32.
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
void espcp_erase_core_dump_partition(void)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system,
                                            espcp_system_function_core_dump_erase, espcp_status_codes_completed_ok,
                                            espcp_get_next_message_id(), NULL, 0);
    if (message != NULL)
    {
        if (espcp_queue_message(message, true) != espcp_status_codes_completed_ok)
        {
            MEADOW_TRACE_INFORMATION("%s: Failed to queue message\n", __func__);
        }
        espcp_delete_message_payload(message);
    }
    else
    {
        MEADOW_TRACE_ERROR("%s: Failed to create message\n", __func__);
    }

    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_get_core_dump
 *
 * Description:
 *  Get the core dump file from the ESP32 and write this to the crash report
 *  directory.
 *
 * Input Parameters:
 *  argument - Work queue argument pointer (this should be NULL as it is not
 *             used).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  This method will be called from a NuttX work queue
 *
 ****************************************************************************/
void espcp_get_core_dump(void *argument)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    char *version = NULL;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    bool retrieve_core_dump = config->esp_retrieve_core_dump;
    if (retrieve_core_dump)
    {
        version = kmm_strdup(config->esp_version.short_string);
    }
    hcom_nx_config_unlock();

    if (retrieve_core_dump)
    {
        if (espcp_generate_core_dump_file(version) != 0)
        {
            MEADOW_TRACE_INFORMATION("%s: Failed to generate core dump file\n", __func__);
        }
        //
        //  TODO: Decide if we are going to erase the coredump on the ESP.
        //
        else
        {
            espcp_erase_core_dump_partition();
        }
    }
    kmm_free(version);

    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
}
