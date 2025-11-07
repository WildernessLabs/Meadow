/****************************************************************************
 * /apps/examples/hcom/tests/ethernet_chat_test.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

// This code was originally modeled after /tcpblaster/tcpblaster_server.c

// This is only a test to determine if TCP is working, nothing more.
// There are a couple of problems!
// 1) It can only connect with one client, ever. And cannot reconnect to the
//    same client.
// 2) This code runs via the HCOM receive thread so, no more HCOM.
// 3) When the client is terminated Meadow must be restarted.
// 4) Only works once.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>

// #include "tcpblaster.h"
#include <netutils/netlib.h>

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/meadow_ethnet_common.h>
#include <meadow/hcom_shared_common.h>

#define ETHERNET_CHAT_TEST_PORT_NO (65123)
#define ETHERNET_CHAT_TEST_BUF_SIZE (4096)
#define ETHERNET_CHAT_MAGIC_ERROR_NUMB (0xef98765) 

#if defined(CONFIG_ETH_CHAT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Lingers on a close() if data is present. This option controls the action
// taken when unsent messages queue on a socket and close() is performed. If
// SO_LINGER is set, the system shall block the process during close() until
// it can transmit the data or until the time expires. 
// #define ENET_CHAT_USE_SOCKET_OPTION_SO_LINGER
// #define ENET_CHAT_MANAGE_RECV_WITH_POLL

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int echo_message_to_sender(int sockfd, char *recvBuff, size_t recvSize);
static FAR void *diag_ethernet_chat_thread(FAR void *arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from a CLI developer -p 5 -v 0 to begin running, userData is not used
void diag_ethernet_chat_server(uint32_t userData)
{
  // Create a thread to run the chat server
  int ret;
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;

  param.sched_priority = HCOM_THREAD_PRIORITY_HCOM_RECEIVE;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_HCOM_RECEIVE);

  ret = pthread_create(&thread, &attr, diag_ethernet_chat_thread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
              __FILE__, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret, errno);
    return;
  }

  return;
}

//=================================================================
// This thread executes the chat test
FAR void *diag_ethernet_chat_thread(FAR void *arg)
{
  int ret;
  struct sockaddr_in myaddr;
#ifdef ENET_CHAT_USE_SOCKET_OPTION_SO_LINGER
  struct linger ling;
#endif
  socklen_t addrlen;
  char *buffer;
  int listensd;
  int acceptsd;
  int nbytesread;
  int optval;
  struct in_addr ipaddr;

  syslog(LOG_INFO, "chat-Meadow Ethernet Chat Server starting\n");

  buffer = (char*)malloc(ETHERNET_CHAT_TEST_BUF_SIZE);
  if (!buffer)
  {
    syslog(LOG_ERR, "Chat Server:failed to allocate buffer\n");
    exit(1);
  }

  /* Create a new TCP socket */

  listensd = socket(PF_INET, SOCK_STREAM, 0);
  if (listensd < 0)
  {
    syslog(LOG_ERR, "Chat Server:socket failure: %d\n", errno);
    goto errout_with_buffer;
  }

  /* Set socket to reuse address */

  optval = 1;
  if (setsockopt(listensd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, sizeof(int)) < 0)
  {
    syslog(LOG_ERR, "Chat Server:setsockopt SO_REUSEADDR failure: %d\n", errno);
    goto errout_with_listensd;
  }

  /* Bind the socket to our assigned ip */

  netlib_get_ipv4addr(MEADOW_ETHMAC_DEVICENAME, &ipaddr);
  myaddr.sin_addr.s_addr        = htonl(ipaddr.s_addr);
  myaddr.sin_family             = AF_INET;
  myaddr.sin_port               = htons(ETHERNET_CHAT_TEST_PORT_NO);

  addrlen = sizeof(struct sockaddr_in);

  syslog(LOG_INFO, "chat-Binding to IPv4 Address: %08lx\n",
         (unsigned long)myaddr.sin_addr.s_addr);

  if (bind(listensd, (struct sockaddr*)&myaddr, addrlen) < 0)
  {
    syslog(LOG_ERR, "Chat Server:bind failure: %d\n", errno);
    goto errout_with_listensd;
  }

  /* Listen for connections on the bound TCP socket */

  if (listen(listensd, 5) < 0)
  {
    syslog(LOG_ERR, "Chat Server:listen failure %d\n", errno);
    goto errout_with_listensd;
  }

  /* Accept only one connection */

  syslog(LOG_INFO, "chat-Chat Server:Accepting connections on port %d\n",
         ETHERNET_CHAT_TEST_PORT_NO);

  acceptsd = accept(listensd, (struct sockaddr*)&myaddr, &addrlen);
  if (acceptsd < 0)
  {
    syslog(LOG_ERR, "Chat Server:accept failure: %d\n", errno);
    goto errout_with_listensd;
  }

  syslog(LOG_INFO, "chat-Chat Server:Connection accepted -- receiving\n");

  /* Configure to "linger" until all data is sent when the socket is closed */

#ifdef ENET_CHAT_USE_SOCKET_OPTION_SO_LINGER
  ling.l_onoff  = 1;
  ling.l_linger = 30;     /* timeout is seconds */

  if (setsockopt(acceptsd, SOL_SOCKET, SO_LINGER, &ling, sizeof(struct linger)) < 0)
  {
    syslog(LOG_ERR, "Chat Server:setsockopt SO_LINGER failure: %d\n", errno);
    goto errout_with_acceptsd;
  }
#endif

  /* Then receive data forever */

  int recvCount = 0;
  for (; ; )
  {
#ifdef ENET_CHAT_MANAGE_RECV_WITH_POLL
    struct pollfd fds[1];
    int ret;

    memset(fds, 0, 1 * sizeof(struct pollfd));
    fds[0].fd     = acceptsd;
    fds[0].events = POLLIN | POLLHUP;

    /* Wait until we can receive data or until the connection is lost */

    ret = poll(fds, 1, -1);
    if (ret < 0)
    {
      syslog(LOG_ERR, "Chat Server:ERROR poll failed: %d\n", errno);
      goto errout_with_acceptsd;
    }

    if ((fds[0].revents & POLLHUP) != 0)
    {
      syslog(LOG_WARNING, "Chat Server:WARNING poll returned POLLHUP\n");
      goto errout_with_acceptsd;
    }
#endif

    nbytesread = recv(acceptsd, buffer, ETHERNET_CHAT_TEST_BUF_SIZE, 0);
    if (nbytesread < 0)
    {
      syslog(LOG_ERR, "Chat Server:recv failed. errno:%d, nbytesread:%d\n", errno, nbytesread);
      goto errout_with_acceptsd;
    }
    else if (nbytesread == 0)
    {
      syslog(LOG_WARNING, "Chat Server:The client broke the connection\n");
      goto errout_with_acceptsd;
    }

    recvCount++;
    // if((recvCount % 10) == 0)
    //   syslog(LOG_INFO, "%d-Rcvd %d bytes\n", recvCount, nbytesread);

#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
    hcom_diag_print_buffer((uint8_t*)buffer, nbytesread, LOG_INFO);
#endif
    ret = echo_message_to_sender(acceptsd, buffer, nbytesread);
    if(ret < 0)
    {
      syslog(LOG_ERR, "Chat Server:Attempt to send failed:0x%08x, errno:%d\n",
                ret, errno);
      
      if(ret == -ETHERNET_CHAT_MAGIC_ERROR_NUMB)
        goto errout_with_acceptsd;
    }
    // else
    // {
    //   syslog(LOG_INFO, "Chat Server:success\n");
    //   syslog(LOG_INFO, "=================================================\n\n");
    // }
  }

errout_with_acceptsd:
  close(acceptsd);

errout_with_listensd:
  close(listensd);

errout_with_buffer:
  free(buffer);
  exit(1);

  return NULL;
}

//=====================================================================
// Reversion the order and sends back to client
int echo_message_to_sender(int sockfd, char *recvBuff, size_t recvSize)
{
  int ret = OK;
  char *outbuf;
  ssize_t nbytessent;

  outbuf = (char*)malloc(ETHERNET_CHAT_TEST_BUF_SIZE);
  if (!outbuf)
  {
    syslog(LOG_ERR, "Chat Server:failed to allocate buffer\n");
    return -ENOMEM;
  }

#if (0)
  // Reverse the data
  off_t recvOff = recvSize - 1;
  for(int i = 0; i < recvSize; i++)
  {
    outbuf[i] = recvBuff[recvOff - i];  // Reverse
  }
#else
  // Just echo
  memcpy(outbuf, recvBuff, recvSize);
#endif

  nbytessent = send(sockfd, outbuf, recvSize, 0);
  if (nbytessent < 0)
  {
    syslog(LOG_ERR, "Chat Server: send failed: %d\n", errno);
    free(outbuf);
    return -ETHERNET_CHAT_MAGIC_ERROR_NUMB;    // Magic number
  }
  else if (nbytessent > 0 && nbytessent < recvSize)
  {
    /* Partial buffers can be sent if there is insufficient buffering
      * space to buffer the whole SENDSIZE request.  This is not an
      * error, but is an interesting thing to keep track of.
      */

    syslog(LOG_ERR, "Chat Server:Only partial message sent (%d of %d)\n",
            nbytessent, recvSize);
  }
  else if (nbytessent != recvSize)
  {
    syslog(LOG_ERR, "Chat Server: Bad send length (%d of %d)\n",
            nbytessent, recvSize);
  }
  else
  {
    // This is normal behavior
    syslog(LOG_ERR, "-->Chat Server:Successfully echoed %d of %d bytes\n",
            nbytessent, recvSize);
  }
  
  free(outbuf);
  return ret;
}

#endif // defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
