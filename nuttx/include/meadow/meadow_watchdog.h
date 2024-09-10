/****************************************************************************
 * meadow_watchdogs.h
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

#ifndef __MEADOW_WATCHDOGS_H
#define __MEADOW_WATCHDOGS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <unistd.h>
#include <syslog.h>

#include <nuttx/config.h>
#include <nuttx/timers/watchdog.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os.h>

#include "../nuttx/wdog.h"

/****************************************************************************
 * Preprocessor Definitions
 ****************************************************************************/

#define ENABLE_MEADOW_WATCHDOGS

#define WATCHDOG_POLL_TIMEOUT_MILLISECONDS    (300 * 1000)
#define WATCHDOG_CLOSE_TIMEOUT_MILLISECONDS   (300 * 1000)
#define WATCHDOG_SOCKET_TIMEOUT_MILLISECONDS  (300 * 1000)
#define WATCHDOG_RECV_TIMEOUT_MILLISECONDS    (300 * 1000)
#define WATCHDOG_SEND_TIMEOUT_MILLISECONDS    (300 * 1000)
#define WATCHDOG_SENDTO_TIMEOUT_MILLISECONDS  (300 * 1000)

typedef enum
{
    POLL_WATCHDOG = 0,
    CLOSE_WATCHDOG,
    SOCKET_WATCHDOG,
    RECV_WATCHDOG, 
    SEND_WATCHDOG,
    SENDTO_WATCHDOG
} meadow_watchdog_methods_e;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void meadow_watchdog_reset_system(int argc, wdparm_t arg);
void meadow_watchdog_activate(struct wdog_s *watchdog, uint32_t timeout, meadow_watchdog_methods_e watchdog_method);
void meadow_watchdog_deactivate(struct wdog_s *watchdog);

#endif /* __MEADOW_WATCHDOGS_H */
