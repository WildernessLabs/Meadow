/****************************************************************************
 * meadow_os_fault_handling.c
 *
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs (Mark Stevens)
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

#include <stdlib.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/meadow_os_fault_handling.h>
#include <meadow/meadow_os_battery_backed_domain.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * External data
 ****************************************************************************/

extern char *g_sysbuffer;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _fault_status = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_fault_handler_save_os_state
 *
 * Description:
 *  
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_fault_handler_save_os_state(void)
{
    //
    //  First we check to see if Mono has recorded a fault.  If it has then
    //  there is nothing for us to do.
    //
    uint32_t fault_status;
    if (meadow_os_bbd_register_get_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, &fault_status) != 0)
    {
        return;
    }
    if (fault_status & FAULT_LOGGING_RT_COMPONENT_ERRORED)
    {
        return;
    }
    //
    //  Now we need to save as much of the OS fault information as possible.
    //
    fault_status |= (FAULT_LOGGING_OS_COMPONENT_ERRORED | FAULT_LOGGING_OS_PHASE1_STARTED);
    meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
    meadow_os_bbd_strdup_to_sram("Unable to save OS fault information.");
    fault_status |= FAULT_LOGGING_OS_PHASE1_COMPLETED;
    meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
}

/****************************************************************************
 * Name: meadow_os_fault_handler_check_fault_code
 *
 * Description:
 *  
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_fault_handler_check_fault_code(void)
{
    if (meadow_os_bbd_register_get_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, &_fault_status) != 0)
    {
        return;
    }
    MEADOW_TRACE_INFORMATION("Fault status: 0x%08x\n", _fault_status);
    if (_fault_status & FAULT_LOGGING_OS_COMPONENT_ERRORED)
    {
        if (((_fault_status & FAULT_LOGGING_OS_PHASE1_STARTED) == 0)  && ((_fault_status & FAULT_LOGGING_OS_PHASE1_COMPLETED) == 0))
        {
            syslog(LOG_INFO, "Meadow OS Component phase 1 logging incomplete.\n");
        }
        else
        {
            char *fault_string = meadow_os_bbd_strdup_from_sram();
            syslog(LOG_INFO, "Meadow OS Component has errored: %s\n", fault_string);
            free(fault_string);
        }
    }
    else
    {
        if (_fault_status & FAULT_LOGGING_RT_COMPONENT_ERRORED)
        {
            syslog(LOG_INFO, "Meadow RT Component has errored.\n");
        }
    }
    //
    //  Clear any fault codes before we exit.
    //
    meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, 0);
}