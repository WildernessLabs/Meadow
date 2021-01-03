/****************************************************************************
 * espcp_interrupt_handlers.h
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

#include "espcp_interrupt_handlers.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Function prototypes for static methods implemented in this file.
 ****************************************************************************/
void espcp_system_get_configuration_interrupt_handler(espcp_message_t *);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 *  Table of interrupt handlers for the WiFi and socket handlers.
 */
static espcp_interrupt_handlers_t _wifi_handlers[] = 
{
    { espcp_wi_fi_function_interrupt_poll_response, espcp_usrsock_poll_interrupt_handler },
    { 0xffffffff, NULL }
};

/**
 *  Table of interrupt handlers for the system interface.
 */
static espcp_interrupt_handlers_t _system_handlers[] = 
{
    { espcp_system_function_get_configuration, espcp_system_get_configuration_interrupt_handler },
    { 0xffffffff, NULL }
};

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_dispatch_interrupt
 *
 * Description:
 *  Dispatches an interrupt to the designated interrupt handler.
 *
 * Input Parameters:
 *  message - Response (interrupt information) to be processed
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_dispatch_interrupt(espcp_message_t *message)
{
    if (message != NULL)
    {
        espcp_interrupt_handlers_t *handler = NULL;
        switch (message->interface)
        {
            case espcp_esp32_interfaces_wi_fi:
                handler = _wifi_handlers;
                break;
            case espcp_esp32_interfaces_system:
                handler = _system_handlers;
                break;
        }
        bool processing = (handler != NULL);
        while (processing)
        {
            if (handler->function != 0xffffffff)
            {
                if (handler->function == message->function)
                {
                    handler->interrupt_handler(message);
                    processing = false;
                }
                else
                {
                    handler++;
                }
            }
            else
            {
                processing = false;
            }
        }
    }
}

/****************************************************************************
 * Name: espcp_system_get_configuration_interrupt_handler
 *
 * Description:
 *   This interrupt handler will be called the GetConfiguration request has
 *   been processed by the ESP32.  The message payload should be the device
 *   configuration.
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the result of the
 *             GetConfiguration request.
 *
 ****************************************************************************/
void espcp_system_get_configuration_interrupt_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        if ((message->payload_length > 0) && (message->payload != NULL))
        {
            espcp_configuration_t *config = espcp_get_configuration();
            config->esp_config = espcp_extract_system_configuration(message->payload);
        }
    }
    espcp_delete_message_and_payload(message);
}
