/****************************************************************************
 * examples/espcptest/espcptest_message_dispatcher.c
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

/*
 *  The g_expected_buffer variable holds a pointer to a byte array of data that
 *  is expected as the result of a test.
 * 
 *  The test methods can use this to send expected results through to helper
 *  methods.
 */
uint8_t *g_expected_rx_buffer = NULL;
uint8_t *g_expected_tx_buffer = NULL;

/*
 *  This variable is used to determine the action of the various test helper
 *  methods.  It allows a helper to map the various stages of a method and
 *  return the appropriate data for that test stage.
 */
uint32_t g_mdh_test_stage = 0;

/*
 *  Pointer to the coprocessor configuration.
 */
static espcp_configuration_t *g_configuration = NULL;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: TEST_SETUP(EspcpMessageDispatcher)
 *
 * Description:
 *  Setup the named message queue and assign the queue ID to a global
 *  variable.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
TEST_SETUP(EspcpMessageDispatcher)
{
  g_configuration = espcp_get_default_configuration();
  /*
   *    The message dispatcher needs an existing message queue.
   */
  g_queue_id = espcp_create_message_queue(ESPCP_MESSAGE_QUEUE_NAME);
  TEST_ASSERT_EQUAL(true, ((int) g_queue_id) != -1);

  TEST_ASSERT_EQUAL(0, espcp_setup_message_dispatcher());
  g_expected_tx_buffer = NULL;
  g_expected_rx_buffer = NULL;
}

/****************************************************************************
 * Name: TEST_TEAR_DOWN(EspcpMessageDispatcher)
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
TEST_TEAR_DOWN(EspcpMessageDispatcher)
{
  TEST_ASSERT_EQUAL(0, espcp_teardown_message_dispatcher());
  TEST_ASSERT_EQUAL(0, espcp_delete_message_queue(g_queue_id));
  g_queue_id = 0;

  free(g_configuration->header);
  free(g_configuration);
  g_configuration = NULL;
}

/****************************************************************************
 * 
 * Verify that espcp_get_next_message_id can get an incrementing message ID.
 * queue.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, GetNextMessageID)
{
  TEST_ASSERT_EQUAL(0x80000001, espcp_get_next_message_id());
  TEST_ASSERT_EQUAL(0x80000002, espcp_get_next_message_id());
}

/****************************************************************************
 * 
 * Verify that espcp_queue_send_response_message will queue the
 * "Send Response" message.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, QueueSendResponseMessage)
{
  struct mq_attr mqStatus; 
  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(0, mqStatus.mq_curmsgs);

  espcp_queue_send_response_message(0, NULL, NULL);

  TEST_ASSERT_EQUAL(OK, mq_getattr(g_queue_id, &mqStatus));
  TEST_ASSERT_EQUAL(1, mqStatus.mq_curmsgs);

  espcp_message_t *retrieved_message;
  int number_of_bytes = mq_receive(g_queue_id, (void *) &retrieved_message, sizeof(retrieved_message), NULL);
  TEST_ASSERT_EQUAL(sizeof(espcp_message_t *), number_of_bytes);
  TEST_ASSERT_EQUAL(0x20, retrieved_message->message_type);
  TEST_ASSERT_EQUAL(5, retrieved_message->interface);
  TEST_ASSERT_EQUAL(1, retrieved_message->function);
}

/****************************************************************************
 * Name: mdh_send_header
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 * length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_send_header(void *tx, void *rx, size_t length)
{
  /*
   *  Stage 0 - The system will send a message header.
   */
  uint8_t expected_message_header[] = 
  { 
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xda, 0x8b, 0x31, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 1 - Emulate the ESP32 sending a valid message header acknowledgement.
   */
  uint8_t valid_encoded_header[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 2 - Emulate the ESP32 sending an invalid message header acknowledgement.
   */
  uint8_t encoded_invalid_header[] = 
  {
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xda, 0x8b, 0x31, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 3 - Emulate sending a valid acknowledgement.
   */
  uint8_t encoded_valid_acknowledgement[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  switch (g_mdh_test_stage)
  {
    case 0:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(expected_message_header), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_message_header, tx, length);
      break;
    case 1:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(valid_encoded_header), length);
      memcpy(rx, valid_encoded_header, length);
      break;
    case 2:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(encoded_invalid_header), length);
      memcpy(rx, encoded_invalid_header, length);
      break;
    case 3:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(encoded_valid_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(encoded_valid_acknowledgement, tx, length);
      break;
    default:
      TEST_ASSERT_EQUAL(false, true);   /* Force an error as we should never get here. */
      break;
  }
  g_mdh_test_stage++;


  // if (tx != NULL)
  // {
  //   TEST_ASSERT_EQUAL_UINT8_ARRAY(g_expected_tx_buffer, tx, length);
  // }
  // if (rx != NULL)
  // {
  //   memcpy(rx, g_expected_rx_buffer, length);
  // }
}

/****************************************************************************
 * 
 * Verify that a header message can be encoded and presented to a method.
 * 
 * In real life the method will send the encoded message to the ESP32,
 * here we will insert our own method that will compare the encoded message
 * with what we expect to be produced.
 * 
 * Pre-requisite:
 *  g_queue_id holds the descriptor to a valid message queue.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, SendHeader)
{
  espcp_message_t *message = malloc(sizeof(espcp_message_t));
  memset(message, 0, sizeof(espcp_message_t));
  message->message_type = 0x80;
  message->interface = 1;
  message->function = 2;
  message->message_id = 3;

  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_send_header;
  /*
   *  Emulate sending a header to the ESP32.
   */
  TEST_ASSERT_EQUAL(0, espcp_send_packet(g_configuration, message));

  /*
   *  Now get a valid message header acknowledgement.
   */
  espcp_message_t *received = NULL;
  TEST_ASSERT_EQUAL(0, espcp_get_message_header_acknowledgement(g_configuration, message, &received));
  TEST_ASSERT_EQUAL(true, received != NULL);

  /*
   *  Now get an invalid message header acknowledgement.
   */
  received = (espcp_message_t *) 0x80000000;
  TEST_ASSERT_EQUAL(true, espcp_get_message_header_acknowledgement(g_configuration, message, &received) != 0);
  TEST_ASSERT_EQUAL(NULL, received);

  /*
   *  Now send a valid acknowledgement to the ESP32.
   */
  espcp_send_acknowledgement(g_configuration, message, 0);

  g_configuration->send_data_to_esp32 = NULL;
  free(message);
}

/****************************************************************************
 * Name: mdh_send_valid_message_body_helper
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 * 
 *  This method supports TEST(EspcpMessageDispatcher, SendValidMessageBody).
  *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 *  length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_send_valid_message_body_helper(void *tx, void *rx, size_t length)
{
  /*
   *  Stage 0 - The method will send a message body.
   */
  uint8_t expected_message_body[] = 
  {
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0xc9, 0x74, 0xbd, 0x58, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 1 - the method will be expecting an ACK or NAK packet.  In this case we will
   *  send an ACK.
   */
  uint8_t acknowledgement[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  switch (g_mdh_test_stage)
  {
    case 0:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(expected_message_body), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_message_body, tx, length);
      break;
    case 1:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(acknowledgement), length);
      memcpy(rx, acknowledgement, length);
      break;
    default:
      TEST_ASSERT_EQUAL(false, true);   /* Force an error as we should never get here. */
      break;
  }
  g_mdh_test_stage++;
}

/****************************************************************************
 * 
 * Test sending a valid message body to the ESP32.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, SendValidMessageBody)
{
  espcp_message_t *sent = malloc(sizeof(espcp_message_t));
  memset(sent, 0, sizeof(espcp_message_t));
  sent->message_type = 0x80;
  sent->interface = 1;
  sent->function = 2;
  sent->message_id = 3;
  sent->payload_length = 5;
  sent->payload = (uint8_t *) malloc(sent->payload_length);
  for (int index = 0; index < sent->payload_length; index++)
  {
    sent->payload[index] = index;
  }
  
  /*
   *  Final step be fore running the test is to prepare the helper method.
   */
  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_send_valid_message_body_helper;
  TEST_ASSERT_EQUAL(0, espcp_send_message_body(g_configuration, sent));

  /*
   *  Now tidy up.
   */
  g_configuration->send_data_to_esp32 = NULL;
  free(sent);
}

/****************************************************************************
 * Name: mdh_get_valid_message_header_helper
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 * 
 *  This method supports TEST(EspcpMessageDispatcher, GetValidMessageHeader).
  *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 *  length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_get_valid_message_header_helper(void *tx, void *rx, size_t length)
{
  /*
   *  Stage 0 - The method will get a message header.
   */
  uint8_t message_header[] = 
  { 
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xda, 0x8b, 0x31, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  /*
   *  Stage 1 - the method will be expecting an ACK or NAK packet.  In this case we will
   *  send an ACK.
   */
  uint8_t acknowledgement[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  switch (g_mdh_test_stage)
  {
    case 0:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(message_header), length);
      memcpy(rx, message_header, length);
      break;
    case 1:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(acknowledgement, tx, length);
      break;
    default:
      TEST_ASSERT_EQUAL(false, true);   /* Force an error as we should never get here. */
      break;
  }
  g_mdh_test_stage++;
}

/****************************************************************************
 * 
 * Test sending a valid message body to the ESP32.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, GetValidMessageHeader)
{
  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_get_valid_message_header_helper;
  espcp_message_t *header = espcp_get_message_header(g_configuration);
  TEST_ASSERT_EQUAL(true, header != NULL);
  TEST_ASSERT_EQUAL(0x80, header->message_type);
  TEST_ASSERT_EQUAL(1, header->interface);
  TEST_ASSERT_EQUAL(2, header->function);
  TEST_ASSERT_EQUAL(3, header->message_id);
  TEST_ASSERT_EQUAL(0, header->status_code);
  TEST_ASSERT_EQUAL(0, header->payload_length);
  TEST_ASSERT_EQUAL(NULL, header->payload);
  /*
   *  Now tidy up.
   */
  g_configuration->send_data_to_esp32 = NULL;
}

/****************************************************************************
 * Name: mdh_get_invalid_message_header_helper
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 * 
 *  This method supports TEST(EspcpMessageDispatcher, SendInvalidMessageHeader).
 * 
 *  This methid deliberately corrupts the header that is sent and so should
 *  get a NAK message back from the method.
 *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 *  length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_get_invalid_message_header_helper(void *tx, void *rx, size_t length)
{
  uint8_t message_header[] = 
  { 
    0x80, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xda, 0x8b, 0x31, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  TEST_ASSERT_EQUAL(NULL, tx);
  TEST_ASSERT_EQUAL(true, rx != NULL);
  TEST_ASSERT_EQUAL(sizeof(message_header), length);
  memcpy(rx, message_header, length);
}

/****************************************************************************
 * 
 * Test getting an invalid message body from the ESP32
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, GetInvalidMessageHeader)
{
  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_get_invalid_message_header_helper;
  espcp_message_t *header = espcp_get_message_header(g_configuration);
  TEST_ASSERT_EQUAL(NULL, header);
  /*
   *  Now tidy up.
   */
  g_configuration->send_data_to_esp32 = NULL;
}

/****************************************************************************
 * Name: mdh_get_valid_message_body_helper
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 * 
 *  This method supports TEST(EspcpMessageDispatcher, GetValidMessageBody).
  *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 *  length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_get_valid_message_body_helper(void *tx, void *rx, size_t length)
{
  /*
   *  Stage 0 - The method will send a message body.
   */
  uint8_t message_body[] = 
  {
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0xc9, 0x74, 0xbd, 0x58, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 1 - the method will be expecting an ACK or NAK packet.  In this case we will
   *  send an ACK.
   */
  uint8_t expected_acknowledgement[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 2 - This is a message & body with a CRC error.
   */
  uint8_t message_body_with_crc_error[] = 
  {
    0x80, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0xc9, 0x74, 0xbd, 0x58, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  /*
   *  Stage 1 - the method will be expecting an ACK or NAK packet.  In this case we will
   *  send an ACK.
   */
  uint8_t expected_crc_error_acknowledgement[] = 
  {
    0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x87, 0x9c, 0x52, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  switch (g_mdh_test_stage)
  {
    case 0:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(message_body), length);
      memcpy(rx, message_body, length);
      break;
    case 1:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(expected_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_acknowledgement, tx, length);
      break;
    case 2:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(message_body_with_crc_error), length);
      memcpy(rx, message_body_with_crc_error, length);
      break;
    case 3:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(expected_crc_error_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_crc_error_acknowledgement, tx, length);
      break;
    default:
      TEST_ASSERT_EQUAL(false, true);   /* Force an error as we should never get here. */
      break;
  }
  g_mdh_test_stage++;
}

/****************************************************************************
 * 
 * Test getting a valid message body to the ESP32.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, GetMessageBody)
{
  espcp_message_t *header = malloc(sizeof(espcp_message_t));
  memset(header, 0, sizeof(espcp_message_t));
  header->message_type = 0x80;
  header->interface = 1;
  header->function = 2;
  header->message_id = 3;
  header->payload_length = 5;

  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_get_valid_message_body_helper;
  espcp_message_t *message = espcp_get_message_body(g_configuration, header);
  TEST_ASSERT_EQUAL(false, message == NULL);
  TEST_ASSERT_EQUAL(0x80, message->message_type);
  TEST_ASSERT_EQUAL(1, message->interface);
  TEST_ASSERT_EQUAL(2, message->function);
  TEST_ASSERT_EQUAL(3, message->message_id);
  TEST_ASSERT_EQUAL(0, message->status_code);
  TEST_ASSERT_EQUAL(5, message->payload_length);
  TEST_ASSERT_EQUAL(false, message->payload == NULL);
  for (int index = 0; index < message->payload_length; index++)
  {
    TEST_ASSERT_EQUAL(index, message->payload[index]);
  }
  free(message);
  free(message->payload);
  /*
   *  Next case should result ina CRC error.
   */
  message = espcp_get_message_body(g_configuration, header);
  TEST_ASSERT_EQUAL(NULL, message);
  /*
   *  Now tidy up.
   */
  g_configuration->send_data_to_esp32 = NULL;
  free(header);
}

/****************************************************************************
 * Name: mdh_get_response_from_esp32_helper
 *
 * Description:
 *  Helper method to emulate the hardware interface interaction with the
 *  ESP32.
 * 
 *  This method supports TEST(EspcpMessageDispatcher, GetResponseFromEsp32).
  *
 * Input Parameters:
 *  tx - Pointer to a buffer of data to be "sent" to the ESP32.
 *  rx - Buffer that will be populated with data "received" from the ESP32.
 *  length - length of the buffer(s).
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void mdh_get_response_from_esp32_helper(void *tx, void *rx, size_t length)
{
  /*
   *  Stage 0 - First step, the ESP will send a message header.
   */
  uint8_t ho_header[] = 
  { 
    0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xda, 0x8b, 0x31, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  /*
   *  Stage 1 - Next, the STM32 will send an acknowledgement, in this case we send an ACK.
   *  send an ACK.
   */
  uint8_t ho_acknowledgement[] = 
  {
    0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x05, 0xcf, 0xee, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  /*
   *  Stage 2 - Message with body, header.
   */
  uint8_t mb_header[] =
  {
    0x40, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0xb1, 0xbf, 0x24, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  /*
   *  Stage 3 - Message with body, header acknowledgement.
   */
  uint8_t mb_header_acknowledgement[] =
  {
    0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x9c, 0x96, 0xf8, 0xa4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00    
  };

  /*
   *  Stage 4 - Message with body, message.
   */
  uint8_t mb_message[] =
  {
    0x80, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0x00, 0xcb, 0xc0, 0xf2, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  /*
   *  Stage 5 - Message with body, message acknowledgement.
   */
  uint8_t mb_message_acknowledgement[] =
  {
    0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x9c, 0x96, 0xf8, 0xa4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  switch (g_mdh_test_stage)
  {
    case 0:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(ho_header), length);
      memcpy(rx, ho_header, length);
      break;
    case 1:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(ho_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(ho_acknowledgement, tx, length);
      break;
    case 2:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(mb_header), length);
      memcpy(rx, mb_header, length);
      break;
    case 3:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(mb_header_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(mb_header_acknowledgement, tx, length);
      break;
    case 4:
      TEST_ASSERT_EQUAL(NULL, tx);
      TEST_ASSERT_EQUAL(true, rx != NULL);
      TEST_ASSERT_EQUAL(sizeof(mb_message), length);
      memcpy(rx, mb_message, length);
      break;
    case 5:
      TEST_ASSERT_EQUAL(NULL, rx);
      TEST_ASSERT_EQUAL(true, tx != NULL);
      TEST_ASSERT_EQUAL(sizeof(mb_message_acknowledgement), length);
      TEST_ASSERT_EQUAL_UINT8_ARRAY(mb_message_acknowledgement, tx, length);
      break;
    default:
      TEST_ASSERT_EQUAL(false, true);   /* Force an error as we should never get here. */
      break;
  }
  g_mdh_test_stage++;
}

/****************************************************************************
 * 
 * Test getting a response message from the ESP32.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, GetResponseFromEsp32)
{
  g_mdh_test_stage = 0;
  g_configuration->send_data_to_esp32 = mdh_get_response_from_esp32_helper;
  /*
   *    First get a message from the ESP32 that is a header only message (no payload).
   */
  TEST_ASSERT_EQUAL(0, espcp_get_response_from_esp32(g_configuration));
  /*
   *    Next up we are going to get a message with a body.
   */
  TEST_ASSERT_EQUAL(0, espcp_get_response_from_esp32(g_configuration));
}

/****************************************************************************
 * 
 * Test getting processing transport messages.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, ProcessTransportMessage)
{
  /*
   *  Passing NULL for the message should generate an error.
   */
  TEST_ASSERT_EQUAL(true, espcp_process_transport_message(g_configuration, NULL) != 0);

  espcp_message_t *message = malloc(sizeof(espcp_message_t));
  memset(message, 0, sizeof(espcp_message_t));
  message->message_type = 0x80;
  message->interface = 5;
  message->function = 1;
  message->message_id = 3;
  message->status_code = 17;

  TEST_ASSERT_EQUAL(0, espcp_process_transport_message(g_configuration, message));

  message->status_code = 26;
  TEST_ASSERT_EQUAL(26, espcp_process_transport_message(g_configuration, message));
  free(message);
}

/****************************************************************************
 * 
 * Test the process of sending a message.
 * 
 * Pre-requisite:
 *  None.
 * 
 ****************************************************************************/
TEST(EspcpMessageDispatcher, SendMessage)
{
  espcp_message_t *message = malloc(sizeof(espcp_message_t));
  memset(message, 0, sizeof(espcp_message_t));
  message->message_type = 0x80;
  message->interface = 5;
  message->function = 1;
  message->message_id = 3;
  message->semaphore = NULL;

  /*
   *  Passing NULL for the message should generate an error.
   */
  g_configuration->send_data_to_esp32 = NULL;
  TEST_ASSERT_EQUAL(3, espcp_send_message(g_configuration, message));
}

/****************************************************************************
 *
 * Run the test cases.
 * 
 ****************************************************************************/
TEST_GROUP(EspcpMessageDispatcher)
{
    RUN_TEST_CASE(EspcpMessageDispatcher, GetNextMessageID);
    RUN_TEST_CASE(EspcpMessageDispatcher, QueueSendResponseMessage);
    RUN_TEST_CASE(EspcpMessageDispatcher, SendHeader);
    RUN_TEST_CASE(EspcpMessageDispatcher, SendValidMessageBody);
    RUN_TEST_CASE(EspcpMessageDispatcher, GetValidMessageHeader);
    RUN_TEST_CASE(EspcpMessageDispatcher, GetInvalidMessageHeader);
    RUN_TEST_CASE(EspcpMessageDispatcher, GetMessageBody);
    RUN_TEST_CASE(EspcpMessageDispatcher, GetResponseFromEsp32);
    RUN_TEST_CASE(EspcpMessageDispatcher, ProcessTransportMessage);
    RUN_TEST_CASE(EspcpMessageDispatcher, SendMessage);
}
