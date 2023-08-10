/*************************************************************************
 * \apps\examples\hcom\misc\espcp_utils.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os.h>

// TODO: Check if these includes are necessary

#include <mqueue.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH       10
#define ESPCP_EVENT_DATA_SIZE                    13
#define ESPCP_EVENT_MESSAGE_QUEUE_NAME           "/Esp32Events"
#define ESPCP_REQUEST_MESSAGE_QUEUE_NAME         "/Esp32Requests"
#define ESPCP_EVENT_HANDLER_MESSAGE_QUEUE_NAME   "/IncomingEvents"
#define ESPCP_DEFAULT_MESSAGE_PRIORITY           1

// Note: these definitions are in the esp32 codebase
#define ESPCP_CELL_CONNECTED_EVENT        0x00
#define ESPCP_CELL_DISCONNECTED_EVENT     0x01
#define ESPCP_CELL_ERROR_EVENT            0x02
#define ESPCP_CELL_AT_CMD_EVENT           0x04
#define ESPCP_CELL_INTERFACE              0x07
#define ESPCP_SIMPLE_EVENT_MESSAGE_ID     0x00
#define ESPCP_COMPLETED_OK_STATUS_CODE    0x00
#define ESPCP_FAILURE_STATUS_CODE         0x03

struct espcp_event_data_s
{
    uint8_t interface;
    uint32_t function;
    uint32_t status_code;
    uint32_t message_id;
};
typedef struct espcp_event_data_s espcp_event_data_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

void espcp_encode_uint32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

void espcp_encode_event_data(espcp_event_data_t *event_data, uint8_t *buffer)
{
    *buffer = event_data->interface;
    buffer += 1;
    espcp_encode_uint32(event_data->function, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->status_code, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->message_id, buffer);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int espcp_queue_event_messages(uint8_t *message) 
{
    mqd_t event_queue_id;
    struct mq_attr queue_attributes;
    int result;

    queue_attributes.mq_maxmsg = ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH;
    queue_attributes.mq_msgsize = ESPCP_EVENT_DATA_SIZE;
    queue_attributes.mq_flags = 0;

    // Open the message queue
    event_queue_id = mq_open(ESPCP_EVENT_MESSAGE_QUEUE_NAME, O_RDWR | O_CREAT, 0666, &queue_attributes);
    if (event_queue_id == (mqd_t)-1)
    {
        hcom_logging_syslog(LOG_ERR, "Failed to open queue for sending event messages\n");
        return -1;
    }

    // Send the message to the queue
    result = mq_send(event_queue_id, (const char *)message, 13, ESPCP_DEFAULT_MESSAGE_PRIORITY);
    if (result == -1)
    {
        hcom_logging_syslog(LOG_ERR, "Failed to queue the event message\n");
        mq_close(event_queue_id);
        return -1;
    }

    // Close the message queue when done
    mq_close(event_queue_id);

    return 0;
}