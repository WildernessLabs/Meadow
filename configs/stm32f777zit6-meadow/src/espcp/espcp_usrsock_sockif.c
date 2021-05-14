/****************************************************************************
 * net/usrsock/usrsock_sockif.c
 *
 *  Copyright (C) 2017 Haltian Ltd. All rights reserved.
 *  Author: Jussi Kivilinna <jussi.kivilinna@haltian.com>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <net/if.h>
#include <sys/socket.h>
#include <nuttx/net/net.h>
#include <poll.h>
#include <strings.h>

#include "espcp_usrsock.h"
#include "espcp_common.h"
#include "espcp_coprocessor.h"
#include "generic_list.h"
#include "espcp_event_handlers.h"
#include "espcp_message_dispatcher.h"

/****************************************************************************
 * Definitions.
 ****************************************************************************/

/*
 *  The SPI bus on the ESP32 can only deal with a maximum of 4094 bytes so
 *  define a smaller buffer to account for overhead.
 */
#define MAXIMUM_READ_WRITE_BUFFER_SIZE 4000

/****************************************************************************
 * Local data structures.
 ****************************************************************************/

/*
 *  Hold information about a poll request that is active on the ESP32.
 */
struct espcp_poll_request_list_item_s
{
    struct pollfd *fd;      /* Pointer to the pollfd structure of the original request. */
    uint32_t request_id;    /* ID of the message sent to the ESP32. */
};
typedef struct espcp_poll_request_list_item_s espcp_poll_request_list_item_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int espcp_usrsock_sockif_setup(struct socket *psock, int protocol);

static sockcaps_t espcp_usrsock_sockif_sockcaps(struct socket *psock);

static void espcp_usrsock_sockif_addref(struct socket *psock);

static ssize_t espcp_usrsock_sockif_send(struct socket *psock, const void *buf, size_t len, int flags);

static int espcp_usrsock_sockif_close(struct socket *psock);

/****************************************************************************
 * Public Data
 ****************************************************************************/

/*
 *  Table of function pointers for the ESP32 networking methods.
 */
const struct sock_intf_s g_usrsock_sockif_esp32 =
{
    espcp_usrsock_sockif_setup,       /* si_setup */
    espcp_usrsock_sockif_sockcaps,    /* si_sockcaps */
    espcp_usrsock_sockif_addref,      /* si_addref */
    espcp_usrsock_bind,               /* si_bind */
    espcp_usrsock_getsockname,        /* si_getsockname */
    espcp_usrsock_getpeername,        /* si_getpeername */
    espcp_usrsock_listen,             /* si_listen */
    espcp_usrsock_connect,            /* si_connect */
    espcp_usrsock_accept,             /* si_accept */
#ifndef CONFIG_DISABLE_POLL
    espcp_usrsock_poll,               /* si_poll */
#endif
    espcp_usrsock_sockif_send,        /* si_send */
    espcp_usrsock_sendto,             /* si_sendto */
#ifdef CONFIG_NET_SENDFILE
    NULL,                             /* si_sendfile */
#endif
    espcp_usrsock_recvfrom,           /* si_recvfrom */
    espcp_usrsock_close,              /* si_close */
    espcp_usrsock_ioctl,              /* si_ioctl */
    /*
      *  All of the above are from the generic usrsock project.
      *  The methods below are specific to the Meadow F7 project.
      */
    espcp_usrsock_setsockopt,          /* si_setsockopt */
    espcp_usrsock_getsockopt,          /* si_getsockopt */
    espcp_usrsock_read,                /* si_read */
    espcp_usrsock_dup2,                /* si_dup2 */
    espcp_usrsock_sendmsg,             /* si_sendmsg */
    espcp_usrsock_shutdown,            /* si_shutdown */
    espcp_usrsock_recvmsg,             /* si_recvmsg */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/*
 *  Static pointer to the name of the file being compiled.  This is used for
 *  logging and making it a static variable ensure that one one instance exists.
 */
static char *_thisFile = __FILE__;

/*
 *  Linked list holding the list of poll requests that are in progress
 *  on the ESP32.
 */
static gl_linked_list_t *_espcp_poll_requests = NULL;

/*
 *  Mutex used to control access to the poll request linked list.
 */
static sem_t _espcp_poll_requests_mutex;

/****************************************************************************
 * Methods
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_usrsock_poll_request_compare_message_id
 *
 * Description:
 *   Compare the specified key with the request ID in the item.
 *
 * Input Parameters:
 *  key    - Message ID to look for.
 *  item   - Poll request item being looked for.
 *
 * Returned Value:
 *  true if the key matches the request ID, false otherwise.
 *
 ****************************************************************************/
static bool espcp_usrsock_poll_request_compare_message_id(uint32_t key, void *item)
{
    return(key == ((espcp_poll_request_list_item_t *) item)->request_id);
}

/****************************************************************************
 * Name: espcp_usrsock_poll_request_compare_fd_pointer
 *
 * Description:
 *   Compare the specified key with the request ID in the item.
 *
 * Input Parameters:
 *  key    - Pointer to the fd structure being looked for.
 *  item   - Poll request item being looked for.
 *
 * Returned Value:
 *  true if the key matches the request ID, false otherwise.
 *
 ****************************************************************************/
static bool espcp_usrsock_poll_request_compare_fd_pointer(uint32_t key, void *item)
{
    return((struct pollfd *) key == ((espcp_poll_request_list_item_t *) item)->fd);
}

/****************************************************************************
 * Name: espcp_usrsock_init
 *
 * Description:
 *   Perform system wide usrsock initialisation for the ESP32 usrsock layer.
 *
 ****************************************************************************/
void espcp_usrsock_init()
{
    /*
     *  Initialise the poll request linked list and the semaphore (mutex) used to
     *  restrict access to the list.
     */
    if (_espcp_poll_requests == NULL)
    {
        _espcp_poll_requests = gl_create_empty_linked_list();
        sem_init(&_espcp_poll_requests_mutex, 0, 1);
        sem_setprotocol(&_espcp_poll_requests_mutex, SEM_PRIO_NONE);
    }
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_setup
 *
 * Description:
 *   Called for socket() to verify that the provided socket type and
 *   protocol are usable by this address family.  Perform any family-
 *   specific socket fields.
 *
 * Input Parameters:
 *   psock    - A pointer to a user allocated socket structure to be
 *              initialized.
 *   protocol - (see sys/socket.h)
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  Otherwise, a negated errno value is
 *   returned.
 *
 ****************************************************************************/
static int espcp_usrsock_sockif_setup(struct socket *psock, int protocol)
{
    int domain = psock->s_domain;
    int type = psock->s_type;

    psock->s_type = PF_UNSPEC;
    psock->s_conn = NULL;

    /* Let the user socket logic handle the setup...
     *
     * A return value of zero means that the operation was
     * successfully handled by usrsock.  A negative value means that
     * an error occurred.  The special error value -ENETDOWN means
     * that usrsock daemon is not running.  The caller should attempt
     * to open socket with kernel networking stack in this case.
     */

    int ret = espcp_usrsock_socket(domain, type, protocol, psock);
    if (ret == -ENETDOWN)
    {
        nwarn("WARNING: usrsock daemon is not running\n");
    }
    return OK;
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_sockcaps
 *
 * Description:
 *   Return the bit encoded capabilities of this socket.
 *
 * Input Parameters:
 *   psock - Socket structure of the socket whose capabilities are being
 *           queried.
 *
 * Returned Value:
 *   The non-negative set of socket capabilities is returned.
 *
 ****************************************************************************/
static sockcaps_t espcp_usrsock_sockif_sockcaps(struct socket *psock)
{
    return SOCKCAP_NONBLOCKING;
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_addref
 *
 * Description:
 *   Increment the reference count on the underlying connection structure.
 *
 * Input Parameters:
 *   psock - Socket structure of the socket whose reference count will be
 *           incremented.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
static void espcp_usrsock_sockif_addref(struct socket *psock)
{
    // struct usrsock_conn_s *conn;

    espcp_usrsock_not_implemented(__func__);

    // DEBUGASSERT(psock != NULL && psock->s_conn != NULL);

    // conn = psock->s_conn;
    // DEBUGASSERT(conn->crefs > 0 && conn->crefs < 255);
    // conn->crefs++;
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_send
 *
 * Description:
 *   The espcp_usrsock_sockif_send() call may be used only when the socket is in
 *   a connected state  (so that the intended recipient is known).
 *
 * Input Parameters:
 *   psock    An instance of the internal socket structure.
 *   buf      Data to send
 *   len      Length of data to send
 *   flags    Send flags (ignored)
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error, a negated
 *   errno value is returned (see send() for the list of appropriate error
 *   values.
 *
 ****************************************************************************/
static ssize_t espcp_usrsock_sockif_send(struct socket *psock, const void *buffer, size_t len, int flags)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }
    //
    //  TODO: Make this call sendto.
    //
    // return(espcp_usrsock_sendto(psock, buffer, len, flags, NULL, 0));
    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_send_request_t *request = (espcp_send_request_t *) malloc(sizeof(espcp_send_request_t));
    if (request == NULL)
    {
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->buffer = (uint8_t *) buffer;
    request->buffer_length = len;
    request->length = len;
    request->flags = flags;

    int payload_length = espcp_send_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return(-1);
    }
    else
    {
        espcp_encode_send_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_send, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            free(payload);
            return(-ENOMEM);
        }
        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                errno = response->response_errno;
                result = response->result;
                free(response);
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_close
 *
 * Description:
 *   Performs the close operation on an USRSOCK socket instance
 *
 * Input Parameters:
 *   psock   Socket instance
 *
 * Returned Value:
 *   0 on success; -1 on error with errno set appropriately.
 *
 * Assumptions:
 *
 ****************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int espcp_usrsock_sockif_close(struct socket *psock)
{
    // struct usrsock_conn_s *conn = psock->s_conn;
    // int ret;

    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }
    espcp_usrsock_not_implemented(__func__);
    return (-1);

    /* Perform some pre-close operations for the USRSOCK socket type. */

    /* Is this the last reference to the connection structure (there
     * could be more if the socket was dup'ed).
     */

    // if (conn->crefs <= 1)
    //   {
    //     /* Yes... inform user-space daemon of socket close. */

    //     ret = espcp_usrsock_close(conn);

    //     /* Free the connection structure */

    //     conn->crefs = 0;
    //     espcp_usrsock_free(psock->s_conn);

    //     if (ret < 0)
    //       {
    //         /* Return with error code, but free resources. */

    //         nerr("ERROR: espcp_usrsock_close failed: %d\n", ret);
    //         return ret;
    //       }
    //   }
    // else
    //   {
    //     /* No.. Just decrement the reference count */

    //     conn->crefs--;
    //   }

    // return OK;
}
#pragma GCC diagnostic pop

/****************************************************************************
 * Name:  espcp_usrsock_accept
 *
 * Description:
 *   The usrsock_sockif_accept function is used with connection-based socket
 *   types (SOCK_STREAM, SOCK_SEQPACKET and SOCK_RDM). It extracts the first
 *   connection request on the queue of pending connections, creates a new
 *   connected socket with mostly the same properties as 'sockfd', and
 *   allocates a new socket descriptor for the socket, which is returned. The
 *   newly created socket is no longer in the listening state. The original
 *   socket 'sockfd' is unaffected by this call.  Per file descriptor flags
 *   are not inherited across an inet_accept.
 *
 *   The 'sockfd' argument is a socket descriptor that has been created with
 *   socket(), bound to a local address with bind(), and is listening for
 *   connections after a call to listen().
 *
 *   On return, the 'addr' structure is filled in with the address of the
 *   connecting entity. The 'addrlen' argument initially contains the size
 *   of the structure pointed to by 'addr'; on return it will contain the
 *   actual length of the address returned.
 *
 *   If no pending connections are present on the queue, and the socket is
 *   not marked as non-blocking, inet_accept blocks the caller until a
 *   connection is present. If the socket is marked non-blocking and no
 *   pending connections are present on the queue, inet_accept returns
 *   EAGAIN.
 *
 * Parameters:
 *   psock    Reference to the listening socket structure
 *   addr     Receives the address of the connecting client
 *   addrlen  Input: allocated size of 'addr', Return: returned size of 'addr'
 *   newsock  Location to return the accepted socket information.
 *
 * Returned Value:
 *   Returns 0 (OK) on success.  On failure, it returns a negated errno
 *   value.  See accept() for a description of the appropriate error value.
 *
 * Assumptions:
 *   The network is locked.
 *
 ****************************************************************************/
int espcp_usrsock_accept(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen, struct socket *newsock)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_accept_request_t *request = (espcp_accept_request_t *) malloc(sizeof(espcp_accept_request_t));
    if (request == NULL)
    {
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_accept_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
    }
    else
    {
        espcp_encode_accept_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_accept, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            free(payload);
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_accept_response_t *response = espcp_extract_accept_response(message->payload);
                if (response == NULL)
                {
                    result = -ENOMEM;
                }
                else
                {
                    result = response->result;
                    if (result > 0)
                    {
                        newsock->s_esp32_sockfd = result;
                        newsock->s_domain = psock->s_domain;
                        newsock->s_type = psock->s_type;
                        newsock->s_sockif = psock->s_sockif;
                        espcp_sock_addr_t *sockAddr = espcp_extract_sock_addr(response->addr);
                        if (sockAddr == NULL)
                        {
                            result = -ENOMEM;
                        }
                        else
                        {
                            if ((addr != NULL) && (addrlen != NULL))
                            {
                                struct sockaddr_in sai = {};
                                sai.sin_family = sockAddr->family;
                                memcpy(&sai.sin_addr, &sockAddr->ip4_address, sizeof(sai.sin_addr));
                                sai.sin_port = sockAddr->port;
                                int copyAmount = (sizeof(struct sockaddr_in) <= *addrlen) ? sizeof(struct sockaddr_in) : *addrlen;
                                memcpy(addr, &sai, copyAmount);
                                *addrlen = sizeof(struct sockaddr);
                            }
                            free(sockAddr);
                        }
                    }
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_bind
 *
 * Description:
 *   usrsock_bind() gives the socket 'conn' the local address 'addr'. 'addr'
 *   is 'addrlen' bytes long. Traditionally, this is called "assigning a name
 *   to a socket." When a socket is created with socket, it exists in a name
 *   space (address family) but has no name assigned.
 *
 * Input Parameters:
 *   conn     usrsock socket connection structure
 *   addr     Socket local address
 *   addrlen  Length of 'addr'
 *
 * Returned Value:
 *   0 on success; -1 on error with errno set appropriately
 *
 *   EACCES
 *     The address is protected, and the user is not the superuser.
 *   EADDRINUSE
 *     The given address is already in use.
 *   EINVAL
 *     The socket is already bound to an address.
 *   ENOTSOCK
 *     psock is a descriptor for a file, not a socket.
 *
 * Assumptions:
 *
 ****************************************************************************/
int espcp_usrsock_bind(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;
    struct sockaddr_in *sin = (struct sockaddr_in *) addr;

    espcp_sock_addr_t *sockAddr = (espcp_sock_addr_t *) malloc(sizeof(espcp_sock_addr_t));
    if (sockAddr == NULL)
    {
        errno = ENOMEM;
        return(-1);
    }
    sockAddr->family = sin->sin_family;
    sockAddr->port = sin->sin_port;
    memcpy(&sockAddr->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
    int encodedSockAddrSize = espcp_sock_addr_buffer_size(sockAddr);
    uint8_t *encodedSockAddr = (uint8_t *) malloc(encodedSockAddrSize);
    if (encodedSockAddr == NULL)
    {
        free(sockAddr);
        errno = ENOMEM;
        return(-1);
    }
    espcp_encode_sock_addr(sockAddr, encodedSockAddr);
    free(sockAddr);

    espcp_bind_request_t *request = (espcp_bind_request_t *) malloc(sizeof(espcp_bind_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        errno = ENOMEM;
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->addr = encodedSockAddr;
    request->addr_length = encodedSockAddrSize;

    int payload_length = espcp_bind_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        errno = ENOMEM;
        free(encodedSockAddr);
        free(request);
        return(-1);
    }
    else
    {
        espcp_encode_bind_request(request, payload);
        free(encodedSockAddr);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_bind, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            errno = ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                if (response == NULL)
                {
                    errno = ENOMEM;
                }
                else
                {
                    result = response->result;
                    errno = response->response_errno;
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_close
 *
 * Description:
 *  Closes a file descriptor, so that it no longer refers to any socket and 
 *  may be reused.
 *
 * Input Parameters:
 *   conn     usrsock socket connection structure
 * 
 * Returns:
 *  0 if successful, -1 on error and errno is set accordingly.
 * 
 *  EBADF
 *      fd isn't a valid open file descriptor.
 *  EIO
 *      An I/O error occurred.
 *  ENETDOWN
 *      Network down / not connected.
 * 
 ****************************************************************************/
int espcp_usrsock_close(struct socket *psock)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_close_request_t *request = (espcp_close_request_t *) malloc(sizeof(espcp_close_request_t));
    if (request == NULL)
    {
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_close_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
    }
    else
    {
        espcp_encode_close_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_close, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            errno = ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_integer_response_t *response = espcp_extract_integer_response(message->payload);
                if (response == NULL)
                {
                    errno = ENOMEM;
                }
                else
                {
                    result = response->result;
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_connect
 *
 * Description:
 *   Perform a usrsock connection
 *
 * Input Parameters:
 *   psock - A reference to the socket structure of the socket to be connected
 *   addr    The address of the remote server to connect to
 *   addrlen Length of address buffer
 *
 * Returned Value:
 *   0 on success, -1 on error and errno will be set accordingly.
 *
 ****************************************************************************/
int espcp_usrsock_connect(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;
    struct sockaddr_in *sin = (struct sockaddr_in *) addr;

    espcp_sock_addr_t *sockAddr = (espcp_sock_addr_t *) malloc(sizeof(espcp_sock_addr_t));
    if (sockAddr == NULL)
    {
        return(-1);
    }
    sockAddr->family = sin->sin_family;
    sockAddr->port = sin->sin_port;
    memcpy(&sockAddr->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
    int encodedSockAddrSize = espcp_sock_addr_buffer_size(sockAddr);
    uint8_t *encodedSockAddr = (uint8_t *) malloc(encodedSockAddrSize);
    if (encodedSockAddr == NULL)
    {
        free(sockAddr);
        return(-1);
    }
    espcp_encode_sock_addr(sockAddr, encodedSockAddr);
    free(sockAddr);

    espcp_connect_request_t *request = (espcp_connect_request_t *) malloc(sizeof(espcp_connect_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->addr = encodedSockAddr;
    request->addr_length = encodedSockAddrSize;

    int payload_length = espcp_connect_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(encodedSockAddr);
        free(request);
    }
    else
    {
        espcp_encode_connect_request(request, payload);
        free(encodedSockAddr);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_connect, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            errno = ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                if (response == NULL)
                {
                    errno = ENOMEM;
                }
                else
                {
                    result = response->result;
                    errno = response->response_errno;
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_getsockpeername
 *
 * Description:
 *   The getsockname() function retrieves the locally-bound name of the
 *   specified socket, stores this address in the sockaddr structure pointed
 *   to by the 'addr' argument, and stores the length of this address in the
 *   object pointed to by the 'addrlen' argument.
 *
 *   If the actual length of the address is greater than the length of the
 *   supplied sockaddr structure, the stored address will be truncated.
 *
 *   If the socket has not been bound to a local name, the value stored in
 *   the object pointed to by address is unspecified.
 *
 * Input Parameters:
 *   conn     usrsock socket connection structure
 *   addr     sockaddr structure to receive data [out]
 *   addrlen  Length of sockaddr structure [in/out]
 *   function Function to call, getsockname or getpeername.
 *
 * Returns:
 *  0 on success, -1 on failure and errno will indicate the cause of the
 *  error
 *
 ****************************************************************************/
static int espcp_usrsock_getsockpeername(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen, enum espcp_wi_fi_function function)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        set_errno(ENETDOWN);
        return(-1);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_get_sock_peer_name_request_t *request = (espcp_get_sock_peer_name_request_t *) malloc(sizeof(espcp_get_sock_peer_name_request_t));
    if (request == NULL)
    {
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_get_sock_peer_name_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return(-1);
    }
    else
    {
        espcp_encode_get_sock_peer_name_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               function, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            set_errno(ENOMEM);
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_get_sock_peer_name_response_t *response = espcp_extract_get_sock_peer_name_response(message->payload);
                if (response == NULL)
                {
                    set_errno(ENOMEM);
                }
                else
                {
                    result = response->result;
                    if (result == 0)
                    {
                        espcp_sock_addr_t *sockAddr = espcp_extract_sock_addr(response->addr);
                        if (sockAddr == NULL)
                        {
                            errno = ENOMEM;
                        }
                        else
                        {
                            if ((addr != NULL) && (addrlen != NULL))
                            {
                                struct sockaddr_in sai = {};
                                sai.sin_family = sockAddr->family;
                                memcpy(&sai.sin_addr, &sockAddr->ip4_address, sizeof(sai.sin_addr));
                                sai.sin_port = sockAddr->port;
                                int copyAmount = (sizeof(struct sockaddr_in) <= *addrlen) ? sizeof(struct sockaddr_in) : *addrlen;
                                memcpy(addr, &sai, copyAmount);
                                *addrlen = sizeof(sai);
                            }
                            free(sockAddr);
                        }
                    }
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_getpeername
 *
 * Description:
 *   The getpeername() function retrieves the remote-connected name of the
 *   specified socket, stores this address in the sockaddr structure pointed
 *   to by the 'addr' argument, and stores the length of this address in the
 *   object pointed to by the 'addrlen' argument.
 *
 *   If the actual length of the address is greater than the length of the
 *   supplied sockaddr structure, the stored address will be truncated.
 *
 *   If the socket has not been bound to a local name, the value stored in
 *   the object pointed to by address is unspecified.
 *
 * Parameters:
 *   conn     usrsock socket connection structure
 *   addr     sockaddr structure to receive data [out]
 *   addrlen  Length of sockaddr structure [in/out]
 *
 * Returns:
 *  0 on success, -1 on failure and errno will indicate the cause of the
 *  error
 *
 ****************************************************************************/
int espcp_usrsock_getpeername(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    return(espcp_usrsock_getsockpeername(psock, addr, addrlen, espcp_wi_fi_function_get_peer_name));
}

/****************************************************************************
 * Name: espcp_usrsock_getsockname
 *
 * Description:
 *   The getsockname() function retrieves the locally-bound name of the
 *   specified socket, stores this address in the sockaddr structure pointed
 *   to by the 'addr' argument, and stores the length of this address in the
 *   object pointed to by the 'addrlen' argument.
 *
 *   If the actual length of the address is greater than the length of the
 *   supplied sockaddr structure, the stored address will be truncated.
 *
 *   If the socket has not been bound to a local name, the value stored in
 *   the object pointed to by address is unspecified.
 *
 * Input Parameters:
 *   conn     usrsock socket connection structure
 *   addr     sockaddr structure to receive data [out]
 *   addrlen  Length of sockaddr structure [in/out]
 *
 * Returns:
 *  0 on success, -1 on failure and errno will indicate the cause of the
 *  error
 *
 ****************************************************************************/
int espcp_usrsock_getsockname(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    return(espcp_usrsock_getsockpeername(psock, addr, addrlen, espcp_wi_fi_function_get_sock_name));
}

/****************************************************************************
 * Name: espcp_usrsock_getsockopt
 *
 * Description:
 *   getsockopt() retrieve thse value for the option specified by the
 *   'option' argument for the socket specified by the 'psock' argument. If
 *   the size of the option value is greater than 'value_len', the value
 *   stored in the object pointed to by the 'value' argument will be silently
 *   truncated. Otherwise, the length pointed to by the 'value_len' argument
 *   will be modified to indicate the actual length of the 'value'.
 *
 *   The 'level' argument specifies the protocol level of the option. To
 *   retrieve options at the socket level, specify the level argument as
 *   SOL_SOCKET.
 *
 *   See <sys/socket.h> a complete list of values for the 'option' argument.
 *
 * Input Parameters:
 *   conn      usrsock socket connection structure
 *   level     Protocol level to set the option
 *   option    identifies the option to get
 *   value     Points to the argument value
 *   value_len The length of the argument value
 *
 * Returns:
 *  0 on success, -1 on failure and errno will indicate the cause of the
 *  error
 *
 ****************************************************************************/
int espcp_usrsock_getsockopt(struct socket *psock, int level, int option,
                             void *value, socklen_t *value_len)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    espcp_usrsock_not_implemented(__func__);
    return(-1);
}

/****************************************************************************
 * Name: espcp_usrsock_ioctl
 *
 * Description:
 *   The usrsock_ioctl() function performs network device specific operations.
 *
 * Parameters:
 *   psock      A pointer to a NuttX-specific, internal socket structure
 *   cmd        The ioctl command
 *   arg        The argument of the ioctl cmd
 *   arglen     Number of bytes 
 * 
 * Returns:
 *  0 on success, -1 on failure and errno will indicate the cause of the
 *  error
 *
 ****************************************************************************/
int espcp_usrsock_ioctl(struct socket *psock, int cmd, void *arg, size_t arglen)
{
    int result = 0;
    espcp_message_t *message = NULL;

    errno = 0;
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    if (arg != NULL)
    {
        struct ifconf *ifc = (struct ifconf *) arg;
        struct ifreq *ifr;

        if ((cmd == SIOCGIFCONF) && (ifc->ifc_req == NULL))
        {
            ifc->ifc_len = sizeof(struct ifreq);
        }
        else
        {
            espcp_ioctl_request_t *request = (espcp_ioctl_request_t *) malloc(sizeof(espcp_ioctl_request_t));
            if (request == NULL)
            {
                errno = ENOMEM;
                return(-1);
            }
            request->command = cmd;

            int payload_length = espcp_ioctl_request_buffer_size(request);
            uint8_t *payload = (uint8_t *) malloc(payload_length);
            if (payload == NULL)
            {
                free(request);
            }
            else
            {
                espcp_encode_ioctl_request(request, payload);
                free(request);

                message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                    espcp_wi_fi_function_ioctl, espcp_status_codes_completed_ok,
                                                    espcp_get_next_message_id(), payload, payload_length);
                if (message == NULL)
                {
                    free(payload);
                    errno = ENOMEM;
                    return(-1);
                }
                if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
                {
                    espcp_ioctl_response_t *response = espcp_extract_ioctl_response(message->payload);
                    if (response != NULL)
                    {
                        switch (cmd)
                        {
                            case SIOCGIFCONF:
                                ifc = (struct ifconf *) arg;
                                if (arglen < (sizeof(struct ifconf)))
                                {
                                    ifc->ifc_len = 0;
                                }
                                else
                                {
                                    ifc->ifc_len = sizeof(struct ifreq);
                                    ifr = ifc->ifc_req;
                                    strcpy(ifr->ifr_name, "wlan0");
                                    struct sockaddr_in sai;
                                    sai.sin_family = AF_INET;
                                    sai.sin_port = 0;
                                    espcp_sock_addr_t *sockAddr = espcp_extract_sock_addr(response->addr);
                                    if (sockAddr == NULL)
                                    {
                                        errno = ENOMEM;
                                    }
                                    else
                                    {
                                        sai.sin_addr.s_addr = sockAddr->ip4_address;
                                        free(sockAddr);
                                        memcpy(&ifr->ifr_ifru.ifru_addr, &sai, sizeof(struct sockaddr));
                                    }
                                }
                                break;
                            case SIOCGIFFLAGS:
                                ifr = (struct ifreq *) arg;
                                strcpy(ifr->ifr_name, "wlan0");
                                ifr->ifr_flags = response->flags;
                                break;
                            default:
                                syslog(LOG_CRIT, "%s@%d Unknown ioctl command %08x.\n", _thisFile, __LINE__, cmd);
                                errno = EINVAL;
                                result = -1;
                                break;
                        }
                        free(response);
                    }
                    else
                    {
                        errno = EINVAL;
                        result = -1;
                    }
                }
            }
        }
    }
    else
    {
        errno = EINVAL;
        result = -1;
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_listen
 *
 * Description:
 *   To accept connections, a socket is first created with psock_socket(), a
 *   willingness to accept incoming connections and a queue limit for
 *   incoming connections are specified with psock_listen(), and then the
 *   connections are accepted with psock_accept().  For the case of AFINET
 *   and AFINET6 sockets, psock_listen() calls this function.  The
 *   psock_listen() call applies only to sockets of type SOCK_STREAM or
 *   SOCK_SEQPACKET.
 *
 * Parameters:
 *   psock    Reference to an internal, bound socket structure.
 *   backlog  The maximum length the queue of pending connections may grow.
 *            If a connection request arrives with the queue full, the client
 *            may receive an error with an indication of ECONNREFUSED or,
 *            if the underlying protocol supports retransmission, the request
 *            may be ignored so that retries succeed.
 *
 * Returned Value:
 *   On success, zero is returned. On error, a negated errno value is
 *   returned.  See list() for the set of appropriate error values.
 *
 ****************************************************************************/
int espcp_usrsock_listen(struct socket *psock, int backlog)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_listen_request_t *request = (espcp_listen_request_t *) malloc(sizeof(espcp_listen_request_t));
    if (request == NULL)
    {
        errno = -ENOMEM;
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->back_log = backlog;

    int payload_length = espcp_listen_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        errno = -ENOMEM;
        free(request);
    }
    else
    {
        espcp_encode_listen_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_listen, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            errno = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                if (response == NULL)
                {
                    errno = -ENOMEM;
                }
                else
                {
                    result = response->result;
                    errno = response->response_errno;
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_poll_setup
 *
 * Description:
 *   Setup a poll request passing the request information to the ESP32.
 *   to this function.
 *
 * Input Parameters:
 *   psock - An instance of the internal socket structure.
 *   fds   - The structure describing the events to be monitored.
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int espcp_usrsock_poll_setup(struct socket *psock, struct pollfd *fds)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int result = 0;

    espcp_poll_request_t *request = (espcp_poll_request_t *) malloc(sizeof(espcp_poll_request_t));
    if (request == NULL)
    {
        return (-ENOMEM);
    }
    memset(request, 0, sizeof(espcp_poll_request_t));
    request->socket_handle = psock->s_esp32_sockfd;
    request->events = fds->events;
    request->timeout = -1;
    request->setup = 1;

    int payload_length = espcp_poll_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return (-ENOMEM);
    }
    espcp_encode_poll_request(request, payload);
    free(request);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                            espcp_wi_fi_function_poll,
                                                            espcp_status_codes_completed_ok,
                                                            espcp_get_next_message_id(), payload, payload_length);       
    if (message == NULL)
    {
        free(payload);
        return (-ENOMEM);
    }

    espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) malloc(sizeof(espcp_poll_request_t));
    if (pr == NULL)
    {
        espcp_delete_message_and_payload(message);
        return (-ENOMEM);
    }
    else
    {
        pr->fd = fds;
        pr->request_id = message->message_id;
        sem_wait(&_espcp_poll_requests_mutex);
        gl_add_item_to_head(_espcp_poll_requests, pr);
        sem_post(&_espcp_poll_requests_mutex);

        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                result = response->result;
                if (result < 0)
                {
                    errno = response->response_errno;
                }
                free(response);
            }
        }
        else
        {
            sem_wait(&_espcp_poll_requests_mutex);
            gl_remove_item(_espcp_poll_requests, message->message_id, espcp_usrsock_poll_request_compare_message_id);
            sem_post(&_espcp_poll_requests_mutex);
            result = -EFAULT;
        }
    }

    espcp_delete_message_and_payload(message);
    return(result);
}
#pragma GCC diagnostic pop

/****************************************************************************
 * Name: espcp_usrsock_poll_teardown
 *
 * Description:
 *  Teardown a a poll request setup with a previous call to
 *  espcp_usrsock_poll_setup
 *
 * Input Parameters:
 *   psock - An instance of the internal socket structure.
 *   fds   - The structure describing the events to be monitored.
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 
 ****************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int espcp_usrsock_poll_teardown(struct socket *psock, struct pollfd *fds)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int result = 0;

    sem_wait(&_espcp_poll_requests_mutex);
    espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) gl_remove_item(_espcp_poll_requests, 
                                                (uint32_t) fds, espcp_usrsock_poll_request_compare_fd_pointer);
    sem_post(&_espcp_poll_requests_mutex);
    if (pr == NULL)
    {
        result = -EFAULT;
    }
    else
    {
        espcp_poll_request_t *request = (espcp_poll_request_t *) malloc(sizeof(espcp_poll_request_t));
        if (request == NULL)
        {
            free(pr);
            return (-ENOMEM);
        }
        memset(request, 0, sizeof(espcp_poll_request_t));
        request->socket_handle = psock->s_esp32_sockfd;
        request->setup = 0;
        request->setup_message_id = pr->request_id;
        free(pr);

        int payload_length = espcp_poll_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            free(request);
            return (-ENOMEM);
        }
        espcp_encode_poll_request(request, payload);
        free(request);

        espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                                espcp_wi_fi_function_poll,
                                                                espcp_status_codes_completed_ok,
                                                                espcp_get_next_message_id(), payload, payload_length);       
        if (message == NULL)
        {
            free(payload);
            return (-ENOMEM);
        }

        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                result = response->result;
                if (result < 0)
                {
                    errno = response->response_errno;
                }
                free(response);
            }
        }

        espcp_delete_message_and_payload(message);
    }

    return(result);
}
#pragma GCC diagnostic pop

/****************************************************************************
 * Name: espcp_usrsock_poll_interrupt_handler
 *
 * Description:
 *   This interrupt handler will be called when the poll request on the ESP32
 *   has a return value.
 *
 * Input Parameters:
 *   message - Message from the ESP32 with the result of the poll request.
 *
 ****************************************************************************/
void espcp_usrsock_poll_interrupt_handler(espcp_message_t *message)
{
    espcp_interrupt_poll_response_t *ipr = espcp_extract_interrupt_poll_response(message->payload);
    if (ipr != NULL)
    {
        sem_wait(&_espcp_poll_requests_mutex);
        espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) gl_remove_item(_espcp_poll_requests, 
                                                    ipr->setup_message_id, espcp_usrsock_poll_request_compare_message_id);
        sem_post(&_espcp_poll_requests_mutex);
        if (pr != NULL)
        {
            pr->fd->revents = ipr->returned_events;
            errno = ipr->response_errno;
            nxsem_post(pr->fd->sem);
            free(pr);
        }
        free(ipr);
        espcp_delete_message_and_payload(message);
    }
}

/****************************************************************************
 * Name: espcp_usrsock_direct_poll
 *
 * Description:
 *   Setup a poll request passing the request information to the ESP32.
 *   to this function.
 *
 * Input Parameters:
 *   psock - An instance of the internal socket structure.
 *   fds   - The structure describing the events to be monitored.
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int espcp_usrsock_direct_poll(struct socket *psock, struct pollfd *fds)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int result = 0;

    espcp_poll_request_t *request = (espcp_poll_request_t *) malloc(sizeof(espcp_poll_request_t));
    if (request == NULL)
    {
        return (-ENOMEM);
    }
    memset(request, 0, sizeof(espcp_poll_request_t));
    request->socket_handle = psock->s_esp32_sockfd;
    request->events = fds->events;
    request->timeout = 5000;
    request->setup = 2;             /* Temporary magic number */

    int payload_length = espcp_poll_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return (-ENOMEM);
    }
    espcp_encode_poll_request(request, payload);
    free(request);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                            espcp_wi_fi_function_poll,
                                                            espcp_status_codes_completed_ok,
                                                            espcp_get_next_message_id(), payload, payload_length);       
    if (message == NULL)
    {
        free(payload);
        return (-ENOMEM);
    }

    if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
    {
        espcp_poll_response_t *response = espcp_extract_poll_response(message->payload);
        if (response == NULL)
        {
            result = -ENOMEM;
        }
        else
        {
            result = response->result;
            if (result < 0)
            {
                errno = response->response_errno;
            }
            fds->revents = response->returned_events;
            if (fds->revents != 0)
            {
                nxsem_post(fds->sem);
            }
            free(response);
        }
    }

    espcp_delete_message_and_payload(message);
    return(result);
}
#pragma GCC diagnostic pop

/****************************************************************************
 * Name: espcp_usrsock_poll
 *
 * Description:
 *   The standard poll() operation redirects operations on socket descriptors
 *   to this function.
 *
 * Input Parameters:
 *   psock - An instance of the internal socket structure.
 *   fds   - The structure describing the events to be monitored.
 *   setup - true: Setup up the poll; false: Teardown the poll
 *
 * Returned Value:
 *  0: Success; Negated errno on failure
 *
 ****************************************************************************/
int espcp_usrsock_poll(struct socket *psock, struct pollfd *fds, bool setup)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    int result = 0;
    // static int pollCount = 0;

    // syslog(LOG_CRIT, "%s@%d %s has been called.\n", _thisFile, __LINE__, __func__);
    // if (fds != NULL)
    // {
    //     syslog(LOG_CRIT, "%s@%d poll event number: %d, request events %d.\n", _thisFile, __LINE__, pollCount++, fds->events);
    // }
    // else
    // {
    //     syslog(LOG_CRIT, "%s@%d fds is null.\n", _thisFile, __LINE__);
    // }

    errno = 0;
    if (setup)
    {
        // result = espcp_usrsock_poll_setup(psock, fds);
        // result = espcp_usrsock_direct_poll(psock, fds);
        if (fds->events == 0)
        {
            usleep(10000);
        }
        fds->revents = fds->events;
        nxsem_post(fds->sem);
    }
    else
    {
        // result = espcp_usrsock_poll_teardown(psock, fds);
    }
    // result = 0;
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_recvfrom
 *
 * Description:
 *   recvfrom() receives messages from a socket, and may be used to receive
 *   data on a socket whether or not it is connection-oriented.
 *
 *   If from is not NULL, and the underlying protocol provides the source
 *   address, this source address is filled in. The argument fromlen
 *   initialized to the size of the buffer associated with from, and modified
 *   on return to indicate the actual size of the address stored there.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   buf      Buffer to receive data
 *   len      Length of buffer
 *   flags    Receive flags (ignored)
 *   from     Address of source (may be NULL)
 *   fromlen  The length of the address structure
 * 
 * Returns:
 *  0 on success, -1 on error and errno will be set accordingly.
 *
 ****************************************************************************/
ssize_t espcp_usrsock_recvfrom(struct socket *psock, void *buffer, size_t len,
                               int flags, struct sockaddr *from, socklen_t *fromlen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    espcp_recv_from_request_t *request = (espcp_recv_from_request_t *) malloc(sizeof(espcp_recv_from_request_t));
    if (request == NULL)
    {
        errno = ENOMEM;
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->length = len;
    request->flags = flags;
    request->get_source_address = (from != NULL);

    int32_t result = -1;
    espcp_message_t *message = NULL;
    void *nextBlock = buffer;
    int totalAmount = 0;
    int amountRemaining = len;
    bool gettingData = true;
    while (gettingData)
    {
        if (message != NULL)
        {
            espcp_delete_message_and_payload(message);
        }

        request->length = (amountRemaining > MAXIMUM_READ_WRITE_BUFFER_SIZE) ? MAXIMUM_READ_WRITE_BUFFER_SIZE : amountRemaining;
        int payload_length = espcp_recv_from_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            free(request);
            errno = ENOMEM;
            return(-1);
        }
        espcp_encode_recv_from_request(request, payload);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_recv_from, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            free(payload);
            errno = ENOMEM;
            gettingData = false;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_recv_from_response_t *response = espcp_extract_recv_from_response(message->payload);
                if (response == NULL)
                {
                    free(payload);
                    errno = ENOMEM;
                    gettingData = false;
                }
                else
                {
                    errno = response->response_errno;
                    result = response->result;

                    int amount = 0;
                    if (result > 0)
                    {
                        if ((totalAmount == 0) && (from != NULL))       /* We only do this the first time. */
                        {
                            espcp_sock_addr_t *sa = espcp_extract_sock_addr(response->source_address);
                            if (sa == NULL)
                            {
                                free(payload);
                                errno = ENOMEM;
                                gettingData = false;
                            }
                            else
                            {
                                struct sockaddr_in sin;
                                sin.sin_family = sa->family;
                                sin.sin_port = sa->port;
                                memcpy(&sin.sin_addr, &sa->ip4_address, sizeof(sin.sin_addr));
                                if (*fromlen > (sizeof(struct sockaddr_in)))
                                {                                                
                                    amount = sizeof(struct sockaddr);
                                }
                                else
                                {
                                    amount = *fromlen;
                                }
                                *fromlen = amount;
                                memcpy(from, &sin, amount);
                                free(sa);
                                request->get_source_address = false;
                            }
                        }
                        if (gettingData)    // Could have been set to false in the above condition indicating an error.
                        {
                            if (amountRemaining > response->result)
                            {
                                amount = response->result;
                            }
                            else
                            {
                                amount = amountRemaining;
                            }
                            memcpy(nextBlock, response->buffer, amount);
                            totalAmount += amount;
                            amountRemaining -= amount;
                            nextBlock += amount;
                            gettingData = ((amountRemaining > 0) && (request->length == result));
                            free(response->buffer);
                            response->buffer = NULL;
                            result = totalAmount;
                        }
                    }
                    else
                    {
                        gettingData = false;
                    }
                    if (response->buffer != NULL)
                    {
                        free(response->buffer);
                    }
                    free(response);
                }
            }
        }
    }

    free(request);

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_sendto
 *
 * Description:
 *   If sendto() is used on a connection-mode (SOCK_STREAM, SOCK_SEQPACKET)
 *   socket, the parameters to and 'tolen' are ignored (and the error EISCONN
 *   may be returned when they are not NULL and 0), and the error ENOTCONN is
 *   returned when the socket was not actually connected.
 *
 * Input Parameters:
 *   psock    A pointer to a NuttX-specific, internal socket structure
 *   buffer   Data to send
 *   len      Length of data to send
 *   flags    Send flags (ignored)
 *   to       Address of recipient
 *   tolen    The length of the address structure
 * 
 * Returns:
 *  0 on success, -1 on failure and errno will be set accordingly.
 *
 ****************************************************************************/
ssize_t espcp_usrsock_sendto(struct socket *psock, const void *buffer,
                             size_t len, int flags, const struct sockaddr *to,
                             socklen_t tolen)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    espcp_sock_addr_t *sa;
    uint8_t *encodedSockAddr;
    int encodedSockAddrLen;
    if (to != NULL)
    {
        sa = (espcp_sock_addr_t *) malloc(sizeof(espcp_sock_addr_t));
        if (sa == NULL)
        {
            errno = ENOMEM;
            return(-1);
        }
        //
        //  TODO: Make this deal with send requests where the buffer is > 4000 bytes.
        //
        struct sockaddr_in *sin = (struct sockaddr_in *) to;
        sa->family = sin->sin_family;
        sa->port = sin->sin_port;
        memcpy(&sa->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
        encodedSockAddr = (uint8_t *) malloc(espcp_sock_addr_buffer_size(sa));
        if (encodedSockAddr == NULL)
        {
            free(sa);
            errno = ENOMEM;
            return(-1);
        }
        espcp_encode_sock_addr(sa, encodedSockAddr);
        encodedSockAddrLen = espcp_sock_addr_buffer_size(sa);
        free(sa); 
    }
    else
    {
        encodedSockAddr = NULL;
        encodedSockAddrLen = 0;
    }

    espcp_send_to_request_t *request = (espcp_send_to_request_t *) malloc(sizeof(espcp_send_to_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        errno = ENOMEM;
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->flags = flags;
    request->destination_address_length = encodedSockAddrLen;
    request->destination_address = encodedSockAddr;

    void *nextBlock = (void *) buffer;
    int totalAmount = 0;
    int amountRemaining = len;
    bool sendingData = true;
    int32_t result = -1;
    espcp_message_t *message = NULL;
    while (sendingData)
    {
        if (message != NULL)
        {
            espcp_delete_message_and_payload(message);
        }

        request->length = (amountRemaining > MAXIMUM_READ_WRITE_BUFFER_SIZE) ? MAXIMUM_READ_WRITE_BUFFER_SIZE : amountRemaining;
        request->buffer_length = request->length;
        request->buffer = nextBlock;
        int payload_length = espcp_send_to_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            if (encodedSockAddr != NULL)
            {
                free(encodedSockAddr);
            }
            errno = ENOMEM;
            free(request);
        }
        else
        {
            espcp_encode_send_to_request(request, payload);

            message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                espcp_wi_fi_function_send_to, espcp_status_codes_completed_ok,
                                                espcp_get_next_message_id(), payload, payload_length);
            if (message == NULL)
            {
                free(payload);
                free(request);
                errno = ENOMEM;
            }
            else
            {
                if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
                {
                    espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                    if (response == NULL)
                    {
                        free(payload);
                        free(request);
                        free(message);
                        errno = ENOMEM;
                    }
                    else
                    {
                        errno = response->response_errno;
                        result = response->result;

                        if (result > 0)
                        {
                            int amount = (amountRemaining > response->result) ? response->result : amountRemaining;
                            totalAmount += amount;
                            amountRemaining -= amount;
                            nextBlock += amount;
                            sendingData = ((amountRemaining > 0) && (request->length == result));
                            result = totalAmount;
                        }
                        else
                        {
                            sendingData = false;
                        }
                        free(response);
                    }
                }
            }
        }
    }

    if (encodedSockAddr != NULL)
    {
        free(encodedSockAddr);
    }
    free(request);

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_setsockopt
 *
 * Description:
 *   psock_setsockopt() sets the option specified by the 'option' argument,
 *   at the protocol level specified by the 'level' argument, to the value
 *   pointed to by the 'value' argument for the socket on the 'psock'
 *   argument.
 *
 *   The 'level' argument specifies the protocol level of the option. To set
 *   options at the socket level, specify the level argument as SOL_SOCKET.
 *
 *   See <sys/socket.h> a complete list of values for the 'option' argument.
 * 
 *   According to the ESP32 documentation, the following socket option types
 *   are NOT supported:
 *      - SoDebug
 *      - SoDontRoute
 *      - SoUseLoopBack
 *      - SoOobInline
 *      - SoReusePort
 *      - SoSndBuf
 *      - SoSndLoWat
 *      - SoRcvLoWat
 * 
 * Input Parameters:
 *   psock     usrsock socket connection structure
 *   level     Protocol level to set the option
 *   option    identifies the option to set
 *   value     Points to the argument value
 *   value_len The length of the argument value
 * 
 * Returns:
 *  0 on success, -1 on failure and errno will be set accordingly.
 *
 ****************************************************************************/
int espcp_usrsock_setsockopt(struct socket *psock, int level, int option,
                             const void *value, socklen_t value_len)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    espcp_set_sock_opt_request_t *request = (espcp_set_sock_opt_request_t *) malloc(sizeof(espcp_set_sock_opt_request_t));
    if (request == NULL)
    {
        errno = ENOMEM;
        return(-1);
    }
    memset(request, 0, sizeof(espcp_set_sock_opt_request_t));
    espcp_time_val_t *tv;
    bool processRequest = true;
    switch (option)
    {
        case SO_SNDTIMEO:
        case SO_RCVTIMEO:
            tv = (espcp_time_val_t *) malloc(sizeof(espcp_time_val_t));
            if (tv == NULL)
            {
                free(request);
                errno = ENOMEM;
                return (-1);
            }
            memset(tv, 0, sizeof(espcp_time_val_t));
            struct timeval *ov = (struct timeval *) value;
            tv->tv_sec = ov->tv_sec;
            tv->tv_usec = ov->tv_usec;
            request->option_value_length = espcp_time_val_buffer_size(tv);
            request->option_value = (uint8_t *) malloc(request->option_value_length);
            if (request->option_value != NULL)
            {
                espcp_encode_time_val(tv, request->option_value);
                free(tv);
                request->option_len = 0;    /* Calculated by the ESP32 code. */
            }
            else
            {
                free(request);
                errno = ENOMEM;
                return (-1);
            }
            break;
        case SO_OOBINLINE:
        case SO_SNDBUF:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
            processRequest = false;     // Above options are not supported.
            break;
        default:
            request->option_value_length = value_len;
            request->option_value = (uint8_t *) malloc(value_len);
            if (request->option_value != NULL)
            {
                memcpy(request->option_value, value, value_len);
            }
            else
            {
                free(request);
                errno = ENOMEM;
                return (-1);
            }
            break;
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;
    if (processRequest)
    {
        request->socket_handle = psock->s_esp32_sockfd;
        request->level = level;
        request->option_name = option;

        int payload_length = espcp_set_sock_opt_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            free(request->option_value);
            free(request);
            errno = ENOMEM;
        }
        else
        {
            espcp_encode_set_sock_opt_request(request, payload);
            free(request->option_value);
            free(request);

            message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                espcp_wi_fi_function_set_sock_opt, espcp_status_codes_completed_ok,
                                                espcp_get_next_message_id(), payload, payload_length);
            if (message == NULL)
            {
                free(payload);
                errno = ENOMEM;
            }
            else
            {
                if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
                {
                    espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                    if (response == NULL)
                    {
                        errno = ENOMEM;
                        result = -1;
                    }
                    else
                    {
                        errno = response->response_errno;
                        result = response->result;
                        if (errno == ENOPROTOOPT)
                        {
                            errno = 0;
                            result = 0;
                        }
                        free(response);
                    }
                }
            }
        }
    }
    else
    {
        //
        //  For non-supported options, pretend we have succeeded.
        //
        errno = 0;
        result = 0;
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_socket
 *
 * Description:
 *   socket() creates an endpoint for communication and returns a socket
 *   structure.
 *
 * Input Parameters:
 *   domain   - (see sys/socket.h)
 *   type     - (see sys/socket.h)
 *   protocol - (see sys/socket.h)
 *   psock    - A pointer to a user allocated socket structure to be
 *              initialized.
 *
 * Returned Value:
 *   0 on success; negative error-code on error
 *
 *   EACCES
 *     Permission to create a socket of the specified type and/or protocol
 *     is denied.
 *   EAFNOSUPPORT
 *     The implementation does not support the specified address family.
 *   EINVAL
 *     Unknown protocol, or protocol family not available.
 *   EMFILE
 *     Process file table overflow.
 *   ENFILE
 *     The system limit on the total number of open files has been reached.
 *   ENOBUFS or ENOMEM
 *     Insufficient memory is available. The socket cannot be created until
 *     sufficient resources are freed.
 *   EPROTONOSUPPORT
 *     The protocol type or the specified protocol is not supported within
 *     this domain.
 *
 * Assumptions:
 *
 ****************************************************************************/
int espcp_usrsock_socket(int domain, int type, int protocol, struct socket *psock)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    int result = -1;

    espcp_socket_request_t *request = (espcp_socket_request_t *) malloc(sizeof(espcp_socket_request_t));
    if (request == NULL)
    {
        return (-ENOMEM);
    }
    memset(request, 0, sizeof(espcp_socket_request_t));
    request->domain = domain;
    request->type = type;
    request->protocol = protocol;

    int payload_length = espcp_socket_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return (-ENOMEM);
    }
    espcp_encode_socket_request(request, payload);
    free(request);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                            espcp_wi_fi_function_socket,
                                                            espcp_status_codes_completed_ok,
                                                            espcp_get_next_message_id(), payload, payload_length);
    if (message == NULL)
    {
        free(payload);
        return (-ENOMEM);
    }
    else
    {
        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            espcp_integer_response_t *response = espcp_extract_integer_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                result = response->result;
                free(response);
                psock->s_domain = domain;
                psock->s_type = type;
                psock->s_esp32_sockfd = result;
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_read
 *
 * Description:
 *  Read the specified number of bytes from the socket and place them in the
 *  buffer.
 * 
 *  This method instructs the ESP32 to call the read method which will in
 *  turn call the equivalent LWIP method.
 * 
 * See:
 *  http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga822040573319cf87bfe6758d511be57f
 * 
 * Input Parameters:
 * 
 *  psock - Pointer to the socket structure to read from
 *  buffer - Buffer to hold the results of the read
 *  count - Size of the buffer.
 *
 * Returned Value:
 *  If successful, the number of bytes read from the socket, -1 otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int32_t espcp_usrsock_read(struct socket *psock, const void *buffer, size_t count)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        errno = ENETDOWN;
        return(-1);
    }

    if ((buffer == NULL) || (count > MAXIMUM_READ_WRITE_BUFFER_SIZE))
    {
        return (-1);
    }

    espcp_read_request_t *request = (espcp_read_request_t *) malloc(sizeof(espcp_read_request_t));
    if (request == NULL)
    {
        errno = ENOMEM;
        return(-1);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->count = count;

    int payload_length = espcp_read_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) malloc(payload_length);
    int32_t result = -1;
    espcp_message_t *message = NULL;
    errno = 0;
    if (payload == NULL)
    {
        free(request);
        errno = ENOMEM;
    }
    else
    {
        espcp_encode_read_request(request, payload);
        free(request);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_read, espcp_status_codes_completed_ok,
                                               espcp_get_next_message_id(), payload, payload_length);

        if (message == NULL)
        {
            errno = ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                espcp_read_response_t *response = espcp_extract_read_response(message->payload);
                if (response == NULL)
                {
                    errno = ENOMEM;
                }
                else
                {
                    if (response->buffer_length > 0)
                    {
                        memcpy((void *) buffer, response->buffer, response->buffer_length);
                        free(response->buffer);
                    }
                    errno = response->read_response_errno;
                    result = response->read_response_result;
                    free(response);
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return (result);
}

/****************************************************************************
 * Name: espcp_usrsock_dup2
 *
 * Description:
 *  TODO: Implement this method and complete the comment header.
 * 
 * Input Parameters:
 * 
 *  old_psock - Pointer to the socket to duplicate.
 *  new_psock - Pointer to the duplicated socket.
 *
 * Returned Value:
 *  
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_usrsock_dup2(struct socket *old_psock, struct socket *new_psock)
{
  espcp_usrsock_not_implemented(__func__);
  return(-1);
}

/****************************************************************************
 * Name: espcp_usrsock_sendmsg
 *
 * Description:
 *  TODO: Implement this method and complete the comment header.
 * 
 * Input Parameters:
 * 
 *  psock - Pointer to the socket structure to read from
 *  msg - Message to send.
 *  flags - Flags
 *
 * Returned Value:
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
size_t espcp_usrsock_sendmsg(struct socket *psock, const struct msghdr *msg, int flags)
{
  espcp_usrsock_not_implemented(__func__);
  return(0);
}

/****************************************************************************
 * Name: espcp_usrsock_shutdown
 *
 * Description:
 *  TODO: Implement this method and complete the comment header.
 * 
 * Input Parameters:
 * 
 *  psock - Pointer to the socket structure to read from
 *  how - Indicate how the socket should be shutdown.
 *
 * Returned Value:
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_usrsock_shutdown(struct socket *psock, int how)
{
  espcp_usrsock_not_implemented(__func__);
  return(-1);
}

/****************************************************************************
 * Name: espcp_usrsock_recvmsg
 *
 * Description:
 *  TODO: Implement this method and complete the comment header.
 * 
 * Input Parameters:
 * 
 *  psock - Pointer to the socket structure to read from
 *  msg - Pointer to a block of memory that will receive the message.
 *  flags - 
 *
 * Returned Value:
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
size_t espcp_usrsock_recvmsg(struct socket *psock, struct msghdr *msg, int flags)
{
  espcp_usrsock_not_implemented(__func__);
  return(0);
}

/****************************************************************************
 * Name: espcp_usrsock_getaddrinfo
 *
 * Description:
 *  TODO: Implement this method and complete the comment header.
 * 
 * Input Parameters:
 * 
 *
 * Returned Value:
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_usrsock_getaddrinfo(const char *hostname, const char *servname,
                const struct addrinfo *hint, struct addrinfo **res)
{
    espcp_usrsock_not_implemented(__func__);
    return(-1);    
}

/****************************************************************************
 * Name: espcp_usrsock_not_implemented
 *
 * Description:
 *  Output a message to the logs indicating that a method that is not 
 *  implemented has been called.
 * 
 * Input Parameters:
 * 
 *  source - Method that has been called.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_usrsock_not_implemented(const char *source)
{
    syslog(LOG_CRIT, "%s@%d %s not implemented.\n", _thisFile, __LINE__, source);
}
