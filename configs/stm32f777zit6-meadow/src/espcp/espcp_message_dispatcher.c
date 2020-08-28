/****************************************************************************
 * espcp_message_dispatcher.c
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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>
#include <nuttx/pthread.h>
#include <nuttx/config.h>

#include "../hcom/hcom_common.h"
#include "espcp_message_dispatcher.h"
#include "espcp_message.h"
#include "espcp_shared_enums.h"
#include "espcp_queue.h"
#include "espcp_encoders.h"
#include "espcp_interrupt_handler.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

#define DEBUG_MESSAGE_DISPATCHER 1
#undef DEBUG_MESSAGE_DISPATCHER

#if defined(DEBUG_MESSAGE_DISPATCHER)
  #define ENTER_MESSAGE(s) syslog(LOG_INFO, "%s Enter.\n", (s));
  #define EXIT_MESSAGE(s) syslog(LOG_INFO, "%s Exit.\n", (s));
#else
  #define ENTER_MESSAGE(s)
  #define EXIT_MESSAGE(s)
#endif

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/**
 *  Mutex used to ensure exclusive access to the message ID.
 */
static pthread_mutex_t g_message_id_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 *  ID of the last message sent to the ESP32.
 */
static uint32_t g_last_message_id = 0;

/**
 *  Linked list containing the messages waiting for a response from the ESP32.
 */
static gl_linked_list_t *g_messages_waiting_for_a_response = NULL;

/**
 *  Mutex for the messages waiting for a response list.
 */
static sem_t g_messages_waiting_for_a_response_mutex;

/**
 *  This message is used to request a response from the ESP32.  It is created
 *  as part of the setup process for the message dispatcher.  It should never
 *  be deleted as it is reused each time a response is requested.
 */
static espcp_message_t *g_request_response_message = NULL;

/**
 *  ID of the message queue that will receive any messages for processing.
 */
static mqd_t g_message_queue = 0;

/**
 *  WiFi interrupt handlers.
 */
static espcp_interrupt_handlers_t *g_wifi_handlers = NULL;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_check_message_id
 *
 * Description:
 *  Method used by the generic linked list code to perform a comparison of
 *  an item in the list to see if it matches the required message ID.
 *
 * Input Parameters:
 *  message_id - message ID to look for in the linked list.
 *  list_item - current list item being examined.
 *
 * Returned Value:
 *  true if the message IDs match, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static bool espcp_check_message_id(uint32_t message_id, void *list_item)
{
  ENTER_MESSAGE(__func__);

  espcp_message_t *message = (espcp_message_t *) list_item;

  EXIT_MESSAGE(__func__);

  return(message->message_id == message_id);
}

/****************************************************************************
 * Name: espcp_register_interrupt_handlers
 *
 * Description:
 *  Register a group oif interrupt handlers with the message dispatcher.
 *
 * Input Parameters:
 *  interface - Interface the handlers are destined for.
 *  handlers - List of function / interrupt handlers pairs.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_register_interrupt_handlers(uint32_t interface, espcp_interrupt_handlers_t *handlers)
{
  ENTER_MESSAGE(__func__);

  switch (interface)
  {
    case espcp_esp32_interfaces_wi_fi:
      g_wifi_handlers = handlers;
      break;
  }

  EXIT_MESSAGE(__func__);
}

/****************************************************************************
 * Name: espcp_check_message_id
 *
 * Description:
 *  Method used by the generic linked list code to perform a comparison of
 *  an item in the list to see if it matches the required message ID.
 *
 * Input Parameters:
 *  message_id - message ID to look for in the linked list.
 *  list_item - current list item being examined.
 *
 * Returned Value:
 *  true if the message IDs match, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void espcp_dispatch_interrupt(espcp_message_t *message)
{
  ENTER_MESSAGE(__func__);

  if (message != NULL)
  {
    espcp_interrupt_handlers_t *handler = NULL;
    switch (message->interface)
    {
      case espcp_esp32_interfaces_wi_fi:
        handler = g_wifi_handlers;
        break;
    }
    bool processing = (handler != NULL);
    while (processing)
    {
      if ((((void *) handler->function) != NULL) && (handler->interrupt_handler != NULL))
      {
        if (handler->function == message->function)
        {
          handler->interrupt_handler(message);
          processing = false;
        }
      }
      else
      {
        processing = false;
      }
    }
  }

  EXIT_MESSAGE(__func__);
}

/****************************************************************************
 * Name: espcp_setup_message_dispatcher
 *
 * Description:
 *  Setup the message dispatcher and any supporting variables.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if the method succeeds, error code if there is a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_setup_message_dispatcher(void)
{
  ENTER_MESSAGE(__func__);

  int result = OK;

  g_request_response_message = (espcp_message_t *) malloc(sizeof(espcp_message_t));
  if (g_request_response_message == NULL)
  {
    return(-1);
  }
  memset(g_request_response_message, 0, sizeof(espcp_message_t));
  g_request_response_message->message_type = espcp_message_types_transport;
  g_request_response_message->interface = espcp_esp32_interfaces_transport;
  g_request_response_message->function = espcp_transport_function_send_response;
  g_request_response_message->semaphore = NULL;

  g_message_queue = mq_open(ESPCP_MESSAGE_QUEUE_NAME, O_WRONLY);
  if ((int) g_message_queue < 0)
  {
    return((int) g_message_queue);
  }

  result = pthread_mutex_lock(&g_message_id_mutex);
  if (result != OK)
  {
    return(result);
  }

  g_last_message_id = 0;

  result = pthread_mutex_unlock(&g_message_id_mutex);
  if (result != OK)
  {
    return(result);
  }

  g_messages_waiting_for_a_response = gl_create_empty_linked_list();

  sem_init(&g_messages_waiting_for_a_response_mutex, 0, 1);

  EXIT_MESSAGE(__func__);

  return(OK);
}

/****************************************************************************
 * Name: espcp_teardown_message_dispatcher
 *
 * Description:
 *  Teardown the message dispatcher and reset any static variables.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if the method succeeds, error code if there is a problem.
 *
 * Assumptions/Limitations:
 *  espcp_setup_message_dispatcher has been called at some point before this
 *  method is called.
 *
 ****************************************************************************/
int espcp_teardown_message_dispatcher(void)
{
  ENTER_MESSAGE(__func__);

  int result = OK;

  if (g_message_queue > 0)
  {
    result = mq_close(g_message_queue);
  }
  g_message_queue = 0;

  if (g_request_response_message != NULL)
  {
    espcp_delete_message_and_payload(g_request_response_message);
  }

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_queue_send_response_message
 *
 * Description:
 *  Add the "Send Response" message to the message queue.
 * 
 *  This message is added to the message queue when the ESP32 indicates
 *  that it has a response or some data for the STM32 to process.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  This method must be quick as it is intended to be called from an
 *  interrupt handler.
 *
 ****************************************************************************/
int espcp_queue_send_response_message(int irq, void *context, void *arg)
{
  ENTER_MESSAGE(__func__);

  espcp_add_message_to_queue(g_message_queue, g_request_response_message);

  EXIT_MESSAGE(__func__);

  return 0;
}

/****************************************************************************
 * Name: espcp_get_next_message_id
 *
 * Description:
 *  Get the next message ID that can be sent to the ESP32.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  0 if there is a problem otherwise a valid message ID (one larger 
 *  than 0x80000000).
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t espcp_get_next_message_id()
{
  ENTER_MESSAGE(__func__);

  uint32_t message_id = OK;
  int result = OK;
  
  result = pthread_mutex_lock(&g_message_id_mutex);
  if (result != OK)
  {
    return(0);
  }

  g_last_message_id++;
  message_id = (g_last_message_id | ESP32_MESSAGE_ID_MASK);

  result = pthread_mutex_unlock(&g_message_id_mutex);
  if (result != OK)
  {
    return(0);
  }

  EXIT_MESSAGE(__func__);

  return(message_id);
}

/****************************************************************************
 * Name: espcp_get_message_header
 *
 * Description:
 *  Get the header for an inbound message.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *
 * Returned Value:
 *  NULL if there is a problem, otherwise a pointer to the decoded header
 *  will be returned.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_get_message_header(espcp_configuration_t *configuration)
{
  ENTER_MESSAGE(__func__);

  espcp_message_t *message_header = NULL;

  if (configuration->send_data_to_esp32 != NULL)
  {
    memset(configuration->header, 0, configuration->header_only_buffer_size);
    configuration->send_data_to_esp32(NULL, configuration->header, configuration->header_only_buffer_size);
    message_header = espcp_extract_message(configuration->header, configuration->header_only_buffer_size, true);
    if (message_header != NULL)
    {
      espcp_send_acknowledgement(configuration, message_header, espcp_status_codes_completed_ok);
    }
  }

  EXIT_MESSAGE(__func__);

  return(message_header);
}

/****************************************************************************
 * Name: espcp_get_message_body
 *
 * Description:
 *  Get the message body from the ESP32.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  header - Pointer to the header that has been received.
 *
 * Returned Value:
 *  NULL if there is a problem, otherwise a pointer to the decoded message
 *  will be returned.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_get_message_body(espcp_configuration_t *configuration, espcp_message_t *header)
{
  ENTER_MESSAGE(__func__);

  uint32_t buffer_length = espcp_calculate_spi_buffer_size(ESPCP_MESSAGE_HEADER_SIZE + header->payload_length);
  uint8_t *buffer = (uint8_t *) malloc(buffer_length);
  espcp_message_t *message = NULL;

  if (configuration->send_data_to_esp32 != NULL)
  {
    memset(buffer, 0, buffer_length);
    configuration->send_data_to_esp32(NULL, buffer, buffer_length);
    message = espcp_extract_message(buffer, buffer_length, false);
    free(buffer);
    espcp_status_codes_t status_code = espcp_status_codes_failure;
    if (message == NULL)
    {
      status_code = espcp_status_codes_crc_error;
    }
    else
    {
      if ((message->interface != header->interface) || (message->message_id != header->message_id))
      {
        status_code = espcp_status_codes_invalid_packet;
        if (message->payload != NULL)
        {
          free(message->payload);
        }
        free(message);
        message = NULL;
      }
      else
      {
        status_code = espcp_status_codes_completed_ok;
      }
      
    }
    espcp_send_acknowledgement(configuration, header, status_code);
  }

  EXIT_MESSAGE(__func__);

  return(message);
}

/****************************************************************************
 * Name: espcp_get_message_header_acknowledgement
 *
 * Description:
 *  Message header has been sent so get the acknowledgement message from the
 *  ESP32.  This can either either positive or negative acknowledgement.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  sent - pointer to the message that was previously sent to the ESP32.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_get_message_header_acknowledgement(espcp_configuration_t *configuration, espcp_message_t *sent)
{
  ENTER_MESSAGE(__func__);

  int result = espcp_status_codes_completed_ok;
  uint8_t *encoded_message = (uint8_t *) malloc(configuration->header_only_buffer_size);

  if (configuration->send_data_to_esp32 != NULL)
  {
    configuration->send_data_to_esp32(NULL, encoded_message, configuration->header_only_buffer_size);
    espcp_message_t *acknowledgement = espcp_extract_message(encoded_message, configuration->header_only_buffer_size, true);
    if (acknowledgement == NULL)
    {
      result = espcp_status_codes_unexpected_data;
    }
    else
    {
      result = acknowledgement->status_code;
      if ((acknowledgement->interface != sent->interface) || (acknowledgement->message_id != sent->message_id))
      {
        result =  espcp_status_codes_unexpected_data;
      }
      free(acknowledgement);
    }
  }
  else
  {
    result = espcp_status_codes_failure;
  }
  
  free(encoded_message);

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_get_response_from_esp32
 *
 * Description:
 *  The ESP32 has indicated that there is a response waiting to be collected
 *  and processed.
 * 
 *  This method will collect the response and then work out which of the
 *  original requests this relates to.  The semaphore for the original
 *  request will then be released.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_get_response_from_esp32(espcp_configuration_t *configuration)
{
  ENTER_MESSAGE(__func__);

  int result = espcp_status_codes_failure;
  espcp_message_t *header = espcp_get_message_header(configuration);

  if (header != NULL)
  {
    espcp_message_t *message = NULL;
    if (header->payload_length > 0)
    {
      message = espcp_get_message_body(configuration, header);
      free(header);
    }
    else
    {
      message = header;
    }
    if (message != NULL)
    {
      result = espcp_status_codes_completed_ok;
      if (message->message_type == espcp_message_types_interrupt)
      {
        espcp_dispatch_interrupt(message);
      }
      else
      {
        sem_wait(&g_messages_waiting_for_a_response_mutex);
        espcp_message_t *waiting_message = (espcp_message_t *) 
                gl_remove_item(g_messages_waiting_for_a_response, message->message_id, espcp_check_message_id);
        sem_post(&g_messages_waiting_for_a_response_mutex);
        if (waiting_message != NULL)
        {
          waiting_message->payload_length = message->payload_length;
          waiting_message->payload = message->payload;
          waiting_message->status_code = message->status_code;
          free(message);
          if (waiting_message->semaphore != NULL)
          {
            sem_post(waiting_message->semaphore);
          }
        }
      }
    }
  }

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_send_header
 *
 * Description:
 *  Send the header of the message to the ESP32.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_send_header(espcp_configuration_t *configuration, espcp_message_t *message)
{
  ENTER_MESSAGE(__func__);

  uint32_t encoded_header_size = 0;
  uint8_t *encoded_header = espcp_encode_message(message, &encoded_header_size, true);
  int result = espcp_status_codes_completed_ok;

  if (encoded_header != NULL)
  {
    configuration->send_data_to_esp32(encoded_header, NULL, encoded_header_size);
  }
  else
  {
    result = espcp_status_codes_failure;
  }

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_send_acknowledgement
 *
 * Description:
 *  Send an acknowledgement to the ESP32.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be acknowledged.
 *  status_code - Status code to be added to the acknowledgement.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_send_acknowledgement(espcp_configuration_t *configuration, espcp_message_t *message, espcp_status_codes_t status_code)
{
  ENTER_MESSAGE(__func__);

  espcp_message_t *acknowledgement = (espcp_message_t *) malloc(sizeof(espcp_message_t));

  memcpy(acknowledgement, message, sizeof(espcp_message_t));
  if (status_code == espcp_status_codes_completed_ok)
  {
    acknowledgement->message_type = espcp_message_types_ack;
  }
  else
  {
    acknowledgement->message_type = espcp_message_types_nak;
  }
  acknowledgement->status_code = status_code;
  acknowledgement->payload = 0;
  acknowledgement->payload_length = 0;

  uint32_t length = 0;
  uint8_t *encoded_message = espcp_encode_message(acknowledgement, &length, false);
  if (configuration->send_data_to_esp32 != NULL)
  {
    configuration->send_data_to_esp32(encoded_message, NULL, length);
  }

  free(encoded_message);
  free(acknowledgement);

  EXIT_MESSAGE(__func__);
}

/****************************************************************************
 * Name: espcp_send_message_body
 *
 * Description:
 *  Send the message body to the ESP32 using the SPI interface.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message (and associated body) to be sent to the ESP32.
 *
 * Returned Value:
 *  0 if successful, or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_send_message_body(espcp_configuration_t *configuration, espcp_message_t *message)
{
  ENTER_MESSAGE(__func__);

  int result = espcp_status_codes_failure;
  uint32_t encoded_length = 0;

  if (configuration->send_data_to_esp32 != NULL)
  {
    uint8_t *encoded_message = espcp_encode_message(message, &encoded_length, false);
    if (encoded_message == NULL)
    {
      result = espcp_status_codes_unexpected_data;
    }
    else
    {
      configuration->send_data_to_esp32(encoded_message, NULL, encoded_length);
      configuration->send_data_to_esp32(NULL, configuration->header, configuration->header_only_buffer_size);
      espcp_message_t *acknowledgement = espcp_extract_message(configuration->header, configuration->header_only_buffer_size, true);
      if (acknowledgement == NULL)
      {
        result = espcp_status_codes_unexpected_data;
      }
      else
      {
        if (acknowledgement->message_type == espcp_message_types_nak)
        {
          syslog(LOG_INFO, "%s TODO: NAK received.\n", __func__);
        }
        else
        {
          if ((acknowledgement->interface != message->interface) || (acknowledgement->message_id != message->message_id))
          {
            result = espcp_status_codes_unexpected_data;
          }
          else
          {
            espcp_delete_message_payload(message);
            result = espcp_status_codes_completed_ok;
          } 
        }
      }
    }
  }

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_send_message
 *
 * Description:
 *  Send the message to the ESP32 using the SPI interface.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_send_message(espcp_configuration_t *configuration, espcp_message_t *message)
{
  ENTER_MESSAGE(__func__);

  int result = espcp_status_codes_failure;
  
  if (configuration->send_data_to_esp32 != NULL)
  {
    result = espcp_send_header(configuration, message);
    if (result != espcp_status_codes_completed_ok)
    {
      syslog(LOG_INFO, "%s TODO: unexpected result %d\n", __func__, result);
      return(result);
    }
    result = espcp_get_message_header_acknowledgement(configuration, message);
    if (result != espcp_status_codes_completed_ok)
    {
      if (result == espcp_status_codes_no_messages_waiting)
      {
        //
        //  This should only happen for the transport message "send response"
        //  and this message is a globally reused message therefore we should
        //  free the message.
        //
        return(espcp_status_codes_completed_ok);
      }
      syslog(LOG_INFO, "%s TODO: unexpected result %d\n", __func__, result);
      return(result);
    }

    /*
     *  There is an assumption that a tranport message CANNOT have a payload.
     */
    if (message->payload_length > 0)
    {
      message->message_type = espcp_message_types_data;
      result = espcp_send_message_body(configuration, message);
      if (result != espcp_status_codes_completed_ok)
      {
        syslog(LOG_INFO, "%s TODO: unexpected result %d\n", __func__, result);
        return(result);
      }
    }
    
    if (message->message_type == espcp_message_types_transport)
    {
      result = espcp_process_transport_message(configuration, message);
    }

    if (message->semaphore != NULL)
    {
      /*
       *  We need a response but we no longer need any payload data as this has
       *  been sent to the ESP32.  So release any memory allocated while waiting
       *  for the response.
       */
      // espcp_delete_message_payload(message);
      sem_wait(&g_messages_waiting_for_a_response_mutex);
      gl_add_item_to_head(g_messages_waiting_for_a_response, message);
      sem_post(&g_messages_waiting_for_a_response_mutex);
    }
  }
  if (result != espcp_status_codes_completed_ok)
  {
    syslog(LOG_INFO, "%s TODO: unexpected result %d\n", __func__, result);
  }

  EXIT_MESSAGE(__func__);

  return(result);
}

/****************************************************************************
 * Name: espcp_process_transport_message
 *
 * Description:
 *  Process a transport message.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  0 if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_process_transport_message(espcp_configuration_t *configuration, espcp_message_t *message)
{
  ENTER_MESSAGE(__func__);

  int result = espcp_status_codes_failure;

  if (message != NULL)
  {
    switch (message->function)
    {
      case espcp_transport_function_send_response:
        if (message->status_code == espcp_status_codes_completed_ok)
        {
          result = espcp_get_response_from_esp32(configuration);
        }
        else
        {
          if (message->status_code == espcp_status_codes_no_messages_waiting)
          {
            result = espcp_status_codes_completed_ok;
          }
          else
          {
            result = message->status_code;
          }
        }
        break;
      default:
        result = espcp_status_codes_failure;
        break;
    }
  }

  EXIT_MESSAGE(__func__);

  return(result);
}