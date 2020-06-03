/****************************************************************************
 * examples/espcptest/espcptest_thread.c
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
 *  Pointer to the coprocessor configuration.
 */
static espcp_configuration_t *g_configuration = NULL;

/*
 *  ID of the message queue that will be used to test this module.
 */
static mqd_t g_message_queue_id = 0;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ESP32 SPI Communications thread test group setup
 *
 * Description:
 *  Setup function executed before each testcase in this test group
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
TEST_SETUP(EspcpThread)
{
  g_configuration = espcp_get_default_configuration();
  g_message_queue_id = espcp_create_message_queue(ESPCP_MESSAGE_QUEUE_NAME);
  TEST_ASSERT_EQUAL(true, g_message_queue_id > 0);
  TEST_ASSERT_EQUAL(0, espcp_thread_start(g_configuration));
}

/****************************************************************************
 * Name: ESP32 SPI Communications thread test group teardown
 *
 * Description:
 *  Delete the message queues and stop any threads after a test has
 *  completed.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
TEST_TEAR_DOWN(EspcpThread)
{
  TEST_ASSERT_EQUAL(0, espcp_thread_stop(g_configuration));
  TEST_ASSERT_EQUAL(0, g_configuration->exit_code);

  TEST_ASSERT_EQUAL(0, espcp_delete_message_queue(g_message_queue_id));
  g_message_queue_id = 0;

  free(g_configuration->header);
  free(g_configuration);
  g_configuration = NULL;

}

/****************************************************************************
 * 
 * Check to see if a thread is runing after thread startup.
 * 
 * Pre-requisite:
 *  Thread has been setup as part of the TEST_SETUP procedure.
 * 
 ****************************************************************************/
TEST(EspcpThread, CheckThreadIsRunning)
{
  TEST_ASSERT_EQUAL(true, espcp_is_thead_running(g_configuration));
}

/****************************************************************************
 * 
 * Make sure that we can start a single instance of the ESP32 SPI thread
 * 
 * Pre-requisite:
 *  Thread has been setup as part of the TEST_SETUP procedure.
 * 
 ****************************************************************************/
TEST(EspcpThread, ThreadSetup)
{
  TEST_ASSERT_EQUAL(true, espcp_is_thead_running(g_configuration));
}

/****************************************************************************
 * 
 * Two instances of the thread should result in a success for the first instance
 * but a failure for the second instance.
 * 
 * Pre-requisite:
 *  Thread has been setup as part of the TEST_SETUP procedure.
 * 
 ****************************************************************************/
TEST(EspcpThread, CreateMoreThanOneThread)
{
  TEST_ASSERT_EQUAL(EALREADY, espcp_thread_start(g_configuration));
}

/****************************************************************************
 * 
 * Added 5 messages to the queue and the thread should pick up messages
 * and attempt to send the messages to the ESP32.  However, setting
 * g_configuration.send_data_to_esp32 to NULL will effectively throw the
 * messages away.
 * 
 * Pre-requisite:
 *  Thread has been setup as part of the TEST_SETUP procedure.
 *  Message queue test should be run first to ensure the queueing works.
 * 
 ****************************************************************************/
TEST(EspcpThread, SendMessagesToThread)
{
  g_configuration->send_data_to_esp32 = NULL;
  for (int index = 0; index < 5; index++)
  {
    espcp_message_t *message = malloc(sizeof(espcp_message_t));
    memset(message, 0, sizeof(espcp_message_t));
    message->message_type = 0x80;
    message->interface = 1;
    message->function = 2;
    message->message_id = index + 1;
    message->semaphore = NULL;

    espcp_add_message_to_queue(g_configuration->request_queue, message);
  }
}

/****************************************************************************
 *
 * Run the test cases.
 * 
 ****************************************************************************/
TEST_GROUP(EspcpThread)
{
  RUN_TEST_CASE(EspcpThread, ThreadSetup);
  RUN_TEST_CASE(EspcpThread, CheckThreadIsRunning);
  RUN_TEST_CASE(EspcpThread, CreateMoreThanOneThread);
  RUN_TEST_CASE(EspcpThread, SendMessagesToThread);
}
