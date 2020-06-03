/****************************************************************************
 * examples/espcptest/espcptest_queue.c
 *
 *   Copyright (C) 2020 Wilderness Labs
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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <debug.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defines.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

#ifndef dbg
  #define dbg _warn
#endif

#define usrsocktest_dbg(...) ((void)0)

#define TEST_SOCKET_SOCKID_BASE 10000U
#define TEST_SOCKET_COUNT 8

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define noinline

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/*
 *  ID of the queue being tested.
 */
static mqd_t g_queue_id = 0;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: TEST_SETUP(EspcpQueue)
 *
 * Description:
 *  Setup the named message queue and assign the queue ID to a global
 *  variable.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
TEST_SETUP(EspcpQueue)
{
  g_queue_id = espcp_create_message_queue(ESPCP_MESSAGE_QUEUE_NAME);
  TEST_ASSERT_EQUAL(true, ((int) g_queue_id) != -1);
}

/****************************************************************************
 * Name: TEST_TEAR_DOWN(EspcpQueue)
 *
 * Description:
 *  Close the message queue and then unlink it.
 * 
 *  At the end of this the queue should no longer exist.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
TEST_TEAR_DOWN(EspcpQueue)
{
  TEST_ASSERT_EQUAL(0, espcp_delete_message_queue(g_queue_id));
  g_queue_id = 0;
}

/****************************************************************************
 * 
 * Verify that espcp_add_message_to_queue can add a message to the message
 * queue.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 * 
 ****************************************************************************/
TEST(EspcpQueue, AddMessage)
{
  /*
   *  There should be no messages in the queue when the queue has just been created.
   */
  struct mq_attr mqStatus;
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);

  /*
   *  Add a message to the queue.
   */
  espcp_message_t message;
  message.message_id = 1;
  sem_t semaphore;
  message.semaphore = &semaphore;
  TEST_ASSERT_EQUAL(OK, espcp_add_message_to_queue(g_queue_id, &message));

  /*
   *  There should now be 1 message in the queue.
   */
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(1, mqStatus.mq_curmsgs);

  /*
   *  Now get the "message" from the queue and see if we get a pointer to
   *  the original message.
   */
  espcp_message_t *retrieved_message;
  int number_of_bytes = mq_receive(g_queue_id, (void *) &retrieved_message, sizeof(retrieved_message), NULL);
  TEST_ASSERT_EQUAL(sizeof(espcp_message_t *), number_of_bytes);
  TEST_ASSERT_EQUAL(&message, retrieved_message);
  TEST_ASSERT_EQUAL(1, retrieved_message->message_id);

  /*
   *  There should now be no messages in the queue.
   */
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);
}

/****************************************************************************
 * 
 * Verify that espcp_get_message_from_queue can retrieve a message from
 * the message queue.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 *  espcp_add_message_to_queue has been tested and verified as working.
 * 
 ****************************************************************************/
TEST(EspcpQueue, AddAndRetrieveMessages)
{
  /*
   *  There should be no messages in the queue when the queue has just been created.
   */
  struct mq_attr mqStatus;
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);

  /*
   *  Add a message to the queue.
   */
  espcp_message_t message;
  message.message_id = 23;
  sem_t semaphore;
  message.semaphore = &semaphore;
  TEST_ASSERT_EQUAL(OK, espcp_add_message_to_queue(g_queue_id, &message));

  /*
   *  There should now be 1 message in the queue.
   */
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(1, mqStatus.mq_curmsgs);

  /*
   *  Now get the "message" from the queue and see if we get a pointer to
   *  the original message.
   */
  espcp_message_t *retrieved_message = (espcp_message_t *) espcp_get_message_from_queue(g_queue_id);
  TEST_ASSERT_EQUAL(&message, retrieved_message);
  TEST_ASSERT_EQUAL(23, retrieved_message->message_id);

  /*
   *  There should now be no messages in the queue.
   */
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);
}

/****************************************************************************
 * 
 * Verify that espcp_queue_kill_nuttx_thread_message will queue the
 * "Kill Nuttx Thread" message.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 * 
 ****************************************************************************/
TEST(EspcpQueue, QueueKillNuttxThreadMessage)
{
  struct mq_attr mqStatus; 
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);

  espcp_queue_kill_nuttx_thread_message(g_queue_id);

  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(1, mqStatus.mq_curmsgs);

  espcp_message_t *retrieved_message;
  int number_of_bytes = mq_receive(g_queue_id, (void *) &retrieved_message, sizeof(retrieved_message), NULL);
  TEST_ASSERT_EQUAL(sizeof(espcp_message_t *), number_of_bytes);
  TEST_ASSERT_EQUAL(0x20, retrieved_message->message_type);
  TEST_ASSERT_EQUAL(5, retrieved_message->interface);
  TEST_ASSERT_EQUAL(2, retrieved_message->function);
}

/****************************************************************************
 *
 * Run the test cases.
 * 
 ****************************************************************************/
TEST_GROUP(EspcpQueue)
{
  RUN_TEST_CASE(EspcpQueue, AddMessage);
  RUN_TEST_CASE(EspcpQueue, AddAndRetrieveMessages);
  RUN_TEST_CASE(EspcpQueue, QueueKillNuttxThreadMessage);
}
