/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_usb_acm_interface.c
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
// The low level receiver and transmitter live here. These are the functions that
// need use the connections file descriptor.
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"

#include <fcntl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;

// Both of the file descriptors are "detached." Meaning that multiple threads
// can use them.
static FAR struct file _usb_read_file_fd;
static bool _is_usb_read_open;
static FAR struct file _usb_write_file_fd;
static bool _is_usb_write_open;

static uint8_t *_tempRecvBuff;
static bool _firstTimeToConnect;
static timer_t _recv_timerid;
static bool _hcom_recv_timed_out;

static sem_t _hostXmitSem;    /* Implements event waiting */
static bool _lastXmitBlocked;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool hcom_usb_com_receive_data(void);
static ssize_t hcom_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout);
static void hcom_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static int hcom_receive_timerstart(timer_t timerid, time_t sec);
static int hcom_recv_timerInit(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_usb_acm_setup()
{
  _shutting_down = false;
  _is_usb_read_open = false;
  _is_usb_write_open = false;
  _firstTimeToConnect = true;
  _lastXmitBlocked = false;

  _tempRecvBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);
  // use sem_init
  sem_init(&_hostXmitSem, 0, 1);
  sem_setprotocol(&_hostXmitSem, SEM_PRIO_NONE);
  return OK;
}

//=======================================================================
void hcom_usb_acm_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  file_close(&_usb_read_file_fd);
  _is_usb_read_open = false;
  file_close(&_usb_write_file_fd);
  _is_usb_write_open = false;
  free(_tempRecvBuff);
  // use sem_destroy
  sem_destroy(&_hostXmitSem);
}

//=======================================================================
// A dedicated thread lives here. However, this thread can call throughout
// hcom
int hcom_usb_acm_recv_thread_loop()
{
  int ret;
  bool wait_before_retry = false;

  // Same thread must init as uses the timer
  hcom_recv_timerInit();

  // This loop only runs when we loose a connection
  while(! _shutting_down)
  {
    // todo - This is a poor solution. Is this really a problem?
    if(wait_before_retry)
      sleep(15);    // Delay for certain return values. Thus limiting error messages
      
    // Establish the connection
    ret = hcom_usb_acm_open_wait_for_usb();
    if (ret < 0)
    {
      // TODO - should not output messages every second (timer or count).
      f7syslog(LOG_ERR, "%s() ERROR: Failed to establish a connection %d\n", __func__, ret);
      sleep(1);   // Can't leave this loop or all hcom will stop
      continue;
    }

    // Some errors need a delay
    wait_before_retry = hcom_usb_com_receive_data();
  }
  return OK;
}

//=======================================================================
// 
int hcom_usb_acm_open_wait_for_usb()
{
  int ret;
  char * monoStartupMsg;
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  if(_is_usb_read_open)
    return OK;

  f7syslog(LOG_DEBUG, "%s() Attempting open read connection to %s\n", __func__,
    HCOM_COMMUNICATIONS_DEVICE_NAME);

  while(!_shutting_down)
  {
    // Open reader
    ret = file_open(&_usb_read_file_fd, HCOM_COMMUNICATIONS_DEVICE_NAME, O_RDONLY);
    if(ret >= 0)
    {
      _is_usb_read_open = true;
      break;
    }

    // TODO - consider inspecting ret for problems?
    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }

    // Wait and try again at first every 50 millisec then every 5 seconds
    usleep(hostConnectionAttemptCount > 0 ? HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

  //--------------------------------------------------------------------------------
  f7syslog(LOG_INFO, "%s() - %s ready for host communications\n", __func__, HCOM_COMMUNICATIONS_DEVICE_NAME);

  if(_firstTimeToConnect)
  {
    _firstTimeToConnect = false;

    // Check if a command was responsible for this restart, If it was a `Concluded` message must be sent
    bool flagCheck = hcom_bbreg_bit_test_and_clear(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);
    if(flagCheck)
    {
      ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestConcluded, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
    }

    //--------------------------------------------
    if(hcom_is_mono_disabled())
      monoStartupMsg = "Mono is currently disabled and will not run applications";
    else
      monoStartupMsg = "Mono is currently enabled to run applications";

    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, monoStartupMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  }
  return OK;
}

//========================================================================
// Receive all host data and calls transmit to responsed as needed. This thread
// is the only thread receiving via usb serial.
bool hcom_usb_com_receive_data()
{
  f7syslog(LOG_DEBUG, "Waiting for message to be received from:'%s'\n",
      HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Stay in this loop forever
  while (!_shutting_down)
  {
    ssize_t readResult = hcom_recv_wait_until_change(_tempRecvBuff,
              hcom_exec_rqst_download_is_download_active() ? HCOM_RECV_TIMEOUT_ACTIVE : HCOM_RECV_TIMEOUT_DEFAULT);

    // Return > 0 valid data received and this is the length
    if (readResult > 0)
    {
      // We've received some data
      int result = hcom_recv_process_raw_data(_tempRecvBuff, readResult);
      if (result != OK)
      {
        f7syslog(LOG_WARNING, "%s() WARNING: Returned error %d\n", __func__, result);
      }
      continue;
    }

    if (readResult == 0)
    {
      // readResult must == 0 (end-of-file). Host PC probably dropped connection
      f7syslog(LOG_INFO, "%s() - HCOM usb received End-Of-File indication\n", __func__);
      continue;
    }

    // readResult < 0
    if (readResult == -ETIMEDOUT) // Time out is usually not a problem
    {
      if (hcom_exec_rqst_download_is_download_active())
      {
        f7syslog(LOG_WARNING, "%s() WARNING: Received %d bytes, then unexpectedly stopped\n",
            __func__, readResult);
      }
      else
      {
        // Timeout received while waiting for a host communication. This is nothing as we will
        // almost always be waiting and not receiving.
        f7syslog(LOG_INFO, "HCOM receive: Thread still running\n");
      }
    }
    else
    {
      bool delayBeforeRetry;

      // Treat all errors the same. Drop the connection and try again
      if (readResult == -ENOTCONN || readResult == -ENOTSOCK || readResult == -ENETDOWN)
      {
        // Host dropped connection - calling read will only repeat the error
        f7syslog(LOG_NOTICE, "%s() - Host dropped USB connection. Will retry shortly.\n", __func__);
        delayBeforeRetry = true;    // Delay retry
      }
      else
      {
        f7syslog(LOG_ERR, "%s() ERROR: HCOM received unexpected error: %d\n", __func__, readResult);        
        delayBeforeRetry = false;    // No retry delay
      }

      file_close(&_usb_read_file_fd);
      _is_usb_read_open = false;

      return delayBeforeRetry; // get a new connection and repeat
    }
  }   // while(!_shutting_down)

  return false;
}

//=============================================================================
// Receive what the host has to send. On error or timeout return > 0
ssize_t hcom_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout)
{
  ssize_t readReturn;

  // From Nuttx User Guide (editied): sched_lock() [non-posix]
  // This function disables context switching by disabling addition of new tasks to
  // the ready-to-run task list. The task that calls this function will be the only task
  // that is allowed to run until it either 1) calls sched_unlock (the appropriate
  // number of times) or 2) packets itself (which the read does if no data).
  sched_lock();
  _hcom_recv_timed_out = false;

  // Start/restart the timer.  Whenever we read data from the host we must anticipate
  // a timeout because we can never be sure that the host won't just die before the end.
  hcom_receive_timerstart(_recv_timerid, readTimeout);

  // This is a blocking read. read() will return:
  // (1) readReturn > 0 and readReturn <= buffer size on success
  // (2) readReturn == 0 on end of file
  // (3) readReturn < 0 on a read error or interruption by a signal
  readReturn = file_read(&_usb_read_file_fd, recvBuffer, HCOM_PROTOCOL_PACKET_MAX_SIZE);
  (void)hcom_receive_timerstart(_recv_timerid, 0); // Stop the timer
  sched_unlock();

  if (readReturn > 0)
    return readReturn; // Received data

  if (readReturn == 0)
  {
    f7syslog(LOG_INFO, "%s() end-of-file\n", __func__);
    return -ENOTCONN; // "Transport endpoint is not connected" [128] - Probably time to shutdown
  }

  // readReturn < 0
  // EINTR (Error Interrupt) is not an error... it simply means that this read was
  // interrupted by a signal before it obtained data. The signal may be SIGALRM
  // indicating an timeout condition. We will know this case because the signal handler
  // set _hcom_recv_timed_out to true 
  if (readReturn == -EINTR)
  {
    // Check timeout flag
    if (_hcom_recv_timed_out)
    {
      // This is normal for this thread as 99.999% of the time there
      // will be no host PC communicating with us.
      // Restart receiving and wait for communications to begin again
      readReturn = -ETIMEDOUT; // "Connection timed out" [116]
    }
    // No.. then just ignore the EINTR.
  }

  return readReturn;
}

/****************************************************************************
 * Name: hcom_recv_timeout_expired
 *
 * Description:
 *   SIGALRM signal handler.  Simply posts the timeout event.
 *
 ****************************************************************************/
void hcom_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context)
{
  /* Just set the timeout flag.
   * REVISIT:  This is a read-modify-write operation and has the potential
   * for atomicity issue.  We might need to use a dedicated boolean value
   * to indicate to timeout!
   */
  _hcom_recv_timed_out = true;
}

/****************************************************************************
 * Name:  hcom_receive_timerstart
 *
 * Description:
 *   Start, restart, or stop the timer.
 *
 ****************************************************************************/
int hcom_receive_timerstart(timer_t timerid, time_t sec)
{
  struct itimerspec todelay;
  int ret;

  /* Start, restart, or stop the timer */
  todelay.it_interval.tv_sec = 0; /* Nonrepeating */
  todelay.it_interval.tv_nsec = 0;
  todelay.it_value.tv_sec = sec;
  todelay.it_value.tv_nsec = 0;

  ret = timer_settime(timerid, 0, &todelay, NULL);
  if (ret < 0)
  {
    int errorcode = errno;
    f7syslog(LOG_ERR, "%s() ERROR: Failed to set the timer: %d\n", __func__, errorcode);
    return -errorcode;
  }
  return OK;
}

/****************************************************************************
 * Name:  hcom_recv_timerInit
 *
 * Description:
 *   Create the POSIX timer used to manage timeouts and attach the SIGALRM
 *   signal handler to catch the timeout events.
 *
 ****************************************************************************/
int hcom_recv_timerInit()
{
  struct sigevent toevent;
  struct sigaction act;
  int ret;

  /* Create a POSIX timer to handle timeouts */
  toevent.sigev_notify = SIGEV_SIGNAL;
  toevent.sigev_signo = SIGALRM;
  //toevent.sigev_value.sival_ptr = pzm;  // Carry value to 'context' on expiration

  ret = timer_create(CLOCK_REALTIME, &toevent, &_recv_timerid);
  if (ret < 0)
  {
    int errorcode = errno;
    f7syslog(LOG_ERR, "%s() ERROR: Failed to create a timer: %d\n", __func__, errorcode);
    return -errorcode;
  }

  /* Attach a signal handler to catch the timeout */
  act.sa_sigaction = hcom_recv_timeout_expired;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  ret = sigaction(SIGALRM, &act, NULL);
  if (ret < 0)
  {
    int errorcode = errno;
    f7syslog(LOG_ERR, "%s() ERROR: Failed to attach a signal handler: %d\n", __func__, errorcode);
    return -errorcode;
  }
  return OK;
}

//===================================================================================
// Wait for the thread writing to exit
static void hcom_usb_acm_transmit_takesem(void)
{
  int ret;

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = sem_wait(&_hostXmitSem);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//=====================================================================
static int hcom_usb_acm_open_host_write_fd(void)
{
  int ret;

  if(_is_usb_write_open)
    return OK;

  f7syslog_x(LOG_DEBUG, "%s() Attempting to open write connection to %s\n", __func__,
      HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Based on observation - If O_NONBLOCK is not specified in the file_open call, the file_read
  // call blocks after writing some number of bytes. It's as if some internal buffer fills causing
  // the file_write call to block. This is not acceptable as the calling thread has other work
  // to do.
  ret = file_open(&_usb_write_file_fd, HCOM_COMMUNICATIONS_DEVICE_NAME, O_WRONLY|O_NONBLOCK);
  if(ret < 0)
  {
    f7syslog_x(LOG_ERR, "%s() ERROR: Failed to open USB write handle %d\n", __func__, errno);
    return ret;
  }

  _is_usb_write_open = true;
  return OK;
}

//=====================================================================
// Usually, no receiver is running and consuming messages, the messages eventually
// will be blocked, after filling internal buffer space. To workaround this, once
// we get a -EAGAIN error (i.e. blocked) we'll attempt to send 0x00 before every
// message. This way, when the CLI begins to consume messages our 0x00 will be the
// first thing to arrive after whatever nuttx has buffered (probably a bunch of
// 0x00 bytes). The CLI is programmed to ignore a single 0x00 byte message.
// Therefore, the message after the blockage is removed can be sent successfully
// and be properly parsed.
//
// This MUST be called before hcom_usb_acm_transmit_to_host() is called.
bool hcom_usb_acm_was_host_xmit_blocked()
{
  int ret;

  // Last attempt was not blocked. Caller should attempt to send.
  if(!_lastXmitBlocked)
    return false;

  // Only one thread at a time can send to host
  hcom_usb_acm_transmit_takesem();

  if(! _is_usb_write_open)
  {
    ret = hcom_usb_acm_open_host_write_fd();
    if(ret < 0)
    {
      sem_post(&_hostXmitSem);
      return true;  // Not blocked but another error
    }
  }

  f7syslog_x(LOG_DEBUG, "%s() - Sending single 0 to test host blockage.\n", __func__);

  uint8_t oneZero[1];
  oneZero[0] = '\0';

  // Send a '0' message that the host knows to ignore.
  ssize_t writeRet = file_write(&_usb_write_file_fd, &oneZero, 1);
  if(writeRet == 1)
  {
    // Write successfull, no longer blocked
    _lastXmitBlocked = false;
    sem_post(&_hostXmitSem);
    return false;
  }
  
  sem_post(&_hostXmitSem);
  return true;    // blocked or some error
}

//===================================================================================
// All messages sent to host pass through here.
// At this time 2 threads use this method
int hcom_usb_acm_transmit_to_host(FAR const uint8_t xmitBuffer[], size_t xmitLength)
{
  #define HCOM_XMIT_MAX_BLOCKED_TIME_DELAY  (5 * 1000)
  #define HCOM_XMIT_MAX_BLOCKED_COUNT_VALUE 800 // 5ms each = 4 seconds

  int ret;
  size_t remainingBytes = xmitLength;
  size_t toWriteOffset = 0;
  size_t blockedCount = 0;

  if(_shutting_down)
    return OK;

  // Only one thread at a time
  hcom_usb_acm_transmit_takesem();

  if(! _is_usb_write_open)
  {
    ret = hcom_usb_acm_open_host_write_fd();
    if(ret < 0)
      return ret;
  }

  // Since there's no guarantee all bytes written at one time, loop until message 100% written
  while (remainingBytes > 0)
  {
    ssize_t writeRet = file_write(&_usb_write_file_fd, &xmitBuffer[toWriteOffset], remainingBytes);
    if(writeRet >= 0)
    {
      remainingBytes -= writeRet;   // Note: if remainingBytes == 0 will exit while loop
      toWriteOffset += writeRet;

      f7syslog_x(LOG_DEBUG, "%s() - Need to send %d bytes, sent %d (%d remaining) will %s\n\n",
          __func__, xmitLength, writeRet, remainingBytes == 0 ? "exit" : "retry");

      continue;
    }

    // Examine error
    // EINTR is not really an error... it simply means that this write was
    // interrupted by a signal before it wrote the data.
    if (writeRet == -EINTR)
      continue;

    if(writeRet == -EAGAIN)
    {
      // Write attempt was blocked, either host PC not connected, CLI not running
      // or PC just can't keep up. Give it a chance to catchup.
      if(blockedCount < HCOM_XMIT_MAX_BLOCKED_COUNT_VALUE)
      {
        blockedCount++;
        usleep(HCOM_XMIT_MAX_BLOCKED_TIME_DELAY);
        f7syslog_x(LOG_DEBUG, "Attempting to re-send after %d attempts\n", blockedCount);
        continue;
      }

      f7syslog_x(LOG_INFO, "After %d attempts, wrote %d bytes %d remained of %d total. Message sent terminated.\n",
                    blockedCount, toWriteOffset, remainingBytes, xmitLength);
      hcom_diag_print_buffer(xmitBuffer, xmitLength, LOG_DEBUG);

      // Set the global flag - seems the host isn't connected or CLI not running
      _lastXmitBlocked = true;
      f7syslog_x(LOG_DEBUG, "FAILED to send complete message after %d blocked attempts\n", blockedCount);

      // No reason to close fd. The caller can sort out what to do with partial data.
      sem_post(&_hostXmitSem);
      return writeRet;
    }

    f7syslog_x(LOG_ERR, "%s() ERROR: Write to host via usb, error %d\n", __func__, writeRet);

    file_close(&_usb_write_file_fd);
    _is_usb_write_open = false;

    sem_post(&_hostXmitSem);
    return writeRet;
  } // while (remainingBytes > 0)

  // Success exit
  _lastXmitBlocked = false;

  sem_post(&_hostXmitSem);
  return OK;
}
