/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_comms_interface.c
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

#include "../hcom_common.h"

#include <fcntl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_COMMS_DEBUG 0
#if HCOM_COMMS_DEBUG > 0
#define hcom_comms_dbg(...) f7syslog(__VA_ARGS__)
#define hcom_comms_dbg_x(...) f7syslog_x(__VA_ARGS__)
#else
#define hcom_comms_dbg(...)
#define hcom_comms_dbg_x(...)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

// Both of the file descriptors are "detached." Meaning that multiple threads
// can use them.
static FAR struct file _connection_read_file_fd;
static bool _is_connection_read_open;
static FAR struct file _connection_write_file_fd;
static bool _is_connection_write_open;

static uint8_t *_tempRecvBuff;
static bool _firstTimeToConnect;
static timer_t _recv_timerid;
static bool _hcom_comms_recv_timed_out;

static uint8_t *_encodedXmitBuff;
static sem_t _hostXmitSem;    /* Implements event waiting */
static bool _lastXmitBlocked;

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

int hcom_comms_setup()
{
  _shutting_down = false;
  _is_connection_read_open = false;
  _is_connection_write_open = false;
  _firstTimeToConnect = true;
  _lastXmitBlocked = false;

  _tempRecvBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);
  _encodedXmitBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);

  sem_init(&_hostXmitSem, 0, 1);
  // p-m pretty sure this is not needed
  sem_setprotocol(&_hostXmitSem, SEM_PRIO_NONE);

  deviceName = CONFIG_HCOM_COMMS_DEVICE_NAME;

  /* If we detect that we are booting into QEMU, then use serial comms
     instead of the configured device name (USB ACM) */

  if (hcom_utils_boot_time_qemu_check())
    deviceName = "/dev/ttyS1";

  return OK;
}

//=======================================================================
void hcom_comms_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  file_close(&_connection_read_file_fd);
  _is_connection_read_open = false;
  file_close(&_connection_write_file_fd);
  _is_connection_write_open = false;
  free(_tempRecvBuff);
  free(_encodedXmitBuff);

  // use sem_destroy
  sem_destroy(&_hostXmitSem);
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

  // This loop only runs when we loose a connection
  while(! _shutting_down)
  {
    // todo - This is a poor solution. Is this really a problem?
    if(wait_before_retry)
      sleep(15);    // Delay for certain return values. Thus limiting error messages
      
    // Establish the connection
    ret = hcom_comms_open_connection();
    if (ret < 0)
    {
      // TODO - should not output messages every second (timer or count).
      f7syslog(LOG_ERR, "%s@%d-Error:connection not made, %d\n", thisFile, __LINE__, ret);
      sleep(1);   // Can't leave this loop or all hcom will stop
      continue;
    }

    if(_firstTimeToConnect)
    {
      _firstTimeToConnect = false;
      hcom_comms_handle_initial_connection();
    }

    // Some errors need a delay
    wait_before_retry = hcom_comms_receive_data();
  }

  return OK;
}

//=======================================================================
int hcom_comms_handle_initial_connection()
{
  int ret;
  char * monoStartupMsg;

  // Check if a command was responsible for this restart, If it was a `Concluded` message must be sent
  bool is_restart = hcom_utils_bbreg_bit_test_and_clear(HCOM_BATTERY_BACKED_REG_BIT_FLAGS,
    HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  if(is_restart)
  {
    ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s@%d-Host message error:%d\n", thisFile, __LINE__, ret);
  }

  //--------------------------------------------
  if(hcom_utils_is_mono_disabled())
    monoStartupMsg = "Mono disabled, will not run app.exe";
  else
    monoStartupMsg = "Mono enabled, will run app.exe";

  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, monoStartupMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s@%d-Host message error:%d\n", thisFile, __LINE__, ret);

  return OK;
}

//=======================================================================
// 
int hcom_comms_open_connection()
{
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  if(_is_connection_read_open)
    return OK;

  hcom_comms_dbg(LOG_DEBUG, "%s@%d-usb open read %s\n", thisFile, __LINE__,
        HCOM_COMMUNICATIONS_DEVICE_NAME);

  while(!_shutting_down)
  {
    // Open reader
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
// Receive all host data and calls transmit to responsed as needed. This thread
// is the only thread receiving via usb serial.
bool hcom_comms_receive_data()
{
  hcom_comms_dbg(LOG_DEBUG, "Waiting for '%s' message\n",
      HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Stay in this loop forever
  while (!_shutting_down)
  {
    ssize_t readResult = hcom_comms_recv_wait_until_change(_tempRecvBuff,
              hcom_exec_rqst_download_is_download_active() ?
                HCOM_RECV_TIMEOUT_ACTIVE : HCOM_RECV_TIMEOUT_DEFAULT);

    // Return > 0 valid data received and this is the length
    if (readResult > 0)
    {
      // We've received some data
      int result = hcom_comms_recv_process_raw_data(_tempRecvBuff, readResult);
      if (result < 0)
      {
        f7syslog(LOG_WARNING, "%s@%d-Warning:%d received\n", thisFile, __LINE__, result);
      }
      continue;
    }

    if (readResult == 0)
    {
      // readResult must == 0 (end-of-file). Host PC probably dropped connection
      f7syslog(LOG_INFO, "%s@%d-HCOM received EOF\n", thisFile, __LINE__);
      continue;
    }

    // readResult < 0
    if (readResult == -ETIMEDOUT) // Time out is usually not a problem
    {
      if (hcom_exec_rqst_download_is_download_active())
      {
        f7syslog(LOG_WARNING, "%s@%d-Warning:Comms stopped. Recvd:%d of msg\n", thisFile, __LINE__, readResult);
      }
      else
      {
        // Timeout received while waiting for a host communication. This is nothing as we will
        // almost always be waiting and not receiving.
        f7syslog(LOG_INFO, "HCOM thread running\n");
      }
    }
    else
    {
      bool delayBeforeRetry;

      // Treat all errors the same. Drop the connection and try again
      if (readResult == -ENOTCONN || readResult == -ENOTSOCK || readResult == -ENETDOWN)
      {
        // Host dropped connection - calling read will only repeat the error
        f7syslog(LOG_NOTICE, "%s@%d-USB connection dropped.\n", thisFile, __LINE__);
        delayBeforeRetry = true;    // Delay retry
      }
      else
      {
        f7syslog(LOG_ERR, "%s@%d-Error:HCOM recv error:%d\n", thisFile, __LINE__, readResult);        
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
    f7syslog(LOG_INFO, "%s@%d-EOF recv\n", thisFile, __LINE__);
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
    f7syslog(LOG_ERR, "%s@%d-Error:setting timer errno:%d\n", thisFile, __LINE__, errorcode);
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
    f7syslog(LOG_ERR, "%s@%d-Error:create timer errno:%d\n", thisFile, __LINE__, errorcode);
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
    f7syslog(LOG_ERR, "%s@%d-Error:attach signal errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }
  return OK;
}

//===================================================================================
// Wait for the thread writing to exit
static void hcom_comms_transmit_takesem(void)
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
static int hcom_comms_open_connection_write(void)
{
  int ret;

  if(_is_connection_write_open)
    return OK;

  hcom_comms_dbg_x(LOG_DEBUG, "%s@%d-Open %s write connection\n", thisFile, __LINE__,
      HCOM_COMMUNICATIONS_DEVICE_NAME);

  // Based on observation - If O_NONBLOCK is not specified in the file_open call, the file_read
  // call blocks after writing some number of bytes. It's as if some internal buffer fills causing
  // the file_write call to begin blocking. This is not acceptable as the calling thread has other
  // work to do.
  ret = file_open(&_connection_write_file_fd, deviceName,
    O_WRONLY|O_NONBLOCK);
  if(ret < 0)
  {
    f7syslog_x(LOG_ERR, "%s@%d-Error:Open USB write errno:%d\n", thisFile, __LINE__, errno);
    return ret;
  }

  _is_connection_write_open = true;
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
// This MUST be called before hcom_comms_transmit_to_host() is called.
bool hcom_comms_was_host_xmit_blocked()
{
  int ret;

  // Last attempt was not blocked. Caller should attempt to send.
  if(!_lastXmitBlocked)
    return false;

  // Only one thread at a time can send to host
  hcom_comms_transmit_takesem();

  if(! _is_connection_write_open)
  {
    ret = hcom_comms_open_connection_write();
    if(ret < 0)
    {
      sem_post(&_hostXmitSem);
      return true;  // Not blocked but another error
    }
  }

  hcom_comms_dbg_x(LOG_DEBUG, "%s@%d-Sending 0 to host\n", thisFile, __LINE__);

  uint8_t oneZero[1];
  oneZero[0] = '\0';

  // Send a '0' message that the host knows to ignore.
  ssize_t writeRet = file_write(&_connection_write_file_fd, &oneZero, 1);
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
int hcom_comms_transmit_to_host(FAR uint8_t xmitBuffer[], size_t xmitLength)
{
  #define HCOM_XMIT_MAX_BLOCKED_TIME_DELAY  (5 * 1000)
  #define HCOM_XMIT_MAX_BLOCKED_COUNT_VALUE 800 // 5ms each = 4 seconds

  int ret;
  size_t remainingBytes;
  size_t toWriteOffset = 0;
  size_t blockedCount = 0;

  if(_shutting_down)
    return OK;

  // Only one thread at a time
  hcom_comms_transmit_takesem();

  // Encode
  size_t encodedLength = hcom_comms_cobs_encoder(xmitBuffer, 0, xmitLength, _encodedXmitBuff);

  // Encoded message needs a terminating delimiter for COBS
  DEBUGASSERT(encodedLength < HCOM_SAFE_PACKET_BUF_SIZE - 1);
  _encodedXmitBuff[encodedLength] = HCOM_PROTOCOL_PACKET_DELIMITER_VALUE;

  encodedLength++;
  remainingBytes = encodedLength;

  if(! _is_connection_write_open)
  {
    ret = hcom_comms_open_connection_write();
    if(ret < 0)
      return ret;
  }

  // Since there's no guarantee all bytes written at one time, loop until message 100% written
  while (remainingBytes > 0)
  {
    ssize_t writeRet = file_write(&_connection_write_file_fd, &_encodedXmitBuff[toWriteOffset], remainingBytes);
    if(writeRet >= 0)
    {
      remainingBytes -= writeRet;   // Note: if remainingBytes == 0 will exit while loop
      toWriteOffset += writeRet;

      hcom_comms_dbg_x(LOG_DEBUG, "%s@%d-Send %d bytes, sent %d (%d remaining) will %s\n\n",
          thisFile, __LINE__, encodedLength, writeRet, remainingBytes == 0 ? "exit" : "retry");

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
        hcom_comms_dbg_x(LOG_DEBUG, "%s@%d-Resend #%d\n", thisFile, __LINE__, blockedCount);
        continue;
      }

      // Set the global flag - seems the host isn't connected or CLI not running
      _lastXmitBlocked = true;
      hcom_comms_dbg_x(LOG_DEBUG, "%d USB write attempts (wrote %d, %d remain of %d bytes), message not sent\n",
                    blockedCount, toWriteOffset, remainingBytes, encodedLength);
      hcom_utils_diag_print_buffer(_encodedXmitBuff, encodedLength, LOG_DEBUG);

      // No reason to close fd. The caller can sort out what to do with partial data.
      sem_post(&_hostXmitSem);
      return writeRet;
    }

    f7syslog_x(LOG_ERR, "%s@%d-Error:USB  host write, error:%d\n", thisFile, __LINE__, writeRet);

    file_close(&_connection_write_file_fd);
    _is_connection_write_open = false;

    sem_post(&_hostXmitSem);
    return writeRet;
  } // while (remainingBytes > 0)

  // Success exit
  _lastXmitBlocked = false;

  sem_post(&_hostXmitSem);
  return OK;
}
