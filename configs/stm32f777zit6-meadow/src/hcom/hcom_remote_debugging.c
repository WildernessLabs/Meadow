/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_remote_debugging.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"

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

static bool _shutting_down;
static FAR struct socket *transmit_sd;

struct remote_dbg_session
{
  FAR struct socket *listen_sd;
  FAR struct socket *connected_sd;
  FAR struct sockaddr_un sock_address; 
  socklen_t addrlen;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
#ifdef CONFIG_BUILD_PROTECTED
static int hcom_remote_dbg_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_remote_dbg_pthread(FAR void *arg);
#endif

static int hcom_remote_dbg_create_infrastructure(void);
static int hcom_remote_dbg_make_thread(void);
static int hcom_remote_dbg_create_server_socket(struct remote_dbg_session *dbgSock);
static int hcom_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);

static int hcom_remote_dbg_accept_connection(struct remote_dbg_session *dbgSock);
static int hcom_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_remote_dbg_setup()
{  
  _shutting_down = false;
  
  // todo - Should this be called by hcom_startup_manager and not from here?
  return hcom_remote_dbg_create_infrastructure();
}

//=======================================================================
void hcom_remote_dbg_shutdown()
{
  _shutting_down = true;
}

//================================================================
static void hcom_remote_dbg_close_and_delay(void)
{
  // Wait and try again
  if(!_shutting_down)
    sleep(5);   // Not a special value, just to prevent hard infinite loop
}

//=======================================================================
int hcom_remote_dbg_create_infrastructure()
{
  int ret;

  // Create a thread to read and forward the debug data
  ret = hcom_remote_dbg_make_thread();
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: hcom_remote_dbg_make_thread failed errno %d\n",
      __func__, HCOM_REMOTE_DBG_SOCKET_NAME, errno);
    return -1;
  }
  
  return OK;
}

//=============================================================
int hcom_remote_dbg_make_thread()
{
  #ifdef CONFIG_BUILD_PROTECTED
    int pid = kthread_create("RemoteDbg",
      120, 2048, (main_t)hcom_remote_dbg_kthread,
      (FAR char * const *)  NULL);
    if(pid <= 0)
    {
      return -ENOEXEC;
    }
  #else
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = 120;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, 1024);

    ret = pthread_create(&thread, &attr, hcom_remote_dbg_pthread, NULL);
    if (ret != OK)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to create thread. Error %s\n", __func__, ret);
      return ret;
    }
  #endif

  return OK;
}

//=================================================================
// This thread receives all pipe messages received from mono
#ifdef CONFIG_BUILD_PROTECTED
int hcom_remote_dbg_kthread(int argc, char *argv[])
#else
FAR void *hcom_remote_dbg_pthread(FAR void *arg)
#endif
{
  int ret;
  struct remote_dbg_session *dbgSock;
  // pid_t pid = getpid();
  // struct tcb_s *rtcb = this_task();
  // syslog(0, "%s() - task = %d, name = '%s'\n", __func__, pid, rtcb->name);

  sleep(1);   // p-m testing so setup message are more obvious

  dbgSock = (struct remote_dbg_session *)kmm_zalloc(sizeof(struct remote_dbg_session));
  if(!dbgSock)
  {
    f7syslog(LOG_ERR, "%s() - dbgSock allocation failed, errno:%d\n", __func__, errno);
    return -ENOMEM;     // Kills thread
  }

  dbgSock->listen_sd = (struct socket *)kmm_zalloc(sizeof(struct socket));
  dbgSock->connected_sd = (struct socket *)kmm_zalloc(sizeof(struct socket));
  if(!dbgSock->listen_sd || !dbgSock->connected_sd)
  {
    f7syslog(LOG_ERR, "%s() - listen_sd or connected_sd allocation failed, errno:%d\n", __func__, errno);
    return -ENOMEM;     // Kills thread
  }

  uint8_t *recvBuffer = malloc(HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  if(recvBuffer == NULL)
  {
    f7syslog(LOG_ERR, "%s() - memory allocation failed, errno:%d\n", __func__, errno);
    ret = -ENOMEM;     // Kills thread
    goto exit_remote_dbg_rcvbuff;
  }

  ret = hcom_remote_dbg_create_server_socket(dbgSock);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "Initialization failure errno: %d\n", errno);
    ret = -1;
    goto exit_remote_dbg_thread;     // Kills thread
  }

  ret = hcom_remote_dbg_connect_and_receive(dbgSock, recvBuffer);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "Initialization failure errno: %d\n", errno);
    ret = -1;
  }

exit_remote_dbg_thread:
  free(recvBuffer);
exit_remote_dbg_rcvbuff:
  free(dbgSock->listen_sd);
  free(dbgSock->connected_sd);
  free(dbgSock);

#ifdef CONFIG_BUILD_PROTECTED
  return ret;
#else
  return NULL;    // Keeps compiler happy
#endif
}

//=========================================================================
// Setup the server socket
int hcom_remote_dbg_create_server_socket(struct remote_dbg_session *dbgSock)
{
  int ret;

  // syslog(0, "D->Entered %s() Will create socket for %s\n", __func__, HCOM_REMOTE_DBG_SOCKET_NAME); usleep(50 * 1000);

  // Create a Unix domain socket (no ip address/port but a UNIX device name)
  ret = psock_socket(PF_LOCAL, SOCK_STREAM, 0, dbgSock->listen_sd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "Socket failure ret:%d errno:%d\n", ret, errno);
    return ret;
  }

  // "Internal OS users of psock_socket() must set the s_crefs field to one if psock_socket() returns success."
  dbgSock->listen_sd->s_crefs++;

  // Set receive timeout to detect server crash and reconnect. 
  // Otherwise, we can get stuck in psock_receive forever.
  struct timeval tv;
  tv.tv_sec  = 5;
  tv.tv_usec = 0;
  ret = psock_setsockopt(dbgSock->listen_sd, SOL_SOCKET, SO_RCVTIMEO,
                          (const void *)&tv, sizeof(tv));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "Socket options failure errno: %d\n", errno);
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
  ret = psock_bind(dbgSock->listen_sd, (struct sockaddr*)&dbgSock->sock_address, dbgSock->addrlen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "Bind failure ret:%d, errno:%d\n", ret, errno);
    return ret;
  }

  // Listen
  ret = psock_listen(dbgSock->listen_sd, 1);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "Listen failure errno %d\n", errno);
    return ret;
  }

  return OK;
}

//=================================================================
int hcom_remote_dbg_accept_connection(struct remote_dbg_session *dbgSock)
{
  // syslog(0, "D->server: Waiting for mono connection request\n"); usleep(50 * 1000);
  
  // Must zero or assertion later on
  memset(dbgSock->connected_sd, 0, sizeof(struct socket));

  // Accept clients
  int ret = psock_accept(dbgSock->listen_sd, (struct sockaddr*)&dbgSock->sock_address, 
          &dbgSock->addrlen, dbgSock->connected_sd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "Accept failure errno: %d\n", errno);
    return ret;
  }
  
  // s_crefs must be set to 1
  dbgSock->connected_sd->s_crefs = 1;

  // syslog(0, "D->server: Mono connection accepted\n"); usleep(50 * 1000);

  transmit_sd = dbgSock->connected_sd;
  return OK;
}

//===================================================================
// Returning from this function will kill the thread
int hcom_remote_dbg_connect_and_receive(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer)
{
  // syslog(0, "D->Server setup Successful. Listening on %s for connection with mono.\n", HCOM_REMOTE_DBG_SOCKET_NAME); usleep(50 * 1000);

  // Loop to accept connections and forward data
  while(!_shutting_down)
  {
    // Blocking call to accept socket connection from mono
    int ret = hcom_remote_dbg_accept_connection(dbgSock);
    if(_shutting_down)
      return OK;
    
    if(ret < 0)
    {
      f7syslog(LOG_ERR, "Accept connection failed. ret:%d, errno:%d\n", ret, errno);
      hcom_remote_dbg_close_and_delay();
      continue;
    }

    // if(transmit_sd == NULL)
    //   syslog(0, "D->Server: transmit_sd is NULL!!\n"); usleep(100* 1000);

    // Read data from mono via socket until error. Error reported in loop.
    hcom_remote_dbg_read_mono_send_to_host_loop(dbgSock, recvBuffer);
    if(_shutting_down)
      return OK;
    
    hcom_remote_dbg_close_and_delay();
  }

  return OK;
}

//=================================================================
// Note: since this is considered a stream we'll just receive and forward
// whatever data happens to be ready.
int hcom_remote_dbg_read_mono_send_to_host_loop(struct remote_dbg_session *dbgSock, uint8_t *recvBuffer)
{
  int ret;
  int nBytesRead;

  while(!_shutting_down)
  {
    // Read from mono
    f7syslog(LOG_INFO, "Will wait until data sent by mono debugging\n");
    nBytesRead = psock_recv(dbgSock->connected_sd, recvBuffer,
                       HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN, 0);
    if (nBytesRead < 0)
    {
      // Note: -ECONNRESET indicates that mono has dropped the connection
      if(nBytesRead != -ECONNRESET)
        f7syslog(LOG_ERR, "Recv failed nBytesRead:%d, errno:%d\n", nBytesRead, errno);
      else
        f7syslog(LOG_ERR, "Recv failed because ECONNRESET\n");
      
      return nBytesRead;
    }
    else if (nBytesRead == 0)
    {
      f7syslog(LOG_INFO, "The client broke the connection (numb read=0\n");
      return nBytesRead;
    }

    f7syslog(LOG_DEBUG, "Forwarding %d bytes to PC for VS\n", nBytesRead);
    // hcom_diag_print_buffer(recvBuffer, nBytesRead, 0);

    // Forward data as-is to host
    ret = hcom_host_msg_bldr_send_simple_buffer_msg(HCOM_MDOW_REQUEST_DEBUGGER_MSG, 
          0, 0, recvBuffer, nBytesRead);
    if(ret < 0)
    {
      f7syslog(LOG_ERR, "Error sending message to host %d errno %d\n", ret, errno);
      return ret;
    }
    // syslog(0, "D->server: %d bytes forwarded to host\n", nBytesRead); usleep(50 * 1000);
  }

  return OK;
}

//==========================================================================
// Data from host - Visual Studio debugging. Forwarded to mono.
// Called by hcom receive thread
void hcom_remote_dbg_recv_host_send_to_mono(const uint8_t *recvPayload, size_t recvPayloadSize, uint32_t userData)
{
  // Forward to mono
  int nbytessent = psock_send(transmit_sd, recvPayload, recvPayloadSize, 0);
  if(nbytessent < 0)
  {
    f7syslog(LOG_ERR, "Error sending message to host %d errno %d\n",nbytessent , errno);
  }

  // syslog(0, "D->server: Received %d bytes from VS. forwarded to Mono.\n", recvPayloadSize); usleep(50 * 1000);
  // hcom_diag_print_buffer(recvPayload, recvPayloadSize, 0); usleep(50 * 1000);
}
