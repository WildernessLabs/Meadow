/*************************************************************************
 * \apps\examples\hcom\misc\espcp_utils.h
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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


#ifndef __CONFIGS_MEADOW_SRC_HCOM_MISC_ESPCP_UTILS__H
#define __CONFIGS_MEADOW_SRC_HCOM_MISC_ESPCP_UTILS__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH       10
#define ESPCP_EVENT_DATA_SIZE                    13
#define ESPCP_EVENT_MESSAGE_QUEUE_NAME           "/Esp32Events"
#define ESPCP_REQUEST_MESSAGE_QUEUE_NAME         "/Esp32Requests"
#define ESPCP_EVENT_HANDLER_MESSAGE_QUEUE_NAME   "/IncomingEvents"
#define ESPCP_DEFAULT_MESSAGE_PRIORITY           1

// Note: Most of these definitions are in the esp32 codebase (espcp_shared_enums.h)
#define ESPCP_CELL_CONNECTED_EVENT        0x00
#define ESPCP_CELL_DISCONNECTED_EVENT     0x01
#define ESPCP_CELL_ERROR_EVENT            0x02
#define ESPCP_CELL_AT_CMD_EVENT           0x04

#define ESPCP_WIFI_NTP_UPDATE_EVENT       0x26
#define ESPCP_ETHERNET_NTP_UPDATE_EVENT   0x26
#define ESPCP_CELL_NTP_UPDATE_EVENT       0x05
#define ESPCP_CELL_CONNECTING_EVENT       0x08
#define ESPCP_CELL_RETRY_EXCEEDED_EVENT   0x09

#define ESPCP_WIFI_INTERFACE              0x01
#define ESPCP_ETHERNET_INTERFACE          0x06
#define ESPCP_CELL_INTERFACE              0x07
#define ESPCP_NONE_INTERFACE              0x00

#define ESPCP_SIMPLE_EVENT_MESSAGE_ID     0x00
#define ESPCP_COMPLETED_OK_STATUS_CODE    0x00
#define ESPCP_FAILURE_STATUS_CODE         0x03

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

struct espcp_event_data_s
{
    uint8_t interface;
    uint32_t function;
    uint32_t status_code;
    uint32_t message_id;
};
typedef struct espcp_event_data_s espcp_event_data_t;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int espcp_queue_event_messages(uint8_t *message);
void espcp_encode_event_data(espcp_event_data_t *event_data, uint8_t *buffer);
void espcp_send_message_to_ntp_queue(const uint32_t message);
int espcp_open_esp32_events_message_queue();

#endif //__CONFIGS_MEADOW_SRC_HCOM_MISC_ESPCP_UTILS__H
