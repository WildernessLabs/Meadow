/****************************************************************************
 * \examples\hcom\hcom_stdxxx_redirect.c
 * 
 *   Copyright (C) 2026 Wilderness Labs. All rights reserved.
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

// This module is responsible for opening 2 fifos for reading stderr and
// stdout from mono and routing the messages to the host PC/Mac for display
// via Meadow.CLI and if configured to syslog.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <sys/stat.h>
#include <ctype.h>
#include <poll.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

/* Configuration ************************************************************/

#define HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE 384
#define HCOM_STDXXX_POLL_OFFSET_STDOUT (0)
#define HCOM_STDXXX_POLL_OFFSET_STDERR (1)
#define HCOM_MONO_APP_STDXXX_SHOW_DIAGNOSTICS (0)

#if(HCOM_MONO_APP_STDXXX_SHOW_DIAGNOSTICS == 1)
#pragma message "(--) hcom_stdxxx_redirect.c"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static bool _lowPowerActive;
static int _stdout_read_fd;
static int _stderr_read_fd;
static uint8_t *_fifo_read_buffer;
static char *_stdxxx_syslog_buffer;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/**
 * @brief Should we copy the application output to the UART (COM1)?
 * Set by hcom_startup_manager.c
 * 
 */
bool g_copy_application_output_to_uart = false;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_mono_stdxxx_pthread(FAR void *arg);
static int hcom_mono_stdout_open_read_fifo(void);
static int hcom_mono_stderr_open_read_fifo(void);
static int hcom_mono_stdxxx_read_fifo_loop(void);
static int hcom_mono_stdxxx_create_infrastructure(void);
static int hcom_mono_stdxxx_make_thread(void);
static int hcom_mono_stdxxx_low_power_notification(bool lpStart);
static void hcom_mono_stdxxx_close_read_fds(bool closeNeeded);
static void hcom_mono_stdxxx_read_mono_fifo(uint32_t pollOffset, int fd_active, uint8_t fifo_read_buffer[]);
static void hcom_mono_stdxxx_publish_message(uint32_t pollOffset, ssize_t msgLength, uint8_t fifo_read_buffer[]);
static void hcom_mono_stdxxx_to_syslog(int useableBufSize, uint8_t fifo_read_buffer[]);
static int hcom_host_send_stdxxx_to_cli(uint32_t pollOffset, int useableBufSize, uint8_t fifo_read_buffer[]);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Does defconfig disallow this module?

// Public function
int hcom_mono_stdxxx_read_setup()
{
  _shutting_down = false;
  _lowPowerActive = false;
  _stdout_read_fd = -1;
  _stderr_read_fd = -1;
  _fifo_read_buffer = NULL;
  _stdxxx_syslog_buffer = NULL;

  // Register with power management so we can properly shutdown before entering
  // a low-power mode.
  int ret = hcom_via_nx_register_pwr_mgmt_callback(hcom_mono_stdxxx_low_power_notification);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Registering for pwr mgmt, ret:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // This buffer is always needed
  _fifo_read_buffer = (uint8_t *) malloc(HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE + 1);
  if (_fifo_read_buffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Memory allocate error, errno:%d\n",
      thisFile, __LINE__, errno);
    return -ENOMEM;
  }

  // This buffer is only for messages for syslog
  if(g_copy_application_output_to_uart)
  {
    _stdxxx_syslog_buffer = (char *) malloc(HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE + 1);
    if (_stdxxx_syslog_buffer == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Memory allocate error, errno:%d\n",
        thisFile, __LINE__, errno);
      free(_fifo_read_buffer);
      return -ENOMEM;
    }
  }

  return hcom_mono_stdxxx_create_infrastructure();
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
// Public function
void hcom_mono_stdxxx_read_shutdown()
{
  _shutting_down = true;

  int ret = close(_stdout_read_fd);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d close read, errno:%d\n",
      thisFile, __LINE__, errno);
  }

  if(_fifo_read_buffer != NULL)
  {
    free(_fifo_read_buffer);
    _fifo_read_buffer = NULL;
  }

  if(_stdxxx_syslog_buffer != NULL)
  {
    free(_stdxxx_syslog_buffer);
    _stdxxx_syslog_buffer = NULL;
  }
}

//==========================================================================
// This will be called when entering and after leaving low-power mode
int hcom_mono_stdxxx_low_power_notification(bool lpStart)
{
  if(lpStart)
  {
    _lowPowerActive = true;  // Low-Power mode is starting very soon
  }
  else
  {
    _lowPowerActive = false;
  }

  return OK;
}

//==========================================================================
// This function creates the stdout fifo
int hcom_mono_stdxxx_create_infrastructure()
{
  int ret;

  // Create stdout fifo
  ret = mkfifo(HCOM_MONO_STDOUT_REDIRECT_FIFO, 0666);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s fifo creation, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  // Create stderr fifo
  ret = mkfifo(HCOM_MONO_STDERR_REDIRECT_FIFO, 0666);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s fifo creation, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDERR_REDIRECT_FIFO, errno);
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  // Create a thread to do the rest of the initialization then read the
  // stdout and stderr messages.
  ret = hcom_mono_stdxxx_make_thread();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-thread create, errno:%d\n",
      thisFile, __LINE__, errno);
    
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  // The startup manager's semaphore will be released by the new thread
  return OK;
}

//==========================================================================
// Thread creation
int hcom_mono_stdxxx_make_thread()
{
  int ret;
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;

  param.sched_priority = HCOM_THREAD_PRIORITY_STDXXX_REDIRECT;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_STDXXX_REDIRECT);

  ret = pthread_create(&thread, &attr, hcom_mono_stdxxx_pthread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Thread create %s error:%d\n",
            thisFile, __LINE__, HCOM_THREAD_NAME_STDXXX_REDIRECT, ret);
    hcom_startup_mgr_release_sem_err(ret);
    return ret;
  }

  return OK;
}

//==========================================================================
// This thread first does a little initialization then goes into a
// loop reading all fifo messages from mono.
// This function creates the infrastructure needed to route mono generated
// stdout and stderr to the host PC / Mac.
void *hcom_mono_stdxxx_pthread(FAR void *arg)
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(LOG_MDIAG, "New pthread [PID:%d],'%s'\n",
    getpid(), HCOM_THREAD_NAME_STDXXX_REDIRECT);
#endif

  // Release startup manager's semaphore to continue startup
  hcom_startup_mgr_release_sem();

  // Thread stays in this loop till shutdown
  while(!_shutting_down)
  {
    ret = hcom_mono_stdout_open_read_fifo();
    if(ret < 0)
    {
      // Insure all fifo's closed
      hcom_mono_stdxxx_close_read_fds(false);
      continue;
    }

    ret = hcom_mono_stderr_open_read_fifo();
    if(ret < 0)
    {
      hcom_mono_stdxxx_close_read_fds(false);
      continue;
    }

    // Read messages from either fifo and forward to CLI
    ret = hcom_mono_stdxxx_read_fifo_loop();
    if(ret < 0)
    {
      // Only return on error
      hcom_mono_stdxxx_close_read_fds(true);
    }
  }

  return OK;
}

//==========================================================================
// Close both fifo read descriptors
void hcom_mono_stdxxx_close_read_fds(bool closeNeeded)
{
  if(closeNeeded && _stdout_read_fd >= 0)
  {
    close(_stdout_read_fd);
    _stdout_read_fd = -1;
  }

  if(closeNeeded && _stderr_read_fd >= 0)
  {
    close(_stderr_read_fd);
    _stderr_read_fd = -1;
  }

  sleep(100 * 1000);   // Just no prevent hard infinite loop
}

//==========================================================================
// Open stdout fifo
int hcom_mono_stdout_open_read_fifo()
{
  if(_stdout_read_fd >= 0)
  {
    close(_stdout_read_fd);
    _stdout_read_fd = -1;
  }

  // Don't wait until the writer opens the pipe
  _stdout_read_fd = open(HCOM_MONO_STDOUT_REDIRECT_FIFO, O_RDONLY | O_NONBLOCK);
  if (_stdout_read_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
    return -errno;
  }

  return OK;
}

//-------------------------------------------------------------------
// Open stderr fifo
int hcom_mono_stderr_open_read_fifo()
{
  if(_stderr_read_fd >= 0)
  {
    close(_stderr_read_fd);
    _stderr_read_fd = -1;
  }

  // Don't wait until the writer opens the pipe
  _stderr_read_fd = open(HCOM_MONO_STDERR_REDIRECT_FIFO, O_RDONLY | O_NONBLOCK);
  if (_stderr_read_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDERR_REDIRECT_FIFO, errno);
    return -errno;
  }

  return OK;
}

//==========================================================================
// This loop waits for either of the fifos to be written to, and then reads
// and forwards what it finds to CLI, and maybe syslog.
// It is expected that full text message will be received. But not
// necessarily C style strings.
int hcom_mono_stdxxx_read_fifo_loop()
{
  int ret;
  short int fdsReverts;
  struct pollfd poll_fds[2];

  // Note: POLLERR, POLLHUP and POLLINVAL should not be included in the events
  // field. If they are, poll may return immediately.
  poll_fds[HCOM_STDXXX_POLL_OFFSET_STDOUT].fd = _stdout_read_fd;
  poll_fds[HCOM_STDXXX_POLL_OFFSET_STDOUT].events = POLLIN;
  poll_fds[HCOM_STDXXX_POLL_OFFSET_STDERR].fd = _stderr_read_fd;
  poll_fds[HCOM_STDXXX_POLL_OFFSET_STDERR].events = POLLIN;

  do
  {
    // Wait for a fd to change
    // Return values from poll:
    //  > 0: the number of structures that have non-zero revents fields
    //  = 0: indicates that the call timed out and no fd was ready
    //  < 0: error, -1 is returned, and errno is set appropriately
    ret = poll(poll_fds, 2, -1);
    if(ret < 0)
    {
      if(errno == -EINTR)
      {
        continue;       // Ignore interruptions
      }

      // Report error and continue
      hcom_logging_syslog(LOG_ERR, "poll() call ret:%d errno:%d", ret, errno);
      usleep(100 * 1000);
      continue;
    }

    // A value of 0 indicates that the call timed out and no file descriptors were ready.
    // This implementation doesn't provide a timeout value.
    if(ret == 0)
    {
      syslog(LOG_WARNING, "stdxxx(poll) - From poll() ret == 0, Timeout\n");
      usleep(20 * 1000);
      continue;
    }

    /* NuttX does not make priority distinctions */
    // #define POLLIN       (0x01)  // Data other than high-priority data may be read without blocking.
    // #define POLLRDNORM   (0x01)  // Normal data may be read without blocking.
    // #define POLLRDBAND   (0x01)  // Priority data may be read without blocking.
    // #define POLLPRI      (0x02)  // High priority data may be read without blocking.
    // #define POLLWRNORM   (0x02)  // Equivalent to POLLOUT.
    // #define POLLWRBAND   (0x02)  // Priority data may be written.
    // #define POLLOUT      (0x04)  // Normal data may be written without blocking.
    // #define POLLERR      (0x08)  // An error has occurred (revents only).
    // #define POLLHUP      (0x10)  // Device has been disconnected (hangup) (revents only).
    // #define POLLNVAL     (0x20)  // Invalid fd member (revents only).

    // Check stdout
    fdsReverts = poll_fds[HCOM_STDXXX_POLL_OFFSET_STDOUT].revents;
    if(fdsReverts > 0)
    {
      // syslog(LOG_MDIAG, "stdout(poll) - reverts:%d (0x%04x)\n", fdsReverts, fdsReverts);
      if(fdsReverts & POLLIN)
      {
        hcom_mono_stdxxx_read_mono_fifo(HCOM_STDXXX_POLL_OFFSET_STDOUT,
          _stdout_read_fd, _fifo_read_buffer);
      }

      if (fdsReverts & POLLHUP)
      {
        syslog(LOG_WARNING, "stdout(poll) - In reverts POLLHUP (hangup)\n");
      }
    }

    // Check stderr
    fdsReverts = poll_fds[HCOM_STDXXX_POLL_OFFSET_STDERR].revents;
    if(fdsReverts > 0)
    {
      // syslog(LOG_MDIAG, "stderr(poll) - reverts:%d (0x%04x)\n", fdsReverts, fdsReverts);
      if (fdsReverts & POLLIN)
      {
        hcom_mono_stdxxx_read_mono_fifo(HCOM_STDXXX_POLL_OFFSET_STDERR,
          _stderr_read_fd, _fifo_read_buffer);
      }
      
      if (fdsReverts & POLLHUP)
      {
        syslog(LOG_WARNING, "stderr(poll) - In reverts POLLHUP (hangup)\n");
      }
    }

  } while(!_shutting_down);

  return OK;
}

//==========================================================================
// Read data from the requested fifo into the provided buffer
// This function will not return any indication of success or error.
void hcom_mono_stdxxx_read_mono_fifo(uint32_t pollOffset, int fd_active,
  uint8_t fifo_read_buffer[])
{
  ssize_t readReturn;

  // read returned value:
  //  > 0 positive non-zero number of bytes read on success
  //  0   on if an end-of-file condition
  //  < 0 on failure with errno set appropriately
  readReturn = read(fd_active, fifo_read_buffer,
    HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE);
  if (readReturn > 0)
  {
    // Have read data from stdxxx, now send to CLI
    hcom_mono_stdxxx_publish_message(pollOffset, readReturn, fifo_read_buffer);
    return;
  }

  if(readReturn == 0)
  {
    // End-of-file
    usleep(100 * 1000);
    return;
  }

  if(readReturn < 0)
  {
    // Read error
    hcom_logging_syslog(LOG_ERR, "fifo read error:%d, errno:%d",
      readReturn, errno);
  }
}

//==========================================================================
// Send the fifo's messages to the Host
void hcom_mono_stdxxx_publish_message(uint32_t pollOffset,
  ssize_t msgLength, uint8_t fifo_read_buffer[])
{
  int ret;
  int useableBufSize;

  // Make sure message fits in allocated fifo_read_buffer, if not, truncate
  if(msgLength >= HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE)
  {
    useableBufSize = HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE - 1;
  }
  else
  {
    useableBufSize = msgLength;
  }

  // Diagnostic
#if (HCOM_MONO_APP_STDXXX_SHOW_DIAGNOSTICS == 1)
  if(pollOffset == HCOM_STDXXX_POLL_OFFSET_STDOUT)
  {
    syslog(LOG_MDIAG, "->stdout - %.*s",
      useableBufSize, fifo_read_buffer);
  }
  else
  {
    syslog(LOG_MDIAG, "->stderr - %.*s",
      useableBufSize, fifo_read_buffer);
  }
#endif

  // Send to host
  ret = hcom_host_send_stdxxx_to_cli(pollOffset, useableBufSize, fifo_read_buffer);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "Send to CLI failed, ret:%d", ret);
    return;
  }
  
  // Route message to syslog if so configured
  if (g_copy_application_output_to_uart)
  {
    hcom_mono_stdxxx_to_syslog(useableBufSize, fifo_read_buffer);
  }
}

//==============================================================================
// Forward a message to CLI
int hcom_host_send_stdxxx_to_cli(uint32_t pollOffset, int useableBufSize,
  uint8_t fifo_read_buffer[])
{
  int ret;
  uint16_t requestType;

  if(pollOffset == HCOM_STDXXX_POLL_OFFSET_STDOUT)
  {
    requestType = HCOM_HOST_REQUEST_TEXT_MONO_STDOUT;
  }
  else
  {
    requestType = HCOM_HOST_REQUEST_TEXT_MONO_STDERR;
  }

  // Send to CLI. Includes ctrl character(s)
  ret = hcom_host_send_raw_string_msg(requestType, 0,
    (char *) fifo_read_buffer, useableBufSize, thisFile, __LINE__);
  if (ret < 0)
  {
    if(ret == -EAGAIN)
    {
      // The main reason the send would be blocked is that there is no host
      // listening for a message. This is normal for HCOM and must be ignored.
      return OK;
    }

    if(_lowPowerActive && errno == ENOTCONN)
    {
      // Don't report the error just after being in low power mode
      _lowPowerActive = false;
      return OK;
    }

    hcom_logging_syslog(LOG_ERR, "%s@%d-stdxxx to host, ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

//============================================================================
// Forward stdxxx message to syslog
void hcom_mono_stdxxx_to_syslog(int useableBufSize, uint8_t fifo_read_buffer[])
{
  int syslog_buffer_index = 0;
  // This needs special care to insure the message is properly formatted
  for (int index = 0; index < useableBufSize; index++)
  {
    // If printable ascii just copy to previously allocated buffer
    if ((fifo_read_buffer[index] >= ' ') && (fifo_read_buffer[index] <= '~'))
    {
      _stdxxx_syslog_buffer[syslog_buffer_index] = fifo_read_buffer[index];
    }

    if ((fifo_read_buffer[index] == '\n') ||
        (syslog_buffer_index >= HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE - 2))
    {
      // If \n or out of buffer space, add \n and null terminate and output
      // message. If not \n nor Buffer full, increment buffer index.
      _stdxxx_syslog_buffer[syslog_buffer_index] = '\n';
      _stdxxx_syslog_buffer[syslog_buffer_index + 1] = '\0';
      syslog_buffer_index = 0;

      hcom_logging_syslog(LOG_INFO, "%s", (char *) _stdxxx_syslog_buffer);
    }
    else
    {
      syslog_buffer_index++;
    }
    // Continue in loop to check the source buffer for more characters.
  }
}

//=====================================================================
// Open stderr or stdout
static int hcom_mono_open_mono_fifo(char *fifoRedirect, int stdxxxFileNo)
{
  int ret;
  int std_temp_write_fd;

  // Open stdxxx fifo
  do
  {
    std_temp_write_fd = open(fifoRedirect, O_WRONLY);
    if(std_temp_write_fd == -1)
    {
      if(errno == -EINTR)
      {
        continue;
      }

      // All errors exit
      hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
                          thisFile, __LINE__, fifoRedirect, errno);
      return -errno;
    }
  } while (std_temp_write_fd < 0);

  // Assign the fifo's write end to the stdxxx fd.
  ret = dup2(std_temp_write_fd, STDOUT_FILENO);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer (%s): dup2 failed ret:%d errno:%d\n",
                        thisFile, __LINE__, fifoRedirect, ret, errno);
    usleep(50 * 1000);
    close(std_temp_write_fd);
    return -errno;
  }

  // Don't close if fd is stdin, stdout, or stderr.
  if(std_temp_write_fd > STDERR_FILENO)
  {
    close(std_temp_write_fd);
  }

  return OK;
}

//==================================================================
// Public function
// This function is called from mono_main's main thread. Thus insuring
// that the fifos can written to by the mono thread.
int hcom_mono_stdxxx_redirect(void)
{
  int ret;

  // syslog(LOG_MDIAG, "Mono - called to open fifos\n"); usleep(20 * 1000);

  // Open stdout fifo
  ret = hcom_mono_open_mono_fifo(HCOM_MONO_STDOUT_REDIRECT_FIFO,
    STDOUT_FILENO);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d stdout error on open(), ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  // Open stderr fifo
  ret = hcom_mono_open_mono_fifo(HCOM_MONO_STDERR_REDIRECT_FIFO,
    STDERR_FILENO);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d stderr error on open(), ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

#endif  // #if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)
