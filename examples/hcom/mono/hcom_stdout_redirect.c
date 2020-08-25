/****************************************************************************
 * \examples\hcom\hcom_stdout_redirect.c
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

// This module is responsible for creating a pipe and redirecting stdout to
// this pipe. And then reading the pipe and routing this information to the host 
// PC/Mac for display via Meadow.CLI

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

#include <sys/stat.h>
#include <ctype.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if HCOM_STDOUT_REDIRECT_INCLUDE_IN_BUILD > 0

#define HCOM_PIPE_READ_OFFSET 0
#define HCOM_PIPE_WRITE_OFFSET 1

/* Configuration ************************************************************/

#define HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE 384

#if (HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE >= HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN)
  #warning "HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE cannot exceed the size of HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static int _pipefd[2];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_mono_stdout_pthread(FAR void *arg);
static int hcom_mono_stdout_create_infrastructure(void);
static int hcom_mono_stdout_make_thread(void);
static int hcom_mono_stdout_read_pipe_loop(void);
static int hcom_mono_stdout_route_mono_text_stdout(uint8_t *recvBuff, int numbBytes);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if HCOM_STDOUT_REDIRECT_INCLUDE_IN_BUILD == 0
int hcom_mono_stdout_setup()
{
  return OK;
}

void hcom_mono_stdout_shutdown()
{
}
#else
int hcom_mono_stdout_setup()
{
  _shutting_down = false;

  return hcom_mono_stdout_create_infrastructure();
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
void hcom_mono_stdout_shutdown()
{
  _shutting_down = true;

  int ret = close(_pipefd[HCOM_PIPE_READ_OFFSET]);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d close read, errno:%d\n",
      thisFile, __LINE__, errno);
  }

  ret = close(_pipefd[HCOM_PIPE_WRITE_OFFSET]);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d close write, errno:%d\n",
      thisFile, __LINE__, errno);
  }
}

//==========================================================================
int hcom_mono_stdout_create_infrastructure()
{
  int ret;

  // Create and open both ends of pipe
  ret = pipe(_pipefd);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-pipe, errno:%d\n",
      thisFile, __LINE__, errno);
    return ret;
  }

  // Create a thread to read the pipe
  ret = hcom_mono_stdout_make_thread();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-thread create, errno:%d\n",
      thisFile, __LINE__, errno);
    return -1;
  }

  return OK;
}

//=============================================================
int hcom_mono_stdout_make_thread()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = HCOM_THREAD_PRIORITY_STDOUT_REDIRECT;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, 2048);

    ret = pthread_create(&thread, &attr, hcom_mono_stdout_pthread, NULL);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_CRIT, "%s@%d-Thread create %s error:%d\n",
              thisFile, __LINE__, HCOM_THREAD_NAME_STDOUT_REDIRECT, ret);
      return ret;
    }

  return OK;
}

//=================================================================
// This thread reads all pipe messages redirected from stdout (mono)
void *hcom_mono_stdout_pthread(FAR void *arg)
{
  int ret;

  // Redirect stdout (STDOUT_FILENO) to our pipe
  ret = dup2(_pipefd[HCOM_PIPE_WRITE_OFFSET], STDOUT_FILENO);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "redirect_writer: dup2 failed ret:%d errno:%d\n", ret, errno);
    return NULL;
  }

  close(_pipefd[HCOM_PIPE_WRITE_OFFSET]);

  // This loop runs forever
  while(!_shutting_down)
  {
    ret = hcom_mono_stdout_read_pipe_loop();
    if(ret < 0)
    {
      sleep(5);
    }
  }

  return NULL;
}

//=================================================================
// The read end of the pipe
// It is expected that only text message will be received. But not
// necessarily C style strings.
int hcom_mono_stdout_read_pipe_loop()
{
  uint8_t buffer[HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE];
  ssize_t readReturn;

  // Read
  while (!_shutting_down)
  {
    // Blocks until stdout writes something
    readReturn = read(_pipefd[HCOM_PIPE_READ_OFFSET], buffer, HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE);
    if (readReturn < 0 )
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-pipe read, readReturn:%d, errno:%d\n",
        thisFile, __LINE__, readReturn, errno);
      return -errno;
    }
    else if (readReturn == 0)    // EOF, last writer closed pipe
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d-pipe read EOF\n", thisFile, __LINE__);
      return -1;
    }
    else
    {
      // Successful read message
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Read %d bytes from pipe\n", thisFile, __LINE__, readReturn);

      // Send to host
      int ret = hcom_mono_stdout_route_mono_text_stdout(buffer, readReturn);
      if (ret < 0 )
      {
        if(ret == -EAGAIN)
        {
          // This message will be lost when read is called again. This is by design.
          // The only reason the send would be blocked is that the host isn't
          // there to receive messages. We cannot queue messages forever!
          // The call to write the message was blocked, no reason to return an error.
          // Returning would just close pipe etc.
          continue;
        }

        hcom_logging_syslog(LOG_ERR, "%s@%d-stdout to host, ret:%d\n",
                thisFile, __LINE__, ret);
        return ret;
      }
    }
  }   // while (!_shutting_down)

  return OK;
}

//=================================================================
// Ship the text from mono app to USB and to host PC
int hcom_mono_stdout_route_mono_text_stdout(uint8_t *recvBuff, int numbBytes)
{
  int availBufSpace;

  if(numbBytes == 0)
    return OK;

  // Make sure message fits in allocated buffer, if not, truncate
  if(numbBytes >= HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE)
    availBufSpace = HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE - 1;
  else
    availBufSpace = numbBytes;

  // Includes ctrl chararacter(s)
  int ret = hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_MONO_MSG, 0, (char *) recvBuff,
          availBufSpace, thisFile, __LINE__);
  if (ret < 0)
  {
    // Transmission blocked. EAGAIN is not an error it means the message was blocked.
    // Because the mono app can send really fast, we have no choice but to throw extras away.
    if(ret != -EAGAIN)
      hcom_logging_syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);
  }

  return ret;
}
#endif
