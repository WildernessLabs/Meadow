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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;
static int _hcom_connection_fd; // Used for sending to and receiving from host
static timer_t _recv_timerid;
static bool _hcom_recv_timed_out;
static bool _firstTime;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_com_make_host_connection(void);
static void hcom_host_com_receive_data(void);
static ssize_t hcom_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout);

static void hcom_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static int hcom_receive_timerstart(timer_t timerid, time_t sec);
static int hcom_recv_timerInit(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_com_xmit_rcv_setup()
{
  _shutting_down = false;
  _firstTime = true;  
  _hcom_connection_fd = 0;    // fd 0 is stdin
  return OK;
}

//=======================================================================
void hcom_host_com_xmit_rcv_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  close(_hcom_connection_fd);
}

//=======================================================================
// A new thread calls here when starting
int hcom_host_com_recv_thread_loop()
{
  int ret;

  if(_firstTime)
  {
    // Same thread must init as will use timer
    hcom_recv_timerInit();
    _firstTime = false;
  }

  while(! _shutting_down)
  {
    // Establish the connection
    ret = hcom_host_com_make_host_connection();
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Failed to establish an initial connection %d\n", __func__, ret);
      return ret;
    }

    // Only returns on exit or loss of connection
    hcom_host_com_receive_data();
  }

  return OK;
}

//=======================================================================
// 
int hcom_host_com_make_host_connection()
{
  FAR const char *devname = HCOM_COMMUNICATIONS_DEVICE_NAME;
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  // Give the Nuttx startup thread a chance to finish, at least until /dev/ttyACM0 exists
  f7syslog(LOG_DEBUG, "Attempting connection to %s\n", devname);

  while(!_shutting_down)
  {
    // Thread will hang here until connection is open
    _hcom_connection_fd = open(devname, O_RDWR);
    if (_hcom_connection_fd >= 0)
      break;

    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }
    // Wait and try again at first every 50 millisec then every 5 seconds
    // this allows for cleaner shutdown
    usleep(hostConnectionAttemptCount > 0 ? HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

  f7syslog(LOG_INFO, "%s ready, waiting for host communications.\n", devname);
  return OK;
}

//========================================================================
// Thread enters here. It receives all host data and calls transmit
// to responsed as needed.
void hcom_host_com_receive_data()
{
  uint8_t tempRecvBuff[HCOM_PACKET_MAX_SIZE];

  f7syslog(LOG_DEBUG, "Waiting for message to be received from:'%s'\n", HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Stay in this loop forever
  while (!_shutting_down)
  {
    ssize_t readResult = hcom_recv_wait_until_change(tempRecvBuff,
                hcom_exec_rqst_download_is_dowload_active() ? HCOM_RECV_TIMEOUT_ACTIVE : HCOM_RECV_TIMEOUT_DEFAULT);

    // Return > 0 probably valid data received and this is the length
    if (readResult > 0)
    {

      // We've received some data
      int result = hcom_recv_process_raw_data(tempRecvBuff, readResult);
      if (result != OK)
      {
        f7syslog(LOG_WARNING, "%s() WARNING: Returned error %d\n", __func__, result);
      }
    }
    else if (readResult < 0) // Returns of negative value can be bad, but not always
    {
      if (readResult == -ETIMEDOUT) // Time out is usually not a problem
      {
        if (hcom_exec_rqst_download_is_dowload_active())
        {
          f7syslog(LOG_WARNING, "%s() WARNING: Host sent %d bytes, then unexpectedly stopped\n", __func__, readResult);
          // ACTION TBD??
        }
        else
        {
          // Timeout receiving while waiting for a host communication. This is nothing as we will
          // almost always be waiting and not receiving.
          f7syslog(LOG_INFO, "HCOM receive: Thread still running\n");
        }
      }
      else
      {
        if (readResult == -ENOTCONN)
        {
          // Host dropped connection - calling read will only repeat the error
          f7syslog(LOG_NOTICE, "Host dropped USB connection. Will retry shortly.\n");

          close(_hcom_connection_fd);
          _hcom_connection_fd = 0;
          return; // get a new connection and repeat
        }

        f7syslog(LOG_ERR, "%s() ERROR: HCOM receive, unexpected error: %d\n", __func__, readResult);
        // ACTION TBD??
      }
    }
    else
    {
      // readResult must be 0 (end-of-file). Not sure this can be detected for a serial connection
      f7syslog(LOG_WARNING, "%s() WARNING: HCOM receive, Received End-Of-File indication\n", __func__);
      // ACTION TBD??
    }
  }
}

//-----------------------------------------------------------------------
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
  readReturn = read(_hcom_connection_fd, recvBuffer, HCOM_PACKET_MAX_SIZE);
  (void)hcom_receive_timerstart(_recv_timerid, 0); // Stop the timer
  sched_unlock();

  if (readReturn > 0)
    return readReturn; // Likely received data

  if (readReturn == 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Unexpected end-of-file\n", __func__);
    return -ENOTCONN; // "Transport endpoint is not connected" [128] - Probably time to shutdown
  }

  // EINTR (Error Interrupt) is not an error... it simply means that this read was
  // interrupted by a signal before it obtained data.  The signal may be SIGALRM
  // indicating an timeout condition. We will know this case because the signal handler
  // will set _hcom_recv_timed_out to true.
  int errorcode = errno;
  if (errorcode == EINTR)
  {
    // Check for a timeout
    if (_hcom_recv_timed_out)
    {
      // This is normal for this thread as 99.999% of the time there
      // will be no host communicating with us.
      // Restart the receiver and wait for communications to begin
      // If handshake was implemented send nak to host
      readReturn = -ETIMEDOUT; // "Connection timed out" [116]
    }
    // No.. then just ignore the EINTR.
  }
  else
  {
    // But anything else is bad and we will return the failure in those cases.
    f7syslog(LOG_ERR, "%s() ERROR: read() call returned: %d and errorcode: %d\n", __func__, readReturn, errorcode);
    readReturn = -errorcode;
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
// All messages sent to host call here.
// Currently, only one thread call here. If this changes extra protection will be needed.
int hcom_host_com_transmit_data(FAR const uint8_t xmitBuffer[], size_t xmitLength)
{
  size_t bytesToWrite = xmitLength;
  size_t toWriteOffset = 0;

  // No guarantee all bytes written in one shot so loop until all written
  while (bytesToWrite > 0)
  {
    size_t numbWritten = write(_hcom_connection_fd, &xmitBuffer[toWriteOffset], bytesToWrite);
    if (numbWritten < 0)
    {
      // Possible error
      int errorcode = errno;

      // EINTR is not an error... it simply means that this write was
      // interrupted by a signal before it wrote the data.
      if (errorcode == EINTR) // Not interrupt
        continue;

      f7syslog(LOG_ERR, "%s() ERROR: While writing to host errno: %d write returned: %d bytes\n",
                __func__, errorcode, numbWritten);
      return -errorcode;
    }
    else
    {
      toWriteOffset += numbWritten;
      bytesToWrite -= numbWritten;
    }
  }
  return OK;
}
