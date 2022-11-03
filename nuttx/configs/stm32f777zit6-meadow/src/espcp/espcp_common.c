/****************************************************************************
 * espcp_common.c
 *
 *  Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
 * 
 *  Methods supporting the ESP system functions (e.g. GetBatteryChargeLevel).
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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>
#include <nuttx/config.h>

#include "espcp_common.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_queue_message
 *
 * Description:
 *  Add a message to the message queue and wait on the semaphore.
 * 
 * Input Parameters:
 *  message - Pointer to an espcp_message_t object.
 *  block - Wait for a response if true, return immediately if false.
 *
 * Returned Value:
 *  Status code (completed OK for success failure if there is a problem).
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t espcp_queue_message(espcp_message_t *message, bool block)
{
    uint32_t result = espcp_status_codes_failure;

    sem_t sem;
    if (block)
    {
        message->semaphore = &sem; // cppcheck-suppress autoVariables
        sem_init(message->semaphore, 0, 0);
        sem_setprotocol(&sem, SEM_PRIO_NONE);
    }
    else
    {
        message->semaphore = NULL;
    }
    espcp_configuration_t *configuration = espcp_get_configuration();
    if (espcp_add_message_to_queue(configuration->request_queue, message) == espcp_status_codes_completed_ok)
    {
        if (block)
        {
            bool waiting = true;
            //
            //  We wait in a loop and check the result code for the sem_wait method
            //  as it is possible to have a return from a sem_wait as a result of a
            //  signal as well as a sem_post.  In the case of a signal we simply wait
            //  again.
            //
            //  TODO: Need to investigate why we get the signal.
            //
            while (waiting)
            {
                if (sem_wait(message->semaphore) == 0)
                {
                    waiting = false;
                }
            }
        }
        result = espcp_status_codes_completed_ok;
    }
    return (result);
}
