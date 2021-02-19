/****************************************************************************
 * espcp_event_handlers.h
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

#include "espcp_event_handlers.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Function prototypes for static methods implemented in this file.
 ****************************************************************************/
void espcp_wi_fi_set_time_of_day_event_handler(espcp_message_t *);

void espcp_system_get_configuration_event_handler(espcp_message_t *);
void espcp_system_error_event_handler(espcp_message_t *);

void espcp_pass_to_managed_event_handler(espcp_message_t *);


/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 *  Table of event handlers for the WiFi and socket handlers.
 */
static espcp_event_handlers_t _wifi_handlers[] = 
{
    { espcp_wi_fi_function_interrupt_poll_response, espcp_usrsock_poll_interrupt_handler },
    { espcp_wi_fi_function_set_time_of_day_event, espcp_wi_fi_set_time_of_day_event_handler },
    { 0xffffffff, NULL }
};

/**
 *  Table of event handlers for the system interface.
 */
static espcp_event_handlers_t _system_handlers[] = 
{
    { espcp_system_function_get_configuration, espcp_system_get_configuration_event_handler },
    { espcp_system_function_error_event, espcp_system_error_event_handler },
    { 0xffffffff, NULL }
};

/**
 *  Table of event handlers for the bluetooth interface.
 */
static espcp_event_handlers_t _bluetooth_handlers[] = 
{
    { 0xffffffff, NULL }
};

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_dispatch_event
 *
 * Description:
 *  Dispatches an event to the designated event handler.
 *
 * Input Parameters:
 *  message - Response (event information) to be processed
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_dispatch_event(espcp_message_t *message)
{
    if (message != NULL)
    {
        espcp_event_handlers_t *handler = NULL;
        switch (message->interface)
        {
            case espcp_esp32_interfaces_wi_fi:
                handler = _wifi_handlers;
                break;
            case espcp_esp32_interfaces_system:
                handler = _system_handlers;
                break;
            case espcp_esp32_interfaces_blue_tooth:
                handler = _bluetooth_handlers;
                break;
        }
        if (handler == NULL)
        {
            espcp_pass_to_managed_event_handler(message);
        }
        else
        {
            bool processing = true;
            while (processing)
            {
                if (handler->function != 0xffffffff)
                {
                    if (handler->function == message->function)
                    {
                        handler->event_handler(message);
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
                    //
                    //  If we get here we cannot find a native handler so pass
                    //  this message on to the managed handlers.
                    //
                    espcp_pass_to_managed_event_handler(message);
                }
            }
        }
    }
}

/****************************************************************************
 * Name: espcp_system_get_configuration_event_handler
 *
 * Description:
 *   This event handler will be called the GetConfiguration request has
 *   been processed by the ESP32.  The message payload should be the device
 *   configuration.
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the result of the
 *             GetConfiguration request.
 *
 ****************************************************************************/
void espcp_system_get_configuration_event_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        if ((message->payload_length > 0) && (message->payload != NULL))
        {
            espcp_configuration_t *config = espcp_get_configuration();
            espcp_config_lock(config);
            if (config->esp_config != NULL)
            {
                free(config->esp_config);
            }
            config->esp_config = espcp_extract_system_configuration(message->payload);
            espcp_config_unlock(config);
        }
    }
    espcp_delete_message_and_payload(message);
}

/****************************************************************************
 * Name: espcp_system_function_error_event_handler
 *
 * Description:
 *   This event handler will be called when the ESP32 generates an
 *   error event.
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the new time information.
 *
 ****************************************************************************/
void espcp_system_error_event_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        if ((message->payload_length > 0) && (message->payload != NULL))
        {
            syslog(LOG_CRIT, "Error event received.");
        }
    }
    espcp_delete_message_and_payload(message);
}

/****************************************************************************
 * Name: espcp_wi_fi_set_time_of_day_event_handler
 *
 * Description:
 *   This event handler will be called when the ESP32 generates a new
 *   time event. 
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the new time information.
 *
 ****************************************************************************/
void espcp_wi_fi_set_time_of_day_event_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        if ((message->payload_length > 0) && (message->payload != NULL))
        {
            espcp_integer_response_t *ir = espcp_extract_integer_response(message->payload);

            syslog(LOG_CRIT, "Setting time of day to %d", ir->result);

            struct timeval tv;
            tv.tv_usec = 0;
            tv.tv_sec = ir->result;
            settimeofday(&tv, NULL);
            free(ir);
        }
    }
    espcp_delete_message_and_payload(message);
}

/****************************************************************************
 * Name: espcp_pass_to_managed_event_handler
 *
 * Description:
 *   This event handler passes the data to the managed code for processing.
 *
 * Input Parameters:
 *   message - Message from the ESP32 containing event data.
 *
 ****************************************************************************/
void espcp_pass_to_managed_event_handler(espcp_message_t *message)
{
    espcp_event_data_t eventData;
    memset(&eventData, 0, sizeof(eventData));
    eventData.interface = message->interface;
    eventData.function = message->function;
    eventData.payload_length = message->payload_length;
    if (message->payload_length != 0)
    {
        //
        //  We put the pointer to this message in the status_code.  We cannot use the payload
        //  or payload_length field as these will be used to pass the address of the buffer
        //  used to hold the payload in the managed layer.
        //
        eventData.status_code = (uint32_t) message;
    }

    uint32_t encodedEventDataSize = espcp_event_data_buffer_size(&eventData);
    if (encodedEventDataSize > 22)
    {
        syslog(LOG_INFO, "Event message too large, even data discarded.");
        espcp_delete_message_and_payload(message);
    }
    else
    {
        uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);
        if (encodedData != NULL)
        {
            espcp_encode_event_data(&eventData, encodedData);

            espcp_configuration_t *config = espcp_get_configuration();
            int result = mq_send(config->event_queue, (const char *) encodedData, encodedEventDataSize, ESPCP_DEFAULT_MESSAGE_PRIORITY);
            if (result < 0)
            {
                syslog(LOG_INFO, "Error adding event to the message queue, result %d, error code %d.", result, get_errno());
            }
            free(encodedData);
        }
    }
}