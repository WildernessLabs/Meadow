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
#include <fcntl.h>

#include <nuttx/semaphore.h>
#include <nuttx/config.h>

#include "espcp_message_dispatcher.h"
#include "espcp_message.h"
#include "espcp_shared_enums.h"
#include "espcp_queue.h"
#include "espcp_encoders.h"
#include "espcp_event_handlers.h"

#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/**
 *  Mutex used to ensure exclusive access to the message ID.
 */
static sem_t g_message_id_mutex;

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

/****************************************************************************
 * Private (static) Function Prototypes
 ****************************************************************************/

static bool espcp_process_immediate_messages(espcp_message_t *);
static int espcp_send_packet(espcp_configuration_t *, espcp_message_t *);
static void espcp_send_acknowledgement(espcp_configuration_t *, espcp_message_t *, espcp_status_codes_t);
static int espcp_get_message_header_acknowledgement(espcp_configuration_t *, espcp_message_t *);

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
    espcp_message_t *message = (espcp_message_t *) list_item;
    return(message->message_id == message_id);
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
    int result = OK;

    g_request_response_message = (espcp_message_t *) malloc(sizeof(espcp_message_t));
    if (g_request_response_message == NULL)
    {
        return (-1);
    }
    memset(g_request_response_message, 0, sizeof(espcp_message_t));
    g_request_response_message->message_type = espcp_message_types_transport;
    g_request_response_message->interface = espcp_esp32_interfaces_transport;
    g_request_response_message->function = espcp_transport_function_send_response;
    g_request_response_message->semaphore = NULL;

    g_message_queue = mq_open(ESPCP_REQUEST_MESSAGE_QUEUE_NAME, O_WRONLY);
    if ((int)g_message_queue < 0)
    {
        return ((int)g_message_queue);
    }

    result = sem_init(&g_message_id_mutex, 0, 1);
    if (result != OK)
    {
        return (result);
    }

    result = sem_setprotocol(&g_message_id_mutex, SEM_PRIO_NONE);
    if (result != OK)
    {
        return (result);
    }

    result = sem_wait(&g_message_id_mutex);
    if (result != OK)
    {
        return (result);
    }

    g_last_message_id = 0;

    result = sem_post(&g_message_id_mutex);
    if (result != OK)
    {
        return (result);
    }

    g_messages_waiting_for_a_response = gl_create_empty_linked_list();

    result = sem_init(&g_messages_waiting_for_a_response_mutex, 0, 1);

    return (result);
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
 *  0 always.
 *
 * Assumptions/Limitations:
 *  This method must be quick as it is intended to be called from an
 *  interrupt handler.
 * 
 *  This method should ONLY be called from the interrupt handler as it
 *  switches the ESP responding flag.  It is assumed that if the interrupt
 *  handler has fired that the ESP is alive as it has generated the interrupt.
 *
 ****************************************************************************/
int espcp_queue_send_response_message(int irq, void *context, void *arg)
{
    espcp_config_lock();
    espcp_configuration_t *config = espcp_get_configuration();
    if (config->esp_not_responding)
    {
        config->esp_not_responding = false;
    }
    espcp_config_unlock();

    espcp_add_message_to_queue(g_message_queue, g_request_response_message);
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
 *  0 if there is a problem otherwise a valid message ID (which will be larger 
 *  than 0x80000000).
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t espcp_get_next_message_id()
{
    uint32_t message_id = OK;
    int result = OK;

    result = sem_wait(&g_message_id_mutex);
    if (result != OK)
    {
        return (0);
    }

    g_last_message_id++;
    message_id = (g_last_message_id | ESP32_MESSAGE_ID_MASK);

    result = sem_post(&g_message_id_mutex);
    if (result != OK)
    {
        return (0);
    }

    return(message_id);
}

/****************************************************************************
 * Name: espcp_clear_spi_buffers
 *
 * Description:
 *  Clear the SPI Tx and Rx buffers.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  Memory allocated for the buffers is at least ESPCP_MAXIMUM_SPI_FRAME_SIZE
 *  long.
 *
 ****************************************************************************/
static void espcp_clear_spi_buffers(espcp_configuration_t *configuration)
{
    espcp_config_lock();
    uint8_t *tx_buffer = configuration->spi_tx_buffer;
    uint8_t *rx_buffer = configuration->spi_rx_buffer;
    espcp_config_unlock();

    memset(tx_buffer, 0, ESPCP_MAXIMUM_SPI_FRAME_SIZE);
    memset(rx_buffer, 0, ESPCP_MAXIMUM_SPI_FRAME_SIZE);
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
static int espcp_get_message_header_acknowledgement(espcp_configuration_t *configuration, espcp_message_t *sent)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    int result = espcp_status_codes_completed_ok;
    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    uint32_t header_only_buffer_size = configuration->header_only_buffer_size;
    uint8_t *rx_buffer = configuration->spi_rx_buffer;
    espcp_config_unlock();

    if (send_data_to_esp32 != NULL)
    {
        espcp_clear_spi_buffers(configuration);
        send_data_to_esp32(NULL, rx_buffer, header_only_buffer_size);
        espcp_message_t *acknowledgement = espcp_extract_message(rx_buffer, header_only_buffer_size, true);
        if (acknowledgement == NULL)
        {
            MEADOW_TRACE_INFORMATION("Cannot decode acknowledgement\n");
            result = espcp_status_codes_unexpected_data;
        }
        else
        {
            result = acknowledgement->status_code;
            if ((acknowledgement->interface != sent->interface) || (acknowledgement->message_id != sent->message_id))
            {
                MEADOW_TRACE_INFORMATION("Interface and ID do not match\n");
                result = espcp_status_codes_unexpected_data;
            }
            free(acknowledgement);
        }
    }
    else
    {
        result = espcp_status_codes_failure;
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);

    return (result);
}

/****************************************************************************
 * Name: espcp_get_message_acknowledgement
 *
 * Description:
 *  Get the response acknowledgement from
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message_id - ID of the message that made the request.
 *
 * Returned Value:
 *  0 (or positive number) if successful, -1 or an error code if a problem arises.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int espcp_get_message_request_acknowledgement(espcp_configuration_t *configuration, uint32_t message_id)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    int result = -1;

    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    uint32_t header_only_buffer_size = configuration->header_only_buffer_size;
    uint8_t *rx_buffer = configuration->spi_rx_buffer;
    espcp_config_unlock();

    if (send_data_to_esp32 != NULL)
    {
        espcp_clear_spi_buffers(configuration);
        send_data_to_esp32(NULL, rx_buffer, header_only_buffer_size);
        espcp_message_t *acknowledgement = espcp_extract_message(rx_buffer, header_only_buffer_size, true);
        if (acknowledgement == NULL)
        {
            MEADOW_TRACE_INFORMATION("Cannot decode acknowledgement\n");
        }
        else
        {
            if ((acknowledgement->interface != espcp_esp32_interfaces_transport) || (acknowledgement->message_id != message_id))
            {
                MEADOW_TRACE_INFORMATION("Interface and ID do not match\n");
            }
            else
            {
                result = acknowledgement->payload_length;
            }
            free(acknowledgement);
        }
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);

    return (result);
}

/****************************************************************************
 * Name: espcp_send_packet
 *
 * Description:
 *  Send a packet of a message to the ESP32.
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
int espcp_send_packet(espcp_configuration_t *configuration, espcp_message_t *message)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    uint8_t *tx_buffer = configuration->spi_tx_buffer;
    espcp_config_unlock();

    #if defined(USE_MEADOW_DEBUG_HELPERS)
        espcp_dump_message(message);
    #endif
    uint32_t encoded_header_size = 0;
    espcp_encode_message(message, tx_buffer, &encoded_header_size, false);
    int result = espcp_status_codes_completed_ok;

    if (send_data_to_esp32 != NULL)
    {
        MEADOW_TRACE_INFORMATION("%s Sending %d bytes to the ESP32\n", __func__, encoded_header_size);
        send_data_to_esp32(tx_buffer, NULL, encoded_header_size);
    }
        
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);

    return (result);
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
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_send_acknowledgement(espcp_configuration_t *configuration, espcp_message_t *message, espcp_status_codes_t status_code)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    uint8_t *tx_buffer = configuration->spi_tx_buffer;
    espcp_config_unlock();

    if (send_data_to_esp32 != NULL)
    {
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
        acknowledgement->packet_offset = 0;
        acknowledgement->packet_length = 0;

        uint32_t length = 0;
        espcp_encode_message(acknowledgement, tx_buffer, &length, false);

        send_data_to_esp32(tx_buffer, NULL, length);

        free(acknowledgement);
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
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
 *  None.
 *
 * Assumptions/Limitations:
 *  It is the responsibility of the caller to put the message back in the
 *  message queue if this method fails.
 * 
 *  SPI interface is locked before this method is called.
 * 
 *  Message queue (for retries) is already setup.
 *
 ****************************************************************************/
void espcp_send_message(espcp_configuration_t *configuration, espcp_message_t *message)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    int result = espcp_status_codes_failure;

    if (espcp_process_immediate_messages(message))
    {
        espcp_delete_message_and_payload(message);
        result = espcp_status_codes_completed_ok;
    }
    else
    {
        #if defined(USE_MEADOW_DEBUG_HELPERS)
            espcp_dump_message(message);
        #endif
        espcp_config_lock();
        espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
        espcp_config_unlock();

        if (send_data_to_esp32 != NULL)
        {
            uint16_t offset = 0;
            uint16_t length = (message->payload_length <= ESPCP_MAXIMUM_PACKET_SIZE) ? message->payload_length : ESPCP_MAXIMUM_PACKET_SIZE;
            bool sending_packets = true;
            while (sending_packets)
            {
                message->packet_offset = offset;
                message->packet_length = length;
                result = espcp_send_packet(configuration, message);
                if (result == espcp_status_codes_completed_ok)
                {
                    espcp_lock_spi_interface();
                    result = espcp_get_message_header_acknowledgement(configuration, message);
                    if (result == espcp_status_codes_completed_ok)
                    {
                        if ((offset + length) == message->payload_length)
                        {
                            sending_packets = false;
                        }
                        else
                        {
                            offset += length;
                            uint16_t remaining = message->payload_length - offset;
                            length = (remaining <= ESPCP_MAXIMUM_PACKET_SIZE) ? remaining : ESPCP_MAXIMUM_PACKET_SIZE;
                        }
                    }
                    else
                    {
                        sending_packets = false;
                    }
                }
            }
            
            if (result != espcp_status_codes_completed_ok)
            {
                if (result == espcp_status_codes_no_messages_waiting)
                {
                    result = espcp_status_codes_completed_ok;
                }
            }
            else
            {
                if (message->semaphore != NULL)
                {
                    /*
                    *  We need a response but we no longer need any payload data as this has been sent to the ESP32.  
                    *  So release any memory allocated while waiting for the response.
                    */
                    espcp_delete_message_payload(message);
                    sem_wait(&g_messages_waiting_for_a_response_mutex);
                    gl_add_item_to_head(g_messages_waiting_for_a_response, message);
                    sem_post(&g_messages_waiting_for_a_response_mutex);
                }
                else
                {
                    if ((message->interface != espcp_esp32_interfaces_transport) && (message->function != espcp_transport_function_send_response))
                    {
                        /*
                        *  This is a non blocking message (as it has no semaphore) and any response will come via the event mechanism 
                        *  so we no longer need the message or payload.
                        * 
                        *  The send response function in the transport interface is a special message,  We hold a static message that
                        *  is reused and so this message should not be deleted, hence the guard condition above.
                        * 
                        *  Note that this is the earliest we can dispose of the message.
                        */
                        espcp_delete_message_and_payload(message);
                    }
                }
            }
        }
    }

    if (result != espcp_status_codes_completed_ok)
    {
        MEADOW_TRACE_INFORMATION("%s@%d TODO: unexpected result.\n", __FILE__, __LINE__);
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
}

/****************************************************************************
 * Name: espcp_get_response_message
 *
 * Description:
 *  Get an event or response type message from the ESP32.
 * 
 *  Responses are messages that hold data following a request for some
 *  action from the ESP32.  These are generated by blocking requests
 *  such as socket, recvfrom, sendto method calls.
 * 
 *  Events are generated by the ESP32 in response to some action such as
 *  connection to an access point or to a non-blocking request such as
 *  connect to default access point.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  Pointer to the message received from the ESP32.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static espcp_message_t *espcp_get_response_message(espcp_configuration_t *configuration, uint32_t amount_of_data)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    uint8_t *rx_buffer = configuration->spi_rx_buffer;
    espcp_config_unlock();

    espcp_message_t *result = NULL;
    uint32_t spi_transaction_size = espcp_calculate_spi_buffer_size(amount_of_data);
    if (send_data_to_esp32 != NULL)
    {
        MEADOW_TRACE_INFORMATION("%s Requesting %d bytes from the ESP32\n", __func__, spi_transaction_size);
        send_data_to_esp32(NULL, rx_buffer, amount_of_data);
        result = espcp_extract_message(rx_buffer, spi_transaction_size, false);
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);

    return (result);
}

static void espcp_process_response(espcp_configuration_t *configuration, espcp_message_t *message)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    if (message != NULL)
    {
        if (message->message_type == espcp_message_types_event)
        {
            espcp_add_message_to_queue(configuration->incoming_event_queue, message);
        }
        else
        {
            sem_wait(&g_messages_waiting_for_a_response_mutex);
            espcp_message_t *waiting_message = (espcp_message_t *) gl_remove_item(g_messages_waiting_for_a_response, message->message_id, espcp_check_message_id);
            sem_post(&g_messages_waiting_for_a_response_mutex);
            if (waiting_message != NULL)
            {
                waiting_message->payload_length = message->payload_length;
                waiting_message->payload = message->payload;
                waiting_message->status_code = message->status_code;
                if (waiting_message->semaphore != NULL)
                {
                    sem_post(waiting_message->semaphore);
                }
                else
                {
                    MEADOW_TRACE_DEBUG("%s:%d Message with ID %d does not have a semaphore.\n", __FILE__, __LINE__, message->message_id);
                }
            }
            else
            {
                MEADOW_TRACE_DEBUG("%s:%d Message with ID %d cannot be located in message waiting responses queue.\n", __FILE__, __LINE__, message->message_id);
            }
            free(message);
        }
    }
    
    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
}

/****************************************************************************
 * Name: espcp_get_message
 *
 * Description:
 *  Get a message to the ESP32 using the SPI interface.
 *
 * Input Parameters:
 *  configuration - pointer to the ESP32 coprocessor configuration.
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  It is the responsibility of the caller to put the message back in the
 *  message queue if this method fails.
 * 
 *  SPI interface is locked before this method is called.
 * 
 ****************************************************************************/
void espcp_get_message(espcp_configuration_t *configuration, espcp_message_t *message)
{
    MEADOW_TRACE_INFORMATION("%s: Enter\n", __func__);

    int result = espcp_status_codes_failure;

    espcp_config_lock();
    espcp_send_data_function_t send_data_to_esp32 = configuration->send_data_to_esp32;
    espcp_config_unlock();

    if (send_data_to_esp32 != NULL)
    {
        message->message_id = espcp_get_next_message_id();      // Dummy send response message always has an ID of 0.
        result = espcp_send_packet(configuration, message);
        if (result == espcp_status_codes_completed_ok)
        {
            espcp_lock_spi_interface();

            int payload_length = espcp_get_message_request_acknowledgement(configuration, message->message_id);

            if (payload_length >= 0)
            {
                espcp_lock_spi_interface();

                espcp_message_t *response = espcp_get_response_message(configuration, payload_length + ESPCP_MESSAGE_HEADER_SIZE);
                if (response != NULL)
                {
                    espcp_lock_spi_interface();
                    espcp_send_acknowledgement(configuration, response, espcp_status_codes_completed_ok);
                    espcp_process_response(configuration, response);
                }
            }
        }
    }
    if (result != espcp_status_codes_completed_ok)
    {
        espcp_add_message_to_queue(g_message_queue, message);
    }

    MEADOW_TRACE_INFORMATION("%s: Exit\n", __func__);
}

/****************************************************************************
 * Name: espcp_process_immediate_messages
 *
 * Description:
 *  Check to see if a transport message is for immedate processing.  If it
 *  is then process the message.
 *
 * Input Parameters:
 *  message - message to be sent to the ESP32.
 *
 * Returned Value:
 *  true if the message was processed, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static bool espcp_process_immediate_messages(espcp_message_t *message)
{
    bool result = false;
    if (message != NULL)
    {
        if (message->interface == espcp_esp32_interfaces_transport)
        {
            switch (message->function)
            {
            case espcp_transport_function_reset_esp32:
                espcp_reset();
                result = true;
                break;
            default:
                result = false;
                break;
            }
        }
    }
    return (result);
}
