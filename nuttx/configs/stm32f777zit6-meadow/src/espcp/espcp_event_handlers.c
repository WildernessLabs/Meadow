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
#include <nuttx/config.h>

#include <sys/types.h>

#include <nuttx/kthread.h>
#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "espcp_event_handlers.h"
#include "../espcp/espcp_system.h"
#include "espcp_coprocessor.h"
#include "generic_list.h"
#include "../ntpclient/ntpclient.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

#define END_OF_HANDLERS_VALUE       0xffffffff

/****************************************************************************
 * Function prototypes for static methods implemented in this file.
 ****************************************************************************/
void espcp_wi_fi_set_time_of_day_event_handler(espcp_message_t *);
void espcp_wi_fi_connect_to_access_point_event_handler(espcp_message_t *);

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
    { espcp_wi_fi_function_connect_to_access_point_event, espcp_wi_fi_connect_to_access_point_event_handler },
    { END_OF_HANDLERS_VALUE, NULL }
};

/**
 *  Table of event handlers for the system interface.
 */
static espcp_event_handlers_t _system_handlers[] = 
{
    { espcp_system_function_get_configuration, espcp_system_get_configuration_event_handler },
    { espcp_system_function_error_event, espcp_system_error_event_handler },
    { END_OF_HANDLERS_VALUE, NULL }
};

/**
 *  Table of event handlers for the bluetooth interface.
 */
static espcp_event_handlers_t _bluetooth_handlers[] = 
{
    { END_OF_HANDLERS_VALUE, NULL }
};

/**
 *  List of events (messages) that have payloads and are pending a request
 *  for the payload from Meadow.Core.
 */
static gl_linked_list_t *_events_with_payloads = NULL;

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_thread
 *
 * Description:
 *  Start the process thread that runs the ESP32 coprocessor thread.
 *
 * Input Parameters (non protected build)
 *  parameters - pointer to the ESP32 coprocessor configuration
 *
 * Input Parameters (protected build)
 *  argc - Number of arguments being passed.
 *  argv - Argument list.
 *
 * Returned Value:
 *  NULL
 *
 * Assumptions/Limitations:
 *  The message queue for this thread must be created before the thread is
 *  started.  The first thing this method will do is to request the
 *  configuration from the ESP32 and this is done through a message.
 *
 ****************************************************************************/
#ifdef CONFIG_BUILD_PROTECTED
static void *espcp_event_handler_thread(int argc, char *argv[])
#else
static void *espcp_event_handler_thread(void *parameters)
#endif
{
#ifdef CONFIG_BUILD_PROTECTED
    espcp_config_lock();
    espcp_configuration_t *configuration = espcp_get_configuration();
#else
    espcp_configuration_t *configuration = parameters;
#endif
    mqd_t queue_id = configuration->incoming_event_queue;
    configuration->incoming_event_handler_thread_running = true;
#ifdef CONFIG_BUILD_PROTECTED
    espcp_config_unlock();
#endif
    while (true)
    {
        espcp_message_t *retrieved_message;
        int number_of_bytes = mq_receive(queue_id, (void *) &retrieved_message, sizeof(retrieved_message), NULL);
        if ((number_of_bytes == sizeof(espcp_message_t *)) && (retrieved_message != NULL))
        {
            espcp_dispatch_event(retrieved_message);
        }
        else
        {
            syslog(LOG_CRIT, "Errno: %d", errno);
            syslog(LOG_CRIT, "%s@%d ESP thread received %d bytes, %d expected.\n", __FILE__, __LINE__, number_of_bytes, sizeof(espcp_message_t));
        }
    }

    return (NULL);
}



/****************************************************************************
 * Name: espcp_event_handlers_thread_start
 *
 * Description:
 *  Start the thread that will process the events from the ESP32.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if the thread was created successfully, otherwise an error code is
 *  returned.
 *
 * Assumptions/Limitations:
 *  Message queue has already been created elsewhere.
 *
 ****************************************************************************/
int espcp_event_handlers_thread_start(void)
{
    espcp_config_lock();
    espcp_configuration_t *configuration = espcp_get_configuration();
    bool is_thread_running = configuration->incoming_event_handler_thread_running;
    espcp_config_unlock();
    if (is_thread_running)
    {
        return (EALREADY);
    }

    int result = OK;
    int thread_id = 0;

#ifdef CONFIG_BUILD_PROTECTED
    thread_id = kthread_create(ESPCP_EVENT_HANDLER_THREAD_NAME, CONFIG_MEADOW_ESPCP_PRIORITY,
                               CONFIG_MEADOW_ESPCP_STACKSIZE, (main_t) espcp_event_handler_thread, (char *const *) NULL);

    if (thread_id <= 0)
    {
        return -ENOEXEC;
    }
#else
    pthread_attr_t thread_attributes;

    result = pthread_attr_init(&thread_attributes);
    if (result != OK)
    {
        return (-result);
    }

    struct sched_param scheduler_parameters;
    scheduler_parameters.sched_priority = CONFIG_MEADOW_ESPCP_PRIORITY;
    result = pthread_attr_setschedparam(&thread_attributes, &scheduler_parameters);
    if (result != OK)
    {
        return (-result);
    }

    result = pthread_attr_setstacksize(&thread_attributes, CONFIG_MEADOW_ESPCP_STACKSIZE);
    if (result != OK)
    {
        return (-result);
    }

    result = pthread_create(&configuration->thread, &thread_attributes, espcp_event_handler_thread, configuration);
    if (result != OK)
    {
        return (-result);
    }
#endif

    mqd_t queue_id = mq_open(ESPCP_EVENT_HANDLER_MESSAGE_QUEUE_NAME, O_RDWR);
    if ((int) queue_id < 0)
    {
        result = -1;
    }

    espcp_config_lock();
    configuration->incoming_event_handler_thread_running = true;
    configuration->incoming_event_queue = queue_id;
    configuration->incoming_event_thread = thread_id;
    espcp_config_unlock();

    return(result);
}



/****************************************************************************
 * Name: espcp_event_handlers_init
 *
 * Description:
 *  Initialise the event handlers.
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
void espcp_event_handlers_init(void)
{
    _events_with_payloads = gl_create_empty_linked_list();
    espcp_event_handlers_thread_start();
}

/****************************************************************************
 * Name: espcp_compare_event_ids
 *
 * Description:
 *  Compare the specified event ID with the one in the pointer to a message.
 * 
 *  This method is used by the generic linked list code.
 *
 * Input Parameters:
 *  message_id - ID of the message to locate in the list.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static bool espcp_compare_event_ids(uint32_t event_id, void *event_data)
{
    bool result = false;
    espcp_message_t *message = (espcp_message_t *) event_data;
    if (message->message_id == event_id)
    {
        result = true;
    }
    return(result);
}

/****************************************************************************
 * Name: espcp_get_event_data
 *
 * Description:
 *  Find the event data in the list of events with payloads.  Remove the event
 *  from the list and return a pointer to the event data.
 * 
 *  There can be a small time delay between the managed code requesting the
 *  event data and it being available.  The retry loop below takes this into
 *  consideration and pauses for a short time before retrying.
 *
 * Input Parameters:
 *  message_id - ID of the message to locate in the list.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
espcp_message_t *espcp_get_event_data(uint32_t message_id)
{
    espcp_message_t *event_data = NULL;

    int retry_count = 0;
    do
    {
        event_data = (espcp_message_t *) gl_remove_item(_events_with_payloads, message_id, espcp_compare_event_ids);
        if (event_data == NULL)
        {
            retry_count++;
            usleep(10000);
        }
    }
    while ((event_data == NULL) && (retry_count < 10));
    return((espcp_message_t *) event_data);
}

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
                if (handler->function != END_OF_HANDLERS_VALUE)
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
 *             GetConfiguration request or the configuration event generated
 *             when the ESP starts and send a message to the STM32.
 *
 ****************************************************************************/
void espcp_system_get_configuration_event_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        if ((message->payload_length > 0) && (message->payload != NULL))
        {
            espcp_system_configuration_t *esp_config = espcp_extract_system_configuration(message->payload);
            if (esp_config != NULL)
            {
                syslog(LOG_INFO, "ESP32 Coprocessor ready, firmware version %s\n", esp_config->software_version);
                hcom_nx_config_process_esp_configuration(esp_config);
                espcp_clean_system_config_object(esp_config);
                free(esp_config);
                hcom_nx_config_process_wifi_credentials_file();
                hcom_nx_config_lock();
                meadow_configuration_t *config = hcom_nx_config_get_pointer();
                bool start = (config->automatically_start_network == 1) && (config->default_access_point != NULL);
                hcom_nx_config_unlock();
                if (start)
                {
                    espcp_queue_add_nonblocking_message(espcp_message_types_header, espcp_esp32_interfaces_wi_fi, 
                                                        espcp_wi_fi_function_connect_to_default_access_point, NULL, 0);
                }
            }
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
            syslog(LOG_CRIT, "Error event received.\n");
        }
    }
    espcp_delete_message_and_payload(message);
}

/****************************************************************************
 * Name: espcp_wi_fi_connect_to_access_point_event_handler
 *
 * Description:
 *   This event handler will be called when the ESP32 generates a connect
 *   to access point event.
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the connect to access point
 *             event data.
 *
 ****************************************************************************/
void espcp_wi_fi_connect_to_access_point_event_handler(espcp_message_t *message)
{
    if (message->status_code == espcp_status_codes_completed_ok)
    {
        bool get_time;
        hcom_nx_config_lock();
        meadow_configuration_t *config = hcom_nx_config_get_pointer();
        get_time = config->get_network_time_at_startup;
        hcom_nx_config_unlock();
        if (get_time)
        {
            ntpc_start();
        }
    }
    espcp_pass_to_managed_event_handler(message);
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
    eventData.status_code = message->status_code;
    if (message->payload_length > 0)
    {
        //
        //  This will indicate to the managed code that there is a payload
        //  to process.
        //
        eventData.message_id = message->message_id;
    }

    uint32_t encodedEventDataSize = espcp_event_data_buffer_size(&eventData);
    bool delete_message = false;
    if (encodedEventDataSize > 22)
    {
        syslog(LOG_INFO, "Event message too large, event data discarded.");
        delete_message = true;
    }
    else
    {
        uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);
        if (encodedData != NULL)
        {
            espcp_encode_event_data(&eventData, encodedData);

            espcp_configuration_t *config = espcp_get_configuration();
            int result = mq_send(config->managed_event_queue, (const char *) encodedData, encodedEventDataSize, ESPCP_DEFAULT_MESSAGE_PRIORITY);
            if (result < 0)
            {
                syslog(LOG_INFO, "Error adding event to the message queue, result %d, error code %d.", result, get_errno());
                delete_message = true;
            }
            else
            {
                if (message->payload_length == 0)
                {
                    delete_message = true;
                }
                else
                {
                    gl_add_item_to_tail(_events_with_payloads, (void *) message);
                }
            }
            free(encodedData);
        }
    }
    if (delete_message)
    {
        espcp_delete_message_and_payload(message);
    }
}