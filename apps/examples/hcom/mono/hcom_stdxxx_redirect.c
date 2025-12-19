/****************************************************************************
 * \examples\hcom\hcom_stdxxx_redirect.c
 * 
 *   Copyright (C) 2025 Wilderness Labs. All rights reserved.
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
// stdout and routing the messages to the host PC/Mac for display via
// Meadow.CLI and if configured to syslog.
// The write end of these fifos are in hcom_mono_control.c

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <sys/stat.h>
#include <ctype.h>
#include <sys/select.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#pragma message "(--) hcom_stdxxx_redirect.c"

#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

/* Configuration ************************************************************/

#define HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE 384

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static bool _lowPowerActive;

static int _stdout_read_fd;
static int _stdout_fifo_fd;
static int _stderr_read_fd;
static int _stderr_fifo_fd;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/**
 * @brief Should we copy the application output to the UART (COM1)?
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
static void hcom_mono_stdxxx_close_delay_read(bool closeNeeded);
static void hcom_mono_stdxxx_read_pub_fifo(int fd_active, uint8_t fifo_buffer[]);
static void hcom_mono_stdxxx_publish_message(ssize_t msgLength, uint8_t fifo_buffer[]);
static void hcom_mono_stdxxx_to_syslog(int useableBufSize, uint8_t fifo_buffer[]);
static int hcom_host_send_stdxxx_to_cli(int useableBufSize, uint8_t fifo_buffer[]);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

// Public function
int hcom_mono_stdxxx_read_setup()
{
  _shutting_down = false;
  _stdout_read_fd = -1;
  _stdout_fifo_fd = -1;
  _lowPowerActive = false;

  // Register with power management so we can properly shutdown before entering
  // a low-power mode.
  int ret = hcom_via_nx_register_pwr_mgmt_callback(hcom_mono_stdxxx_low_power_notification);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Registering for pwr mgmt:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // It would be nice if this initialization could be postponed until we
  // knew if mono was running. This was attempted but created timing issues.
  // And since the whole point of Meadow is to run mono, there isn't any
  // compelling reason to worry about it doing this initialization if mono
  // isn't running.
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
}

//==========================================================================
// This will be called when entering and after leaving low-power mode
int hcom_mono_stdxxx_low_power_notification(bool lpStart)
{
  if(lpStart)
  {
    // Low-Power mode is starting very soon
    _lowPowerActive = true;
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

  // Create a thread to read the fifo messages
  ret = hcom_mono_stdxxx_make_thread();
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

//==========================================================================
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
// loop reading all fifo messages redirected from stdxxx (mono).
// This function creates the infrastructure needed to route mono generated
// stdout and stderr to the host PC / Mac.
void *hcom_mono_stdxxx_pthread(FAR void *arg)
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(LOG_MDIAG, "New pthread [PID:%d],'%s'\n",
    getpid(), HCOM_THREAD_NAME_STDXXX_REDIRECT);
#endif

  // Release startup manager to continue startup
  hcom_startup_mgr_release_sem();

  // Thread stays in this loop till shutdown
  while(!_shutting_down)
  {
    ret = hcom_mono_stdout_open_read_fifo();
    if(ret < 0)
    {
      // Insure all fifo's closed
      hcom_mono_stdxxx_close_delay_read(false);
      continue;
    }

    ret = hcom_mono_stderr_open_read_fifo();
    if(ret < 0)
    {
      hcom_mono_stdxxx_close_delay_read(false);
      continue;
    }

    // Read a message from either fifo and forward to CLI
    ret = hcom_mono_stdxxx_read_fifo_loop();
    if(ret < 0)
    {
      // Only return on error
      hcom_mono_stdxxx_close_delay_read(true);
    }
  }

  return NULL;
}

//==========================================================================
// Close both fifo read descriptors
void hcom_mono_stdxxx_close_delay_read(bool closeNeeded)
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
// Note: This has already been redirected by hcom_mono_control.c at startup
int hcom_mono_stdout_open_read_fifo()
{
  if(_stdout_read_fd >= 0)
  {
    close(_stdout_read_fd);
    _stdout_read_fd = -1;
  }

  // Docs say that this open call will block until the writer opens the pipe
  _stdout_read_fd = open(HCOM_MONO_STDOUT_REDIRECT_FIFO, O_RDONLY);
  if (_stdout_read_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
    return -1;
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

  // Docs say that this open call will block until the writer opens the pipe
  _stderr_read_fd = open(HCOM_MONO_STDERR_REDIRECT_FIFO, O_RDONLY);
  if (_stderr_read_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open %s, errno:%d\n",
      thisFile, __LINE__, HCOM_MONO_STDERR_REDIRECT_FIFO, errno);
    return -1;
  }

  return OK;
}

//==========================================================================
// This loop  waits for a fifo to be written to and then reads
// and forwards what it finds to CLI.
// It is expected that only text message will be received. But not
// necessarily C style strings.
int hcom_mono_stdxxx_read_fifo_loop()
{
  int max_fd;
  int nfds;
  fd_set read_fds;
  fd_set run_fds;
  uint8_t fifo_buffer[HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE];

  FD_ZERO(&read_fds);
  FD_SET(_stdout_read_fd, &read_fds);
  FD_SET(_stderr_read_fd, &read_fds);

  // Remember max_fd is the fd with the largest numeric value + 1
  max_fd = _stdout_read_fd;
  if(max_fd < _stderr_read_fd)
     max_fd = _stderr_read_fd;
  max_fd++;

  do
  {
    // Take a copy that can be corrupted by the select call
    memcpy(&run_fds, &read_fds, sizeof(fd_set));

    //  Returned Value:
    //   0  : Timer expired
    //   > 0: The number of bits set in the three sets of descriptors
    //   -1 : An error occurred (errno will be set appropriately)
    nfds = select(max_fd, &run_fds, NULL, NULL, NULL);
    if(nfds < 0)
    {
      if(errno == -EINTR)
        continue;       // Ignore interruptions

      // Report error and continue
      syslog(LOG_ERR, "select() call errno:%d", errno);
      continue;
    }
    
    if(nfds == 0)
    {
      // (--) MAY WANT TO SLEEP HERE?????
      continue;   // ?? Timeout (not used)
    }

    // Valid return from select so, call the appropriate read fifo.
    if( FD_ISSET(_stdout_read_fd, &run_fds ) )
    {
      // Read and publish stdout
      hcom_mono_stdxxx_read_pub_fifo(_stdout_read_fd, fifo_buffer);
    }

    if( FD_ISSET(_stderr_read_fd, &run_fds ) )
    {
      // Read and publish stderr
      hcom_mono_stdxxx_read_pub_fifo(_stderr_read_fd, fifo_buffer);
    }

    // Only thing left is fifo was empty (readReturn == 0)
  } while(!_shutting_down);
  
  return OK;
}

//==========================================================================
// Read data from the requested fifo into the provided buffer
// This function will not return any indication of success or error.
void hcom_mono_stdxxx_read_pub_fifo(int fd_active, uint8_t fifo_buffer[])
{
  ssize_t readReturn;

  // read returned Value:
  //  The positive non-zero number of bytes read on success, 0 on if an
  //  end-of-file condition, or -1 on failure with errno set appropriately.
  readReturn = read(fd_active, fifo_buffer, HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE);
  if (readReturn > 0)
  {
    // Have read data from stdxxx, now publish to waiting receivers
    hcom_mono_stdxxx_publish_message(readReturn, fifo_buffer);
    return;
  }

  if(readReturn < 0)
  {
    // Read error
    hcom_logging_syslog(LOG_ERR, "fifo read error:%d, errno:%d", readReturn, errno);
  }
}

//==========================================================================
// Send the fifo messages to the Host
void hcom_mono_stdxxx_publish_message(ssize_t msgLength, uint8_t fifo_buffer[])
{
  int ret;
  int useableBufSize;

  // Make sure message fits in allocated fifo_buffer, if not, truncate
  if(msgLength >= HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE)
  {
    useableBufSize = HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE - 1;
  }
  else
  {
    useableBufSize = msgLength;
  }

  // Send to host PC/MAC/Linux
  ret = hcom_host_send_stdxxx_to_cli(useableBufSize, fifo_buffer);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "Send to CLI failed, ret:%d", ret);
    return;
  }

  // Route message to syslog.
  // (--) WHY NOT USE BBR? OR FUNCTION TO SET BOOL TRUE OR FALSE?
  if (g_copy_application_output_to_uart)
  {
    hcom_mono_stdxxx_to_syslog(useableBufSize, fifo_buffer);
  }
}

//============================================================================
// Forward stdxxx message to syslog
void hcom_mono_stdxxx_to_syslog(int useableBufSize, uint8_t fifo_buffer[])
{
  int syslog_buffer_index = 0;
  char *syslog_stdxxx_buffer = NULL;

  syslog_stdxxx_buffer = (char *) malloc(HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE + 1);
  if (syslog_stdxxx_buffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Memory allocate error, errno:%d\n",
      thisFile, __LINE__, errno);
    return -ENOMEM;
  }

  // This needs special care to insure the message is properly formatted
  for (int index = 0; index < useableBufSize; index++)
  {
    if ((fifo_buffer[index] >= ' ') && (fifo_buffer[index] <= '~'))
    {
      syslog_stdxxx_buffer[syslog_buffer_index] = fifo_buffer[index];
    }
    if ((fifo_buffer[index] == '\n') ||
        (syslog_buffer_index >= HCOM_MONO_APP_STDXXX_REDIRECT_BUFF_SIZE - 2))
    {
      syslog_stdxxx_buffer[syslog_buffer_index] = '\n';
      syslog_stdxxx_buffer[syslog_buffer_index + 1] = '\0';
      syslog_buffer_index = 0;
      hcom_logging_syslog(LOG_INFO, "%s", (char *) syslog_stdxxx_buffer);
    }
    else
    {
      syslog_buffer_index++;
    }
  }

  if(syslog_stdxxx_buffer != NULL)
  {
    free(syslog_stdxxx_buffer);
  }
}

//==============================================================================
// Forward a message to CLI, if possible
int hcom_host_send_stdxxx_to_cli(int useableBufSize, uint8_t fifo_buffer[])
{
  int ret;

  // Send to CLI. Includes ctrl character(s)
  ret = hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_MONO_STDOUT, 0,
    (char *) fifo_buffer, useableBufSize, thisFile, __LINE__);

  if (ret < 0)
  {
    if(ret == -EAGAIN)
    {
      // The only reason the send would be blocked is that there is no host
      // listening for a receive message. This is normal for HCOM
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

//=====================================================================
// Open specified stdxxx per caller supplied arguments
static int hcom_mono_open_mono_fifo(int stdxxx_fifo_fd,
  char *fifoRedirect)
{
  int ret;

  if (stdxxx_fifo_fd < 0)
  {
    // Open stdxxx fifo
    do
    {
      // Opening with O_NONBLOCK seems like the right thing to do but
      // it is NOT. It causes the mono app to halt.
      stdxxx_fifo_fd = open(fifoRedirect, O_WRONLY);
      if (stdxxx_fifo_fd >= 0)
        break; // Success

      if (errno != ENOENT) // ENOENT = Error No Entity -> No such file or directory
      {
        // All other errors exit
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
                            thisFile, __LINE__, fifoRedirect, errno);
        stdxxx_fifo_fd = -1;
        return -errno;
      }

      // Sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the fifo's write end to the stdxxx fd.
    ret = dup2(stdxxx_fifo_fd, STDOUT_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer (%s): dup2 failed ret:%d errno:%d\n",
                          thisFile, __LINE__, fifoRedirect, ret, errno);
      return -errno;
    }

    close(stdxxx_fifo_fd);
  }

  return OK;
}

//==================================================================
// Public function
// This function is called from the mono_main's main thread. This insures
// that the fifos can redirect the stdout and stderr messages from mono
// here to be routed.
int hcom_mono_stdxxx_redirect(void)
{
  int ret;

  // Open stdout fifo
  ret = hcom_mono_open_mono_fifo(_stdout_fifo_fd, HCOM_MONO_STDOUT_REDIRECT_FIFO);
  if(ret < 0)
  {
    return ret;
  }
  
  // Open stderr fifo
  ret = hcom_mono_open_mono_fifo(_stderr_fifo_fd, HCOM_MONO_STDERR_REDIRECT_FIFO);
  if(ret < 0)
  {
    return ret;
  }
  return OK;
}

#else // #if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)

int hcom_mono_stdxxx_read_setup()
{
  return OK;
}
void hcom_mono_stdxxx_read_shutdown()
{
}
#endif  // #if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)
