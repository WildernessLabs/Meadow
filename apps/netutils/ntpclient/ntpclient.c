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
#include <mqueue.h>

#include "../../examples/hcom/hcom_common.h"
#include "../../examples/hcom/misc/espcp_utils.h"

#include <netinet/in.h>
#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>
#include <netdb.h> 
#define MEADOW_USE_HCOM_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#ifdef CONFIG_LIBC_NETDB
#  include <netdb.h>
#  include <arpa/inet.h>
#endif

#include "netutils/ntpclient.h"

#include "ntpv3.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

extern void dns_clear_answer(void);

/* Configuration ************************************************************/

#if defined(CONFIG_LIBC_NETDB) && !defined(CONFIG_NETUTILS_NTPCLIENT_SERVER)
#  error CONFIG_NETUTILS_NTPCLIENT_SERVER my be provided
#endif

#if !defined(CONFIG_LIBC_NETDB) && !defined(CONFIG_NETUTILS_NTPCLIENT_SERVERIP)
#  error CONFIG_NETUTILS_NTPCLIENT_SERVERIP my be provided
#endif

/* NTP Time is seconds since 1900. Convert to Unix time which is seconds
 * since 1970
 */

#define NTP2UNIX_TRANLSLATION 2208988800u
#define NTP_VERSION          3
#define NTP_INITIAL_SOCKET_TIMEOUT      5
#define NTP_MAX_RETRY_ATTEMPTS      3

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This enumeration describes the state of the NTP daemon */

enum ntpc_daemon_e
{
  NTP_NOT_RUNNING = 0,
  NTP_STARTED,
  NTP_RUNNING,
  NTP_STOP_REQUESTED,
  NTP_STOPPED
};

/* This type describes the state of the NTP client daemon.  Only one
 * instance of the NTP daemon is permitted in this implementation.
 */

struct ntpc_daemon_s
{
  volatile uint8_t state; /* See enum ntpc_daemon_e */
  sem_t interlock;        /* Used to synchronize start and stop events */
  pid_t pid;              /* Task ID of the NTP daemon */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* This type describes the state of the NTP client daemon.  Only one
 * instance of the NTP daemon is permitted in this implementation.  This
 * limitation is due only to this global data structure.
 */

static struct ntpc_daemon_s g_ntpc_daemon;
static char** ntp_servers;
static uint32_t ntp_server_count;
static unsigned int ntpc_refresh_period_seconds = CONFIG_NETUTILS_NTPCLIENT_POLLDELAYSEC;

static char *thisFile = __FILE__;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ntpc_update_event
 *
 * Description:
 *   Handle the NTP update event for the network interface, encode
 *   the event data, and queue the event messages for processing.
 *
 ****************************************************************************/
void ntpc_update_event(void) 
{
    meadow_configuration_t *config = meadow_os_deep_copy_config();
    uint32_t default_interface_type = config->default_interface->interface_type;
    espcp_event_data_t message;

    switch (default_interface_type)
    {
        case MEADOW_IFT_ESP32:
            message.interface = ESPCP_WIFI_INTERFACE;
            message.function = ESPCP_WIFI_NTP_UPDATE_EVENT;
            break;
        case MEADOW_IFT_CELL:
            message.interface = ESPCP_CELL_INTERFACE;
            message.function = ESPCP_CELL_NTP_UPDATE_EVENT;
            break;
        case MEADOW_IFT_ETHERNET:
            message.interface = ESPCP_ETHERNET_INTERFACE;
            message.function = ESPCP_ETHERNET_NTP_UPDATE_EVENT;
            break;
        default:
            // TODO: Handle unknown interface
            message.interface = ESPCP_NONE_INTERFACE;
            message.function = ESPCP_WIFI_NTP_UPDATE_EVENT;
            break;
    }

    meadow_os_config_free_resources(config);

    message.status_code = ESPCP_COMPLETED_OK_STATUS_CODE;
    message.message_id = ESPCP_SIMPLE_EVENT_MESSAGE_ID;

    uint32_t encodedEventDataSize = ESPCP_EVENT_DATA_SIZE;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    MEADOW_TRACE_INFORMATION("%s@%d-NTP update event result: %d\n", thisFile, __LINE__, result);
}

/****************************************************************************
 * Name: ntpc_getuint32
 *
 * Description:
 *   Return the big-endian, 4-byte value in network (big-endian) order.
 *
 ****************************************************************************/

static inline uint32_t ntpc_getuint32(FAR uint8_t *ptr)
{
  /* Network order is big-endian; host order is irrelevant */

  return (uint32_t)ptr[3] |          /* MS byte appears first in data stream */
         ((uint32_t)ptr[2] << 8) |
         ((uint32_t)ptr[1] << 16) |
         ((uint32_t)ptr[0] << 24);
}

/****************************************************************************
 * Name: ntpc_settime
 *
 * Description:
 *   Given the NTP time in seconds, set the system time
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

  tmp  = ((uint64_t)frac * 1953125) >> 23;
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

  a16  = frac >> 16;
  b0   = frac & 0xffff;

  /* Get the b32 and b0 terms
   *
   * t32 = (a << 16) * 0x1d << 16)
   * t0  = b * 0xcd65
   */

  t32  = 0x001d * a16;
  t0   = 0xcd65 * b0;

  /* Get the first b16 term
   *
   * (a << 16) * 0xcd65
   */

  t16  = 0xcd65 * a16;

  /* Add the upper 16-bits to the b32 accumulator */

  t32 += (t16 >> 16);

  /* Add the lower 16-bits to the b0 accumulator, handling carry to the b32
   * accumulator
   */

  t16  <<= 16;
  if (t0 > (0xffffffff - t16))
    {
      t32++;
    }

  t0 += t16;

  /* Get the second b16 term
   *
   * b * (0x1d << 16)
   */

  t16  = 0x001d * b0;

  /* Add the upper 16-bits to the b32 accumulator */

  t32 += (t16 >> 16);

  /* Add the lower 16-bits to the b0 accumulator, handling carry to the b32
   * accumulator
   */

  t16  <<= 16;
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

  tp.tv_sec  = seconds;
  tp.tv_nsec = nsec;
  clock_settime(CLOCK_REALTIME, &tp);

  sinfo("Set time to %lu seconds: %d\n", (unsigned long)tp.tv_sec, ret);
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
        MEADOW_TRACE_ERROR("%s@%d-ERROR: socket failed: %d\n", thisFile, __LINE__, errno);
        return ERROR;
    }

    /* Setup a receive timeout on the socket */
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    result = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    if (result < 0)
    {
        MEADOW_TRACE_ERROR("%s@%d-ERROR: setsockopt failed: %d\n", thisFile, __LINE__, errno);
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
        MEADOW_TRACE_INFORMATION("%s@%d-INFO: '%s' resolved to: %s\n", thisFile, __LINE__, server_name, inet_ntoa(server->sin_addr));
    }
    else
    {
        MEADOW_TRACE_ERROR("%s@%d-ERROR: Failed to resolve '%s'\n", thisFile, __LINE__, server_name);
        close(sd);
        return ERROR;
    }

    return(sd);
}

/****************************************************************************
 * Name: ntpc_daemon
 *
 * Description:
 *   This is the NTP client daemon. This implementation initializes a message
 *   queue for inter-process communication. The daemon waits for a NTP start
 *   message to begin NTP synchronization operations and can receive a NTP stop
 *   message to terminate. It sends NTP requests to configured servers and updates
 *   the system clock upon receiving valid responses. The daemon cycles through
 *   the configured servers and retries failed attempts a limited number of times.
 *   The lifecycle of the daemon is controlled using message queue and semaphore
 *   synchronization.
 *
 ****************************************************************************/
static int ntpc_daemon(int argc, char **argv)
{
    struct sockaddr_in server;
    struct ntp_datagram_s xmit;
    struct ntp_datagram_s recv;

    socklen_t socklen;
    ssize_t nbytes;
    int sd;
    int result;

    bool getting_time = true;
    uint32_t socket_timeout = NTP_INITIAL_SOCKET_TIMEOUT;
    int current_server = 0;
    int retry_count = 0;

    mqd_t mq;
    char buffer[NTPC_QUEUE_MSG_MAX_SIZE];

    // Open the message queue for reading with non-blocking mode
    mq = mq_open(NTPC_QUEUE_INTERFACE, O_RDONLY | O_CREAT | O_NONBLOCK, 0644, NULL);
    if (mq == (mqd_t)-1)
    {
        MEADOW_TRACE_ERROR("%s@%d-Failed to open NTP message queue, error: %d\n", thisFile, __LINE__, errno);
        return EXIT_FAILURE;
    }

    // Main loop to handle the entire daemon lifecycle
    while (1)
    {
        // Loop to wait for the NTP start message
        MEADOW_TRACE_INFORMATION("%s@%d-Waiting for the NTP start message: \n", thisFile, __LINE__);
        while (1)
        {
            // Attempt to receive a message from the queue
            ssize_t bytes_read = mq_receive(mq, buffer, NTPC_QUEUE_MSG_MAX_SIZE, NULL);
            if (bytes_read >= 0)
            {
                uint32_t received_value;
                memcpy(&received_value, buffer, sizeof(received_value));
                MEADOW_TRACE_INFORMATION("%s@%d-Received NTP message: %u\n", thisFile, __LINE__, received_value);
                if (received_value == NTPC_START)
                {
                    MEADOW_TRACE_INFORMATION("%s@%d-Received NTP start message: %u\n", thisFile, __LINE__, received_value);
                    break;
                }
            }
            else
            {
                if (errno == EBADF || errno == EINVAL)
                {
                    MEADOW_TRACE_ERROR("%s@%d-Failed to receive NTP message, error: %d\n", thisFile, __LINE__, errno);
                    return EXIT_FAILURE;
                }
                // Sleep briefly to avoid busy-waiting if no message is available
                usleep(100000);
            }
        }

        /* Indicate that we have started */
        g_ntpc_daemon.state = NTP_RUNNING;
        sem_post(&g_ntpc_daemon.interlock);

        // Main loop to perform NTP operations
        while (g_ntpc_daemon.state != NTP_STOP_REQUESTED)
        {
            // Loop to attempt getting time from NTP servers
            while (getting_time && (retry_count < NTP_MAX_RETRY_ATTEMPTS))
            {
                // Check for NTP stop message
                ssize_t bytes_read = mq_receive(mq, buffer, NTPC_QUEUE_MSG_MAX_SIZE, NULL);
                if (bytes_read >= 0)
                {
                    uint32_t received_value;
                    memcpy(&received_value, buffer, sizeof(received_value));
                    if (received_value == NTPC_STOP)
                    {
                        MEADOW_TRACE_INFORMATION("%s@%d-Received NTP stop message: %u\n", thisFile, __LINE__, received_value);
                        g_ntpc_daemon.state = NTP_STOP_REQUESTED;
                        sem_post(&g_ntpc_daemon.interlock);
                        break;
                    }
                }

                sd = ntpc_connect_to_server(ntp_servers[current_server], &server, socket_timeout);
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
                            // Successfully received NTP response, update system time
                            sched_lock();
                            ntpc_settime(recv.recvtimestamp);
                            sched_unlock();
                            getting_time = false;
                            ntpc_update_event();
                            MEADOW_TRACE_INFORMATION("%s@%d-NTP update event triggered!\n", thisFile, __LINE__);
                        }
                    }
                    close(sd);
                }
                if (getting_time)
                {
                    // Try the next server if unable to get time
                    current_server++;
                    if (current_server == ntp_server_count)
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

            if (g_ntpc_daemon.state == NTP_RUNNING)
            {
                MEADOW_TRACE_INFORMATION("%s@%d-NTP daemon waiting for %d seconds\n", thisFile, __LINE__, ntpc_refresh_period_seconds);
                (void)sleep(ntpc_refresh_period_seconds);
                getting_time = true;
                retry_count = 0;
            }
        }

        /* The NTP client is stopping */
        MEADOW_TRACE_INFORMATION("%s@%d-NTP daemon is stopping\n", thisFile, __LINE__);
        g_ntpc_daemon.state = NTP_STOPPED;
        sem_post(&g_ntpc_daemon.interlock);

        // Reset state and prepare to wait for the start message again
        getting_time = true;
        socket_timeout = NTP_INITIAL_SOCKET_TIMEOUT;
        current_server = 0;
        retry_count = 0;
    }

    /* The NTP client is terminating */
    if (mq_close(mq) == -1)
    {
        MEADOW_TRACE_ERROR("%s@%d-Failed to close NTP message queue, error: %d\n", thisFile, __LINE__, errno);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/****************************************************************************
 * Name: ntpc_start
 *
 * Description:
 *   Start the NTP daemon
 *
 * Returned Value:
 *   On success, the non-negative task ID of the NTPC daemon is returned;
 *   On failure, a negated errno value is returned.
 *
 ****************************************************************************/

int ntpc_start(void)
{
  /* Is the NTP in a non-running state? */

  sched_lock();
  if (g_ntpc_daemon.state == NTP_NOT_RUNNING ||
      g_ntpc_daemon.state == NTP_STOPPED)
    {
      /* Is this the first time that the NTP daemon has been started? */

      if (g_ntpc_daemon.state == NTP_NOT_RUNNING)
        {
          /* Yes... then we will need to initialize the state structure */

          meadow_configuration_t *config = meadow_os_deep_copy_config();

          ntpc_refresh_period_seconds = config->ntp_refresh_period_seconds;
          ntp_server_count = config->ntp_servers_count;

          MEADOW_TRACE_INFORMATION("%s@%d-NTP server count: %d \n", thisFile, __LINE__, ntp_server_count);
          MEADOW_TRACE_INFORMATION("%s@%d-NTPC refresh period seconds: %d\n", thisFile, __LINE__, ntpc_refresh_period_seconds);

          ntp_servers = (char **)malloc(ntp_server_count * sizeof(char *));
          if (ntp_servers == NULL)
          {
            nerr("ERROR: Failed to allocate memory for NTP servers\n");
            return EXIT_FAILURE;
          }

          MEADOW_TRACE_INFORMATION("%s@%d-NTP servers:\n", thisFile, __LINE__);

          for (int i = 0; i < ntp_server_count; ++i)
          {
            ntp_servers[i] = strdup(config->ntp_servers[i]);
            if (ntp_servers[i] == NULL)
            {
                nerr("ERROR: Failed to copy NTP server string\n");
                /* Free previously allocated strings and array */
                for (int j = 0; j < i; ++j)
                  {
                    free(ntp_servers[j]);
                  }
                free(ntp_servers);
                return EXIT_FAILURE;
            }
            hcom_logging_syslog(LOG_INFO, "%s-%d-%s\n", thisFile, __LINE__, ntp_servers[i]);
            MEADOW_TRACE_INFORMATION("%s@%d-%s\n", thisFile, __LINE__, ntp_servers[i]);
          }

          meadow_os_config_free_resources(config);

          sem_init(&g_ntpc_daemon.interlock, 0, 0);
        }

      /* Start the NTP daemon */

      g_ntpc_daemon.state = NTP_STARTED;
      g_ntpc_daemon.pid =
        task_create("NTP daemon", LPSDAEMON_THREAD_PRIORITY,
                    CONFIG_NETUTILS_NTPCLIENT_STACKSIZE, ntpc_daemon,
                    NULL);

      /* Handle failures to start the NTP daemon */

      if (g_ntpc_daemon.pid < 0)
        {
          int errval = errno;
          DEBUGASSERT(errval > 0);

          g_ntpc_daemon.state = NTP_STOPPED;
          nerr("ERROR: Failed to start the NTP daemon\n", errval);
          sched_unlock();
          return -errval;
        }
    }

  sched_unlock();

  MEADOW_TRACE_INFORMATION("%s@%d-NTP client daemon launched successfully\n", thisFile, __LINE__);

  return g_ntpc_daemon.pid;
}

/****************************************************************************
 * Name: ntpc_stop (deprecated)
 *
 * Description:
 *   Do not use this function to stop the NTP daemon. Instead, send an NTP stop 
 *   message using espcp_send_message_to_ntp_queue() method.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.  The current
 *   implementation only returns success.
 *
 ****************************************************************************/

#ifndef CONFIG_DISABLE_SIGNALS
int ntpc_stop(void)
{
  int ret;

  /* Is the NTP in a running state? */

  sched_lock();
  if (g_ntpc_daemon.state == NTP_STARTED ||
      g_ntpc_daemon.state == NTP_RUNNING)
    {
      /* Yes.. request that the daemon stop. */

      g_ntpc_daemon.state = NTP_STOP_REQUESTED;

      /* Wait for any daemon state change */

      do
        {
          /* Signal the NTP client */

          ret = kill(g_ntpc_daemon.pid,
                     CONFIG_NETUTILS_NTPCLIENT_SIGWAKEUP);

          if (ret < 0)
            {
              nerr("ERROR: kill pid %d failed: %d\n",
                   g_ntpc_daemon.pid, errno);
              break;
            }

          /* Wait for the NTP client to respond to the stop request */

          (void)sem_wait(&g_ntpc_daemon.interlock);
        }
      while (g_ntpc_daemon.state == NTP_STOP_REQUESTED);
    }

  sched_unlock();
  return OK;
}
#endif
