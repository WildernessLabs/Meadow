/****************************************************************************
 * espcp_posix.h
 *
 *  Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
 * 
 *  Header for the methods supporting the POSIX functions required by Mono.
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
#ifndef _ESPCP_POSIX_H
#define _ESPCP_POSIX_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#include <nuttx/semaphore.h>
#include <nuttx/pthread.h>
#include <nuttx/config.h>

// #include "../hcom/hcom_common.h"
#include "espcp_message_dispatcher.h"
#include "espcp_message.h"
#include "espcp_shared_enums.h"
#include "espcp_queue.h"
#include "espcp_encoders.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

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
void espcp_posix_network_init(void);
int espcp_getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
// int32_t espcp_connect(int, const struct sockaddr *, socklen_t);
void espcp_freeaddrinfo(struct addrinfo *);
// int32_t espcp_setsockopt(int, int, int, const void *, socklen_t);
int32_t espcp_write(int, const void *, size_t);
int32_t espcp_read(int, const void *, size_t);

#endif /* _ESPCP_POSIX_H */