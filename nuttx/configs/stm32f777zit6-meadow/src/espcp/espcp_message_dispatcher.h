/****************************************************************************
 * espcp_message_dispatcher.h
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
#ifndef _ESPCP_MESSAGE_DISPATCHER_H
#define _ESPCP_MESSAGE_DISPATCHER_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>
#include <nuttx/config.h>

#include "espcp_message.h"
#include "espcp_coprocessor.h"
#include "espcp_shared_enums.h"
#include "espcp_event_handlers.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/
/*
 *  Mask used to identify messages from the STM32 to the ESP32.  All messages
 *  with and ID under 0x80000000 are from the ESP32 to the STM32, messages
 *  over 0x80000000 are from the STM32 to the ESP32.
 */
#define ESP32_MESSAGE_ID_MASK  0x80000000

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
int espcp_setup_message_dispatcher(void);
int espcp_teardown_message_dispatcher(void);
void espcp_queue_send_response_message(void);
uint32_t espcp_get_next_message_id(void);
void espcp_send_message(espcp_configuration_t *, espcp_message_t *);
void espcp_get_message(espcp_configuration_t *, espcp_message_t *);
bool espcp_cancel_waiting_message(espcp_message_t *message);

#endif /* _ESPCP_MESSAGE_DISPATCHER_H */
