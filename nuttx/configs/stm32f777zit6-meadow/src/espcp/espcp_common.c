/****************************************************************************
 * espcp_common.c
 *
 *  Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
 * 
 *  Methods supporting the ESP system functions (e.g. GetBatteryChargeLevel).
 *  
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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>
#include <nuttx/config.h>

#include "espcp_common.h"
#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_queue_message
 *
 * Description:
 *  Add a message to the message queue and wait on the semaphore.
 * 
 * Input Parameters:
 *  message - Pointer to an espcp_message_t object.
 *  block - Wait for a response if true, return immediately if false.
 *
 * Returned Value:
 *  Status code (completed OK for success failure if there is a problem).
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t espcp_queue_message(espcp_message_t *message, bool block)
{
    uint32_t result = espcp_status_codes_failure;

    sem_t sem;
    if (block)
    {
        message->semaphore = &sem; // cppcheck-suppress autoVariables
        sem_init(message->semaphore, 0, 0);
        sem_setprotocol(&sem, SEM_PRIO_NONE);
    }
    else
    {
        message->semaphore = NULL;
    }
    espcp_configuration_t *configuration = espcp_get_configuration();
    if (espcp_add_message_to_queue(configuration->request_queue, message) == espcp_status_codes_completed_ok)
    {
        if (block)
        {
            bool waiting = true;
            //
            //  We wait in a loop and check the result code for the sem_wait method
            //  as it is possible to have a return from a sem_wait as a result of a
            //  signal as well as a sem_post.  In the case of a signal we simply wait
            //  again.
            //
            //  TODO: Need to investigate why we get the signal.
            //
            while (waiting)
            {
                if (sem_wait(message->semaphore) == 0)
                {
                    waiting = false;
                }
            }
        }
        result = espcp_status_codes_completed_ok;
    }
    return (result);
}

/****************************************************************************
 * Name: espcp_queue_ethernet_connection_changed_event
 *
 * Description:
 *  Create an event message for a connection change for wired ethernet.
 * 
 * Input Parameters:
 *  connected - true is a connection has been made, false if a connection
 *              is lost.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void espcp_queue_ethernet_connection_changed_event(bool connected)
{
    espcp_config_lock();
    espcp_configuration_t *config = espcp_get_configuration();
    mqd_t queue_id = config->incoming_event_queue;
    espcp_config_unlock();

    espcp_message_t *connection_message = (espcp_message_t *) zalloc(sizeof(espcp_message_t));
    connection_message->message_type = espcp_message_types_event;
    connection_message->interface = espcp_esp32_interfaces_wired_ethernet;
    connection_message->semaphore = NULL;
    if (connected)
    {
        connection_message->function = espcp_wi_fi_function_network_connected_event;
        espcp_connect_event_data_t *data = (espcp_connect_event_data_t *) zalloc(sizeof(espcp_connect_event_data_t));
        if (data != NULL)
        {
            hcom_nx_config_lock();
            meadow_configuration_t *meadow_config = hcom_nx_config_get_pointer();
            data->ip_address = meadow_config->default_interface->ip_address;
            data->gateway = meadow_config->default_interface->gateway;
            data->subnet_mask = meadow_config->default_interface->netmask;
            hcom_nx_config_unlock();
            connection_message->payload_length = espcp_connect_event_data_buffer_size(data);
            connection_message->payload = (uint8_t *) zalloc(connection_message->payload_length);
            if (connection_message->payload == NULL)
            {
                free(data);
                free(connection_message);
                return;
            }
            espcp_encode_connect_event_data(data, connection_message->payload);
            free(data);
        }
        else
        {
            free(connection_message);
            return;
        }
    }
    else
    {
        connection_message->function = espcp_wi_fi_function_network_disconnected_event;
    }

    espcp_add_message_to_queue(queue_id, connection_message);
}

