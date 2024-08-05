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

#include "espcp_utils.h"

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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

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

/****************************************************************************
 * Name: espcp_send_message_to_ntp_queue
 *
 * Description:
 *   Sends a message to the NTP queue, which should have been opened in the
 *   hcom startup (hcom_startup_manager.c). It sends a provided message to it,
 *   and then closes the queue. The function logs information and errors at 
 *   various stages of execution.
 *
 * Input Parameters:
 *   message - The message to be sent to the NTP queue.
 *
 ****************************************************************************/
void espcp_send_message_to_ntp_queue(const char *message)
{
    hcom_logging_syslog(LOG_INFO, "%s: Enter\n", __func__);

    mqd_t mq;

    // Open the message queue with write access, create it if it doesn't exist
    mq = mq_open(NTPC_QUEUE_INTERFACE, O_WRONLY | O_CREAT, 0644, NULL);
    if (mq == (mqd_t)-1)
    {
        hcom_logging_syslog(LOG_INFO, "%s: Failed to open NTP queue\n", __func__);
        return;
    }

    // Send the message to the queue
    if (mq_send(mq, message, strlen(message) + 1, 0) == -1)
    {
        hcom_logging_syslog(LOG_INFO, "%s: Failed to send message to NTP queue\n", __func__);
    } 
    else
    {
        hcom_logging_syslog(LOG_INFO, "%s: Message sent to NTP queue: %s\n", __func__, message);
    }

    // Close the message queue
    if (mq_close(mq) == -1)
    {
        hcom_logging_syslog(LOG_INFO, "%s: Failed to close NTP queue\n", __func__);
    }

    hcom_logging_syslog(LOG_INFO, "%s: Exit\n", __func__);
}
