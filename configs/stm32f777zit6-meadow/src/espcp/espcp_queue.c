/****************************************************************************
 * espcp_queue.c
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

#include <arch/irq.h>

#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>
#include <nuttx/pthread.h>
#include <nuttx/mqueue.h>
#include <nuttx/config.h>

#include "../hcom/hcom_common.h"
#include "espcp_coprocessor.h"
#include "espcp_queue.h"
#include "espcp_message.h"
#include "espcp_shared_enums.h"

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
 * Name: espcp_create_message_queue
 *
 * Description:
 *  Create a nameed message queue to hold the messages that should be
 *  sent to the ESP32 for processing.
 *
 * Input Parameters:
 *  name - name of the message queue to be created.
 *
 * Returned Value:
 *  ID of the message queue if the queue was created successfully
 *  -1 or an error code if the queue creation failed.
 *
 * Assumptions/Limitations:
 *  Message queue does not already exist.
 *
 ****************************************************************************/
mqd_t espcp_create_message_queue(char *name)
{
  mqd_t queue_id = 0;
  
  struct mq_attr queue_attributes;
  memset(&queue_attributes, 0, sizeof(queue_attributes));
  mode_t mode;
  memset(&mode, 0, sizeof(mode));
  queue_attributes.mq_maxmsg = ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH;
  queue_attributes.mq_msgsize = sizeof(struct message_and_semaphore_s *);
  queue_attributes.mq_flags = 0;
  queue_id = mq_open(ESPCP_MESSAGE_QUEUE_NAME, O_RDWR | O_CREAT, mode, &queue_attributes);
  
  return(queue_id);
}

/****************************************************************************
 * Name: espcp_delete_message_queue
 *
 * Description:
 *  Close the message queue and mark it for deletion (unlink the queue).
 * 
 *  Note that the mq_unlink method will only delete the named queue if it is
 *  not opened by another thread / process.  If it is still opened elsewhere
 *  then the it is marked for deletion later (when the final close for this
 *  queue is executed).
 *
 * Input Parameters:
 *   queue_id - ID of the queue to be closed and unlinked.
 *
 * Returned Value:
 *  OK if the queue deletion succeeded.
 *  -1 if an error occurred.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int espcp_delete_message_queue(mqd_t queue_id)
{
  if (mq_close(queue_id) < 0)
  {
    return(-1);
  }
  return(mq_unlink(ESPCP_MESSAGE_QUEUE_NAME));
}

/****************************************************************************
 * Name: espcp_add_message_to_queue
 *
 * Description:
 *  Add a new outbound message to the queue of messages waiting to be sent
 *  to the ESP32.
 *
 * Input Parameters:
 *  queue_id - Message queue ID.
 *  message - Message to be processed.
 *
 * Returned Value:
 *  OK if the message could be added to the queue.
 *  Error code if an error occurred.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_add_message_to_queue(mqd_t queue_id, espcp_message_t *message)
{
  int result = OK;
  
  result = mq_send(queue_id, (const void *) &message, sizeof(message), 
                   ESPCP_DEFAULT_MESSAGE_PRIORITY);
  
  return (result);
}

/****************************************************************************
 * Name: espcp_get_message_from_queue
 *
 * Description:
 *  Get a message from the specified message queue.
 *
 * Input Parameters:
 *  queue_id - Message queue ID.
 *
 * Returned Value:
 *  Pointer to the message that has been retrieved, NULL if there was a 
 *  problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void *espcp_get_message_from_queue(mqd_t queue_id)
{
  void *message = NULL;
  
  int number_of_bytes = mq_receive(queue_id, (void *) &message, sizeof(message), NULL);
  if (number_of_bytes != sizeof(message))
  {
    message = NULL;
  }
    
  return message;
}

/****************************************************************************
 * Name: espcp_queue_kill_nuttx_thread_message
 *
 * Description:
 *  Add the "Kill NUTTX Process Thread" message to the message queue.
 * 
 *  This message will eventually cause the NUTTX thread that is processing
 *  the message queue to exit.
 * 
 *  Calling this method will also close the connection to the message
 *  queue.
 * 
 * Input Parameters:
 *  queue_id - Message queue to add the kill message to.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_queue_kill_nuttx_thread_message(mqd_t queue_id)
{
  espcp_message_t *kill_thread_message = (espcp_message_t *) malloc(sizeof(espcp_message_t));
  memset(kill_thread_message, 0, sizeof(espcp_message_t));
  kill_thread_message->message_type = espcp_message_types_transport;
  kill_thread_message->interface = espcp_esp32_interfaces_transport;
  kill_thread_message->function = espcp_transport_function_kill_nuttx_thread;
  kill_thread_message->semaphore = NULL;

  espcp_add_message_to_queue(queue_id, kill_thread_message);
}

