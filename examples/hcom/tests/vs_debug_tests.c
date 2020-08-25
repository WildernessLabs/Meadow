/****************************************************************************
 * \apps\examples\hcom\tests\vs_debug_tests.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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

// This test code is intended to simulate mono interacting with a UNIX socket.
// This is a simple echo server that can exercise the hcom code that will
// eventually be the intermediate between mono and the host.

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MONO_DEBUG_TEST_ECHO_BUFF_SIZE 500

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
// For network testing
#include <sys/socket.h>
#include <sys/un.h>
#endif

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
static char *thisFile = __FILE__;

static int _sockfd = -1;
static bool _connected = false;
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
static int MonoDebugTestInitialize(void);
static FAR void *hcom_vs_debug_test_pthread(FAR void *arg);
static int MonoDebugTestExecute(void);
static int MonoDebugTestConnect(void);
static int MonoDebugTestSend(uint8_t *sendBuffer, int sendSize);
static int MonoDebugTestReceive(uint8_t *recvBuffer);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// One of the developer CLI commands lands here. The userData determines what
// to do.
int MonoVsRemoteDebugTests(uint32_t userData)
{
#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
  switch(userData)
  {
    case 1:
      return MonoDebugTestInitialize();
      break;

  }
#endif
  return OK;
}

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
//=================================================================
// Need to create a pthread to run this test so the caller can return
// to do it's work.
int MonoDebugTestInitialize()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = 120;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, 2048);

    ret = pthread_create(&thread, &attr, hcom_vs_debug_test_pthread, NULL);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread, err:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

  return OK;
}

//=================================================================
FAR void *hcom_vs_debug_test_pthread(FAR void *arg)
{
  // syslog(1, "MTst->Thread running vs debug tests PID:%d\n", getpid());
  MonoDebugTestExecute();
  return NULL;    // Keeps compiler happy
}

//=================================================================
// Receives and echos back to host n times
int MonoDebugTestExecute()
{
  int ret;
  FAR uint8_t *echobuf;

  // syslog(1, "MTst->Mono debug test is starting\n");

  echobuf  = (uint8_t*)malloc(MONO_DEBUG_TEST_ECHO_BUFF_SIZE);
  if (echobuf == NULL)
  {
    // syslog(1, "MTst->failed to allocate echobuf %d long must exit\n", MONO_DEBUG_TEST_ECHO_BUFF_SIZE);
    return -1;
  }

  int xmitCount = 0;

  while(true)
  {
    ret = MonoDebugTestConnect();
    if(ret < 0)
    {
      // syslog(1, "MTst->Connection could not be made\n");
      return -1;
    }
    
    // Loop forever
    do
    {
      xmitCount++;

      // Blocking call. 
      ret = MonoDebugTestReceive(echobuf);
      if(ret < 0)
      {
        // syslog(1, "MTst->Receive failed\n");
        continue;
      }

      // This is an echo client so we send whatever we receive
      // Send
      ret = MonoDebugTestSend(echobuf, ret);
      if(ret < 0)
      {
        // syslog(1, "MTst->Send failed\n");
      }
    } while(xmitCount % 5 != 0);
    
    // syslog(1, "MTst->Closing socket. Will reconnect %d.\n", xmitCount);
    close(_sockfd);
  }

  exit(1);
}

//====================================================
// 
int MonoDebugTestConnect()
{
  struct sockaddr_un myaddr;
  socklen_t addrlen;
  int ret;

  _sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (_sockfd < 0)
  {
    syslog(LOG_ERR, "Error:Socket creation failed _sockfd:%d errno:%d\n", _sockfd, errno);
    goto errout_with_nothing;
  }

  /* Connect the socket to the server */
  addrlen = strlen(HCOM_REMOTE_DBG_SOCKET_NAME);
  if (addrlen > UNIX_PATH_MAX - 1)
    addrlen = UNIX_PATH_MAX - 1;

  myaddr.sun_family = AF_LOCAL;
  strncpy(myaddr.sun_path, HCOM_REMOTE_DBG_SOCKET_NAME, addrlen);
  myaddr.sun_path[addrlen] = '\0';
  addrlen += sizeof(sa_family_t) + 1;

  // syslog(1, "MTst-> Connect to %s...\n", HCOM_REMOTE_DBG_SOCKET_NAME);

  int attemptCnt = 0;
  do
  {
    attemptCnt++;
    ret = connect(_sockfd, (struct sockaddr *)&myaddr, addrlen);
    if (ret < 0)
    {
      syslog(LOG_INFO, "Connect failed, will retry, ret: %d, errno:%d attempt:%d\n",
       ret, errno, attemptCnt);
      sleep(1);
    }
  } while(ret < 0);

  // syslog(1, "MTst->hcom connect attempted:%d SUCCESSFUL\n", attemptCnt);
  _connected = true;  
  return OK;

errout_with_nothing:
  return -1;
}

//==========================================================
int MonoDebugTestSend(uint8_t *sendBuffer, int sendSize)
{
  int nbytessent;
  
  // syslog(1, "MTst->Sending: %d bytes\n", sendSize);
  
  /* Then send one message */
  nbytessent = send(_sockfd, sendBuffer, sendSize, 0);
  if (nbytessent < 0)
  {
    // syslog(1, "MTst->send failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytessent != sendSize)
  {
    // syslog(1, "MTst->Bad send length: %d Expected: %d\n", nbytessent, sendSize);
    goto errout_with_socket;
  }

  // syslog(1, "MTst->Sent %d bytes to hcom\n", sendSize);
  return OK;

errout_with_socket:
  // syslog(1, "MTst->Exit error\n");
  return 1;
}

//-------------------------------------------------------
// This is a blocking call
int MonoDebugTestReceive(uint8_t *recvBuffer)
{
  int nbytesrecvd;  
  
  // syslog(1, "MTst->Waiting to receive VS dbg info via hcom\n");
  nbytesrecvd = recv(_sockfd, recvBuffer, MONO_DEBUG_TEST_ECHO_BUFF_SIZE, 0);
  if (nbytesrecvd < 0)
  {
    // syslog(1, "MTst->recv failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytesrecvd == 0)
  {
    // syslog(1, "MTst->The server closed the connection\n");
    goto errout_with_socket;
  }

  // syslog(1, "MTst->Received %d bytes\n", nbytesrecvd);
  
  //close(_sockfd);
  return nbytesrecvd;

errout_with_socket:
  // syslog(1, "MTst->RECEIVE Exit error\n");
  return -1;
}

#endif
