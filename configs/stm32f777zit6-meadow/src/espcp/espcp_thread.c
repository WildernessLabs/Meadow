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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <fcntl.h>

#include <arch/irq.h>

#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>
#include <nuttx/pthread.h>
#include <nuttx/config.h>
#include <nuttx/kthread.h>

#include "espcp_coprocessor.h"
#include "espcp_message.h"
#include "espcp_thread.h"
#include "espcp_queue.h"
#include "espcp_shared_enums.h"
#include "espcp_message_dispatcher.h"
#include "espcp_system.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *_thisFile = __FILE__;

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
 *  Locking / access to the configuration is controlled by the called.
 *
 ****************************************************************************/
bool espcp_is_thead_running(espcp_configuration_t *configuration)
{
  return(configuration->thread_running);
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
  espcp_configuration_t *configuration = espcp_get_configuration();
#else
  espcp_configuration_t *configuration = parameters;
#endif

    if (espcp_spi_setup(espcp_queue_send_response_message) == OK)
    {
        espcp_get_device_configuration();
    }
    else
    {
        syslog(LOG_CRIT, "%s@%d ESP interface initialisation failed\n", _thisFile, __LINE__);
        return(FAR void *) (intptr_t) NULL;
    }

    configuration->thread_running = true;
    while (configuration->thread_running)
    {
      espcp_message_t *retrieved_message;
      int number_of_bytes = mq_receive(configuration->request_queue, (void *) &retrieved_message, sizeof(retrieved_message), NULL);
      if (number_of_bytes == sizeof(espcp_message_t *))
      {
        if (retrieved_message != NULL)
        {
          if ((retrieved_message->message_type == espcp_message_types_transport) && (retrieved_message->function == espcp_transport_function_kill_nuttx_thread))
          {
            configuration->thread_running = false;
            configuration->exit_code = OK;
            free(retrieved_message);
            pthread_exit(configuration);
          }
          else
          {
            espcp_send_message(configuration, retrieved_message);
          }
        }
        else
        {
          /*
          *  If the message is NULL then there is nothing to process
          *  so simply release the resources associated with the message.
          */
          espcp_delete_message_and_payload(retrieved_message);
        }
      }
      else
      {
        /* TODO: Deal with this error. */
      }
    }

    return(FAR void *) (intptr_t) NULL;
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
  if (espcp_is_thead_running(configuration))
  {
    return(EALREADY);
  }


#ifdef CONFIG_BUILD_PROTECTED
  configuration->thread = kthread_create(ESPCP_THREAD_NAME, CONFIG_MEADOW_ESPCP_PRIORITY,
    CONFIG_MEADOW_ESPCP_STACKSIZE, (main_t) espcp_thread, (char * const *)  NULL);

  if(configuration->thread <= 0)
  {
    return -ENOEXEC;
  }
#else
  int result = OK;
  pthread_attr_t thread_attributes;

  result = pthread_attr_init(&thread_attributes);
  if (result != OK)
  {
    return(-result);
  }

  struct sched_param scheduler_parameters;
  scheduler_parameters.sched_priority = CONFIG_MEADOW_ESPCP_PRIORITY;
  result = pthread_attr_setschedparam(&thread_attributes, &scheduler_parameters);
  if (result != OK)
  {
    return(-result);
  }
  
  result = pthread_attr_setstacksize(&thread_attributes, CONFIG_MEADOW_ESPCP_STACKSIZE);
  if (result != OK)
  {
    return(-result);
  }
  
  result = pthread_create(&configuration->thread, &thread_attributes, espcp_thread, configuration);
  if (result != OK)
  {
    return(-result);
  }
#endif

  configuration->request_queue = mq_open(ESPCP_MESSAGE_QUEUE_NAME, O_RDWR);
  if ((int) configuration->request_queue < 0)
  {
    configuration->exit_code = -1;
    return(-1);
  }

  return OK;
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
  result= pthread_join(configuration->thread, NULL);
  configuration->thread = 0;

  mq_close(configuration->request_queue);
  configuration->request_queue = (mqd_t) -1;

  return(result);
}
