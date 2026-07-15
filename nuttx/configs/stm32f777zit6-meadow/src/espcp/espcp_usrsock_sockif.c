/****************************************************************************
 * espcp_usrsock_sockif.c
 *
 *   Copyright (C) 2019-21 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
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
#include <errno.h>
#include <debug.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <nuttx/net/net.h>
#include <nuttx/net/ioctl.h>
#include <poll.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <nuttx/arch.h>

#include "espcp_usrsock.h"

/* _SF_NONBLOCK / _SS_ISNONBLOCK live in NuttX's private net/socket/socket.h, which
 * is not on the board-src include path. Mirror them (bit 3, value 0x08) so we can
 * honor the socket's non-blocking state. Guarded so it composes if that header is
 * ever pulled in. Keep in sync with net/socket/socket.h. */
#ifndef _SF_NONBLOCK
#  define _SF_NONBLOCK 0x08
#endif
#ifndef _SS_ISNONBLOCK
#  define _SS_ISNONBLOCK(s) (((s) & _SF_NONBLOCK) != 0)
#endif
#include "espcp_common.h"
#include "espcp_coprocessor.h"
#include "generic_list.h"
#include "espcp_event_handlers.h"
#include "espcp_message_dispatcher.h"

//
//  Define USE_MEADOW_DEBUG_HELPERS to enable Meadow debug tracing in this file.
//

// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//
//  Temporarily undefine NDEBUG to be able to use assertions in this file.
//
// #undef NDEBUG
#include <assert.h>

#include <nuttx/clock.h>
#include <meadow/meadow_watchdog.h>


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

/**
 *  @brief Hold information about a poll request that is active on the ESP32.
 */
struct espcp_poll_request_list_item_s
{
    struct pollfd *fd;      /* Pointer to the pollfd structure of the original request. */
    struct socket *psock;   /* Socket the poll is armed on (readiness-cache updates). */
    int32_t esp_sockfd;     /* ESP socket handle at arm time: struct socket slots are
                             * reused after close, so a late interrupt must only touch
                             * the cache if the slot still belongs to this socket. */
    uint32_t armed_tick;    /* clock_systimer() at arm time.  Poll lifecycles are
                             * ~100ms; any interrupt for an entry armed long ago is a
                             * straggler whose pollfd memory may have been recycled --
                             * writing through it corrupts the current owner (observed:
                             * user-heap corruption faulting in mbedTLS bignum). */
    uint32_t request_id;    /* ID of the message sent to the ESP32. */
};
typedef struct espcp_poll_request_list_item_s espcp_poll_request_list_item_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int espcp_usrsock_sockif_setup(struct socket *psock, int protocol);

static sockcaps_t espcp_usrsock_sockif_sockcaps(struct socket *psock);

static void espcp_usrsock_sockif_addref(struct socket *psock);

/****************************************************************************
 * Public Data
 ****************************************************************************/

/**
 *  @brief Table of function pointers for the ESP32 networking methods.
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
    espcp_usrsock_send,               /* si_send */
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
 * Name: espcp_lock_poll_requests_queue
 *
 * Description:
 *   Lock the poll requests queue.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 ****************************************************************************/
static inline void espcp_lock_poll_requests_queue(void)
{
    //
    //  Ideally we would use the line below (commented out) but for now we will
    //  use the block of code following this in order to see if we are getting an interrupt
    //  signal when we are waiting for the semaphore.  This may be causing the poll
    //  timeout issue.
    //
    // while ((sem_wait(&_espcp_poll_requests_mutex) != 0) && (get_errno() == EINTR));
    while (sem_wait(&_espcp_poll_requests_mutex) != 0)
    {
        int error = get_errno();
        if (error != EINTR)
        {
            syslog(LOG_INFO, "Error %d waiting for poll requests mutex\n", error);
            break;
        }
        else
        {
            syslog(LOG_INFO, "Interrupted waiting for poll requests mutex, retrying mutex\n");
        }
    }
}

/****************************************************************************
 * Name: espcp_unlock_poll_requests_queue
 *
 * Description:
 *   Unlock the poll requests queue.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 ****************************************************************************/
static inline void espcp_unlock_poll_requests_queue(void)
{
    sem_post(&_espcp_poll_requests_mutex);
}

/****************************************************************************
 * Name: espcp_unlock_poll_requests_queue
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
    return((int) key == ((espcp_poll_request_list_item_t *) item)->fd->fd);
}

/****************************************************************************
 * Name: espcp_sock_addr_to_sockaddr
 *
 * Description:
 *  Convert an espcp sock_addr structure into a Nuttx sockaddr structure.
 *
 * Parameters:
 *  destination - Pointer to a block of memory used to hold the sockaddr
 *                data.
 *  sockAddr - Pointer to the encoded espcp_sock_addr_t object holding the
 *             datafrom the ESP32.
 *
 * Returns:
 *  None.
 *
 ****************************************************************************/
static int espcp_sock_addr_to_sockaddr(void *destination, uint8_t *source)
{
    int result = OK;
    if ((destination == NULL) || (source == NULL))
    {
        result = -EFAULT;
    }
    else
    {
        espcp_sock_addr_t *sai = espcp_extract_sock_addr(source);
        struct sockaddr_in *dest = (struct sockaddr_in *) destination;
        memset(dest, 0, sizeof(struct sockaddr_in));
        dest->sin_family = AF_INET;
        dest->sin_port = sai->port;
        dest->sin_addr.s_addr = sai->ip4_address;
    }
    return(result);
}

/****************************************************************************
 * Name: espcp_usrsock_sockif_setup
 *
 * Description:
 *  Perform system wide usrsock initialisation for the ESP32 usrsock layer.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
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

    return (espcp_usrsock_socket(domain, type, protocol, psock));
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
 * Name: espcp_usrsock_send
 *
 * Description:
 *   The espcp_usrsock_send() call may be used only when the socket is in
 *   a connected state  (so that the intended recipient is known).
 *
 * Input Parameters:
 *   psock    An instance of the internal socket structure.
 *   buf      Data to send
 *   len      Length of data to send
 *   flags    Send flags (ignored)
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On error, a negated
 *   errno value is returned (see send() for the list of appropriate error
 *   values.
 *
 ****************************************************************************/
ssize_t espcp_usrsock_send(struct socket *psock, const void *buffer, size_t len, int flags)
{
    MEADOW_TRACE_INFORMATION("send(%d, 0x%08x, %d, %d)\n", psock->s_esp32_sockfd, (uint32_t) buffer, len, flags);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("send - result ENETDOWN\n");
        return(-ENETDOWN);
    }
    //
    //  According to: https://man7.org/linux/man-pages/man2/send.2.html
    //
    //  send is equivalent to sendto with the two default parameters added at the
    //  end of the parameter list.
    //
    MEADOW_TRACE_INFORMATION("Passing on to sendto\n");
    return(espcp_usrsock_sendto(psock, buffer, len, flags, NULL, 0));
}

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
    MEADOW_TRACE_INFORMATION("accept(%d, 0x%08x, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, (uint32_t) addr, (uint32_t) addrlen, (uint32_t) newsock);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("accept - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_accept_request_t *request = (espcp_accept_request_t *) zalloc(sizeof(espcp_accept_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("accept - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_accept_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        result = -ENOMEM;
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
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
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
                                else
                                {
                                    result = -response->response_errno;
                                }
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("accept - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *   0 on success, a negated errno is returned on error.
 *
 *   EACCES
 *     The address is protected, and the user is not the superuser.
 *   EADDRINUSE
 *     The given address is already in use.
 *   EINVAL
 *     The socket is already bound to an address.
 *   ENOTSOCK
 *     psock is a descriptor for a file, not a socket.
 *   ENETDOWN
 *     Network not started.
 *
 * Assumptions:
 *
 ****************************************************************************/
int espcp_usrsock_bind(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    MEADOW_TRACE_INFORMATION("bind(%d, 0x%08x, %d)\n", psock->s_esp32_sockfd, (uint32_t) addr, addrlen);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("bind - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;
    struct sockaddr_in *sin = (struct sockaddr_in *) addr;

    espcp_sock_addr_t *sockAddr = (espcp_sock_addr_t *) zalloc(sizeof(espcp_sock_addr_t));
    if (sockAddr == NULL)
    {
        MEADOW_TRACE_DEBUG("bind - result ENOMEM\n");
        return(-ENOMEM);
    }
    sockAddr->family = sin->sin_family;
    sockAddr->port = sin->sin_port;
    memcpy(&sockAddr->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
    int encodedSockAddrSize = espcp_sock_addr_buffer_size(sockAddr);
    uint8_t *encodedSockAddr = (uint8_t *) zalloc(encodedSockAddrSize);
    if (encodedSockAddr == NULL)
    {
        free(sockAddr);
        MEADOW_TRACE_DEBUG("bind - result ENOMEM\n");
        return(-ENOMEM);
    }
    espcp_encode_sock_addr(sockAddr, encodedSockAddr);
    free(sockAddr);

    espcp_bind_request_t *request = (espcp_bind_request_t *) zalloc(sizeof(espcp_bind_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        MEADOW_TRACE_DEBUG("bind - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->addr = encodedSockAddr;
    request->addr_length = encodedSockAddrSize;

    int payload_length = espcp_bind_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(encodedSockAddr);
        free(request);
        MEADOW_TRACE_DEBUG("bind - result ENOMEM\n");
        return(-ENOMEM);
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = -response->response_errno;
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("bind - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 if successful, negated errno on error.
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
    MEADOW_TRACE_INFORMATION("close(%d)\n", psock->s_esp32_sockfd);

    struct wdog_s g_watchdog_close;
    meadow_watchdog_activate(&g_watchdog_close, WATCHDOG_CLOSE_TIMEOUT_MILLISECONDS, CLOSE_WATCHDOG);

    if (espcp_get_configuration()->esp_not_responding)
    {
        meadow_watchdog_deactivate(&g_watchdog_close);
        MEADOW_TRACE_DEBUG("close - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_close_request_t *request = (espcp_close_request_t *) zalloc(sizeof(espcp_close_request_t));
    if (request == NULL)
    {
        meadow_watchdog_deactivate(&g_watchdog_close);
        MEADOW_TRACE_DEBUG("close - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_close_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        result = -ENOMEM;
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
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
                                    result = -response->response_errno;
                                }
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);
    meadow_watchdog_deactivate(&g_watchdog_close);

    MEADOW_TRACE_INFORMATION("close - socket %d, result %d\n", psock->s_esp32_sockfd, result);


    psock->s_esp32_state = 0;
    psock->s_esp32_sockfd = -1;   /* invalidate: blocks stale poll interrupts from
                                   * matching this slot after reuse */

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
 *   0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_connect(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    MEADOW_TRACE_INFORMATION("connect(%d, 0x%08x, %d)\n", psock->s_esp32_sockfd, (uint32_t) addr, addrlen);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("connect - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;
    struct sockaddr_in *sin = (struct sockaddr_in *) addr;

    espcp_sock_addr_t *sockAddr = (espcp_sock_addr_t *) zalloc(sizeof(espcp_sock_addr_t));
    if (sockAddr == NULL)
    {
        MEADOW_TRACE_DEBUG("connect - result -1 (sockAddr is NULL)\n");
        return(-1);
    }
    sockAddr->family = sin->sin_family;
    sockAddr->port = sin->sin_port;
    memcpy(&sockAddr->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
    int encodedSockAddrSize = espcp_sock_addr_buffer_size(sockAddr);
    uint8_t *encodedSockAddr = (uint8_t *) zalloc(encodedSockAddrSize);
    if (encodedSockAddr == NULL)
    {
        free(sockAddr);
        MEADOW_TRACE_DEBUG("connect - result ENOMEM\n");
        return(-ENOMEM);
    }
    espcp_encode_sock_addr(sockAddr, encodedSockAddr);
    free(sockAddr);

    espcp_connect_request_t *request = (espcp_connect_request_t *) zalloc(sizeof(espcp_connect_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        MEADOW_TRACE_DEBUG("connect - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->addr = encodedSockAddr;
    request->addr_length = encodedSockAddrSize;

    int payload_length = espcp_connect_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(encodedSockAddr);
        free(request);
        MEADOW_TRACE_DEBUG("connect - result ENOMEM\n");
        return(-ENOMEM);
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = -response->response_errno;
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    /* Track non-blocking connect: while EINPROGRESS is pending, POLLOUT
     * means "connect completed" and must come from the ESP, so the local
     * POLLOUT fast path in poll_setup is suspended for this socket.
     */
    if (result == -EINPROGRESS)
    {
        psock->s_esp32_state |= ESP32_SF_CONNECT_INPROGRESS;
    }
    else if (result == 0)
    {
        psock->s_esp32_state &= ~ESP32_SF_CONNECT_INPROGRESS;
    }


    MEADOW_TRACE_INFORMATION("connect - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
static int espcp_usrsock_getsockpeername(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen, enum espcp_wi_fi_function function)
{
    MEADOW_TRACE_INFORMATION("getsockpeername - socket %d\n", psock->s_esp32_sockfd);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("getsockpeername - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_get_sock_peer_name_request_t *request = (espcp_get_sock_peer_name_request_t *) zalloc(sizeof(espcp_get_sock_peer_name_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("getsockpeername - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;

    int payload_length = espcp_get_sock_peer_name_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        MEADOW_TRACE_DEBUG("getsockpeername - result ENOMEM\n");
        return(-ENOMEM);
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_get_sock_peer_name_response_t *response = espcp_extract_get_sock_peer_name_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = response->result;
                                if (result == 0)
                                {
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
                                            *addrlen = sizeof(sai);
                                        }
                                        free(sockAddr);
                                    }
                                }
                                else
                                {
                                    result = -response->response_errno;
                                }
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("getsockpeername - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_getpeername(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    MEADOW_TRACE_INFORMATION("getpeername(%d, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, (uint32_t) addr, (uint32_t) addrlen);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("getpeername - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int result = espcp_usrsock_getsockpeername(psock, addr, addrlen, espcp_wi_fi_function_get_peer_name);

    MEADOW_TRACE_INFORMATION("getpeername - socket %d result %d\n", psock->s_esp32_sockfd, result);

    return(result);
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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_getsockname(struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    MEADOW_TRACE_INFORMATION("getsockname(%d, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, (uint32_t) addr, (uint32_t) addrlen);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("getsockname - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int result = espcp_usrsock_getsockpeername(psock, addr, addrlen, espcp_wi_fi_function_get_sock_name);

    MEADOW_TRACE_INFORMATION("getsockname - socket %d result %d\n", psock->s_esp32_sockfd, result);

    return(result);
}

/****************************************************************************
 * Name: espcp_usrsock_send_ioctl_to_esp
 *
 * Description:
 *  The simple cases have been taken care of so we now head over to the ESP32
 *  and let it perform the ioctl request.
 *
 * Parameters:
 *   psock      A pointer to a NuttX-specific, internal socket structure
 *   cmd        The ioctl command
 *   arg        The argument of the ioctl cmd
 *   arglen     Number of bytes
 *
 * Returns:
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
static int espcp_usrsock_send_ioctl_to_esp(struct socket *psock, int cmd, void *arg, size_t arglen)
{
    int result = 0;
    espcp_ioctl_request_t *request = (espcp_ioctl_request_t *) zalloc(sizeof(espcp_ioctl_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("espcp_usrsock_send_ioctl_to_esp - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->command = cmd;
    struct ifconf *ifc = (struct ifconf *) arg;
    struct lifreq *lifr = (struct lifreq *) arg;

    /* SIOCSESPNONBLOCK is the one *per-socket* ioctl: it ships the ESP socket
     * handle plus an int (1=non-blocking, 0=blocking) so the ESP can flip
     * O_NONBLOCK on the matching lwip socket.  Build its 12-byte payload inline
     * -- [command][socket_handle][nonblock] -- so the generated, single-field
     * espcp_ioctl_request_t and its encoder stay untouched; every other ioctl
     * keeps the bare 4-byte command wire. */
    int payload_length;
    uint8_t *payload;
    if (cmd == SIOCSESPNONBLOCK)
    {
        int nonblock = (arg != NULL && arglen >= sizeof(int)) ? *(const int *) arg : 0;
        payload_length = 12;
        payload = (uint8_t *) zalloc(payload_length);
        if (payload != NULL)
        {
            espcp_encode_int32(cmd, payload);
            espcp_encode_int32(psock->s_esp32_sockfd, payload + 4);
            espcp_encode_int32(nonblock, payload + 8);
        }
    }
    else
    {
        payload_length = espcp_ioctl_request_buffer_size(request);
        payload = (uint8_t *) zalloc(payload_length);
        if (payload != NULL)
        {
            espcp_encode_ioctl_request(request, payload);
        }
    }

    if (payload == NULL)
    {
        free(request);
        MEADOW_TRACE_DEBUG("espcp_usrsock_send_ioctl_to_esp - result ENOMEM\n");
        return(-ENETDOWN);
    }
    else
    {
        free(request);

        espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                            espcp_wi_fi_function_ioctl, espcp_status_codes_completed_ok,
                                            espcp_get_next_message_id(), payload, payload_length);
        if (message == NULL)
        {
            free(payload);
            MEADOW_TRACE_DEBUG("espcp_usrsock_send_ioctl_to_esp - result ENOMEM\n");
            return(-ENOMEM);
        }
        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            switch (message->status_code)
            {
                case espcp_status_codes_completed_ok:
                    {
                        espcp_ioctl_response_t *response = espcp_extract_ioctl_response(message->payload);
                        if (response != NULL)
                        {
                            if (response->result != -1)
                            {
                                struct sockaddr sa;
                                memset(&sa, 0, sizeof(struct sockaddr));
                                sa.sa_family = AF_INET;
                                switch (cmd)
                                {
                                    case SIOCGIFCONF:
                                        if (arglen < (sizeof(struct ifconf)))
                                        {
                                            ifc->ifc_len = 0;
                                        }
                                        else
                                        {
                                            ifc->ifc_len = sizeof(struct ifreq);
                                            struct ifreq *ifr = ifc->ifc_req;
                                            result = espcp_sock_addr_to_sockaddr((void *) &ifr->ifr_ifru.ifru_addr, response->addr);
                                        }
                                        break;
                                    case SIOCGIFADDR:       /* Get IP address */
                                    case SIOCGIFNETMASK:    /* Get network mask */
                                        //
                                        //  This relies upon the fact that the ifru_addr and ifru_netmask are in a union.
                                        //
                                        result = espcp_sock_addr_to_sockaddr((void *) &lifr->lifr_ifru.lifru_addr, response->addr);
                                        break;
                                    case SIOCGIFHWADDR:     /* Get hardware address */
                                        memset(&lifr->lifr_ifru.lifru_hwaddr, 0, sizeof(&lifr->lifr_ifru.lifru_hwaddr));
                                        lifr->lifr_ifru.lifru_hwaddr.sa_family = AF_INET;
                                        memcpy((void *) &lifr->lifr_ifru.lifru_hwaddr.sa_data, (void *) response->addr, MEADOW_MAC_ADDRESS_SIZE);
                                        // memcpy((void *) &lifr->lifr_ifru.lifru_hwaddr, (void *) , sizeof(sa));
                                        break;
                                    case SIOCGIFFLAGS:
                                        lifr->lifr_flags = response->flags;
                                        lifr->lifr_flags |= IFF_WIFI;
                                        break;
                                    default:
                                        MEADOW_TRACE_CRITICAL("%s@%d Unknown ioctl command %08x.\n", _thisFile, __LINE__, cmd);
                                        result = -EINVAL;
                                        break;
                                }
                            }
                            else
                            {
                                result = -response->response_errno;
                            }
                            free(response);
                        }
                        else
                        {
                            result = -EINVAL;
                        }
                    }
                    break;
                case espcp_status_codes_thread_pool_is_full:
                case espcp_status_codes_esp_out_of_memory:
                    result = -ENOMEM;
                    break;
                default:
                    result = -1;
                    break;
            }
        }
        espcp_delete_message_and_payload(message);
    }
    return(result);
}

/****************************************************************************
 * Name: espcp_usrsock_ioctl
 *
 * Description:
 *  The usrsock_ioctl() function performs network device specific operations.
 *
 * Parameters:
 *   psock      A pointer to a NuttX-specific, internal socket structure
 *   cmd        The ioctl command
 *   arg        The argument of the ioctl cmd
 *   arglen     Number of bytes
 *
 * Returns:
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_ioctl(struct socket *psock, int cmd, void *arg, size_t arglen)
{
    MEADOW_TRACE_INFORMATION("ioctl(%d, %d, 0x%08x, %d)\n", psock->s_esp32_sockfd, cmd, (uint32_t) arg, arglen);

    int result = 0;

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("ioctl - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    if (arg != NULL)
    {
        // struct ifconf *ifc = (struct ifconf *) arg;
        struct lifreq *lifr = (struct lifreq *) arg;
        switch (cmd)
        {
            // case SIOCGIFCONF:
            //     if (ifc->ifc_req == NULL)
            //     {
            //         ifc->ifc_len = sizeof(struct lifreq);
            //     }
            //     break;
            case SIOCGIFNAME:
                if (lifr->lifr_ifindex > 1)
                {
                    result = -ENOTTY;
                }
                else
                {
                    strcpy(lifr->lifr_name, "wlan0");
                }
                break;
            case SIOCGIFBRDADDR:    /* Get broadcast IP address */
            case SIOCGIFDSTADDR:    /* Get P-to-P address */
                //
                //  The sa structure has been filled with zeroes so the address will be 0.0.0.0.
                //
                memset((void *) &lifr->lifr_ifru.lifru_dstaddr, 0, sizeof(struct sockaddr));
                break;
            default:
                result = espcp_usrsock_send_ioctl_to_esp(psock, cmd, arg, arglen);
                break;
        }
    }
    else
    {
        result = -EINVAL;
    }

    MEADOW_TRACE_INFORMATION("ioctl - socket %d result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_listen(struct socket *psock, int backlog)
{
    MEADOW_TRACE_INFORMATION("listen(%d, %d)\n", psock->s_esp32_sockfd, backlog);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("listen - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int32_t result = -1;
    espcp_message_t *message = NULL;

    espcp_listen_request_t *request = (espcp_listen_request_t *) zalloc(sizeof(espcp_listen_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("listen - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->back_log = backlog;

    int payload_length = espcp_listen_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        result = -ENOMEM;
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = -response->response_errno;
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("listen - socket %d result %d\n", psock->s_esp32_sockfd, result);

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
 *   psock - Pointer to the structure holding information about the socket.
 *   fds   - The structure describing the events to be monitored.
 *
 * Returned Value:
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
static int espcp_usrsock_poll_setup(struct socket *psock, struct pollfd *fds)
{
    if (espcp_get_configuration()->esp_not_responding)
    {
        return(-ENETDOWN);
    }

    /* Fast path: answer the poll locally, without a coprocessor round-trip,
     * whenever the F7 already knows a requested event is pending.
     *
     * Why this exists: .NET's poll-based SocketAsyncEngine delivers events
     * only from a 0-timeout "Phase A" sample over all registered sockets
     * (see WaitForSocketEventsInner in pal_networking.c).  A wire poll can
     * never complete its ~10ms ESP round-trip inside a 0-timeout window, so
     * readiness must be answerable locally:
     *
     *   - POLLOUT: a connected TCP (or any UDP) socket is always writable.
     *     Answer locally except while writability is genuinely unknown
     *     (non-blocking connect in progress, or send buffer full after an
     *     EAGAIN) -- then only the ESP knows, so fall through to the wire.
     *   - POLLIN/POLLHUP/POLLERR: answered from the readiness cache
     *     (ESP32_SF_RD_*), which is fed by wire-poll interrupts (the blocking
     *     "Phase B" polls arm those) and drained back to unknown when a recv
     *     returns EAGAIN.
     *
     * Every wire POLLOUT round-trip this avoids matters: with truly
     * non-blocking sockets (MEADOW_BRIDGE_NONBLOCK) a TLS handshake measured
     * ~150 polls per send/recv pair, enough espcp load to starve reads and
     * drop the WiFi association.
     */
    /* MEADOW_POLL_LOCAL_READY: answer polls locally from the readiness cache.
     * REQUIRED whenever the strict poll-teardown (remove pr before the wire
     * call) is in place: the old teardown's race window was what delivered
     * readiness to .NET's 0-timeout Phase-A polls; the cache is its safe
     * replacement.  Stale-interrupt cache poisoning on reused socket slots is
     * prevented by the esp_sockfd guard in the poll interrupt handler.
     */
#define MEADOW_POLL_LOCAL_READY 1
#ifdef MEADOW_POLL_LOCAL_READY
    short ready = 0;

    if ((fds->events & POLLOUT) != 0 &&
        ((psock->s_esp32_state & (ESP32_SF_CONNECT_INPROGRESS |
                                  ESP32_SF_SEND_EAGAIN)) == 0 ||
         (psock->s_esp32_state & ESP32_SF_WR_READY) != 0))
    {
        ready |= POLLOUT;
    }
    if ((fds->events & POLLIN) != 0 &&
        (psock->s_esp32_state & ESP32_SF_RD_READY) != 0)
    {
        ready |= POLLIN;
    }
    if ((psock->s_esp32_state & ESP32_SF_RD_HUP) != 0)
    {
        ready |= POLLHUP;
    }
    if ((psock->s_esp32_state & ESP32_SF_RD_ERR) != 0)
    {
        ready |= POLLERR;
    }

    if (ready != 0)
    {
        fds->revents |= ready;
        nxsem_post(fds->sem);
        return(0);
    }
#endif /* MEADOW_POLL_LOCAL_READY */

    int result = 0;

    espcp_poll_request_t *request = (espcp_poll_request_t *) zalloc(sizeof(espcp_poll_request_t));
    if (request == NULL)
    {
        return (-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->events = fds->events;
    /* Bound the ESP-side worker hold time.  A hardened ESP honours this and
     * self-completes the poll (revents=0) if the teardown poke is ever lost,
     * releasing its worker; the legacy ESP ignores the field (poll(...,-1)),
     * which is the historical behaviour.  Normal lifecycle tears polls down
     * within ~100ms, so this only bounds the orphan case.
     */
    request->timeout = 60000;
    request->setup = 1;
    request->setup_message_id = espcp_get_next_message_id();

    int payload_length = espcp_poll_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        return (-ENOMEM);
    }
    espcp_encode_poll_request(request, payload);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                                            espcp_wi_fi_function_poll,
                                                            espcp_status_codes_completed_ok,
                                                            request->setup_message_id, payload, payload_length);
    free(request);
    if (message == NULL)
    {
        free(payload);
        return (-ENOMEM);
    }

    espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) zalloc(sizeof(espcp_poll_request_list_item_t));
    if (pr == NULL)
    {
        espcp_delete_message_and_payload(message);
        return (-ENOMEM);
    }
    else
    {
        MEADOW_TRACE_INFORMATION("poll setup - Setting up poll request ID %08x, socket %d\n", request->setup_message_id, psock->s_esp32_sockfd);
        pr->fd = fds;
        pr->psock = psock;
        pr->esp_sockfd = psock->s_esp32_sockfd;
        pr->armed_tick = clock_systimer();
        pr->request_id = message->message_id;
        espcp_lock_poll_requests_queue();
        gl_add_item_to_head(_espcp_poll_requests, pr);
        espcp_unlock_poll_requests_queue();

        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            DEBUGASSERT(message->payload != NULL);
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                if (response->result < 0)
                {
                    result = -response->response_errno;
                }
                free(response);
            }
        }
        else
        {
            espcp_lock_poll_requests_queue();
            gl_remove_item(_espcp_poll_requests, message->message_id, espcp_usrsock_poll_request_compare_message_id);
            espcp_unlock_poll_requests_queue();
            if (message->status_code == espcp_status_codes_esp_out_of_memory)
            {
                result = -ENOMEM;
            }
            else
            {
                result = -EFAULT;
            }
        }
    }

    espcp_delete_message_and_payload(message);
    return(result);
}

/****************************************************************************
 * Name: espcp_usrsock_poll_teardown
 *
 * Description:
 *  Teardown a a poll request setup with a previous call to
 *  espcp_usrsock_poll_setup
 *
 * Input Parameters:
 *   psock - Pointer to the structure holding information about the socket.
 *   fds   - The structure describing the events to be monitored.
 *
 * Returned Value:
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
static int espcp_usrsock_poll_teardown(struct socket *psock, struct pollfd *fds)
{
    MEADOW_TRACE_INFORMATION("poll teardown\n");

    int result = 0;

    /* CRITICAL INVARIANT: the list item (pr) holds pointers to the caller's
     * pollfd (often heap memory freed right after poll() returns) and must
     * NEVER outlive this call.  Remove it from the list FIRST, on every
     * path -- a leaked pr lets a late ESP interrupt_poll_response write
     * revents / post a semaphore through dangling pointers into freed
     * memory (observed: kernel heap free-list corruption -> hard fault in
     * mm_malloc).  If we then fail to send the wire teardown, the ESP-side
     * poll stays armed and its eventual interrupt is dropped as an
     * unmatched request ID (benign, logged as NOMATCH).
     */

    espcp_lock_poll_requests_queue();
    espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) gl_find_item(_espcp_poll_requests,
                                                (uint32_t) fds->fd, espcp_usrsock_poll_request_compare_fd_pointer);
    if (pr != NULL)
    {
        gl_remove_item(_espcp_poll_requests, (uint32_t) fds->fd, espcp_usrsock_poll_request_compare_fd_pointer);
    }
    espcp_unlock_poll_requests_queue();

    if (pr == NULL)
    {
        //
        //  The request could have been removed from the queue by the interrupt handler so we treat this as a success.
        //
        MEADOW_TRACE_INFORMATION("Poll teardown - Cannot find poll request for socket %d\n", psock->s_esp32_sockfd);
    }
    else if (espcp_get_configuration()->esp_not_responding)
    {
        free(pr);
        return(-ENETDOWN);
    }
    else
    {
        uint32_t teardown_request_id = pr->request_id;
        free(pr);

        espcp_poll_request_t *request = (espcp_poll_request_t *) zalloc(sizeof(espcp_poll_request_t));
        if (request == NULL)
        {
            return (-ENOMEM);
        }
        request->socket_handle = psock->s_esp32_sockfd;
        request->setup = 0;
        request->setup_message_id = teardown_request_id;

        int payload_length = espcp_poll_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) zalloc(payload_length);
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

        /* A lost teardown orphans the ESP-side armed poll: its worker sits in
         * lwip poll(...,-1) until the polled socket closes, and enough orphans
         * exhaust the ESP worker pool (everything then fails ThreadPoolIsFull).
         * Retry the wire teardown a few times before giving up.
         */
        espcp_status_codes_t sp_qres = espcp_status_codes_failure;
        for (int sp_try = 0; sp_try < 3; sp_try++)
        {
            sp_qres = espcp_queue_message(message, true);
            if (sp_qres == espcp_status_codes_completed_ok)
            {
                break;
            }
            usleep(10000);
        }

        if (sp_qres == espcp_status_codes_completed_ok)
        {
            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
            if (response == NULL)
            {
                result = -ENOMEM;
            }
            else
            {
                if (response->result < 0)
                {
                    result = -response->response_errno;
                }
                free(response);
            }
        }

        espcp_delete_message_and_payload(message);
    }

    MEADOW_TRACE_INFORMATION("poll teardown - exit\n");

    return(result);
}

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
    MEADOW_TRACE_INFORMATION("poll interrupt handler - enter\n");
    espcp_interrupt_poll_response_t *ipr = espcp_extract_interrupt_poll_response(message->payload);
    uint32_t request_id = 0;
    if (ipr != NULL)
    {
        request_id = ipr->setup_message_id;
        espcp_lock_poll_requests_queue();
        espcp_poll_request_list_item_t *pr = (espcp_poll_request_list_item_t *) gl_find_item(_espcp_poll_requests,
                                                    request_id, espcp_usrsock_poll_request_compare_message_id);
        if (pr != NULL)
        {
            MEADOW_TRACE_INFORMATION("poll interrupt handler - found originating request %08x\n", request_id);
            /* Feed the F7-local readiness cache so 0-timeout polls (.NET's
             * Phase-A sample) can be answered locally: a wire poll cannot
             * complete its ESP round-trip inside a 0-timeout window.  RD_READY
             * is cleared when a recv drains to EAGAIN; HUP/ERR stick until
             * close.
             *
             * Guard against socket-slot reuse: if the socket this poll was
             * armed on has since closed (slot recycled -- s_esp32_sockfd
             * changed or cleared), a late interrupt must NOT poison the new
             * socket's cache (a stale HUP/ERR makes every poll on the fresh
             * connection report failure -> "Unknown socket error" on all new
             * connections).
             */
            /* Age gate FIRST: a healthy poll lives ~100ms between arm and
             * interrupt/teardown.  A delivery for an entry armed >10s ago is
             * by definition a straggler whose pollfd (heap) memory may have
             * been reallocated -- pointer range checks cannot prove liveness
             * for recycled heap, so do not touch it at all.
             */
            uint32_t sp_age = clock_systimer() - pr->armed_tick;
            if (sp_age > SEC2TICK(10))
            {
                syslog(LOG_ERR, "espcp: STALE-BY-AGE poll entry id=%08x age=%lus espfd=%ld rev=%02x\n",
                       (unsigned int)request_id, (unsigned long)(sp_age / TICK_PER_SEC),
                       (long)pr->esp_sockfd, ipr->returned_events);
                gl_remove_item(_espcp_poll_requests, request_id, espcp_usrsock_poll_request_compare_message_id);
                free(pr);
                espcp_unlock_poll_requests_queue();
                free(ipr);
                espcp_delete_message_and_payload(message);
                return;
            }

            uintptr_t sp_ps = (uintptr_t) pr->psock;
            bool sp_ps_ok = (sp_ps >= 0x20000000 && sp_ps < 0x20080000);
            if (sp_ps_ok && pr->psock->s_esp32_sockfd == pr->esp_sockfd)
            {
                if (ipr->returned_events & POLLIN)
                {
                    pr->psock->s_esp32_state |= ESP32_SF_RD_READY;
                }
                if (ipr->returned_events & POLLOUT)
                {
                    /* Makes connect-completion (EINPROGRESS -> writable) and
                     * send-buffer drain visible to 0-timeout polls; without
                     * this the completion only reaches the blocking poll,
                     * whose caller re-samples instead of delivering, and
                     * non-blocking connect livelocks. */
                    pr->psock->s_esp32_state |= ESP32_SF_WR_READY;
                }
                if (ipr->returned_events & POLLHUP)
                {
                    pr->psock->s_esp32_state |= ESP32_SF_RD_HUP;
                }
                if (ipr->returned_events & POLLERR)
                {
                    pr->psock->s_esp32_state |= ESP32_SF_RD_ERR;
                }
            }

            /* Deliver one-shot and CONSUME the entry: once delivered there is
             * no further legitimate use of this pr (teardown already treats a
             * missing entry as success), and consuming it here closes every
             * stale-entry window for late interrupts (the hardened ESP fires
             * poll timeouts up to 60s after arming).
             *
             * Defensively validate the pollfd/semaphore pointers before
             * touching them: a stale entry dereferenced here was observed as
             * a hard fault in nxsem_post (sem=0x0e4c0012).  If validation
             * fails, log everything -- that log line identifies the path
             * that leaked the entry.
             */
            uintptr_t sp_fd  = (uintptr_t) pr->fd;
            uintptr_t sp_sem = (pr->fd != NULL) ? (uintptr_t) pr->fd->sem : 0;
            bool sp_fd_ok  = (sp_fd  >= 0x20000000 && sp_fd  < 0x20080000) ||
                             (sp_fd  >= 0xC0000000 && sp_fd  < 0xC2000000);
            bool sp_sem_ok = sp_fd_ok &&
                             ((sp_sem >= 0x20000000 && sp_sem < 0x20080000) ||
                              (sp_sem >= 0xC0000000 && sp_sem < 0xC2000000));
            if (sp_sem_ok)
            {
                pr->fd->revents = ipr->returned_events;
                MEADOW_TRACE_INFORMATION("poll interrupt handler - fd=%d events=%hd revents=%hd\n", pr->fd->fd, pr->fd->events, pr->fd->revents);
                nxsem_post(pr->fd->sem);
            }
            else
            {
                syslog(LOG_ERR, "espcp: STALE poll entry id=%08x pr=%p fd=%p sem=%p psock=%p espfd=%ld rev=%02x\n",
                       (unsigned int)request_id, pr, (void *)sp_fd, (void *)sp_sem,
                       pr->psock, (long)pr->esp_sockfd, ipr->returned_events);
            }

            gl_remove_item(_espcp_poll_requests, request_id, espcp_usrsock_poll_request_compare_message_id);
            free(pr);
        }
        else
        {
            MEADOW_TRACE_INFORMATION("poll interrupt handler - Cannot find request %08x\n", request_id);
        }
        espcp_unlock_poll_requests_queue();
        free(ipr);
    }
    espcp_delete_message_and_payload(message);
    MEADOW_TRACE_INFORMATION("poll interrupt handler - exit, request ID: %08x\n", request_id);
}

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_poll(struct socket *psock, struct pollfd *fds, bool setup)
{
    MEADOW_TRACE_INFORMATION("poll(%d, 0x%08x, %d)\n", psock->s_esp32_sockfd, (uint32_t) fds, setup ? 1 : 0);

    struct wdog_s g_watchdog_poll;
    meadow_watchdog_activate(&g_watchdog_poll, WATCHDOG_POLL_TIMEOUT_MILLISECONDS, POLL_WATCHDOG);

    /* Teardown must ALWAYS run its local list cleanup, even when the ESP is
     * unresponsive -- skipping it leaves a dangling pollfd pointer in the
     * request list (use-after-free when a late interrupt matches it).  The
     * teardown path handles the esp_not_responding case itself after the
     * local cleanup.
     */
    if (setup && espcp_get_configuration()->esp_not_responding)
    {
        meadow_watchdog_deactivate(&g_watchdog_poll);
        MEADOW_TRACE_DEBUG("poll - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int result = 0;
    if (setup)
    {
        result = espcp_usrsock_poll_setup(psock, fds);
    }
    else
    {
        result = espcp_usrsock_poll_teardown(psock, fds);
        MEADOW_TRACE_INFORMATION("poll - teardown returned %d\n", result);
    }

    meadow_watchdog_deactivate(&g_watchdog_poll);
    MEADOW_TRACE_INFORMATION("poll - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
ssize_t espcp_usrsock_recvfrom(struct socket *psock, void *buffer, size_t len,
                               int flags, struct sockaddr *from, socklen_t *fromlen)
{
    MEADOW_TRACE_INFORMATION("recvfrom(%d, 0x%08x, %d, %d, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, (uint32_t) buffer, len, flags, (uint32_t) from, (uint32_t) fromlen);


    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("recvfrom - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    espcp_recv_from_request_t *request = (espcp_recv_from_request_t *) zalloc(sizeof(espcp_recv_from_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("recvfrom - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    if (len > ESPCP_MAXIMUM_PAYLOAD_SIZE)
    {
        len = ESPCP_MAXIMUM_PAYLOAD_SIZE;
    }
    request->length = len;
    request->flags = flags;
    /* .NET sets the socket O_NONBLOCK and drives readiness via poll(); without this
     * the ESP recvfrom() blocks server-side until data arrives (observed: a single
     * read blocked 28s), which holds a thread-pool thread hostage and starves the
     * MQTT keepalive PINGREQ so the broker drops the connection. Forward MSG_DONTWAIT
     * (0x0040, mapped to lwip MSG_DONTWAIT on the ESP) for non-blocking sockets so the
     * ESP returns EAGAIN immediately; the managed SocketAsyncEngine then waits on the
     * poll path (poll_setup/poll_interrupt_handler) without holding a thread. */
    if (_SS_ISNONBLOCK(psock->s_flags))
    {
        request->flags |= MSG_DONTWAIT;
    }
    request->get_source_address = (from != NULL);

    int payload_length = espcp_recv_from_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        MEADOW_TRACE_DEBUG("recvfrom - result ENOMEM\n");
        return(-ENOMEM);
    }
    espcp_encode_recv_from_request(request, payload);
    free(request);

    espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                            espcp_wi_fi_function_recv_from, espcp_status_codes_completed_ok,
                                            espcp_get_next_message_id(), payload, payload_length);
    int32_t result = -1;
    if (message == NULL)
    {
        free(payload);
        result = -ENOMEM;
    }
    else
    {
        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            switch (message->status_code)
            {
                case espcp_status_codes_completed_ok:
                    {
                        espcp_recv_from_response_t *response = espcp_extract_recv_from_response(message->payload);
                        if (response == NULL)
                        {
                            MEADOW_TRACE_DEBUG("recvfrom - result ENOMEM\n");
                            result = -ENOMEM;       // Message and payload deleted at the end of the method.
                        }
                        else
                        {
                            if (response->result > 0)
                            {
                                if (from != NULL)
                                {
                                    espcp_sock_addr_t *sa = espcp_extract_sock_addr(response->source_address);
                                    if (sa == NULL)
                                    {
                                        MEADOW_TRACE_DEBUG("recvfrom - result ENOMEM\n");
                                        result = -ENOMEM;   // Message and payload deleted at the end of the method.
                                    }
                                    else
                                    {
                                        struct sockaddr_in sin;
                                        sin.sin_family = sa->family;
                                        sin.sin_port = sa->port;
                                        memcpy(&sin.sin_addr, &sa->ip4_address, sizeof(sin.sin_addr));
                                        if (*fromlen > (sizeof(struct sockaddr_in)))
                                        {
                                            *fromlen = sizeof(struct sockaddr);
                                        }
                                        memcpy(from, &sin, *fromlen);
                                        free(sa);
                                    }
                                }
                                result = response->result;
                                if (response->result > len)
                                {
                                    result = len;
                                }
                                memcpy(buffer, response->buffer, result);   // response->buffer freed below.
                            }
                            else
                            {
                                result = -response->response_errno;
                            }
                            free(response->buffer);
                            free(response);
                        }
                    }
                    break;
                case espcp_status_codes_thread_pool_is_full:
                case espcp_status_codes_esp_out_of_memory:
                    result = -ENOMEM;
                    break;
                default:
                    // No need for a default action here as result is set to -1.
                    break;
            }
        }
    }

    espcp_delete_message_and_payload(message);

    /* Data flowing proves the socket is connected. */
    if (result >= 0)
    {
        psock->s_esp32_state &= ~ESP32_SF_CONNECT_INPROGRESS;
    }

    /* Readiness cache drain: EAGAIN means the ESP-side receive buffer is
     * empty again, so "readable" is unknown until the next wire-poll
     * interrupt reports it.
     */
    if (result == -EAGAIN)
    {
        psock->s_esp32_state &= ~ESP32_SF_RD_READY;
    }



    MEADOW_TRACE_INFORMATION("recvfrom - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
ssize_t espcp_usrsock_sendto(struct socket *psock, const void *buffer,
                             size_t len, int flags, const struct sockaddr *to,
                             socklen_t tolen)
{
    MEADOW_TRACE_INFORMATION("sendto(%d, 0x%08x, %d, %d, 0x%08x, %d)\n", psock->s_esp32_sockfd, (uint32_t) buffer, len, flags, (uint32_t) to, tolen);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("sendto - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    espcp_sock_addr_t *sa;
    uint8_t *encodedSockAddr;
    int encodedSockAddrLen;
    if (to != NULL)
    {
        sa = (espcp_sock_addr_t *) zalloc(sizeof(espcp_sock_addr_t));
        if (sa == NULL)
        {
            MEADOW_TRACE_DEBUG("sendto - result ENOMEM\n");
            return(-ENOMEM);
        }
        struct sockaddr_in *sin = (struct sockaddr_in *) to;
        sa->family = sin->sin_family;
        sa->port = sin->sin_port;
        memcpy(&sa->ip4_address, &sin->sin_addr, sizeof(sin->sin_addr));
        encodedSockAddr = (uint8_t *) zalloc(espcp_sock_addr_buffer_size(sa));
        if (encodedSockAddr == NULL)
        {
            free(sa);
            MEADOW_TRACE_DEBUG("sendto - result ENOMEM\n");
            return(-ENOMEM);
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

    espcp_send_to_request_t *request = (espcp_send_to_request_t *) zalloc(sizeof(espcp_send_to_request_t));
    if (request == NULL)
    {
        free(encodedSockAddr);
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->flags = flags;
    request->destination_address_length = encodedSockAddrLen;
    request->destination_address = encodedSockAddr;

    int32_t result = -1;
    espcp_message_t *message = NULL;

    if (len > ESPCP_MAXIMUM_PAYLOAD_SIZE)
    {
        len = ESPCP_MAXIMUM_PAYLOAD_SIZE;
    }
    request->length = len;
    request->buffer = (uint8_t *) buffer;
    request->buffer_length = request->length;
    int payload_length = espcp_send_to_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        if (encodedSockAddr != NULL)
        {
            free(encodedSockAddr);
        }
        result = -ENOMEM;
    }
    else
    {
        espcp_encode_send_to_request(request, payload);
        free(encodedSockAddr);

        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                            espcp_wi_fi_function_send_to, espcp_status_codes_completed_ok,
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
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = (response->result < 0) ? -response->response_errno : response->result;
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }
    free(request);

    espcp_delete_message_and_payload(message);

    /* Writability tracking for the local POLLOUT fast path: a send that hit
     * EAGAIN means the ESP send buffer is full -- subsequent POLLOUT polls
     * must go to the ESP until a send succeeds again.  Any successful send
     * also proves the socket is connected.
     */
    if (result == -EAGAIN)
    {
        /* Buffer full (again): writability unknown until the next wire
         * POLLOUT interrupt, so any cached WR_READY is stale now. */
        psock->s_esp32_state |= ESP32_SF_SEND_EAGAIN;
        psock->s_esp32_state &= ~ESP32_SF_WR_READY;
    }
    else if (result >= 0)
    {
        psock->s_esp32_state &= ~(ESP32_SF_SEND_EAGAIN |
                                  ESP32_SF_CONNECT_INPROGRESS |
                                  ESP32_SF_WR_READY);
    }


    MEADOW_TRACE_INFORMATION("sendto: socket %d, result: %d\n", psock->s_esp32_sockfd, result);

    return (result);
}

/****************************************************************************
 * Name: espcp_log_socket_option_name
 *
 * Description:
 *  Send the name of the socket option to the logging stream (when logging
 *  is enabled).
 *
 * Input Parameters:
 *  option - option ID to be decoded and sent to the log stream.
 *
 * Returns:
 *  None.
 *
 ****************************************************************************/
static void espcp_log_socket_option_name(int level, int option)
{
    if (level == SOL_SOCKET)
    {
        switch (option)
        {
            case SO_ACCEPTCONN:
                MEADOW_TRACE_INFORMATION("Socket option: SO_ACCEPTCONN\n");
                break;
            case SO_BROADCAST:
                MEADOW_TRACE_INFORMATION("Socket option: SO_BROADCAST\n");
                break;
            case SO_DEBUG:
                MEADOW_TRACE_INFORMATION("Socket option: SO_DEBUG\n");
                break;
            case SO_DONTROUTE:
                MEADOW_TRACE_INFORMATION("Socket option: SO_DONTROUTE\n");
                break;
            case SO_ERROR:
                MEADOW_TRACE_INFORMATION("Socket option: SO_ERROR\n");
                break;
            case SO_KEEPALIVE:
                MEADOW_TRACE_INFORMATION("Socket option: SO_KEEPALIVE\n");
                break;
            case SO_LINGER:
                MEADOW_TRACE_INFORMATION("Socket option: SO_LINGER\n");
                break;
            case SO_OOBINLINE:
                MEADOW_TRACE_INFORMATION("Socket option: SO_OOBINLINE\n");
                break;
            case SO_RCVBUF:
                MEADOW_TRACE_INFORMATION("Socket option: SO_RCVBUF\n");
                break;
            case SO_RCVLOWAT:
                MEADOW_TRACE_INFORMATION("Socket option: SO_RCVLOWAT\n");
                break;
            case SO_RCVTIMEO:
                MEADOW_TRACE_INFORMATION("Socket option: SO_RCVTIMEO\n");
                break;
            case SO_REUSEADDR:
                MEADOW_TRACE_INFORMATION("Socket option: SO_REUSEADDR\n");
                break;
            case SO_SNDBUF:
                MEADOW_TRACE_INFORMATION("Socket option: SO_SNDBUF\n");
                break;
            case SO_SNDLOWAT:
                MEADOW_TRACE_INFORMATION("Socket option: SO_SNDLOWAT\n");
                break;
            case SO_SNDTIMEO:
                MEADOW_TRACE_INFORMATION("Socket option: SO_SNDTIMEO\n");
                break;
            case SO_TYPE:
                MEADOW_TRACE_INFORMATION("Socket option: SO_TYPE\n");
                break;
            default:
                MEADOW_TRACE_INFORMATION("Unknown socket option name: 0x%x (%d)\n", option, option);
                break;
        }
    }
    else
    {
        if (level == SOL_TCP)
        {
            switch (option)
            {
                case TCP_NODELAY:
                    MEADOW_TRACE_INFORMATION("TCP option: TCP_NODELAY\n");
                    break;
                default:
                    MEADOW_TRACE_INFORMATION("Unknown TCP option name: 0x%x (%d)\n", option, option);
                    break;
            }
        }
    }
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
 *   getsockopt is documented here: https://linux.die.net/man/3/getsockopt
 *
 * Input Parameters:
 *   conn      usrsock socket connection structure
 *   level     Protocol level to set the option
 *   option    identifies the option to get
 *   value     Points to the argument value
 *   value_len The length of the argument value
 *
 * Returns:
 *  0 on success, negated errno on error.
 *
 *  -EINVAL: Value length is not large enough to store the result.
 *
 ****************************************************************************/
int espcp_usrsock_getsockopt(struct socket *psock, int level, int option,
                             void *value, socklen_t *value_len)
{
    MEADOW_TRACE_INFORMATION("getsockopt(%d, %d, %d, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, level, option, (uint32_t) value, (uint32_t) value_len);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("getsockopt - result ENETDOWN\n");
        return(-ENETDOWN);
    }
    espcp_log_socket_option_name(level, option);

    if (value == NULL)
    {
        return(-EFAULT);
    }
    switch (option)
    {
        case SO_LINGER:
            //
            //  Answer SO_LINGER LOCALLY with "linger disabled" (the default).
            //  The ESP coprocessor answers getsockopt(SO_LINGER) unreliably — the
            //  error surfaces in managed as "Unknown socket error" and breaks
            //  MQTTnet's connect, which reads Socket.LingerState every time. We
            //  never enable linger, so a zeroed struct is always correct.
            //
            {
                struct linger lg;
                lg.l_onoff = 0;
                lg.l_linger = 0;
                if (value_len == NULL || *value_len < (socklen_t)sizeof(lg))
                {
                    return(-EINVAL);
                }
                memcpy(value, &lg, sizeof(lg));
                *value_len = (socklen_t)sizeof(lg);
                return(OK);
            }
        case SO_SNDTIMEO:
        case SO_RCVTIMEO:
        case SO_RCVBUF:
            //
            //  Decode and store value.
            //
            break;
        case SO_DEBUG:
        case SO_DONTROUTE:
        case SO_OOBINLINE:
        case SO_SNDBUF:
        case SO_SNDLOWAT:
        case SO_RCVLOWAT:
            //
            //  Not supported by the ESP32.
            //
            return(-EPFNOSUPPORT);
            break;
        case SO_ERROR:
            //
            //  Send to the ESP, which clears and returns the pending socket error
            //  (an int, decoded below like SO_RCVBUF). REQUIRED for non-blocking
            //  connect: .NET reads SO_ERROR after the socket polls writable to get
            //  the final connect result (0 = connected). Without this the F7 used to
            //  short-circuit with EPFNOSUPPORT, so connect-completion never resolved
            //  and the engine spun on POLLOUT forever.
            //
            break;
        case SO_ACCEPTCONN:
        case SO_TYPE:
            //
            //  Supported by the ESP but not implemented yet.
            //
            return(-EPFNOSUPPORT);
            break;
        default:
            //
            //  If we get here then we have an option that has not been considered.
            //
            return(-EPFNOSUPPORT);
            break;
    }

    int result = -1;
    espcp_get_sock_opt_request_t *request = (espcp_get_sock_opt_request_t *) zalloc(sizeof(espcp_get_sock_opt_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("getsockopt - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->option_name = option;
    request->level = level;
    int payload_length = espcp_get_sock_opt_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    espcp_message_t *message = NULL;
    if (payload == NULL)
    {
        free(request);
        result = -ENOMEM;
    }
    else
    {
        espcp_encode_get_sock_opt_request(request, payload);
        free(request);
        message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi,
                                               espcp_wi_fi_function_get_sock_opt, espcp_status_codes_completed_ok,
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
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_get_sock_opt_response_t *response = espcp_extract_get_sock_opt_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                result = (response->result < 0) ? -response->response_errno : response->result;
                                if (result == 0)
                                {
                                    void *source = NULL;
                                    int source_size = 0;
                                    switch (option)
                                    {
                                        case SO_LINGER:
                                            {
                                                espcp_linger_t *esp_lv = espcp_extract_linger(response->option_value);
                                                if (esp_lv == NULL)
                                                {
                                                    result = -ENOMEM;
                                                }
                                                else
                                                {
                                                    source_size = sizeof(struct linger);
                                                    source = zalloc(source_size);
                                                    if (source == NULL)
                                                    {
                                                        result = -ENOMEM;
                                                    }
                                                    else
                                                    {
                                                        struct linger *lv = (struct linger *) source;
                                                        lv->l_linger = esp_lv->l_linger;
                                                        lv->l_onoff = esp_lv->l_on_off;
                                                    }
                                                    free(esp_lv);
                                                }
                                            }
                                            break;
                                        case SO_SNDTIMEO:
                                        case SO_RCVTIMEO:
                                            {
                                                espcp_time_val_t *esp_tv = espcp_extract_time_val(response->option_value);
                                                if (esp_tv == NULL)
                                                {
                                                    result = -ENOMEM;
                                                }
                                                else
                                                {
                                                    source_size = sizeof(struct timeval);
                                                    source = zalloc(source_size);
                                                    if (source == NULL)
                                                    {
                                                        result = -ENOMEM;
                                                    }
                                                    else
                                                    {
                                                        struct timeval *tv = (struct timeval *) source;
                                                        tv->tv_sec = esp_tv->tv_sec;
                                                        tv->tv_usec = esp_tv->tv_usec;
                                                    }
                                                    free(esp_tv);
                                                }
                                            }
                                            break;
                                        case SO_ERROR:
                                        case SO_RCVBUF:
                                            {
                                                espcp_integer_response_t *esp_iv = espcp_extract_integer_response(response->option_value);
                                                if (esp_iv == NULL)
                                                {
                                                    result = -ENOMEM;
                                                }
                                                else
                                                {
                                                    source_size = sizeof(int);
                                                    source = zalloc(source_size);
                                                    if (source == NULL)
                                                    {
                                                        result = -ENOMEM;
                                                    }
                                                    else
                                                    {
                                                        *((int *) source) = esp_iv->result;
                                                    }
                                                    free(esp_iv);
                                                }
                                            }
                                            break;
                                    }
                                    if (*value_len < source_size)
                                    {
                                        source_size = *value_len;
                                    }
                                    memcpy(value, source, source_size);
                                    free(source);
                                    *value_len = source_size;
                                }
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    /* .NET's non-blocking connect completion protocol is: poll POLLOUT, then
     * read SO_ERROR for the final result.  Whatever the outcome, the connect
     * attempt is over -- resume answering POLLOUT locally in poll_setup.
     */
    if (option == SO_ERROR)
    {
        psock->s_esp32_state &= ~(ESP32_SF_CONNECT_INPROGRESS |
                                  ESP32_SF_WR_READY);
    }

    MEADOW_TRACE_INFORMATION("getsockopt - socket %d result %d\n", psock->s_esp32_sockfd, result);

    return(result);
}

/**
 * @brief Encode an integer value for the ESP32.
 *
 * @param data
 *      Pointer to the integer value to be encoded.
 *
 * @return uint8_t*
 *      Buffer containing the encoded value or NULL if there was a problem.
 */
static uint8_t *espcp_usrsock_encode_integer(const void *data)
{
    uint8_t *result = NULL;

    if (data != NULL)
    {
        result = (uint8_t *) zalloc(sizeof(int));
        if (result != NULL)
        {
            espcp_encode_int32(*((int32_t *) data), result);
        }
    }

    return(result);
}

/**
 * @brief Encode a TCP option value for the ESP32.
 *
 * @param request
 *      Pointer to a setsockopt request to be sent to the ESP32.
 *
 * @param option
 *      Name of the option value to be encoded.
 *
 * @param value
 *      Pointer to the option value data.
 *
 * @param length
 *      Length of the option value data.
 *
 * @return int
 *      0 on success or negated error code if there was a problem.
 */
static int espcp_usrsock_encode_socket_option_value(espcp_set_sock_opt_request_t *request, int option, const void *value, socklen_t length)
{
    int result = 0;

    if (value == NULL)
    {
        result = -EINVAL;
    }
    else
    {
        switch (option)
        {
            case SO_SNDTIMEO:
            case SO_RCVTIMEO:
                {
                    espcp_time_val_t *tv = (espcp_time_val_t *) zalloc(sizeof(espcp_time_val_t));
                    if (tv == NULL)
                    {
                        result = -ENOMEM;
                    }
                    else
                    {
                        struct timeval *ov = (struct timeval *) value;
                        tv->tv_sec = ov->tv_sec;
                        tv->tv_usec = ov->tv_usec;
                        request->option_value_length = espcp_time_val_buffer_size(tv);
                        request->option_value = (uint8_t *) zalloc(request->option_value_length);
                        if (request->option_value != NULL)
                        {
                            espcp_encode_time_val(tv, request->option_value);
                        }
                        else
                        {
                            result = -ENOMEM;
                        }
                        free(tv);
                    }
                }
                break;
            case SO_LINGER:
                //
                //  Add implementation when enabled in the ESP32 build.
                //
                break;
            case SO_DEBUG:
            case SO_DONTROUTE:
            case SO_OOBINLINE:
            case SO_SNDBUF:
            case SO_RCVBUF:
            case SO_SNDLOWAT:
            case SO_RCVLOWAT:
                //
                //  Not supported by the ESP32 (lwIP socket buffer sizes are fixed).
                //  Treat as a no-op success instead of forwarding to the ESP. SO_SNDBUF
                //  was already handled this way; SO_RCVBUF was being forwarded and the
                //  ESP rejected it with EADDRNOTAVAIL, which threw a SocketException out
                //  of standard socket code that sets ReceiveBufferSize (e.g. MQTTnet's
                //  MqttTcpChannel.ConnectAsync -> set_ReceiveBufferSize), breaking MQTT.
                //
                break;
            case SO_REUSEADDR:
                request->option_value = espcp_usrsock_encode_integer(value);
                if (request->option_value == NULL)
                {
                    result = -ENOMEM;
                }
                else
                {
                    request->option_value_length = sizeof(int);
                }
                break;
            case SO_ACCEPTCONN:
            case SO_ERROR:
            case SO_TYPE:
                //
                //  Supported by the ESP but not implemented yet.
                //
                break;
            default:
                break;
        }
    }

    return(result);
}

/**
 * @brief Encode a TCP option value for the ESP32.
 *
 * @param request
 *      Pointer to a setsockopt request to be sent to the ESP32.
 *
 * @param option
 *      Name of the option value to be encoded.
 *
 * @param value
 *      Pointer to the option value data.
 *
 * @param length
 *      Length of the option value data.
 *
 * @return int
 *      0 on success or negated error code if there was a problem.
 */
static int espcp_usrsock_encode_tcp_option_value(espcp_set_sock_opt_request_t *request, int option, const void *value, socklen_t length)
{
    int result = 0;

    if (value == NULL)
    {
        result = -EINVAL;
    }
    else
    {
        switch(option)
        {
            case TCP_NODELAY:
                request->option_value = espcp_usrsock_encode_integer(value);
                if (request->option_value == NULL)
                {
                    result = -ENOMEM;
                }
                else
                {
                    request->option_value_length = sizeof(int);
                }
                break;
            default:
                result = -EINVAL;
                break;
        }
    }

    return(result);
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
 *  0 on success, negated errno on error.
 *
 ****************************************************************************/
int espcp_usrsock_setsockopt(struct socket *psock, int level, int option,
                             const void *value, socklen_t value_len)
{
    MEADOW_TRACE_INFORMATION("setsockopt(%d, %d, %d, 0x%08x, 0x%08x)\n", psock->s_esp32_sockfd, level, option, (uint32_t) value, (uint32_t) value_len);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("setsockopt - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    espcp_set_sock_opt_request_t *request = (espcp_set_sock_opt_request_t *) zalloc(sizeof(espcp_set_sock_opt_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("setsockopt - result ENOMEM\n");
        return(-ENOMEM);
    }

    espcp_log_socket_option_name(level, option);

    int32_t result = 0;
    switch (level)
    {
        case SOL_SOCKET:
            result = espcp_usrsock_encode_socket_option_value(request, option, value, value_len);
            break;
        case SOL_TCP:
            result = espcp_usrsock_encode_tcp_option_value(request, option, value, value_len);
            break;
        default:
            break;
    }
    if (result < 0)
    {
        free(request);
        return(result);
    }

    espcp_message_t *message = NULL;
    if (request->option_value != NULL)
    {
        request->socket_handle = psock->s_esp32_sockfd;
        request->level = level;
        request->option_name = option;

        int payload_length = espcp_set_sock_opt_request_buffer_size(request);
        uint8_t *payload = (uint8_t *) zalloc(payload_length);
        if (payload == NULL)
        {
            free(request->option_value);
            free(request);
            result = -ENOMEM;
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
                result = -ENOMEM;
            }
            else
            {
                if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
                {
                    switch (message->status_code)
                    {
                        case espcp_status_codes_completed_ok:
                            {
                                espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                                if (response == NULL)
                                {
                                    result = -ENOMEM;
                                }
                                else
                                {
                                    result = (response->result < 0) ? -response->response_errno : response->result;
                                    free(response);
                                }
                            }
                            break;
                        case espcp_status_codes_thread_pool_is_full:
                        case espcp_status_codes_esp_out_of_memory:
                            result = -ENOMEM;
                            break;
                        default:
                            result = -1;
                            break;
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
        result = 0;
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("setsockopt - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  0 on success, negated errno on error.
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
    MEADOW_TRACE_INFORMATION("socket(%d, %d, %d, %d)\n", domain, type, protocol, psock->s_esp32_sockfd);
    struct wdog_s g_watchdog_socket;
    meadow_watchdog_activate(&g_watchdog_socket, WATCHDOG_SOCKET_TIMEOUT_MILLISECONDS, SOCKET_WATCHDOG);

    if (espcp_get_configuration()->esp_not_responding)
    {
        meadow_watchdog_deactivate(&g_watchdog_socket);
        MEADOW_TRACE_DEBUG("socket - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    int result = -1;

    espcp_socket_request_t *request = (espcp_socket_request_t *) zalloc(sizeof(espcp_socket_request_t));
    if (request == NULL)
    {
        meadow_watchdog_deactivate(&g_watchdog_socket);
        MEADOW_TRACE_DEBUG("socket - result ENOMEM\n");
        return (-ENOMEM);
    }
    request->domain = domain;
    request->type = type;
    request->protocol = protocol;

    int payload_length = espcp_socket_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    if (payload == NULL)
    {
        free(request);
        meadow_watchdog_deactivate(&g_watchdog_socket);
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
        meadow_watchdog_deactivate(&g_watchdog_socket);
        MEADOW_TRACE_DEBUG("socket - result ENOMEM\n");
        return (-ENOMEM);
    }
    else
    {
        if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
        {
            switch (message->status_code)
            {
                case espcp_status_codes_completed_ok:
                    {
                        espcp_integer_and_errno_response_t *response = espcp_extract_integer_and_errno_response(message->payload);
                        if (response == NULL)
                        {
                            result = -ENOMEM;
                        }
                        else
                        {
                            if (response->result < 0)
                            {
                                result = -response->response_errno;
                            }
                            else
                            {
                                result = response->result;
                            }
                            free(response);
                            psock->s_domain = domain;
                            psock->s_type = type;
                            psock->s_esp32_sockfd = result;
                            psock->s_esp32_state = 0;   /* struct socket slots are reused */

                            /* Bound ESP-side blocking sends.  The ESP services
                             * send/recv/connect from a small shared worker pool;
                             * a send stuck on a dead peer (TCP retransmit can
                             * run for many minutes) pins a worker.  A few such
                             * sockets exhaust the pool and EVERY subsequent
                             * request fails with ThreadPoolIsFull (surfaces as
                             * ENOMEM / "Unknown socket error" on all new
                             * connections).  lwip honours SO_SNDTIMEO, so cap
                             * worker hold time.  Best effort by design.
                             */
                            {
                                struct timeval sp_tv;
                                sp_tv.tv_sec = 10;
                                sp_tv.tv_usec = 0;
                                (void)espcp_usrsock_setsockopt(psock, SOL_SOCKET,
                                        SO_SNDTIMEO, &sp_tv, sizeof(sp_tv));
                            }
                        }
                    }
                    break;
                case espcp_status_codes_thread_pool_is_full:
                case espcp_status_codes_esp_out_of_memory:
                    result = -ENOMEM;
                    break;
                default:
                    result = -1;
                    break;
            }
        }
    }

    espcp_delete_message_and_payload(message);
    meadow_watchdog_deactivate(&g_watchdog_socket);


    MEADOW_TRACE_INFORMATION("socket - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  If successful, the number of bytes read from the socket.
 *  On error a negated errno is returned.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int32_t espcp_usrsock_read(struct socket *psock, const void *buffer, size_t count)
{
    MEADOW_TRACE_INFORMATION("read(%d, 0x%08x, %d)\n", psock->s_esp32_sockfd, (uint32_t) buffer, count);

    if (espcp_get_configuration()->esp_not_responding)
    {
        MEADOW_TRACE_DEBUG("read - result ENETDOWN\n");
        return(-ENETDOWN);
    }

    if ((buffer == NULL) || (count > MAXIMUM_READ_WRITE_BUFFER_SIZE))
    {
        MEADOW_TRACE_DEBUG("read - result EFAULT\n");
        return (-EFAULT);
    }

    espcp_read_request_t *request = (espcp_read_request_t *) zalloc(sizeof(espcp_read_request_t));
    if (request == NULL)
    {
        MEADOW_TRACE_DEBUG("read - result ENOMEM\n");
        return(-ENOMEM);
    }
    request->socket_handle = psock->s_esp32_sockfd;
    request->count = count;

    int payload_length = espcp_read_request_buffer_size(request);
    uint8_t *payload = (uint8_t *) zalloc(payload_length);
    int32_t result = -1;
    espcp_message_t *message = NULL;
    if (payload == NULL)
    {
        free(request);
        result = -ENOMEM;
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
            result = -ENOMEM;
        }
        else
        {
            if (espcp_queue_message(message, true) == espcp_status_codes_completed_ok)
            {
                switch (message->status_code)
                {
                    case espcp_status_codes_completed_ok:
                        {
                            espcp_read_response_t *response = espcp_extract_read_response(message->payload);
                            if (response == NULL)
                            {
                                result = -ENOMEM;
                            }
                            else
                            {
                                if (response->buffer_length > 0)
                                {
                                    memcpy((void *) buffer, response->buffer, response->buffer_length);
                                    free(response->buffer);
                                }
                                result = (response->read_response_result < 0) ? -response->read_response_errno : response->read_response_result;
                                free(response);
                            }
                        }
                        break;
                    case espcp_status_codes_thread_pool_is_full:
                    case espcp_status_codes_esp_out_of_memory:
                        result = -ENOMEM;
                        break;
                    default:
                        result = -1;
                        break;
                }
            }
        }
    }

    espcp_delete_message_and_payload(message);

    MEADOW_TRACE_INFORMATION("read - socket %d, result %d\n", psock->s_esp32_sockfd, result);

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
 *  -1
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
 *  -1
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
size_t espcp_usrsock_sendmsg(struct socket *psock, const struct msghdr *msg, int flags)
{
  espcp_usrsock_not_implemented(__func__);
  return(-1);
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
 *  -1
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
 *  -1
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
size_t espcp_usrsock_recvmsg(struct socket *psock, struct msghdr *msg, int flags)
{
  espcp_usrsock_not_implemented(__func__);
  return(-1);
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
 *  -1
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
