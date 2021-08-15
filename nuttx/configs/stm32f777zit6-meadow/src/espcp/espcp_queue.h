/****************************************************************************
 * espcp_queue.h
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

#ifndef __ESPCP_QUEUE_H
#define __ESPCP_QUEUE_H

#pragma once

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <arch/irq.h>

#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>
#include <nuttx/mqueue.h>
#include <nuttx/config.h>

#include "espcp_coprocessor.h"
#include "espcp_message.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * 
 *  Name of the message queue used to store the outbound messages.
 * 
****************************************************************************/
#define ESPCP_REQUEST_MESSAGE_QUEUE_NAME    "/Esp32Requests"

#define ESPCP_EVENT_HANDLER_MESSAGE_QUEUE_NAME    "/IncomingEvents"

/****************************************************************************
 * 
 *  Name of the message queue used to store the event messages.
 * 
****************************************************************************/
#define ESPCP_EVENT_MESSAGE_QUEUE_NAME      "/Esp32Events"

/****************************************************************************
 * 
 *  Maximum number of messages that can be added to the outbound message
 *  queue.
 * 
****************************************************************************/
#define ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH 10

/****************************************************************************
 * 
 *  Priority of the messages added to the message queue.
 * 
****************************************************************************/
#define ESPCP_DEFAULT_MESSAGE_PRIORITY 1

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
bool espcp_create_message_queues(espcp_configuration_t *);
int espcp_delete_message_queue(mqd_t);
int espcp_add_message_to_queue(mqd_t, espcp_message_t *);
int espcp_queue_add_nonblocking_message(uint8_t, uint8_t, uint32_t, uint8_t *, uint32_t);
void *espcp_get_message_from_queue(mqd_t);
void espcp_queue_kill_nuttx_thread_message(mqd_t);

#endif /* __ESPCP_QUEUE_H */