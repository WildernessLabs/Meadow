/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_comms_receive.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

#include "../hcom_common.h"

#include <fcntl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

// Both of the file descriptors are "detached." Meaning that multiple threads
// can use them.
static FAR struct file _connection_read_file_fd;
static bool _is_connection_read_open;

static uint8_t *_tempRecvBuff;
static bool _firstTimeToConnect;
static timer_t _recv_timerid;
static bool _hcom_comms_recv_timed_out;
static const char *deviceName;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool hcom_comms_receive_data(void);
static ssize_t hcom_comms_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout);
static void hcom_comms_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static int hcom_comms_recv_timer_start(timer_t timerid, time_t sec);
static int hcom_comms_recv_timer_init(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_comms_recv_setup()
{
  _shutting_down = false;
  _is_connection_read_open = false;
  _firstTimeToConnect = true;
  _tempRecvBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);  
  deviceName = CONFIG_HCOM_COMMS_DEVICE_NAME;

  /* If we detect that we are booting into QEMU, then use serial comms
     instead of the configured device name (USB ACM) */

  if (hcom_utils_boot_time_qemu_check())
    deviceName = "/dev/ttyS1";

  return OK;
}

//=======================================================================
const char *hcom_comms_recv_get_device_name()
{
  return deviceName;
}

//=======================================================================
void hcom_comms_recv_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  file_close(&_connection_read_file_fd);
  _is_connection_read_open = false;
  free(_tempRecvBuff);
}

//=======================================================================
// A dedicated thread lives here. However, this thread can call throughout
// hcom
int hcom_comms_recv_thread_loop()
{
  int ret;
  bool wait_before_retry = false;

  // Same thread must init as uses the timer
  hcom_comms_recv_timer_init();

  // This loop only runs initially and when we loose a host connection
  while(! _shutting_down)
  {
    // todo - This is a poor solution. Is this really a problem?
    if(wait_before_retry)
      sleep(1);    // Delay for certain return values. Thus limiting error messages
      
    // Attempt to establish the connection
    ret = hcom_comms_recv_open_connection();
    if (ret < 0)
    {
      // TODO - should not output messages every second.
      hcom_utils_f7syslog(LOG_ERR, "%s@%d-connection not made, %d\n", thisFile, __LINE__, ret);
      sleep(1);   // Can't leave this loop or all hcom will stop
      continue;
    }

    if(_firstTimeToConnect)
    {
      _firstTimeToConnect = false;
      hcom_comms_recv_restart_concluded();
    }

    // Some errors need a delay
    wait_before_retry = hcom_comms_receive_data();
  }

  return OK;
}

//=======================================================================
// When Meadow is restarted by some CLI command we need to send the
// "Concluded" message type
int hcom_comms_recv_restart_concluded()
{
  char *monoStartupMsg;

  // Check if a CLI command was responsible for this restart, If it was a
  // `Concluded` message must be sent
  if(hcom_utils_bbreg_is_bit_set_clear(HCOM_BATTERY_BACKED_REG_BIT_FLAGS,
    HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG))
  {
    hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
  }

  //--------------------------------------------
  // Report to host the status of mono
  if(hcom_utils_is_mono_disabled())
    monoStartupMsg = "Mono disabled, will not run app.exe";
  else
    monoStartupMsg = "Mono enabled, will run app.exe";

  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          monoStartupMsg, thisFile, __LINE__);
  return OK;
}

//=======================================================================
// 
int hcom_comms_recv_open_connection()
{
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  if(_is_connection_read_open)
    return OK;

  hcom_comms_dbg(LOG_DEBUG, "%s@%d-usb open read %s\n", thisFile, __LINE__,
        HCOM_COMMUNICATIONS_DEVICE_NAME);

  while(!_shutting_down)
  {
    // Open reader. file_open differs from 'open' in that open uses the
    // file descriptor that is part of a task where as file_open allows
    // for a "disconnected" file descriptor which can be shared by different
    // threads/tasks.
    int ret = file_open(&_connection_read_file_fd, deviceName, O_RDONLY);
    if(ret >= 0)
    {
      _is_connection_read_open = true;
      break;
    }

    // TODO - consider inspecting ret for problems?
    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }

    // Wait and try again at first every 50 millisec then every 5 seconds
    usleep(hostConnectionAttemptCount > 0 ?
      HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

  //--------------------------------------------------------------------------------
  hcom_comms_dbg(LOG_DEBUG, "%s ready for host comms\n", deviceName);

  return OK;
}

//========================================================================
// This thread receives all host data and may call transmit to responsed as
// needed. This thread is the only thread receiving via USB serial data.
bool hcom_comms_receive_data()
{
  hcom_comms_dbg(LOG_DEBUG, "Waiting for '%s' message\n", HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Stay in this loop forever
  while (!_shutting_down)
  {
    ssize_t readResult = hcom_comms_recv_wait_until_change(_tempRecvBuff,
              hcom_exec_rqst_download_is_download_active() ?
                HCOM_RECV_TIMEOUT_ACTIVE_SECONDS : HCOM_RECV_TIMEOUT_DEFAULT_SECONDS);

    // Return > 0 valid data received and this is the length
    if (readResult > 0)
    {
      // We've received some data
      int result = hcom_comms_recv_process_raw_data(_tempRecvBuff, readResult);
      if (result < 0)
      {
        hcom_utils_f7syslog(LOG_WARNING, "%s@%d-%d received\n", thisFile, __LINE__, result);
      }
      continue;
    }

    if (readResult == 0)
    {
      // readResult must == 0 (end-of-file). Host PC probably dropped connection
      hcom_utils_f7syslog(LOG_INFO, "%s@%d-HCOM received EOF\n", thisFile, __LINE__);
      continue;
    }

    // readResult < 0
    if (readResult == -ETIMEDOUT) // Time out is usually not a problem
    {
      if (hcom_exec_rqst_download_is_download_active())
      {
        hcom_utils_f7syslog(LOG_WARNING, "%s@%d-Comms stopped. Recvd:%d of msg\n", thisFile, __LINE__, readResult);
      }
      else
      {
        // Timeout received while waiting for a host communication. This is normal as we will
        // almost always be waiting and not receiving.
        hcom_utils_f7syslog(LOG_INFO, "%s thread running\n", HCOM_THREAD_NAME_HCOM_RECEIVE);
      }
    }
    else
    {
      bool delayBeforeRetry;

      // Treat all these errors the same. Drop the connection and try again
      if (readResult == -ENOTCONN || readResult == -ENOTSOCK || readResult == -ENETDOWN)
      {
        // Host dropped connection - calling read will only repeat the error
        hcom_utils_f7syslog(LOG_NOTICE, "%s@%d-USB connection dropped.\n", thisFile, __LINE__);
        delayBeforeRetry = true;    // Delay retry
      }
      else
      {
        hcom_utils_f7syslog(LOG_ERR, "%s@%d-HCOM recv error:%d\n", thisFile, __LINE__, readResult);        
        delayBeforeRetry = false;    // No retry delay
      }

      file_close(&_connection_read_file_fd);
      _is_connection_read_open = false;

      return delayBeforeRetry; // get a new connection and repeat
    }
  }   // while(!_shutting_down)

  return false;
}

//=============================================================================
// Receive what the host has to send. On error or timeout return > 0
ssize_t hcom_comms_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout)
{
  ssize_t readReturn;

  // From Nuttx User Guide (editied): sched_lock() [non-posix]
  // This function disables context switching by disabling addition of new tasks to
  // the ready-to-run task list. The task that calls this function will be the only task
  // that is allowed to run until it either 1) calls sched_unlock (the appropriate
  // number of times) or 2) packets itself (which the read does if no data).
  sched_lock();
  _hcom_comms_recv_timed_out = false;

  // Start/restart the timer.  Whenever we read data from the host we must anticipate
  // a timeout because we can never be sure that the host won't just die before the end.
  hcom_comms_recv_timer_start(_recv_timerid, readTimeout);

  // This is a blocking read. read() will return:
  // (1) readReturn > 0 and readReturn <= buffer size on success
  // (2) readReturn == 0 on end of file
  // (3) readReturn < 0 on a read error or interruption by a signal
  readReturn = file_read(&_connection_read_file_fd, recvBuffer, HCOM_PROTOCOL_PACKET_MAX_SIZE);
  (void)hcom_comms_recv_timer_start(_recv_timerid, 0); // Stop the timer
  sched_unlock();

  if (readReturn > 0)
    return readReturn; // Received data

  if (readReturn == 0)
  {
    hcom_utils_f7syslog(LOG_INFO, "%s@%d-EOF recv\n", thisFile, __LINE__);
    return -ENOTCONN; // "Transport endpoint is not connected" [128] - Probably time to shutdown
  }

  // readReturn < 0
  // EINTR (Error Interrupt) is not an error... it simply means that this read was
  // interrupted by a signal before it obtained data. The signal may be SIGALRM
  // indicating an timeout condition. We will know this case because the signal handler
  // set _hcom_comms_recv_timed_out to true 
  if (readReturn == -EINTR)
  {
    // Check timeout flag
    if (_hcom_comms_recv_timed_out)
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
 * Name: hcom_comms_recv_timeout_expired
 *
 * Description:
 *   SIGALRM signal handler.  Simply posts the timeout event.
 *
 ****************************************************************************/
void hcom_comms_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context)
{
  /* Just set the timeout flag.
   * REVISIT:  This is a read-modify-write operation and has the potential
   * for atomicity issue.  We might need to use a dedicated boolean value
   * to indicate to timeout!
   */
  _hcom_comms_recv_timed_out = true;
}

/****************************************************************************
 * Name:  hcom_comms_recv_timer_start
 *
 * Description:
 *   Start, restart, or stop the timer.
 *
 ****************************************************************************/
int hcom_comms_recv_timer_start(timer_t timerid, time_t sec)
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
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-setting timer errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }
  return OK;
}

/****************************************************************************
 * Name:  hcom_comms_recv_timer_init
 *
 * Description:
 *   Create the POSIX timer used to manage timeouts and attach the SIGALRM
 *   signal handler to catch the timeout events.
 *
 ****************************************************************************/
int hcom_comms_recv_timer_init()
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
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-create timer errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }

  /* Attach a signal handler to catch the timeout */
  act.sa_sigaction = hcom_comms_recv_timeout_expired;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  ret = sigaction(SIGALRM, &act, NULL);
  if (ret < 0)
  {
    int errorcode = errno;
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-attach signal errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }
  return OK;
}
