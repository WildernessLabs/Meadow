/****************************************************************************
 * espcp_event_handlers.h
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

#ifndef __ESPCP_EVENT_HANDLERS_H__
#define __ESPCP_EVENT_HANDLERS_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <fcntl.h>

#include "espcp_wifi.h"
#include "espcp_shared_enums.h"
#include "espcp_message.h"
#include "espcp_encoders.h"
#include "espcp_system.h"
#include "espcp_common.h"
#include "espcp_usrsock.h"

/****************************************************************************
 * Structures
 ****************************************************************************/

/*
 *  Structure used to hold a table of interrupt handlers for the ESP functions.
 */
struct espcp_event_handlers_s
{
    /**
     *  Function expecting to receive and interrupt.
     */
    uint32_t function;

    /**
     *  Method that will take the message and process it.
     */
    void (*event_handler)(espcp_message_t *);
};
typedef struct espcp_event_handlers_s espcp_event_handlers_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
void espcp_event_handlers_init(void);
espcp_message_t *espcp_get_event_data(uint32_t);
void espcp_usrsock_poll_interrupt_handler(espcp_message_t *);   // Found in espcp_usrsock_sockif.c
void espcp_dispatch_event(espcp_message_t *);

#endif /* __ESPCP_EVENT_HANDLERS_H__ */