/****************************************************************************
 * meadow_os_espcp.c
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

#include <nuttx/config.h>

#include <stdlib.h>

#include <meadow/meadow_os.h>

#include "espcp/espcp_coprocessor.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_espcp_reset
 *
 * Description:
 *  Reset the ESP32.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_os_espcp_reset(void)
{
    espcp_reset();
}

/****************************************************************************
 * Name: meadow_os_espcp_enter_programming_mode
 *
 * Description:
 *  Force the ESP32 into programming mode so that HCOM can reprogram the
 *  chip.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_espcp_enter_programming_mode(void)
{
    return(espcp_enter_programming_mode());
}

/****************************************************************************
 * Name: meadow_os_network_monitor_process_line
 *
 * Description:
 *  Process the line of data received from the UART connected to the ESP32.
 *
 * Input Parameters:
 *  line - line of text to be processed.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_os_espcp_monitor_process_line(char *line)
{
    espcp_uart_monitor_process_line(line);
}

/****************************************************************************
 * Name: meadow_os_coprocessor_deep_sleep
 *
 * Description:
 *  Send the deep sleep message to the ESP32 and update NuttX accordingly.
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
void meadow_os_coprocessor_deep_sleep(void)
{
    espcp_deep_sleep();
}
