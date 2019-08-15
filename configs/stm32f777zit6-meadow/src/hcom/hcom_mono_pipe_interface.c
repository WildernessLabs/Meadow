/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_mono_pipe_interface.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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

#include "hcom_common.h"
#include <nuttx/kthread.h>
#include <sys/stat.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE 256

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;
static int _pipe_fd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_mono_pipe_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_mono_pipe_pthread(FAR void *arg);
#endif

static int hcom_mono_pipe_make_thread(void);
static int hcom_mono_pipe_open_pipe(void);
static int hcom_mono_pipe_read_pipe_loop(void);
static int hcom_mono_pipe_route_message(uint8_t *recvBuff, int numbBytes);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// This must be called by the 'hcom main thread'
int hcom_mono_pipe_setup()
{
  int ret;

syslog(0, "pipe->%s() - Entry. calling mkfifo\n", __func__); //sleep(1);

  // Create named pipe
  ret = mkfifo(HCOM_MONO_STDOUT_REDIRECT_PIPE, 0666);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: mkfifo of %s failed with errno=%d\n",
      __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errno);
    return -1;
  }

  // Create a thread to read the pipe
syslog(0, "pipe->%s() - Entry. calling hcom_mono_pipe_make_thread\n", __func__); //sleep(1);

  ret = hcom_mono_pipe_make_thread();
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: hcom_mono_pipe_make_thread failed with errno=%d\n",
      __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errno);
    return -1;
  }

syslog(0, "pipe->%s() - Exiting.\n", __func__); //sleep(1);
  return OK;
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
void hcom_mono_pipe_shutdown()
{
  _shutting_down = true;

  int ret = close(_pipe_fd);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: close of %s failed with errno=%d\n",
      __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errno);
  }
  _pipe_fd = -1;
}

//=============================================================
int hcom_mono_pipe_make_thread()
{
syslog(0, "%s() - Entered. About to create MonoPipe thread\n", __func__); sleep(2);

  #ifdef CONFIG_BUILD_PROTECTED
    int pid = kthread_create("MonoPipe",
      100, 1024, (main_t)hcom_mono_pipe_kthread,
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

    param.sched_priority = 100;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, 1024);

    ret = pthread_create(&thread, &attr, hcom_mono_pipe_pthread, NULL);
    if (ret != OK)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to create thread. Error %s\n", __func__, ret);
      return ret;
    }
  #endif

  return OK;
}

//================================================================
static void hcom_mono_pipe_close_and_delay(bool closeNeeded)
{
  if(closeNeeded)
    close(_pipe_fd);
  _pipe_fd = -1;

  // Wait and try again
  if(!_shutting_down)
    sleep(5);   // Noa special value, just no hard infinite loop
}

//=================================================================
// This thread receives all pipe messages received from mono
#ifdef CONFIG_BUILD_PROTECTED
int hcom_mono_pipe_kthread(int argc, char *argv[])
#else
FAR void *hcom_mono_pipe_pthread(FAR void *arg)
#endif
{
  int ret;
  bool isFirstTime = true;

syslog(0, "pipe->%s() - Entering pipe open / read loop\n", __func__);

// TESTING!!!
//syslog(0, "pipe->%s() - Wait 20 seconds, then Create thread which starts the pipe reading\n", __func__);
//sleep(20);    // TIME FOR MONO PIPE TESTER TO RUN A BIT

  while(!_shutting_down)
  {
    ret = hcom_mono_pipe_open_pipe();
    if(ret < 0)
    {
      hcom_mono_pipe_close_and_delay(false);      
      continue;
    }

    if(isFirstTime)
    {
      isFirstTime = false;
      hcom_exec_rqst_misc_developer_3(0);      
    }

    ret = hcom_mono_pipe_read_pipe_loop();
    if(ret < 0)
    {
      hcom_mono_pipe_close_and_delay(true);      
    }
  }

#ifdef CONFIG_BUILD_PROTECTED
  return OK;
#else
  return NULL;    // Keeps compiler happy
#endif
}

//=================================================================
int hcom_mono_pipe_open_pipe()
{
  syslog(0, "pipe->%s() - Entry about to open from pipe this will block till message written\n", __func__);

  // The docs say that is open call will block until some writer opens the pipe
  _pipe_fd = open(HCOM_MONO_STDOUT_REDIRECT_PIPE, O_RDONLY);
  syslog(0, "pipe->%s() - pipe open returned with fd of %d\n", __func__, _pipe_fd);
  if (_pipe_fd < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: open() of %s failed with errno=%d\n",
      __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errno);
    return -1;
  }

  return OK;
}

//=================================================================
int hcom_mono_pipe_read_pipe_loop()
{
  uint8_t buffer[HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE];
  ssize_t readReturn;

  syslog(0, "pipe->%s() - Entering pipe read loop. Will stay in loop till pipe closed.\n", __func__);

  // Read and send to host. Whatever is read is sent. The host receiving
  // app can rebuild the message even if fragmented.
  while (!_shutting_down)
  {
    readReturn = read(_pipe_fd, buffer, HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE);
    if (readReturn < 0 )
    {
      syslog(0, "pipe->%s() - pipe read readReturn = %d Errno %d \n", __func__, readReturn, errno);
      f7syslog(LOG_ERR, "Error: pipe read failed, errno=%d\n", errno);
      return -errno;
    }
    else if (readReturn == 0)    // EOF, last writer closed pipe
    {
      syslog(0, "pipe->%s() - pipe read returned EOF i.e. readReturn = 0\n", __func__);
      f7syslog(LOG_WARNING, "Warning: pipe read returned EOF\n");
      sleep(1);
      continue;
    }
    else
    {
      // Successful pipe read, message or part of message
      f7syslog(LOG_DEBUG, "%s() - Read %d bytes from pipe'%s'\n", __func__, readReturn, buffer);
      int ret = hcom_mono_pipe_route_message(buffer, readReturn);
      syslog(0, "pipe->%s() - Returning from hcom_mono_pipe_route_message.\n", __func__, ret);
    }
  }

  return OK;
}

//=================================================================
// Ship the text from mono app to USB and to host PC
int hcom_mono_pipe_route_message(uint8_t *recvBuff, int numbBytes)
{
  char *hostTextMsg;
  int availBufSpace;

syslog(0, "pipe->%s() - Entered.\n", __func__);

  hostTextMsg = malloc(HCOM_MAX_RETURN_TEXT_TO_HOST);
  if(hostTextMsg == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Memory allocation failed\n", __func__);
    return -1;
  }

  // The message begins with "MonoMsg: " for the receiver to know it's source
  strcpy(hostTextMsg, "MonoMsg: ");
  int preambleLen = strlen("MonoMsg: ");

  // Make sure will fit in allocated buffer, if not truncate
  if(preambleLen + numbBytes >= HCOM_MAX_RETURN_TEXT_TO_HOST)
    availBufSpace = HCOM_MAX_RETURN_TEXT_TO_HOST - preambleLen - 1;
  else
    availBufSpace = numbBytes;
  
  memcpy(hostTextMsg + preambleLen, recvBuff, availBufSpace);

  int totalLength = availBufSpace + preambleLen;
  hostTextMsg[totalLength] = '\0'; // Must null terminate text for CLI implementationsyslog(0, "%s() Entered.\n", __func__);

// syslog(0, "pipe->%s() - Routing '%s' to host\n", __func__, hostTextMsg);

  int ret = hcom_host_msg_bldr_send_text(hostTextMsg, totalLength);
  if (ret < 0)
  {
    if(ret != -EAGAIN)      // Transmission blocked (EAGAIN) is not an error worth mentioning
      f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
//   else
//   {
// syslog(0, "pipe->%s() - Success - returned from sending '%s' to  host PC.\n", __func__, hostTextMsg);    
//   }

  free(hostTextMsg);
  return ret;
}