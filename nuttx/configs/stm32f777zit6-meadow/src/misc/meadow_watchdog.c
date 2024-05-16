/****************************************************************************
 * meadow_watchdogs.c
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

// Implementation file for handling watchdog timers in the Meadow platform.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/meadow_watchdog.h>

#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void meadow_watchdog_reset_system(int argc, char *argv[])
{
    syslog(LOG_ERR, "Detected a network deadlock. Propagating OS exception to the managed environment...\n");

    meadow_os_raise_simple_exception(espcp_status_codes_network_deadlock);
}

void meadow_watchdog_activate(struct wdog_s *watchdog, uint32_t timeout)
{
    // Clear the WDOGF_ACTIVE flag to ensure that the watchdog starts in an inactive state.
    WDOG_CLRACTIVE(watchdog);

    int ret = wd_start(watchdog, timeout, (wdentry_t)meadow_watchdog_reset_system, 0);
    if (ret < 0)
    {
        syslog(LOG_ERR, "Failed to activate watchdog: %d\n", ret);
    }
}

void meadow_watchdog_deactivate(struct wdog_s *watchdog)
{
    int ret = wd_cancel(watchdog);
    if (ret < 0)
    {
        syslog(LOG_ERR, "Failed to deactivate watchdog: %d\n", ret);
    }
}
