/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_mono_pipe_interface.c
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
#include <ctype.h>

#if HCOM_TASK_CREATE_SHOW_TASK_INFORMATION > 0
#include <nuttx/sched.h>
#include <../sched/sched/sched.h>
#endif
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE 512
#define HCOM_MONO_APP_DBG_HOST_BUFF_SIZE 512

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static int _pipe_fd;
static char *_pipeTextMsg;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_mono_pipe_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_mono_pipe_pthread(FAR void *arg);
#endif

static int hcom_mono_pipe_create_infrastructure(void);
static int hcom_mono_pipe_make_thread(void);
static int hcom_mono_pipe_open_pipe(void);
static int hcom_mono_pipe_read_pipe_loop(void);
static int hcom_mono_pipe_route_mono_text_stdout(uint8_t *recvBuff, int numbBytes);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_mono_pipe_setup()
{
  _shutting_down = false;

  _pipeTextMsg = malloc(HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE);
  if(_pipeTextMsg == NULL)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Mem alloc\n", thisFile, __LINE__);
    return -1;
  }
    
  // todo - Should this be called by hcom_startup_manager?
  return hcom_mono_pipe_create_infrastructure();
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
void hcom_mono_pipe_shutdown()
{
  _shutting_down = true;

  int ret = close(_pipe_fd);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:%s close, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_MAIN_STDOUT_PIPE, errno);
  }
  _pipe_fd = -1;
  
  free(_pipeTextMsg);
}

//==========================================================================
int hcom_mono_pipe_create_infrastructure()
{
  int ret;

  // Create named pipe
  ret = mkfifo(HCOM_MONO_MAIN_STDOUT_PIPE, 0666);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:%s mkfifo, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_MAIN_STDOUT_PIPE, errno);
    return -1;
  }

  // Create a thread to read the pipe
  ret = hcom_mono_pipe_make_thread();
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:thread create, errno:%d\n",
      thisFile, __LINE__, errno);
    return -1;
  }
  
  return OK;
}

//=============================================================
int hcom_mono_pipe_make_thread()
{
  #ifdef CONFIG_BUILD_PROTECTED
    int pid = kthread_create(
      HCOM_THREAD_NAME_STDOUT_PIPE,
      HCOM_THREAD_PRIORITY_STDOUT_PIPE,
      2048, (main_t)hcom_mono_pipe_kthread,
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

    ret = pthread_create(&thread, &attr, hcom_mono_pipe_pthread, NULL);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s@%d-Error:Thread create error:%d\n", thisFile, __LINE__, ret);
      return ret;
    }
  #endif

  return OK;
}

//================================================================
static void hcom_mono_pipe_close_and_delay(bool closeNeeded)
{
  if(closeNeeded)
  {
    close(_pipe_fd);
    _pipe_fd = -1;
  }

  // Wait and try again
  if(!_shutting_down)
    sleep(5);   // Not a special value, just no prevent hard infinite loop
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
  
#if HCOM_TASK_CREATE_SHOW_TASK_INFORMATION > 0
  struct tcb_s *rtcb = this_task();
  syslog(0, "Created Task:'%s' as #%d\n", rtcb->name, getpid());
#endif

  while(!_shutting_down)
  {
    ret = hcom_mono_pipe_open_pipe();
    if(ret < 0)
    {
      hcom_mono_pipe_close_and_delay(false);      
      continue;
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
  if(_pipe_fd >= 0)
  {
    close(_pipe_fd);
    _pipe_fd = -1;
  }

  // The docs say that is open call will block until some writer opens the pipe
  _pipe_fd = open(HCOM_MONO_MAIN_STDOUT_PIPE, O_RDONLY);
  if (_pipe_fd < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_MAIN_STDOUT_PIPE, errno);
    return -1;
  }

  return OK;
}

//=================================================================
// The other end of this pipe is connected to the nuttx_user stdout.
// It is expected that only text message will be received. But not
// necessarily C style strings.
int hcom_mono_pipe_read_pipe_loop()
{
  uint8_t buffer[HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE];
  ssize_t readReturn;

  // Read pipe
  while (!_shutting_down)
  {
    readReturn = read(_pipe_fd, buffer, HCOM_MONO_APP_DBG_PIPE_BUFF_SIZE);
    if (readReturn < 0 )
    {
      f7syslog(LOG_ERR, "%s@%d-Error:pipe read, readReturn:%d, errno:%d\n",
        thisFile, __LINE__, readReturn, errno);
      return -errno;
    }
    else if (readReturn == 0)    // EOF, last writer closed pipe
    {
      f7syslog(LOG_WARNING, "%s@%d-Warning:pipe read EOF\n", thisFile, __LINE__);
      sleep(1);
      continue;
    }
    else
    {
      // Successful pipe read message
      hcom_comms_dbg(LOG_DEBUG, "%s@%d-Read %d bytes from pipe\n", thisFile, __LINE__, readReturn);

      // Send to host
      int ret = hcom_mono_pipe_route_mono_text_stdout(buffer, readReturn);

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

        f7syslog(LOG_ERR, "%s@%d-Error:stdout to host, ret:%d\n",
                thisFile, __LINE__, ret);
        return ret;
      }
    }
  }   // while (!_shutting_down)

  return OK;
}

//=================================================================
// Ship the text from mono app to USB and to host PC
int hcom_mono_pipe_route_mono_text_stdout(uint8_t *recvBuff, int numbBytes)
{
  int availBufSpace;

  if(numbBytes == 0)
    return OK;
    
  // Make sure message fits in allocated buffer, if not, truncate
  if(numbBytes >= HCOM_MAX_HOST_STRING_BUFF_LENGTH)
    availBufSpace = HCOM_MAX_HOST_STRING_BUFF_LENGTH - 1;
  else
    availBufSpace = numbBytes;

  // Includes ctrl chararacter
  int ret = hcom_comms_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_MONO_MSG, 0, (char *) recvBuff, availBufSpace);
  if (ret < 0)
  {
    if(ret != -EAGAIN)      // Transmission blocked. EAGAIN is not an error it means the message was blocked
      f7syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);
  }

  return ret;
}