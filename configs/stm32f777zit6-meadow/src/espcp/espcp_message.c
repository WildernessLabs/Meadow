 /****************************************************************************
 * Esp32Messaging.c
 *
 *   Generic messaging constants, structures and definitions used
 *   in the messaging system.
 *  
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/config.h>

#include "espcp_message.h"

/****************************************************************************
 * Name: espcp_create_message_on_heap
 *
 * Description:
 *  Create an instance of a message on the heap.
 * 
 * Input Parameters:
 *  message_type - Type of the message.
 *  interface - Interface the message is destined for.
 *  function - Function on the interface to be executed.
 *  status_code - Status code for any function that has been executed.
 *  message_id - ID of this message.
 *  payload - Binary payload for the message.
 *  payload_length - Size of the binary data (payload)
 *
 * Returned Value:
 *  Pointer to a new message.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_create_message_on_heap(uint8_t message_type, uint8_t interface, uint32_t function, uint32_t status_code, uint32_t message_id, uint8_t *payload, uint32_t payload_length)
{
    espcp_message_t *new_message = (espcp_message_t *) malloc(sizeof(espcp_message_t));
    if (new_message != NULL)
    {
        new_message->message_type = message_type;
        new_message->interface = interface;
        new_message->function = function;
        new_message->status_code = status_code;
        new_message->message_id = message_id;
        new_message->payload = payload;
        new_message->payload_length = payload_length;
        new_message->semaphore = NULL;
    }
    return(new_message);
}

/****************************************************************************
 * Name: espcp_create_copy_of_message_on_heap
 *
 * Description:
 *  Create a copy of the specified message on the heap.
 * 
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *  copy_payload - Boolean indicating if we want to copy the message and the
 *                 payload or just the message header.
 *
 * Returned Value:
 *  Pointer to a copy of the original message.  Note that any semaphores in
 *  in the message will not be copied nor will a  new semaphore be created,
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_create_copy_of_message_on_heap(espcp_message_t *message, bool copy_payload)
{
    uint8_t *payload = NULL;
    uint32_t payload_length = 0;

    if (copy_payload)
    {
        payload_length = message->payload_length;
        payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            return(NULL);
        }
        memcpy((void *) payload, (void *) message->payload, (size_t) payload_length);
    }
    espcp_message_t *new_message = espcp_create_message_on_heap(message->message_type,
        message->interface, message->function, message->status_code, message->message_id, payload, payload_length);
    return(new_message);
}

/****************************************************************************
 * Name: espcp_delete_message_payload
 *
 * Description:
 *  Delete the heap storage associated with the payload.
 * 
 *  On exit, the payload will be set to a NULL and the payload length
 *  set to 0.
 *
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_delete_message_payload(espcp_message_t *message)
{
    if ((message != NULL) && (message->payload != NULL))
    {
        free(message->payload);
        message->payload = NULL;
        message->payload_length = 0;
    }
}

/****************************************************************************
 * Name: espcp_delete_message_and_payload
 *
 * Description:
 *  Delete the heap storage associated with the message and any payload.
 * 
 *  The memory associated with the pointer will be invalid on exit.
 *
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_delete_message_and_payload(espcp_message_t *message)
{
    if (message != NULL)
    {
        if (message->semaphore != NULL)
        {
            sem_destroy(message->semaphore);
        }
        espcp_delete_message_payload(message);
        free(message);
    }
}
