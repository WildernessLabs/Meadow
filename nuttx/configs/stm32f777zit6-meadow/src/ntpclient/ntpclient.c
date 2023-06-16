/****************************************************************************
 * netutils/ntpclient/ntpclient.c
 *
 *   Copyright (C) 2014, 2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
 * 
 * This file is a modified version of the NTPClient application file in the
 * NuttX apps repository.
 * 
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <debug.h>
#include <strings.h>

#include <netinet/in.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <nuttx/kthread.h>
#include <nuttx/wqueue.h>
#include "ntpclient.h"

#include "ntpv3.h"

#include "../hcom_nx/hcom_nx_common.h"
#include <meadow/hcom_nuttx_shared.h>
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "../espcp/espcp_message.h"
#include "../espcp/espcp_event_handlers.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#ifndef CONFIG_SCHED_LPWORK
#error ".../stm32f777zit6-meadow/src/ntpclient/ntpclient.c requires CONFIG_SCHED_LPWORK"
#endif

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/* Configuration ************************************************************/

/* NTP Time is seconds since 1900. Convert to Unix time which is seconds
 * since 1970
 */

#define NTP2UNIX_TRANLSLATION 2208988800u
#define NTP_VERSION 3

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

// Each work_s struct can support one queued worker. If, while one worker is
// waiting to be run another call to work_queue is made with the same work_s
// the first invocation is over-written by the second.
static struct work_s _ntpclient_work_q_struct;
static uint32_t _refresh_period;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern void dns_clear_answer(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ntpc_getuint32
 *
 * Description:
 *  Return the big-endian, 4-byte value in network (big-endian) order.
 * 
 *  Network order is big-endian; host order is irrelevant
 * 
 * Input Parameters:
 *  ptr - Pointer to a byte array containing the data to be converted.
 *
 * Returned Value:
 *  32-bit number derived from the data in the byte array (ptr).
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static inline uint32_t ntpc_getuint32(FAR uint8_t *ptr)
{
    return (uint32_t) ptr[3] | /* MS byte appears first in data stream */
           ((uint32_t) ptr[2] << 8) |
           ((uint32_t) ptr[1] << 16) |
           ((uint32_t) ptr[0] << 24);
}

/****************************************************************************
 * Name: ntpc_settime
 *
 * Description:
 *  Set the system time using the information from the NTP server.
 * 
 * Input Parameters:
 *  timestamp - Pointer to the memory holding the information from the
 *              NTP server.
 *
 * Returned Value:
 *  None..
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void ntpc_settime(FAR uint8_t *timestamp)
{
    struct timespec tp;
    time_t seconds;
    uint32_t frac;
    uint32_t nsec;
#ifdef CONFIG_HAVE_LONG_LONG
    uint64_t tmp;
#else
    uint32_t a16;
    uint32_t b0;
    uint32_t t32;
    uint32_t t16;
    uint32_t t0;
#endif

    /* NTP timestamps are represented as a 64-bit fixed-point number, in
     * seconds relative to 0000 UT on 1 January 1900.  The integer part is
     * in the first 32 bits and the fraction part in the last 32 bits, as
     * shown in the following diagram.
     *
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                         Integer Part                          |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                         Fraction Part                         |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    seconds = ntpc_getuint32(timestamp);

    /* Translate seconds to account for the difference in the origin time */

    if (seconds > NTP2UNIX_TRANLSLATION)
    {
        seconds -= NTP2UNIX_TRANLSLATION;
    }

    /* Conversion of the fractional part to nanoseconds:
     *
     *  NSec = (f * 1,000,000,000) / 4,294,967,296
     *       = (f * (5**9 * 2**9) / (2**32)
     *       = (f * 5**9) / (2**23)
     *       = (f * 1,953,125) / 8,388,608
     */

    frac = ntpc_getuint32(timestamp + 4);
#ifdef CONFIG_HAVE_LONG_LONG
    /* if we have 64-bit long long values, then the computation is easy */

    tmp = ((uint64_t)frac * 1953125) >> 23;
    nsec = (uint32_t)tmp;
#else
    /* If we don't have 64 bit integer types, then the calculation is a little
    * more complex:
    *
    * Let f         = a    << 16 + b
    *     1,953,125 = 0x1d << 16 + 0xcd65
    * NSec << 23 =  ((a << 16) + b) * ((0x1d << 16) + 0xcd65)
    *            = (a << 16) * 0x1d << 16) +
    *              (a << 16) * 0xcd65 +
    *              b         * 0x1d << 16) +
    *              b         * 0xcd65;
    */

    /* Break the fractional part up into two values */

    a16 = frac >> 16;
    b0 = frac & 0xffff;

    /* Get the b32 and b0 terms
    *
    * t32 = (a << 16) * 0x1d << 16)
    * t0  = b * 0xcd65
    */

    t32 = 0x001d * a16;
    t0 = 0xcd65 * b0;

    /* Get the first b16 term
    *
    * (a << 16) * 0xcd65
    */

    t16 = 0xcd65 * a16;

    /* Add the upper 16-bits to the b32 accumulator */

    t32 += (t16 >> 16);

    /* Add the lower 16-bits to the b0 accumulator, handling carry to the b32
    * accumulator
    */

    t16 <<= 16;
    if (t0 > (0xffffffff - t16))
    {
        t32++;
    }

    t0 += t16;

    /* Get the second b16 term
    *
    * b * (0x1d << 16)
    */

    t16 = 0x001d * b0;

    /* Add the upper 16-bits to the b32 accumulator */

    t32 += (t16 >> 16);

    /* Add the lower 16-bits to the b0 accumulator, handling carry to the b32
    * accumulator
    */

    t16 <<= 16;
    if (t0 > (0xffffffff - t16))
    {
        t32++;
    }

    t0 += t16;

    /* t32 and t0 represent the 64 bit product.  Now shift right by 23 bits to
    * accomplish the divide by by 2**23.
    */

    nsec = (t32 << (32 - 23)) + (t0 >> 23);
#endif

  /* Set the system time */

    tp.tv_sec = seconds;
    tp.tv_nsec = nsec;
    clock_settime(CLOCK_REALTIME, &tp);
}

/****************************************************************************
 * Name: ntpc_connect_to_server
 *
 * Description:
 *  Connect to the NTP server.
 * 
 *  This method will populate the memory pointed to by the server parameter
 *  with information about the NTP server.
 * 
 * Input Parameters:
 *  server - Pointer to a socket address structure.
 *
 * Returned Value:
 *  Socket descriptor or ERROR if there is a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int ntpc_connect_to_server(char *server_name, struct sockaddr_in *server, uint32_t timeout)
{
    struct timeval tv;
    struct hostent *he;
    struct in_addr **addr_list;
    int sd;
    int result;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        MEADOW_TRACE_ERROR("ERROR: socket failed: %d\n", errno);
        return ERROR;
    }

    /* Setup a receive timeout on the socket */
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    result = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    if (result < 0)
    {
        MEADOW_TRACE_ERROR("ERROR: setsockopt failed: %d\n", errno);
        close(sd);
        return ERROR;
    }

    //
    //  Setup or sockaddr_in struct with information about the server we are
    //  going to ask the time from.
    //
    memset(server, 0, sizeof(struct sockaddr_in));
    server->sin_family = AF_INET;
    server->sin_port = htons(CONFIG_NETUTILS_NTPCLIENT_PORTNO);
    he = gethostbyname(server_name);
    if ((he != NULL ) && (he->h_addrtype == AF_INET))
    {
        addr_list = (struct in_addr **)he->h_addr_list;
        server->sin_addr.s_addr = addr_list[0]->s_addr;
        MEADOW_TRACE_INFORMATION("INFO: '%s' resolved to: %s\n", server_name, inet_ntoa(server->sin_addr));
    }
    else
    {
        MEADOW_TRACE_ERROR("ERROR: Failed to resolve '%s'\n", server_name);
        close(sd);
        return ERROR;
    }
    return(sd);
}

/****************************************************************************
 * Name: ntpc_daemon
 *
 * Description:
 *  Implementation of the NTP daemon.  This method should be run in its own
 *  thread.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void ntpc_raise_time_changed_event(enum espcp_esp32_interfaces interface)
{
    espcp_message_t *message = (espcp_message_t *) zalloc(sizeof(espcp_message_t));
    if (message != NULL)
    {
        message->message_type = espcp_message_types_event;
        message->interface = interface;
        message->function = espcp_wi_fi_function_ntp_update_event;
        message->status_code = espcp_status_codes_completed_ok;
        espcp_dispatch_event(message);
    }
}

/****************************************************************************
 * Name: ntpc_daemon
 *
 * Description:
 *  Implementation of the NTP daemon.  This method should be run in its own
 *  thread.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static uint32_t ntpc_daemon(void)
{
    struct sockaddr_in server;
    struct ntp_datagram_s xmit;
    struct ntp_datagram_s recv;

    socklen_t socklen;
    ssize_t nbytes;
    int sd;
    int result;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    uint32_t number_of_servers = config->ntp_servers_count;
    hcom_nx_config_unlock();

    bool getting_time = true;
    uint32_t socket_timeout = NTP_INITIAL_SOCKET_TIMEOUT;
    int current_server = 0;
    char server_name[64];
    int retry_count = 0;
    while (getting_time && (retry_count < 3))
    {
        hcom_nx_config_lock();
        config = hcom_nx_config_get_pointer();
        strncpy(server_name, config->ntp_servers[current_server], 64);
        hcom_nx_config_unlock();
        MEADOW_TRACE_INFORMATION("Getting time from %s\n", server_name);
        sd = ntpc_connect_to_server(server_name, &server, socket_timeout);
        if (sd >= 0)
        {
            memset(&xmit, 0, sizeof(xmit));
            xmit.lvm = MKLVM(0, 3, NTP_VERSION);

            result = sendto(sd, &xmit, sizeof(struct ntp_datagram_s), 0, (FAR struct sockaddr *) &server, sizeof(struct sockaddr_in));
            if (result >= 0)
            {
                socklen = sizeof(struct sockaddr_in);
                nbytes = recvfrom(sd, (void *) &recv, sizeof(struct ntp_datagram_s), 0, (FAR struct sockaddr *) &server, &socklen);
                if (nbytes >= (ssize_t) NTP_DATAGRAM_MINSIZE)
                {
                    sched_lock();
                    ntpc_settime(recv.recvtimestamp);
                    sched_unlock();
                    getting_time = false;
                    MEADOW_TRACE_INFORMATION("Time received from server.\n");
                    ntpc_raise_time_changed_event(espcp_esp32_interfaces_wi_fi);
                }
            }
            close(sd);
        }
        if (getting_time)
        {
            current_server++;
            if (current_server == number_of_servers)
            {
                //
                //  We can sometimes find ourselves with IP addresses for different
                //  servers, say 0.uk.pool.ntp.org, 1.uk.pool.ntp.org etc. and we do
                //  not get a response from any of them.  If we then lookup the IP
                //  addresses again we just get the values from the cache and loop
                //  through the servers and do not get a result again.  Flushing the
                //  DNS cache should force the server IP addresses to change.
                //
                dns_clear_answer();
                current_server = 0;
                retry_count++;
            }
        }
    }
    return 0;
}

/****************************************************************************
 * Name: ntpc_daemon_requeue_worker
 *
 * Description:
 *  Periodically execute NTP daemon.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void ntpc_daemon_requeue_worker(void * arg)
{
    ntpc_daemon();      // Find time again.

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    _refresh_period = config->ntp_refresh_period_seconds;
    hcom_nx_config_unlock();

    // Requeue
    memset(&_ntpclient_work_q_struct, 0, sizeof (struct work_s));
    work_queue(LPWORK, &_ntpclient_work_q_struct, ntpc_daemon_requeue_worker,
            NULL, (_refresh_period * 1000)/MSEC_PER_TICK);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ntpc_start
 *
 * Description:
 *  Start the NTP daemon.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int ntpc_start(void)
{
    // Create a work queue to get the time the next time
    return(work_queue(LPWORK, &_ntpclient_work_q_struct,
                ntpc_daemon_requeue_worker, NULL, 0));
}

/****************************************************************************
 * Name: ntpc_stop
 *
 * Description:
 *  Stop the NTP daemon.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void ntpc_stop(void)
{
    work_cancel(LPWORK, &_ntpclient_work_q_struct);
}
