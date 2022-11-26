/****************************************************************************
 * \examples\hcom\hcom_stdout_redirect.c
 * 
 *   Copyright (C) 2019 - 2022 Wilderness Labs. All rights reserved.
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

// This module is responsible for creating a fifo, reading the fifo and
// routing this information to the host PC/Mac for display via Meadow.CLI
// Note: This module is an identical twin of hcom_stderr_redirect.c (except
// the name stdout). While these could have been placed in a single file I
// decided that the benefits (less duplicate code and 1 thread vs 2) were not
// as great as the benefits of having twins (much less complexity).

/****************************************************************************
 * Included Files
 ****************************************************************************/
#warning "Peter working here (---)"

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

#include <sys/stat.h>
#include <ctype.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

/* Configuration ************************************************************/

#define HCOM_MONO_APP_STDOUT_REDIRECT_BUFF_SIZE 384

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static int _read_fd;
static int _stdout_fd;
static bool _lowPowerActive;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_mono_stdout_pthread(FAR void *arg);
static int hcom_mono_stdout_create_infrastructure(void);
static int hcom_mono_stdout_make_thread(void);
static int hcom_mono_stdout_read_fifo_loop(void);
static void hcom_mono_stdout_close_delay_read(bool closeNeeded);
static int hcom_mono_stdout_open_read_fifo(void);
static int hcom_mono_stdout_low_power_notification(bool lpStart);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)
int hcom_mono_stdout_read_setup()
{
  _shutting_down = false;
  _read_fd = -1;
  _stdout_fd = -1;
  _lowPowerActive = false;

  // Register with power management so we can properly shutdown before entering
  // a low-power mode.
  int ret = hcom_via_nx_register_pwr_mgmt_callback(hcom_mono_stdout_low_power_notification);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Registering for pwr mgmt:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // It would be nice if this initialization could be postponed
  // until we know if mono was running. This was quickly attempted
  // and created timing issues so everything was reverted.
  // Note that this suggestion would only provide minimal
  // value since mono will usually be running.
  return hcom_mono_stdout_create_infrastructure();
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
void hcom_mono_stdout_read_shutdown()
{
  _shutting_down = true;

  int ret = close(_read_fd);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d close read, errno:%d\n",
      thisFile, __LINE__, errno);
  }
}

//=======================================================================
// This will be called when entering and after leaving low-power mode
int hcom_mono_stdout_low_power_notification(bool lpStart)
{
  int ret = OK;
  
  syslog(2, "--->>> stdout notified of %s low-power mode\n", lpStart ? "Starting" : "Ending"); usleep(10 * 1000);
    
  // if(lpStart)
  // {
  //   // Low-Power mode is starting very soon
  //   _lowPowerActive = true;
  //   _shutting_down = true;    // This ends the thread when fd closed

  //   close(_read_fd);
  //   _read_fd = -1;
  //   ret = OK;
  // }
  // else
  // {
  //   // Low-Power mode has ended
  //   _shutting_down = false;

  //   // Restart stdout
  //   ret = hcom_mono_stdout_make_thread();
  // }
  
  return ret;
}

//==========================================================================
// This function creates the stdout fifo
int hcom_mono_stdout_create_infrastructure()
{
  int ret;

  // Creates a fifo 
  ret = mkfifo(HCOM_MONO_STDOUT_REDIRECT_FIFO, 0666);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s fifo creation, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  // Create a thread to read the fifo
  ret = hcom_mono_stdout_make_thread();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-thread create, errno:%d\n",
      thisFile, __LINE__, errno);
    
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  // The startup semaphore will be released by the new thread
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
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_STDOUT_REDIRECT);

  ret = pthread_create(&thread, &attr, hcom_mono_stdout_pthread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Thread create %s error:%d\n",
            thisFile, __LINE__, HCOM_THREAD_NAME_STDOUT_REDIRECT, ret);
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  return OK;
}

//=================================================================
// This thread first does a little initialization then goes into a
// loop reading all fifo messages redirected from stdout (mono).
// This function creates the infrastructure needed to route mono generated
// stdout and stderr to the host PC / Mac.
void *hcom_mono_stdout_pthread(FAR void *arg)
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_STDOUT_REDIRECT);
#endif

  if(_lowPowerActive)
  {
    _lowPowerActive = false;
  }
  else
  {
    // Release startup manager to continue startup
    hcom_startup_mgr_release_sem();
  }

  // This loop runs forever
  while(!_shutting_down)
  {
    ret = hcom_mono_stdout_open_read_fifo();
    if(ret < 0)
    {
      hcom_mono_stdout_close_delay_read(false);
      continue;
    }

    ret = hcom_mono_stdout_read_fifo_loop();
    if(ret < 0)
    {
      // ENOTCONN occurs after low-power mode
      if(ret == -ENOTCONN)
        hcom_mono_stdout_open_read_fifo();
      else
        hcom_mono_stdout_close_delay_read(true);
    }
  }

  return NULL;
}

//================================================================
void hcom_mono_stdout_close_delay_read(bool closeNeeded)
{
  if(closeNeeded && _read_fd >= 0)
  {
    close(_read_fd);
    _read_fd = -1;
  }

  sleep(100 * 1000);   // Not a special value, just no prevent hard infinite loop
}

//=================================================================
// Note: This has already been redirected by hcom_mono_control.c at startup
int hcom_mono_stdout_open_read_fifo()
{
  if(_read_fd >= 0)
  {
    close(_read_fd);
    _read_fd = -1;
  }

  // The docs say that this open call will block until some writer opens the pipe
  _read_fd = open(HCOM_MONO_STDOUT_REDIRECT_FIFO, O_RDONLY);
  if (_read_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
    return -1;
  }

  return OK;
}

//=================================================================
// This loop reads the fifo and forwards what it finds to CLI
// It is expected that only text message will be received. But not
// necessarily C style strings.
int hcom_mono_stdout_read_fifo_loop()
{
  int ret;
  int availBufSpace;
  uint8_t buffer[HCOM_MONO_APP_STDOUT_REDIRECT_BUFF_SIZE];
  ssize_t readReturn;

  // Read
  while (!_shutting_down)
  {
    // Blocks until stdout writes something
    readReturn = read(_read_fd, buffer, HCOM_MONO_APP_STDOUT_REDIRECT_BUFF_SIZE);
    if (readReturn < 0 )
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-fifo read, readReturn:%d, errno:%d\n",
        thisFile, __LINE__, readReturn, errno);
      return -errno;
    }
    else if (readReturn == 0)    // EOF, last writer closed fifo
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d-fifo read EOF\n", thisFile, __LINE__);
      return -1;
    }
    else
    {
      // Successfully read message
      // (---) DIRECTLY SHOW TEXT
      syslog(1, "stdout-> %.*s\n", readReturn, buffer);

      // hcom_logging_syslog(1, "%s@%d-Read %d bytes from fifo\n", thisFile, __LINE__, readReturn);

      // Send to host
      // Make sure message fits in allocated buffer, if not, truncate
      if(readReturn >= HCOM_MONO_APP_STDOUT_REDIRECT_BUFF_SIZE)
        availBufSpace = HCOM_MONO_APP_STDOUT_REDIRECT_BUFF_SIZE - 1;
      else
        availBufSpace = readReturn;

      // Includes ctrl chararacter(s)
      ret = hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_MONO_STDOUT, 0, (char *) buffer,
            availBufSpace, thisFile, __LINE__);
      if (ret < 0)
      {
        if(ret == -EAGAIN)
        {
          // This message will be lost when read is called again. This is by design.
          // The only reason the send would be blocked is that the host isn't
          // there to receive messages. We cannot queue messages forever!
          // The call to write the message was blocked, no reason to return an error.
          // Returning would just close fifo etc.
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

//==================================================================
// Since the mono_main task's main thread called this function it will
// cause it's stdout calls to be routed to the correct fifo
int hcom_mono_stdout_redirect(void)
{
  int ret;

  if (_stdout_fd < 0)
  {
    // Open stdout fifo
    do
    {
      // Opening with O_NONBLOCK seems like the right thing to do but
      // it is NOT. It causes the mono app to halt.
      _stdout_fd = open(HCOM_MONO_STDOUT_REDIRECT_FIFO, O_WRONLY);
      if (_stdout_fd >= 0)
        break; // Success

      if (errno != ENOENT) // ENOENT = Error No Entity -> No such file or directory
      {
        // All other errors exit
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
                            thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
        _stdout_fd = -1;
        return -errno;
      }

      // Sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the fifo's write end to the stdout fd.
    // Note: ret should equal the stdout fd
    ret = dup2(_stdout_fd, STDOUT_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer: dup2 failed ret:%d errno:%d\n",
                          thisFile, __LINE__, ret, errno);
      return -errno;
    }
    close(_stdout_fd);
  }

  return OK;
}

#else // #if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

int hcom_mono_stdout_read_setup()
{
  return OK;
}
void hcom_mono_stdout_read_shutdown()
{
}
#endif  // #if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)
