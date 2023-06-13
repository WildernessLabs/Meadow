/****************************************************************************
 * long_period_timer.h
 *
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

#include <nuttx/config.h>

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <debug.h>
#include <sys/types.h>

#include <ctype.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Default timer period in seconds.
 */
#define LPS_DEFAULT_PERIOD  60

/**
 * 
 */
#ifndef CONFIG_LPSDAEMON_STACKSIZE
#  define CONFIG_LPSDAEMON_STACKSIZE 4096
#endif

/**
 * @brief Long period daemon task priority.
 * 
 *  This can be low as the task should only run periodically and the tasks
 *  added to the scheduler should be low priority activities.
 */
#ifndef CONFIG_LPSDAEMON_SERVERPRIO
#  define CONFIG_LPSDAEMON_SERVERPRIO 50
#endif


/****************************************************************************
 * Type defintions.
 ****************************************************************************/

/**
 *  @brief Function prototype for the method that will be executed on the
 *         requested period.
 */
typedef uint32_t (*lps_handler_t)(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int lps_add_handler(lps_handler_t, uint32_t);
void lps_remove_handler(lps_handler_t);