/****************************************************************************
 * espcp_message.h
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
#ifndef _ESPCP_MESSAGE_H
#define _ESPCP_MESSAGE_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/**
 *  Default constructor for a Message structure.
 */
#define DEFAULT_ESPCP_MESSAGE = \
    { \
        message_type = 0, \
        interface = 0 \
        function = 0, \
        status_code = 0, \
        message_id = 0, \
        payload = NULL, \
        payload_length = 0 \
    }

/**
 *  Default constructor for a Message structure.
 */
#define DEFAULT_ESPCP_GET_RESPONSE_MESSAGE = \
    { \
        message_type = 0, \
        interface = 0 \
        function = 0, \
        status_code = 0, \
        message_id = 0, \
        payload = NULL, \
        payload_length = 0 \
    }


/****************************************************************************
 * Private Constants
 ****************************************************************************/

/*
 *  Size of an encoded message header (in bytes).
 */
static const uint32_t ESPCP_MESSAGE_HEADER_SIZE = 23;

/*
 *  Offset of the CRC in an encoded message header.
 */
static const uint32_t ESPCP_MESSAGE_CRC_OFFSET = 18;

/*
 *  Message ID used to indicate an invalid (or unknown) message ID.
 */
static const uint32_t ESPCP_MESSAGE_INVALID_MESSAGE_ID = 0xffffffff;

/****************************************************************************
 * Public Types
 ****************************************************************************/

/*
 *  Messages between the ESP32 and the STM32 will be encoded and
 *  extracted through the message structure below.
 */
struct espcp_message_s
{
    /*
     *  Type of message.
     */
    uint8_t message_type;

    /*
     *  Interface that this message is destined for.
     */
    uint8_t interface;

    /*
     *  Function (on the interface) to be executed.
     */
    uint32_t function;

    /*
     *  Status code (for returning messages) from the function.
     */
    uint32_t status_code;

    /*
     *  Unique ID of this message.
     */
    uint32_t message_id;

    /*
     *  Pointer to the payload data to be processed (or returned from) the function.
     */
    uint8_t *payload;

    /*
     *  Number of bytes in the payload.
     */
    uint32_t payload_length;

    sem_t *semaphore;
};
typedef struct espcp_message_s espcp_message_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
espcp_message_t *espcp_create_message_on_heap(uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint8_t *, uint32_t);
espcp_message_t *espcp_create_copy_of_message_on_heap(espcp_message_t *, bool);
void espcp_delete_message_payload(espcp_message_t *);

#endif /* _ESPCP_MESSAGE_H */
