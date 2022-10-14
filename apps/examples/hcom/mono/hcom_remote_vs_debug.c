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
#include <meadow/hcom_bbreg_defn.h>

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
#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 

static char *thisFile = __FILE__;
static bool _shutting_down;
static int _transmit_sd;
static bool _hcom_mono_remote_dbg_running;
static bool _hcom_mono_remote_dbg_socket_active;

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
static int hcom_mono_remote_dbg_startup(void);
static int hcom_mono_remote_dbg_create_thread(void);
static int hcom_mono_remote_dbg_create_server_socket(struct remote_dbg_session *dbgSock);
static int hcom_mono_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);

static int hcom_mono_remote_dbg_accept_connection(struct remote_dbg_session *dbgSock);
static void hcom_mono_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
int hcom_mono_remote_dbg_setup()
{
  _shutting_down = false;
  _transmit_sd = -1;
  _hcom_mono_remote_dbg_running = false;
  _hcom_mono_remote_dbg_socket_active = false;

  // If required start VS Debugging
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_MONO_DEBUGGING_START_BIT))
  {
    hcom_mono_remote_dbg_startup();
    
    // Clear bit so no future Meadow restart will start debugging
    hcom_bbreg_clear_bbr_bits(HCOM_BBREG_MONO_DEBUGGING_START_BIT);
  }

  return OK;
}

//=======================================================================
bool hcom_mono_remote_dbg_is_active()
{
  return _hcom_mono_remote_dbg_socket_active;
}

//=======================================================================
// This can only be started by CLI / Visual Studio / VS Code command
int hcom_mono_remote_dbg_startup(void)
{
  int ret;

  if(_hcom_mono_remote_dbg_running)
    return OK;

  // Create a thread to read and forward the debug data
  ret = hcom_mono_remote_dbg_create_thread();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s thread create, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_REMOTE_DBG_SOCKET_NAME, errno);
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
    usleep(500 * 1000);   // Not a special value, just to prevent hard infinite loop
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
// This thread receives all messages received from mono
FAR void *hcom_mono_remote_dbg_pthread(FAR void *arg)
{
  int ret;
  struct remote_dbg_session *dbgSock;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_REMOTE_DBG);
#endif

  dbgSock = (struct remote_dbg_session *)malloc(sizeof(struct remote_dbg_session));
  if(!dbgSock)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-dbgSock allocation, errno:%d\n",
              thisFile, __LINE__, errno);
    return NULL;
  }

  uint8_t *recvBuffer = malloc(HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
  if(recvBuffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-recvBuf alloc, errno:%d\n",
              thisFile, __LINE__, errno);
    goto exit_remote_dbg_rcvbuff;
  }

  ret = hcom_mono_remote_dbg_create_server_socket(dbgSock);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg server create, ret:%d errno:%d\n",
              thisFile, __LINE__, ret, errno);
    goto exit_remote_dbg_thread;     // Kills thread
  }

  ret = hcom_mono_remote_dbg_connect_and_receive(dbgSock, recvBuffer);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg server connect, ret:%d errno:%d\n",
              thisFile, __LINE__, ret, errno);
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
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg sock opt, ret:%d errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  dbgSock->addrlen = strlen(HCOM_MONO_REMOTE_DBG_SOCKET_NAME);
  if (dbgSock->addrlen > UNIX_PATH_MAX - 1)
    dbgSock->addrlen = UNIX_PATH_MAX - 1;

  //Note: the letters 'SC0' & 'CS0' will be appended to the 2 sockets.
  // SC = server to client and CS = client to server
  dbgSock->sock_address.sun_family = AF_LOCAL;
  strncpy(dbgSock->sock_address.sun_path, HCOM_MONO_REMOTE_DBG_SOCKET_NAME,
            dbgSock->addrlen);
  dbgSock->sock_address.sun_path[dbgSock->addrlen] = '\0';

  dbgSock->addrlen += sizeof(sa_family_t) + 1;

  // Bind - assign a name to the nameless socket
  // Note: sockaddr_un allows a longer path to be up to UNIX_PATH_MAX, while
  // 'struct sockaddr' only allows a length of 14
  ret = bind(dbgSock->listen_sd, (struct sockaddr*)&dbgSock->sock_address,
            dbgSock->addrlen);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg bind, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Listen
  ret = listen(dbgSock->listen_sd, 2);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg listen, ret:%d errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  _hcom_mono_remote_dbg_socket_active = true;
  return OK;
}

//===================================================================
// Returning from this function kills the thread
int hcom_mono_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock,
          uint8_t *recvBuffer)
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
  // Accept client
  dbgSock->connected_sd = accept(dbgSock->listen_sd,
            (struct sockaddr*)&dbgSock->sock_address, 
          &dbgSock->addrlen);
  if (dbgSock->connected_sd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Remote dbg accept, errno:%d\n",
              thisFile, __LINE__, errno);
    return dbgSock->connected_sd;
  }
  
  _transmit_sd = dbgSock->connected_sd;
  return OK;
}

//=================================================================
// The next 2 functions send / received debugging information to/from
// the host PC/Mac
//=================================================================
// This function forwards the mono generated debugging information to CLI
// which will forward it to Visual Studio
// Note: since this is cons idered a binary stream we'll just receive
// and forward whatever data happens to be ready, assuming the other
// end can piece it back together.
void hcom_mono_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock,
          uint8_t *recvBuffer)
{
  int nBytesRead;
  int firstDebugMessage = 1;

  while(!_shutting_down)
  {
    // Read from mono
    hcom_logging_syslog(LOG_INFO, "%s@%d-Waiting data from mono debug\n",
              thisFile, __LINE__);

    nBytesRead = recv(dbgSock->connected_sd, recvBuffer,
                       HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN, 0);
    if (nBytesRead < 0)
    {
      if(errno == ECONNRESET)
        // Note: ECONNRESET indicates that mono has dropped the connection
        hcom_logging_syslog(LOG_WARNING, "%s@%d-Mono dropped connection\n",
                  thisFile, __LINE__);
      else
        hcom_logging_syslog(LOG_ERR, "%s@%d-Recv, nBytesRead:%d, errno:%d\n",
                  thisFile, __LINE__, nBytesRead, errno);
      return;
    }
    else if (nBytesRead == 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-mono broke the connection\n",
                thisFile, __LINE__);
      return;
    }

    // Received some bytes from mono.
#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Forwarding %d bytes to host PC for VS\n",
              thisFile, __LINE__, nBytesRead);
#endif

    //
    //  For some reason the first message is being missed by Visual Studio, the 2s
    //  delay for the first message allows the two systems (VS & OS) to establish
    //  communication.
    //
    //  TODO: Long term solution is required.
    //
    if (firstDebugMessage == 1)
    {
      firstDebugMessage = 0;
      usleep(2000000);
    }

    // Forward data as-is to CLI to forward to VS
    hcom_host_send_binary_data_msg(HCOM_HOST_REQUEST_DEBUGGING_MONO_DATA, 0,
            recvBuffer, nBytesRead, thisFile, __LINE__);
  }
}

//==========================================================================
// Called with data from CLI. Our job forward to mono.
void hcom_mono_remote_dbg_recv_host_sending_to_mono(const HcomProtoHdrMsg_t *hdrMsg,
        size_t packetSize, uint32_t userData)
{
  HcomProtoBinMsg_t *binMsg = (HcomProtoBinMsg_t *)hdrMsg;

  if(_transmit_sd < 1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-message from host but no transmit_sd\n",
            thisFile, __LINE__);
  }

  size_t dbgDataLen = packetSize - HCOM_PROTOCOL_BIN_DATA_OFFSET;

  // Forward to mono
  int nbytessent = send(_transmit_sd, binMsg->binData, dbgDataLen, 0);
  if(nbytessent < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-message from host, errno:%d\n",
            thisFile, __LINE__, errno);
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Received %d bytes from VS. forwarded to Mono.\n",
          thisFile, __LINE__, packetSize);
#endif

}

//======================================================================================
// This call is the result of a CLI command --StartDebugging
// Called from Meadow.CLI to enable visual studio debugging support.
void hcom_mono_remote_dbg_enable(uint32_t userData)
{
  hcom_bbreg_set_bbr_bits(HCOM_BBREG_MONO_DEBUGGING_START_BIT);
}
#else   // #if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
int hcom_mono_remote_dbg_setup()
{
  return OK;
}
void hcom_mono_remote_dbg_shutdown()
{
}
#endif
