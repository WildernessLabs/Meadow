/****************************************************************************
 * espcp_system.c
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

#include "espcp_system.h"
#include "espcp_encoders.h"
#include "espcp_shared_enums.h"
#include "espcp_message_dispatcher.h"
#include "espcp_coprocessor.h"
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
 * Name: espcp_get_battery_charge_level
 *
 * Description:
 *  Get the current battery charge level.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Current battery charge level in millivolts or -1 if there is a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int32_t espcp_get_battery_charge_level(void)
{
    int32_t result = -1;      // Assume failure.

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system, 
            espcp_system_function_get_battery_charge_level, espcp_status_codes_completed_ok,
            espcp_get_next_message_id(), NULL, 0);
    if (message == NULL)
    {
        return(result);
    }

    if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
    {
        if (message->payload_length == 4)
        {
            espcp_get_battery_charge_level_response_t *charge = espcp_extract_get_battery_charge_level_response(message->payload);
            if (charge != NULL)
            {
                result = charge->level;
                free(charge);
            }
            else
            {
                result = -1;
            }
        }
        else
        {
            result = -1;
        }
    }

    espcp_delete_message_and_payload(message);
    return(result);
}

/****************************************************************************
 * Name: espcp_get_device_configuration
 *
 * Description:
 *  Get the system configuration information from the ESP32.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  This method will queue an asynchronous request for the ESP32 to get
 *  the software configuration.  g_espcp_configuration->esp_config starts
 *  out NULL and will be set when the asynchronous method returns a result. 
 *
 ****************************************************************************/
void espcp_get_device_configuration(void)
{
    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_system, 
            espcp_system_function_get_configuration, espcp_status_codes_completed_ok,
            espcp_get_next_message_id(), NULL, 0);
    if (message != NULL)
    {
        espcp_queue_message(message, false);
    }
}