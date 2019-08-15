/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_usb_acm_interface.c
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

static bool _shutting_down = false;

// Special detached file descriptor that is thread neutral
static FAR struct file _usb_read_file;
static FAR struct file _usb_write_file;
static bool _is_usb_read_active;
static bool _is_usb_write_active;
static timer_t _recv_timerid;
static bool _hcom_recv_timed_out;

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
  _is_usb_read_active = false;  
  return OK;
}

//=======================================================================
void hcom_usb_acm_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  file_close(&_usb_read_file);
  _is_usb_read_active = false;
  file_close(&_usb_write_file);
  _is_usb_write_active = false;
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

  // This loop only occurs when we loose a connection
  while(! _shutting_down)
  {
    if(wait_before_retry)
      sleep(15);    // This keeps certain disconnect messages to a reasonable number
      
    // Establish the connection
    // todo - THERE ARE TWO FILE DESCRIPTORS ONE FOR READ AND ANOTHER
    // FOR WRITE. BUT, CURRENTLY THEY ARE OPENED AS IF THERE WAS
    // ONLY ONE. RETHING THIS.
    ret = hcom_usb_acm_open_wait_for_usb();
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Failed to establish a connection %d\n", __func__, ret);
      sleep(1);  // Can't ever return or this thread will cease to exist
      continue;   // Try again
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
  FAR const char *devname = HCOM_COMMUNICATIONS_DEVICE_NAME;
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  // TODO test if valid connection exists? Is this reliable?
  // if(file_fsync(&_usb_read_file) == OK)
  //   return OK;
  if(_is_usb_read_active && _is_usb_write_active)
    return OK;

  f7syslog(LOG_DEBUG, "Attempting connection to %s\n", devname);

  // Give the Nuttx startup thread a chance to finish, at least until /dev/ttyACM0 exists
  while(!_shutting_down)
  {
    // Open reader
    ret = file_open(&_usb_read_file, devname, O_RDONLY);
    if(ret >= 0)
    {
      // Open writer
      ret = file_open(&_usb_write_file, devname, O_WRONLY|O_NONBLOCK);
      if(ret >= 0)
      {
        _is_usb_read_active = true;
        _is_usb_write_active = true;
        break;
      }
      else
      {
        file_close(&_usb_read_file);
      }
    }

    // TODO - consider inspecting ret for problems?
    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }

    // Wait and try again at first every 50 millisec then every 5 seconds
    usleep(hostConnectionAttemptCount > 0 ? HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

  f7syslog(LOG_INFO, "%s() - %s ready for host communications.\n", __func__, devname);
  return OK;
}

//========================================================================
// Receive all host data and calls transmit to responsed as needed. This thread
// is the only thread running in hcom.
bool hcom_usb_com_receive_data()
{
  uint8_t tempRecvBuff[HCOM_PACKET_MAX_SIZE];

  f7syslog(LOG_DEBUG, "Waiting for message to be received from:'%s'\n", HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Stay in this loop forever
  while (!_shutting_down)
  {
    ssize_t readResult = hcom_recv_wait_until_change(tempRecvBuff,
              hcom_exec_rqst_download_is_download_active() ? HCOM_RECV_TIMEOUT_ACTIVE : HCOM_RECV_TIMEOUT_DEFAULT);

    // Return > 0 valid data received and this is the length
    if (readResult > 0)
    {
      // We've received some data
      int result = hcom_recv_process_raw_data(tempRecvBuff, readResult);
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
      // Treat all errors the same. Drop the connection and try again
      if (readResult == -ENOTCONN || readResult == -ENOTSOCK || readResult == -ENETDOWN)
      {
        // Host dropped connection - calling read will only repeat the error
        f7syslog(LOG_NOTICE, "%s() - Host dropped USB connection. Will retry shortly.\n", __func__);
        return true;    // Delay retry
      }
      else
      {
        f7syslog(LOG_ERR, "%s() ERROR: HCOM received unexpected error: %d\n", __func__, readResult);        
      }

      file_close(&_usb_read_file);
      _is_usb_read_active = false;

      file_close(&_usb_write_file);
      _is_usb_write_active = false;

      return false; // get a new connection and repeat
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
  readReturn = file_read(&_usb_read_file, recvBuffer, HCOM_PACKET_MAX_SIZE);
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
  else
  {
    // But anything else needs to be sorted out by the caller.
    readReturn = readReturn;
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
// All messages sent to host pass through here.
int hcom_usb_acm_transmit_to_host(FAR const uint8_t xmitBuffer[], size_t xmitLength)
{
  size_t bytesToWrite = xmitLength;
  size_t toWriteOffset = 0;

  irqstate_t flags; // Attempt to fix message overwrite

// Based on observation - Only after some internal buffer fills will th file_write
// block the calling thread and prevent any additional output. This is not acceptable.
// As a disconnected host will be the "normal" state.
// THIS NEEDS MORE TESTING AND VERIFICATION.
  if(_shutting_down)
    return OK;

  flags = enter_critical_section();

  // No guarantee all bytes written in one shot so loop until all written
  while (bytesToWrite > 0)
  {
    ssize_t numbWritten = file_write(&_usb_write_file, &xmitBuffer[toWriteOffset], bytesToWrite);
    if(numbWritten >= 0)
    {
      toWriteOffset += numbWritten;
      bytesToWrite -= numbWritten;
      
      syslog(LOG_DEBUG, "%s() - wrote %d bytes with %d remaining\n",
          __func__, numbWritten, bytesToWrite);
      continue;
    }

    if (numbWritten < 0)
    {
      // EINTR is not an error... it simply means that this write was
      // interrupted by a signal before it wrote the data.
      if (numbWritten == -EINTR) // Not interrupt
        continue;

      if(numbWritten == -EAGAIN)
      {
        // Blocked call, probably host PC not listening with internal buffer full.
        // We ignore the error but pass it on to the caller.
        leave_critical_section(flags);
        return numbWritten;
      }

      f7syslog(LOG_ERR, "%s() ERROR: Write to host via usb, errno: %d error %d. Returning\n",
                __func__, errno, numbWritten);
      leave_critical_section(flags);
      return numbWritten;
    }
  }
  leave_critical_section(flags);
  return OK;
}
