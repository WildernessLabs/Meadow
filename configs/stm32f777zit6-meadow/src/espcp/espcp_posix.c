/****************************************************************************
 * espcp_posix.h
 *
 *  Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
 * 
 *  Methods supporting the POSIX functions required by Mono.
 *  
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <nuttx/net/netdev.h>
#include <netdb.h>

#include <nuttx/semaphore.h>
#include <nuttx/pthread.h>
#include <nuttx/config.h>

#include "espcp_posix.h"
#include "espcp_system.h"
#include "espcp_encoders.h"
#include "espcp_shared_enums.h"
#include "espcp_message_dispatcher.h"
#include "espcp_coprocessor.h"
#include "espcp_common.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Constants
 ****************************************************************************/
/*
 *  The ESP32 can only handle messages less than 4096 bytes.  The maximum
 *  buffer etc is therefore less than 4096 as each message has an overhead.
 * 
 *  TODO: Think where this should be defined and also if it should be set
 *        by a "get config" message to the ESP32.
 */
static const uint32_t MAXIMUM_READ_WRITE_BUFFER_SIZE = 4000;

/****************************************************************************
 * Private Types
 ****************************************************************************/

/*
 *  Structure to map the address of an addrinfo structure on the STM to the
 *  correspnding address of the structure on the ESP32.
 */
struct espcp_address_table_entry_s
{
    struct addrinfo *stm;
    void *esp;
};
typedef struct espcp_address_table_entry_s espcp_address_table_entry_t;

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/*
 *  Static pointer to the name of the file being compiled.  This is used for
 *  logging and making it a static variable ensure that one one instance exists.
 */
static char *_thisFile = __FILE__;

static gl_linked_list_t *g_addrinfo_mappings = NULL;

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_posix_network_init
 *
 * Description:
 *  Initialise any data structures / variables required to support the ESP
 *  coprocessor POSIX methods.
 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_posix_network_init(void)
{
    g_addrinfo_mappings = gl_create_empty_linked_list();
}

/****************************************************************************
 * Name: espcp_check_stm_address
 *
 * Description:
 *  Check the to see if the STM address matches that address being searched
 *  for.
 *  
 *  This method supports the generic list class used to hold STM / ESP
 *  address pairs.
 *
 * Input Parameters:
 *  address - Address to check.
 *  list_item - current list item being examined.
 *
 * Returned Value:
 *  true if the address matches the STM address.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static bool espcp_check_stm_address(uint32_t address, void *list_item)
{
    espcp_address_table_entry_t *address_table_entry = (espcp_address_table_entry_t *)list_item;

    return (address_table_entry->stm == (void *)address);
}

/****************************************************************************
 * Name: espcp_check_esp_address
 *
 * Description:
 *  Check the to see if the ESP address matches that address being searched
 *  for.
 *  
 *  This method supports the generic list class used to hold STM / ESP
 *  address pairs.
 *
 * Input Parameters:
 *  address - Address to check.
 *  list_item - current list item being examined.
 *
 * Returned Value:
 *  true if the address matches the ESP address.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static bool espcp_check_esp_address(uint32_t address, void *list_item)
{
    espcp_address_table_entry_t *address_table_entry = (espcp_address_table_entry_t *)list_item;

    return (address_table_entry->esp == (void *)address);
}
#pragma GCC diagnostic pop

/****************************************************************************
 * Name: espcp_getaddrinfo
 *
 * Description:
 *  Provide a protocol-independent translation from an ANSI host name to an
 *  address.
 * 
 * This method instructs the ESP32 to call the getaddrinfo method which
 * will in turn call the equivalent LWIP method.
 * 
 * See:
 * http://www.nongnu.org/lwip/2_0_x/group__netdbapi.html#ga558191530d91c101621b49e43bd5bbf5
 * http://www.nongnu.org/lwip/2_0_x/lwip_2netdb_8h.html#af356989c172a51187e22b557f2d4165
 *
 * Input Parameters:
 *  node - Server (node) name or numeric host address string (IP address).
 *  service - Service name or port number of the service.
 *  hints - AddrInfo structure that contains hints about the type of socket the
 *          caller supports.
 *  res - Pointer to a list of AddrInfo structures containing information about
 *        the host.
 *
 * Returned Value:
 *  Error code if there is a problem, 0 if successful.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{

    syslog(LOG_CRIT, "%s@%d %s called.\n", _thisFile, __LINE__, __func__);
    int32_t result = 0;

    espcp_get_addr_info_request_t *request = (espcp_get_addr_info_request_t *)malloc(sizeof(espcp_get_addr_info_request_t));
    if (request == NULL)
    {
        return (-1);
    }
    request->node_name = (char *)node;
    request->serv_name = (char *)service;
    request->result_length = 0;
    request->result = NULL;

    espcp_addr_info_t *h = (espcp_addr_info_t *)malloc(sizeof(espcp_addr_info_t));
    memset(h, 0, sizeof(espcp_addr_info_t));
    h->my_heap_address = 0;
    h->flags = hints->ai_flags;
    h->family = hints->ai_family;
    h->socket_type = hints->ai_socktype;
    h->protocol = hints->ai_protocol;
    h->addr_len = hints->ai_addrlen;
    h->addr = (uint8_t *)hints->ai_addr;
    h->canon_name = hints->ai_canonname;
    h->next = hints->ai_next;

    request->hints_length = espcp_addr_info_buffer_size(h);
    request->hints = (uint8_t *)malloc(request->hints_length);
    if (request->hints == NULL)
    {
        free(h);
        free(request);
        return (-1);
    }
    espcp_encode_addr_info(h, request->hints);

    free(h);

    int payload_length = espcp_get_addr_info_request_buffer_size(request);
    uint8_t *payload = (uint8_t *)malloc(payload_length);
    if (payload == NULL)
    {
        free(request->hints);
        free(request);
        return (-1);
    }
    espcp_encode_get_addr_info_request(request, payload);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                            espcp_wi_fi_function_get_addr_info, espcp_status_codes_completed_ok,
                                                            espcp_get_next_message_id(), payload, payload_length);
    free(request->hints);
    free(request);

    if (message == NULL)
    {
        free(payload);
        return (-1);
    }

    *res = NULL;
    if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
    {
        espcp_get_addr_info_response_t *response = espcp_extract_get_addr_info_response(message->payload);
        result = response->addr_info_response_errno;
        if (result == 0)
        {
            espcp_addr_info_t *ai = espcp_extract_addr_info(response->res);
            if (ai != NULL)
            {
                struct addrinfo *r = (struct addrinfo *)malloc(sizeof(struct addrinfo));
                if (r == NULL)
                {
                    free(ai);
                }
                else
                {
                    *res = r;
                    espcp_address_table_entry_t *address_mapping = (espcp_address_table_entry_t *)malloc(sizeof(espcp_address_table_entry_t));
                    if (address_mapping == NULL)
                    {
                        free(r);
                        *res = NULL;
                        free(ai);
                    }
                    else
                    {
                        r->ai_flags = ai->flags;
                        r->ai_family = ai->family;
                        r->ai_socktype = ai->socket_type;
                        r->ai_protocol = ai->protocol;
                        r->ai_addrlen = ai->addr_len;
                        r->ai_addr = (struct sockaddr *)ai->addr;
                        r->ai_canonname = ai->canon_name;
                        r->ai_next = NULL; /* TODO: Fix this hack. */
                        address_mapping->stm = r;
                        address_mapping->esp = (void *)ai->my_heap_address;
                        gl_add_item_to_head(g_addrinfo_mappings, address_mapping);
                        free(ai);
                    }
                }
            }
            free(response->res);
        }
        free(response);
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_freeaddrinfo
 *
 * Description:
 *  Free any previously allocated addrinfo structure and pointers.
 *
 *  This method instructs the ESP32 to call the socket method which will in
 *  turn call the equivalent LWIP method.
 * 
 * See:
 * http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga862d8f4070c66dddb979540ce9ba6a83
 * http://www.nongnu.org/lwip/2_0_x/lwip_2netdb_8h.html#a7f65ff5982a0743849a644ef2cd15ef5
 * 
 * Input Parameters:
 *  ai - Pointer to the addrinfo object on the ESP32.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_freeaddrinfo(struct addrinfo *ai)
{
    espcp_message_t *message = NULL;

    espcp_address_table_entry_t *mapping = (espcp_address_table_entry_t *)gl_remove_item(g_addrinfo_mappings, (uint32_t)ai, espcp_check_stm_address);
    if (mapping != NULL)
    {
        espcp_free_addr_info_request_t *request = (espcp_free_addr_info_request_t *)malloc(sizeof(espcp_free_addr_info_request_t));
        request->addr_info_address = (uint32_t)mapping->esp;

        int payload_length = espcp_free_addr_info_request_buffer_size(request);
        uint8_t *payload = (uint8_t *)malloc(payload_length);
        if (payload == NULL)
        {
            free(request);
        }
        else
        {
            espcp_encode_free_addr_info_request(request, payload);
            free(request);

            message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                   espcp_wi_fi_function_free_addr_info, espcp_status_codes_completed_ok,
                                                   espcp_get_next_message_id(), payload, payload_length);

            espcp_queue_message(message, true);
        }

        espcp_delete_message_and_payload(message);
        if (ai->ai_addr != NULL)
        {
            free(ai->ai_addr);
        }
        free(ai);
        free(mapping);
    }
}

/****************************************************************************
 * Name: espcp_write
 *
 * Description:
 *  Write the specified number of bytes to the socket.
 * 
 *  This method instructs the ESP32 to call the write method which will in 
 *  turn call the equivalent LWIP method.
 * 
 * See:
 *  http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga0a651eb5fb5e6127f5e5153ce2251f3d
 * 
 * Input Parameters:
 *  socket_handle - ESP32 handle for the socket to write the data to.
 *  buffer - Data to be written to the socket.
 *  count - Number of bytes to write to the socket.
 *
 * Returned Value:
 *  If successful, the number of bytes written to the socket, -1 otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int32_t espcp_write(int socket_handle, const void *buffer, size_t count)
{
    int32_t result = -1;
    espcp_message_t *message = NULL;

    if ((buffer == NULL) || (count > MAXIMUM_READ_WRITE_BUFFER_SIZE))
    {
        return (-1);
    }

    espcp_write_request_t *request = (espcp_write_request_t *)malloc(sizeof(espcp_write_request_t));
    request->socket_handle = socket_handle;
    request->buffer = (uint8_t *)buffer;
    request->buffer_length = count;
    request->count = count;

    int payload_length = espcp_write_request_buffer_size(request);
    uint8_t *payload = (uint8_t *)malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
    }
    else
    {
        espcp_encode_write_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_write, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);

        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            errno = response->response_errno;
            result = response->result;
            free(response);
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}
