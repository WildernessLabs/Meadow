/****************************************************************************
 * meadow_cell.c
 *
 *  Copyright (C) 2025 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
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
#include <stdio.h>

#include <nuttx/semaphore.h>
#include <nuttx/config.h>

#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

static cell_event_data_t *g_cell_event_data;
static sem_t cell_sem;
static size_t cell_event_data_len = 0;
static bool g_cell_init = false;

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

 /****************************************************************************
 * Name: meadow_cell_lock
 *
 * Description:
 *  Lock the cell event.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
 static void meadow_cell_lock(void)
{
    sem_wait(&cell_sem);
}

/****************************************************************************
 * Name: meadow_cell_unlock
 *
 * Description:
 *  Unlock the cell event.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void meadow_cell_unlock(void)
{
    sem_post(&cell_sem);
}

/****************************************************************************
 * Name: meadow_cell_event_init
 *
 * Description:
 *  Initialize the cell event.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/

static void meadow_cell_event_init(void)
{
    sem_init(&cell_sem, 0, 1);
    sem_setprotocol(&cell_sem, SEM_PRIO_NONE);
    g_cell_init = true;
}

/****************************************************************************
 * Name: meadow_cell_event_get_at_cmd
 *
 * Description:
 *  Get the at command from a cell event.
 *
 * Input Parameters:
 *  message - Pointer to an espcp_message_t object.
 *
 * Returned Value:
 *   OK if successful, -EINVAL otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_cell_event_get_at_cmd(espcp_message_t *message)
{
    if (message == NULL)
    {
        return -EINVAL;
    }

    if (!g_cell_init)
    {
        meadow_cell_event_init();
    }

    meadow_cell_lock();

    if (g_cell_event_data)
    {
        free(g_cell_event_data);
        g_cell_event_data = NULL;
    }

    g_cell_event_data = espcp_extract_cell_event_data(message->payload);

    if (g_cell_event_data)
    {
        cell_event_data_len = message->payload_length;
    }
    else
    {
        g_cell_event_data = NULL;
        cell_event_data_len = 0;
    }
    meadow_cell_unlock();
    return OK;
}

/****************************************************************************
 * Name: meadow_cell_event_get_script
 *
 * Description:
 *  Get the script to run on the cell.
 *
 * Input Parameters:
 *  script - store the commands.
 *  len    - max size of the script.
 *
 * Returned Value:
 *   OK if successful, -ENODATA if no data available, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_cell_event_get_script(char *script, size_t len)
{
    if (script == NULL || !g_cell_init)
    {
        return ERROR;
    }

    if (cell_event_data_len <= 0)
    {
        return -ENODATA;
    }
    meadow_cell_lock();

    snprintf(script, len, "TIMEOUT %d \"\" %s PAUSE 3 %s \\c", 
                    g_cell_event_data->timeout, 
                    g_cell_event_data->command,
                    g_cell_event_data->response  == 0 ? "OK" : "" );
    cell_event_data_len = 0;
    meadow_cell_unlock();
    return OK;
}