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

static int _socksd = -1;
//static bool _connected = false;
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
static int MonoDebugTestInitialize(void);
static FAR void *hcom_vs_debug_test_pthread(FAR void *arg);
static int MonoDebugTestExecute(void);
// static int MonoDebugTestConnect(void);
static int MonoDebugTestSend(uint8_t *sendBuffer, int sendSize);
static int MonoDebugTestReceive(uint8_t *recvBuffer);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// hcom_mono_control.c calls here instead of mono_main for testing.
// Based on the command line args we find the socket descriptor and sve it.
int MonoVsRemoteDebugTestSetup(int argc, char *argv[])
{
  while(argc-- > 0)
  {
    char *equalSign = strchr(*argv, '=');
    int offset = equalSign - *argv;
    int cmp = strncmp(HCOM_MONO_REMOTE_DBG_CMD_LINE_SD, *argv, offset);
    if(cmp == 0)
    {
      // skip '='
      _socksd = atoi((*argv) + offset + 1);      
      break;
    }
    argv++;
  }
  // syslog(2, "DBGTest->Setup:found SD:%d\n", _socksd);

  int ret = MonoDebugTestInitialize();
  return ret;
}

//=================================================================
// Called via CLI to kick off test. It will receive and echo back to
// host a few times.
// Note: Need to create a pthread to run this test so the caller
// (hcom receive thread) can return to do it's work.
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

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(1, "New pthread [PID:%d],'%s'\n", getpid(), "debug_test_pthread");
#endif

  MonoDebugTestExecute();
  return NULL;    // Keeps compiler happy
}

//=================================================================
// pthread lives here
int MonoDebugTestExecute(void)
{
  int ret;
  FAR uint8_t *echobuf;

  // syslog(2, "DBGTest->Exec:Mono debug test is starting with SD:%d\n", _socksd);

  echobuf  = (uint8_t*)malloc(MONO_DEBUG_TEST_ECHO_BUFF_SIZE);
  if (echobuf == NULL)
  {
    syslog(2, "DBGTest->Exec:failed to allocate echobuf %d long must exit\n", MONO_DEBUG_TEST_ECHO_BUFF_SIZE);
    return -1;
  }

  int xmitCount = 10;
  while(xmitCount > 0)
  {
    xmitCount--;
    // The following was used before the SD was provided by the caller.
    // This was left if it is discovered that mono cannot depend on
    // the caller provided open serial descriptor.
    //
    // hcom_mono_control opens connection
    // ret = MonoDebugTestConnect();
    // if(ret < 0)
    // {
      // syslog(2, "DBGTest->Connection could not be made\n");
    //   return -1;
    // }

    // Blocking call.
    // syslog(2, "DBGTest->Exec:Waiting for message:%d\n", xmitCount);
    ret = MonoDebugTestReceive(echobuf);
    if(ret < 0)
    {
      syslog(2, "DBGTest->Receive failed\n"); usleep(20 * 1000);
      continue;
    }

    // This is an echo client so we send whatever we receive
    // Send
    // syslog(2, "DBGTest->Exec:Sending message:%d\n", xmitCount);
    ret = MonoDebugTestSend(echobuf, ret);
    if(ret < 0)
    {
      syslog(2, "DBGTest->Exec:Send failed\n");
    }
  }

  // syslog(2, "DBGTest->Exec:Echoed %d times or error. Bye!\n", xmitCount);
  exit(1);
}

//====================================================
// NO LONGER NEEDED
// int MonoDebugTestConnect()
// {
//   struct sockaddr_un myaddr;
//   socklen_t addrlen;
//   int ret;

//   _socksd = socket(PF_LOCAL, SOCK_STREAM, 0);
//   if (_socksd < 0)
//   {
//     syslog(LOG_ERR, "Error:Socket creation failed _socksd:%d errno:%d\n", _socksd, errno);
//     goto errout_with_nothing;
//   }

//   /* Connect the socket to the server */
//   addrlen = strlen(HCOM_MONO_REMOTE_DBG_SOCKET_NAME);
//   if (addrlen > UNIX_PATH_MAX - 1)
//     addrlen = UNIX_PATH_MAX - 1;

//   myaddr.sun_family = AF_LOCAL;
//   strncpy(myaddr.sun_path, HCOM_MONO_REMOTE_DBG_SOCKET_NAME, addrlen);
//   myaddr.sun_path[addrlen] = '\0';
//   addrlen += sizeof(sa_family_t) + 1;

// // syslog(2, "DBGTest-> Connect to %s...\n", HCOM_MONO_REMOTE_DBG_SOCKET_NAME);

//   int attemptCnt = 0;
//   do
//   {
//     attemptCnt++;
//     ret = connect(_socksd, (struct sockaddr *)&myaddr, addrlen);
//     if (ret < 0)
//     {
//       syslog(LOG_INFO, "Connect failed, will retry, ret: %d, errno:%d attempt:%d\n",
//        ret, errno, attemptCnt);
//       sleep(1);
//     }
//   } while(ret < 0);

//   // syslog(2, "DBGTest->hcom connect attempted:%d SUCCESSFUL\n", attemptCnt);
//   _connected = true;  
//   return OK;

// errout_with_nothing:
//   return -1;
// }

//==========================================================
int MonoDebugTestSend(uint8_t *sendBuffer, int sendSize)
{
  int nbytessent;
  
  // syslog(2, "DBGTest->Sending: %d bytes\n", sendSize);
  
  /* Then send one message */
  nbytessent = send(_socksd, sendBuffer, sendSize, 0);
  if (nbytessent < 0)
  {
    syslog(2, "DBGTest->send failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytessent != sendSize)
  {
    syslog(2, "DBGTest->Bad send length: %d Expected: %d\n", nbytessent, sendSize);
    goto errout_with_socket;
  }

  // syslog(2, "DBGTest->Sent %d bytes to hcom\n", sendSize);
  return OK;

errout_with_socket:
  syslog(2, "DBGTest->Exit error\n");
  return 1;
}

//-------------------------------------------------------
// This is a blocking call
int MonoDebugTestReceive(uint8_t *recvBuffer)
{
  int nbytesrecvd;  
  
  // syslog(2, "DBGTest->Receive:Waiting to receive VS dbg info via hcom sd:%d\n", _socksd);
  nbytesrecvd = recv(_socksd, recvBuffer, MONO_DEBUG_TEST_ECHO_BUFF_SIZE, 0);
  if (nbytesrecvd < 0)
  {
    syslog(2, "DBGTest->Receive:recv failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytesrecvd == 0)
  {
    syslog(2, "DBGTest->Receive:The server closed the connection\n");
    goto errout_with_socket;
  }

  // syslog(2, "DBGTest->Received %d bytes\n", nbytesrecvd);
  return nbytesrecvd;

errout_with_socket:
  syslog(2, "DBGTest->RECEIVE Exit error\n");
  return -1;
}

#endif
