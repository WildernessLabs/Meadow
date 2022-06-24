/****************************************************************************
 * espcp_thread.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <fcntl.h>

#include <arch/irq.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>
#include <nuttx/kthread.h>

#include "espcp_coprocessor.h"
#include "espcp_message.h"
#include "espcp_thread.h"
#include "espcp_queue.h"
#include "espcp_shared_enums.h"
#include "espcp_message_dispatcher.h"
#include "espcp_system.h"

// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>


/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_is_thead_running
 *
 * Description:
 *  Check the configuration object to see if the ESP32 coprocessor thread is
 *  running.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration
 *
 * Returned Value:
 *  True if the thread is running, false otherwise.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
bool espcp_is_thread_running(espcp_configuration_t *configuration)
{
    espcp_config_lock();
    bool thread_running = configuration->thread_running;
    espcp_config_unlock();

    return (thread_running);
}

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
static void *espcp_thread(int argc, char *argv[])
#else
static void *espcp_thread(void *parameters)
#endif
{
#ifdef CONFIG_BUILD_PROTECTED
    espcp_config_lock();
    espcp_configuration_t *configuration = espcp_get_configuration();
#else
    espcp_configuration_t *configuration = parameters;
#endif

    bool thread_running = true;
    configuration->thread_running = thread_running;
#ifdef CONFIG_BUILD_PROTECTED
    espcp_config_unlock();
#endif

    while (thread_running)
    {
        espcp_message_t *retrieved_message;
        MEADOW_TRACE_INFORMATION("Retrieving message to send to ESP32.\n");
        int number_of_bytes = mq_receive(configuration->request_queue, (void *)&retrieved_message, sizeof(retrieved_message), NULL);
        if (number_of_bytes == sizeof(espcp_message_t *))
        {
            if (retrieved_message != NULL)
            {
                if ((retrieved_message->message_type == espcp_message_types_transport) && (retrieved_message->function == espcp_transport_function_kill_nuttx_thread))
                {
                    thread_running = false;
                    espcp_config_lock();
                    configuration->thread_running = thread_running;
                    configuration->exit_code = OK;
                    espcp_config_unlock();
                    free(retrieved_message);
#ifdef CONFIG_BUILD_PROTECTED
                    kthread_delete(0);
#else
                    pthread_exit(configuration);
#endif
                }
                else
                {
                    MEADOW_TRACE_INFORMATION("Waiting for SPI interface.\n");
                    espcp_lock_spi_interface();
                    MEADOW_TRACE_INFORMATION("Sending message.\n");
                    if ((retrieved_message->interface == espcp_esp32_interfaces_transport) && (retrieved_message->function == espcp_transport_function_send_response))
                    {
                        espcp_get_message(configuration, retrieved_message);
                    }
                    else
                    {
                        espcp_send_message(configuration, retrieved_message);
                    }
                }
            }
        }
        else
        {
            MEADOW_TRACE_CRITICAL("%s@%d ESP thread received %d bytes, %d expected.\n", _thisFile, __LINE__, number_of_bytes, sizeof(espcp_message_t));
        }
    }

    return (NULL);
}

/****************************************************************************
 * Name: espcp_thread_start
 *
 * Description:
 *  Start the thread that will process the messages for the ESP32
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration
 *
 * Returned Value:
 *  OK if the thread was created successfully, otherwise an error code is
 *  returned.
 *
 * Assumptions/Limitations:
 *  Message queue has already been created elsewhere.
 *
 ****************************************************************************/
int espcp_thread_start(espcp_configuration_t *configuration)
{
    if (espcp_is_thread_running(configuration))
    {
        return (EALREADY);
    }

    int result = OK;
    int thread_id = 0;

#ifdef CONFIG_BUILD_PROTECTED
    thread_id = kthread_create(ESPCP_THREAD_NAME, CONFIG_MEADOW_ESPCP_PRIORITY,
                                           CONFIG_MEADOW_ESPCP_STACKSIZE, (main_t) espcp_thread, (char *const *) NULL);

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

    result = pthread_create(&configuration->thread, &thread_attributes, espcp_thread, configuration);
    if (result != OK)
    {
        return (-result);
    }
#endif

    mqd_t queue_id = mq_open(ESPCP_REQUEST_MESSAGE_QUEUE_NAME, O_RDWR);
    if ((int) queue_id < 0)
    {
        result = -1;
    }

    espcp_config_lock();
    configuration->request_queue = queue_id;
    configuration->thread = thread_id;
    configuration->exit_code = result;
    espcp_config_unlock();

    return(result);
}

/****************************************************************************
 * Name: espcp_thread_stop
 *
 * Description:
 *  Stop the thread that is running the communication with the ESP32.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration
 *
 * Returned Value:
 *  OK if the thread was created successfully, otherwise an error code is
 *  returned.
 *
 * Assumptions/Limitations:
 *  Message queue has been set up so that the kill message can be sent to
 *  the thread.
 *
 ****************************************************************************/
int espcp_thread_stop(espcp_configuration_t *configuration)
{
    int result = OK;

    espcp_queue_kill_nuttx_thread_message(configuration->request_queue);
#ifdef CONFIG_BUILD_PROTECTED
    result = kthread_delete(configuration->thread);
#else
    result = kthread_delete(configuration->thread, NULL);
#endif
    mq_close(configuration->request_queue);

    espcp_config_lock();
    configuration->request_queue = (mqd_t) -1;
    configuration->thread = 0;
    espcp_config_unlock();

    return (result);
}
