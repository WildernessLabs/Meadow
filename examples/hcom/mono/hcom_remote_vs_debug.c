/****************************************************************************
 * \apps\examples\hcom\mono\hcom_remote_vs_debug.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

// This is a Unix domain stream socket server. It allows the mono debugging
// interface to interact with hcom to forward the debug information to the host.

// NOTE: This module is a WIP as it has never been put into use. Therefore,
// there are remaining syslog entries that have not been removed.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if HCOM_VS_REMOTE_DEBUGGING_INCLUDE_IN_BUILD > 0
static char *thisFile = __FILE__;

static bool _shutting_down;
static int _transmit_sd;
static bool _hcom_mono_remote_dbg_running;

struct remote_dbg_session
{
  struct sockaddr_un sock_address; 
  int listen_sd;
  int connected_sd;
  socklen_t addrlen;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static FAR void *hcom_mono_remote_dbg_pthread(FAR void *arg);

static int hcom_mono_remote_dbg_create_thread(void);
static int hcom_mono_remote_dbg_create_server_socket(struct remote_dbg_session *dbgSock);
static int hcom_mono_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);

static int hcom_mono_remote_dbg_accept_connection(struct remote_dbg_session *dbgSock);
static int hcom_mono_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#if HCOM_VS_REMOTE_DEBUGGING_INCLUDE_IN_BUILD == 0
int hcom_mono_remote_dbg_setup()
{
  return OK;
}
void hcom_mono_remote_dbg_shutdown()
{
}
#else
int hcom_mono_remote_dbg_setup()
{
  _shutting_down = false;
  _transmit_sd = -1;
  _hcom_mono_remote_dbg_running = false;
  
  return OK;
}

//=======================================================================
// This can only be started by CLI / Visual Studio / VS Code command
int hcom_mono_remote_dbg_lazy_startup(void)
{
  int ret;

  if(_hcom_mono_remote_dbg_running)
    return OK;

  // Create a thread to read and forward the debug data
  ret = hcom_mono_remote_dbg_create_thread();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s thread create, errno:%d\n",
      thisFile, __LINE__, HCOM_REMOTE_DBG_SOCKET_NAME, errno);
    return ret;
  }

  _hcom_mono_remote_dbg_running = true;
  return OK;
}

//=======================================================================
void hcom_mono_remote_dbg_shutdown()
{
  _shutting_down = true;
}

//================================================================
static void hcom_mono_remote_dbg_close_and_delay(void)
{
  // Wait and try again
  if(!_shutting_down)
    sleep(5);   // Not a special value, just to prevent hard infinite loop
}

//=============================================================
// Need a unique thread to run the debugging session
int hcom_mono_remote_dbg_create_thread()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = HCOM_THREAD_PRIORITY_REMOTE_DBG;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_REMOTE_DBG);

    ret = pthread_create(&thread, &attr, hcom_mono_remote_dbg_pthread, NULL);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
                thisFile, __LINE__, HCOM_THREAD_NAME_REMOTE_DBG, ret, errno);
      return ret;
    }

  return OK;
}

//=================================================================
// This thread receives all stdout messages received from mono
FAR void *hcom_mono_remote_dbg_pthread(FAR void *arg)
{
  int ret;
  struct remote_dbg_session *dbgSock;

  dbgSock = (struct remote_dbg_session *)malloc(sizeof(struct remote_dbg_session));
  if(!dbgSock)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-dbgSock allocation, errno:%d\n", thisFile, __LINE__, errno);
    return NULL;
  }

  uint8_t *recvBuffer = malloc(HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  if(recvBuffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-recvBuf alloc, errno:%d\n", thisFile, __LINE__, errno);
    goto exit_remote_dbg_rcvbuff;
  }

  ret = hcom_mono_remote_dbg_create_server_socket(dbgSock);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg server create, ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
    goto exit_remote_dbg_thread;     // Kills thread
  }

  ret = hcom_mono_remote_dbg_connect_and_receive(dbgSock, recvBuffer);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg server connect, ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
  }

exit_remote_dbg_thread:
  free(recvBuffer);
exit_remote_dbg_rcvbuff:
  free(dbgSock);

  return NULL;    // Keeps compiler happy
}

//=========================================================================
// Setup the server socket
int hcom_mono_remote_dbg_create_server_socket(struct remote_dbg_session *dbgSock)
{
  int ret;

  // syslog(1, "VSD->%s@%d-Will create socket %s\n", thisFile, __LINE__, HCOM_REMOTE_DBG_SOCKET_NAME);

  // Create a Unix domain socket. Not with ip address/port but a UNIX device name
  // (i.e. /dev/sockname) added via bind()
  // error -106 is EAFNOSUPPORT Address Family not supported - PF_LOCAL not supported
  dbgSock->listen_sd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (dbgSock->listen_sd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg socket create, ret:%d errno:%d\n",
              thisFile, __LINE__, dbgSock->listen_sd, errno);
    return dbgSock->listen_sd;
  }

  // Set receive timeout to detect server crash and reconnect. 
  // Otherwise, we can get stuck in psock_receive forever.
  struct timeval tv;
  tv.tv_sec  = 5;
  tv.tv_usec = 0;
  ret = setsockopt(dbgSock->listen_sd, SOL_SOCKET, SO_RCVTIMEO,
                          (const void *)&tv, sizeof(tv));
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg sock opt, ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  dbgSock->addrlen = strlen(HCOM_REMOTE_DBG_SOCKET_NAME);
  if (dbgSock->addrlen > UNIX_PATH_MAX - 1)
    dbgSock->addrlen = UNIX_PATH_MAX - 1;

  //Note: the letters 'SC0' & 'CS0' will be appended to the 2 sockets.
  // SC = server to client and CS = client to server
  dbgSock->sock_address.sun_family = AF_LOCAL;
  strncpy(dbgSock->sock_address.sun_path, HCOM_REMOTE_DBG_SOCKET_NAME, dbgSock->addrlen);
  dbgSock->sock_address.sun_path[dbgSock->addrlen] = '\0';

  dbgSock->addrlen += sizeof(sa_family_t) + 1;

  // Bind - assign a name to the nameless socket
  // Note: sockaddr_un allows a longer path to be up to UNIX_PATH_MAX, while
  // 'struct sockaddr' only allows a length of 14
  ret = bind(dbgSock->listen_sd, (struct sockaddr*)&dbgSock->sock_address, dbgSock->addrlen);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg bind, ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  // syslog(1, "VSD->%s@%d-Listening for a connection request %s\n", thisFile, __LINE__, HCOM_REMOTE_DBG_SOCKET_NAME);
  // Listen
  ret = listen(dbgSock->listen_sd, 2);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg listen, ret:%d errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

//===================================================================
// Returning from this function kills the thread
int hcom_mono_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer)
{
  // Loop to accept connections and forward data
  while(!_shutting_down)
  {
    // Blocking call to accept socket connection from mono
    int ret = hcom_mono_remote_dbg_accept_connection(dbgSock);
    
    if(_shutting_down)
      return OK;
    
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d-Accept connection failed. ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      hcom_mono_remote_dbg_close_and_delay();
      continue;
    }

    // Read data from mono via socket until error. Error reported in loop.
    hcom_mono_remote_dbg_read_mono_send_to_host_loop(dbgSock, recvBuffer);
    if(_shutting_down)
      return OK;
    
    hcom_mono_remote_dbg_close_and_delay();
  }

  return OK;
}

//=================================================================
int hcom_mono_remote_dbg_accept_connection(struct remote_dbg_session *dbgSock)
{
  // syslog(1, "VSD->server: Waiting for mono connection request\n");

  // Accept client
  dbgSock->connected_sd = accept(dbgSock->listen_sd, (struct sockaddr*)&dbgSock->sock_address, 
          &dbgSock->addrlen);
  if (dbgSock->connected_sd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg accept, errno:%d\n", thisFile, __LINE__, errno);
    return dbgSock->connected_sd;
  }
  
  // syslog(1, "VSD->server: Mono debug connection accepted\n");

  _transmit_sd = dbgSock->connected_sd;
  return OK;
}

//=================================================================
// Note: since this is considered a stream we'll just receive and
// forward whatever data happens to be ready, assuming the other
// end can piece it back together.
int hcom_mono_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock,
          uint8_t *recvBuffer)
{
  int ret;
  int nBytesRead;

  while(!_shutting_down)
  {
    // Read from mono
    hcom_logging_syslog(LOG_INFO, "%s@%d-Waiting data from mono debug\n", thisFile, __LINE__);
    nBytesRead = recv(dbgSock->connected_sd, recvBuffer,
                       HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN, 0);
    if (nBytesRead < 0)
    {
      // Note: -ECONNRESET indicates that mono has dropped the connection
      if(nBytesRead != -ECONNRESET)
        hcom_logging_syslog(LOG_ERR, "%s@%d-Recv, nBytesRead:%d, errno:%d\n",
                  thisFile, __LINE__, nBytesRead, errno);
      else
        hcom_logging_syslog(LOG_ERR, "%s@%d-Recv, ECONNRESET\n", thisFile, __LINE__);
      
      return nBytesRead;
    }
    else if (nBytesRead == 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-mono broke the connection\n", thisFile, __LINE__);
      return nBytesRead;
    }

    // Received some bytes from mono.
    // syslog(1, "VSD->Server:Forwarding %d bytes to host PC for VS\n", nBytesRead);

    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Forwarding %d bytes to host PC for VS\n",
              thisFile, __LINE__, nBytesRead);
#if HCOM_OUTPUT_DATA_BUFFER_INFO_VIA_SYSLOG > 0
    hcom_utils_diag_print_buffer(recvBuffer, nBytesRead, LOG_DEBUG);
#endif

    // Forward data as-is to CLI to forward to VS
    ret = hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_MONO_DEBUGGER_MSG, 0, (char *)recvBuffer, nBytesRead,
            thisFile, __LINE__);
  }

  return ret;
}

//==========================================================================
// This call is the result of a CLI command --VSDebug plus --VSDebugPort 4024.
// This information is sent directly to mono to initiates host PC / Visual Studio debugging.
void hcom_mono_remote_dbg_recv_host_sending_to_mono(const uint8_t *recvPayload,
        size_t recvPayloadSize, uint32_t userData)
{  
  // If not already created, creates a thread to run debugging
  hcom_mono_remote_dbg_lazy_startup();

  if(_transmit_sd < 1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-message from host but no transmit_sd\n",
            thisFile, __LINE__);
  }

  // Forward to mono
  int nbytessent = send(_transmit_sd, recvPayload, recvPayloadSize, 0);
  if(nbytessent < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-message from host, errno:%d\n",
            thisFile, __LINE__, errno);
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Received %d bytes from VS. forwarded to Mono.\n",
          thisFile, __LINE__, recvPayloadSize);
#if HCOM_OUTPUT_DATA_BUFFER_INFO_VIA_SYSLOG > 0
  hcom_utils_diag_print_buffer(recvPayload, recvPayloadSize, LOG_DEBUG);
#endif
}
#endif
