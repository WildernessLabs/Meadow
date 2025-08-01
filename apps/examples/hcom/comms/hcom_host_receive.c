/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_receive.c
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

// The low level hcom receiver lives here.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/meadow_pwr_mgmt.h>

#include <fcntl.h>

// Diagnostic only
// #define MEADOW_USE_HCOM_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define USE_ORIGINAL_READ_SCHEME (0)

// It seems the number of bytes requested for the read has little
// relationship on the number read. A value higher than 256 makes no
// difference. I assume it was because of the comms bandwidth limit.
// But I set it higher since the buffer is large enough, so why not?
#define HCOM_HOST_RECEIVE_MAX_READ_SIZE (1024)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

static int _comms_read_fd;
static uint8_t *_recvDataBuffer;
static bool _firstTimeToConnect;
static const char *deviceName;
static bool _lowPowerSoon;
static sem_t _hcomRecvLPSem;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool hcom_host_recv_received_data(void);
static void hcom_host_recv_open_connection(void);
static int hcom_host_recv_restart_concluded(void);
static FAR void *hcom_host_recv_pthread(FAR void *arg);
static int hcom_host_recv_low_power_notification(bool lpStart);
static void hcom_host_receive_takesem(sem_t *semaphore);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_recv_setup()
{
  int ret;
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;

  _shutting_down = false;
  _comms_read_fd = -1;
  _lowPowerSoon = false;
  _firstTimeToConnect = true;

  _recvDataBuffer = malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);  
  if(_recvDataBuffer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Should this really be configurable via menuconfig?
  // Currently it must be '/dev/ttyACM0' and is defined by
  // HCOM_COMMUNICATIONS_DEVICE_NAME
  deviceName = CONFIG_HCOM_COMMS_DEVICE_NAME;

  // Register with power management so we can properly shutdown before entering
  // a low-power mode.
  ret = hcom_via_nx_register_pwr_mgmt_callback(hcom_host_recv_low_power_notification);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Registering for pwr mgmt:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Create a semaphore used to prevent the received thread from running while
  // in low-power mode.
  sem_init(&_hcomRecvLPSem, 0, 0);
  sem_setprotocol(&_hcomRecvLPSem, SEM_PRIO_NONE);

  // Create the hcom receive thread
  param.sched_priority = HCOM_THREAD_PRIORITY_HCOM_RECEIVE;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_HCOM_RECEIVE);

  ret = pthread_create(&thread, &attr, hcom_host_recv_pthread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
              thisFile, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret, errno);
    return ret;
  }

  return OK;
}

//=======================================================================
// This will be called when entering and after leaving low-power mode
int hcom_host_recv_low_power_notification(bool lpStart)
{
  int ret = OK;

  if(lpStart)
  {
    // Low-Power mode is starting very soon
    _lowPowerSoon = true;

    // Note: because the calling thread is from Kernal Land and this close
    // is attempting to close a fd assigned to a task, the user must be
    // becareful. The only thing I know of is that the fd must still be closed
    // by a member of this "Task Group." If this is not done the fd will be
    // orphaned and unuseable again.
    close(_comms_read_fd);
  }
  else
  {
    _lowPowerSoon = false;

    // Low-Power mode has ended, allow the hcom receive thread to proceed.
    sem_post(&_hcomRecvLPSem);
  }

  return ret;
}

//=======================================================================
static void hcom_host_receive_takesem(sem_t *semaphore)
{
  int ret;

  do
  {
    /* Take the semaphore (perhaps waiting) */
    ret = sem_wait(semaphore);
  }
  while (ret == -EINTR);
}

//=======================================================================
// Return the current comms device name
const char *hcom_host_recv_get_device_name()
{
  return deviceName;
}

//=======================================================================
void hcom_host_recv_shutdown()
{
  _shutting_down = true;

  // Forces a receive error which, causes the thread to return.
  close(_comms_read_fd);
  _comms_read_fd = -1;

  free(_recvDataBuffer);
}

//=================================================================
// This thread receives all messages received from CLI
FAR void *hcom_host_recv_pthread(FAR void *arg)
{
  bool delayBeforeRetry = false;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_HCOM_RECEIVE);
#endif

  // Notify the startup manager that it can continue the startup process
  hcom_startup_mgr_release_sem();

  // Never exit this loop unless stopping
  while(! _shutting_down)
  {
    if(_lowPowerSoon)
    {
      // Entering low-power mode, therefore must wait until the low-power
      // event has ended before attempting to reconnect. We'll wait on this
      // semaphore.
      MEADOW_TRACE_INFORMATION("%s@%d-Taking LP semaphore\n", __FILE__, __LINE__);
      hcom_host_receive_takesem(&_hcomRecvLPSem);
    }

    // Attempt to establish a connection. Note: this is not a actual
    // connection to CLI, but an internal connection.
    if(_comms_read_fd < 0)
    {
      // This call only returns if we have a valid descriptor or about to enter
      // low-power mode.
      hcom_host_recv_open_connection();

      if(_lowPowerSoon)
        continue;         // Execute the top of loop, assuming no connection
    }

    if(_firstTimeToConnect)
    {
      // Might need to send a concluded message to CLI
      _firstTimeToConnect = false;
      hcom_host_recv_restart_concluded();
    }

    // Begin reading data. Return on error. The function will return whether
    // a delay is needed or not.
    delayBeforeRetry = hcom_host_recv_received_data();

    // Whatever the reason for receive failure, close the connection and
    // attempt to re-connect. We'll close here if informed of an imminent
    // low-power event.
    close(_comms_read_fd);
    _comms_read_fd = -1;

    // Should we wait a bit before attempting to reconnect?
    if(delayBeforeRetry)
      sleep(1);    // Delay so recurring errors don't waste MCU cycles
  }

  // Thread dies if we get here
  return NULL;    // Keeps compiler happy
}

//=======================================================================
// When Meadow is restarted by some of the CLI commands we need to send the
// "Concluded" message type
int hcom_host_recv_restart_concluded()
{
  // Check if a CLI command was responsible for this restart, If it was
  // then a `Concluded` message must be sent
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_RESTART_INITIATED_BY_HOST_CMD_BIT))
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0,
      "", thisFile, __LINE__);
  }

  return OK;
}

//=======================================================================
// Make a connection to the host
void hcom_host_recv_open_connection()
{
  int hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-usb open read %s\n", thisFile, __LINE__,
        HCOM_COMMUNICATIONS_DEVICE_NAME);
#endif

  while(!_shutting_down)
  {
    if(_lowPowerSoon)
      break;      // Exit here and go to top of receive loop

    _comms_read_fd = open(deviceName, O_RDONLY);
    if(_comms_read_fd >= 0)
    {
      MEADOW_TRACE_INFORMATION("%s@%d-open returned descriptor:%d\n", __FILE__, __LINE__, _comms_read_fd);
      break;    // Return valid descriptor
    }

    // It was found that on open attempts after low power would always return
    // errno is 128 (ENOTCONN). This will be delayed by
    // HCOM_CONNECTION_TIMEOUT_STARTUP (currently 250 ms) and then successfully
    // connected. This is probably due to timing in that the call to reopen the
    // connection probably got here before the low-power mode had begun. So when
    // full power was restored the open failed.

    if(_lowPowerSoon)
      break;

    // Encountered an error

    // Is this code really necessary? Could this be based on the ret/errno?
    // After x attempts switch to a slower attempt rate
    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }

    // Wait and try again at first every 250 millisec then every 5 seconds
    usleep(hostConnectionAttemptCount > 0 ?
      HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-%s ready for host comms\n",
            thisFile, __LINE__, deviceName);
#endif
}

//========================================================================
// This thread receives all host data and may call transmit to response as
// needed. This thread is the only thread receiving via USB serial data.
bool hcom_host_recv_received_data()
{
#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Waiting for '%s' message\n",
            thisFile, __LINE__, HCOM_COMMUNICATIONS_DEVICE_NAME);
#endif

#if(USE_ORIGINAL_READ_SCHEME > 0)
  while (!_shutting_down)
  {
    // This is a blocking read. read() will return:
    // (1) readReturn > 0 and readReturn is amount of data in buffer
    // (2) readReturn == 0 on end of file
    // (3) readReturn < 0 on a read error or interruption by a signal, value in errno
    ssize_t readResult = read(_comms_read_fd, _recvDataBuffer, g_current_hcom_maximum_packet_size);
    if (readResult > 0)
    {
      // We've received some data. Next step is to write it into a circular
      // buffer and return, allowing the processing thread to read and
      // process the message.
      int result = hcom_host_enq_deq_enqueue_rcvd_data(_recvDataBuffer, readResult);
      if (result < 0)
      {
        hcom_logging_syslog(LOG_WARNING, "%s@%d-received result:%d \n", thisFile, __LINE__, result);
      }
      continue;
    }
#else
  uint32_t dataBufOffset = 0;
  ssize_t readResult;

  // Stay in this loop until the HCOM is shutdown
  while (!_shutting_down)
  {
    // I found that the read call doesn't wait for a large number of bytes to be
    // received. I may be it just returns the number that have already been
    // received, as the first read is usually < 8 bytes. The typical number read
    // is 64 or 128, sometimes 256.
    
    // This is a blocking read. read() will return:
    // (1) readReturn > 0 and readReturn is amount of data in buffer
    // (2) readReturn == 0 on end of file
    // (3) readReturn < 0 on a read error or interruption by a signal, value in errno
    readResult = read(_comms_read_fd, &_recvDataBuffer[dataBufOffset],
      (size_t)HCOM_HOST_RECEIVE_MAX_READ_SIZE);
    if (readResult > 0)
    {
      // Did we get a delimiter in the last read?
      char *delim = memchr(_recvDataBuffer + dataBufOffset,
        HCOM_PROTOCOL_COBS_DELIMITER, readResult);

      dataBufOffset += readResult;    // New end of Buffer offset

      // Anywhere near the upper limit of the buffer?
      if((dataBufOffset + HCOM_HOST_RECEIVE_MAX_READ_SIZE) >= HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-dataBufOffset value is near to array overflow\n",
            thisFile, __LINE__);
        return false;
      }

      if(delim == NULL)
      {
        continue;   // Read more bytes
      }

      int result = hcom_host_enq_deq_enqueue_rcvd_data(_recvDataBuffer, dataBufOffset);
      if (result < 0)
      {
        hcom_logging_syslog(LOG_WARNING, "%s@%d-received result:%d \n",
            thisFile, __LINE__, result);
      }

      dataBufOffset = 0;    // Reset buffer offset
      continue;
    }

#endif
    // readResult == 0 (end-of-file). Host PC probably dropped connection
    if (readResult == 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-HCOM received EOF\n", thisFile, __LINE__);
      continue;
    }

    // Some type of error must of happened
    
    // Check if entering low-power mode, if we are need to exit and wait
    if(_lowPowerSoon && errno == ENOTCONN)
    {
      MEADOW_TRACE_INFORMATION("Entering low-power because read() returned:%d, errno:%d\n", readResult, errno);
      // No need to wait since entering low-power mode
      return (false);
    }

    // If we get this far, we have an interrupt or an error (errno tells which)
    // EINTR (Error Interrupt) is not an error... it simply means that this read was
    // interrupted by a signal before it obtained data.
    if (errno == EINTR) 
    {
      MEADOW_TRACE_INFORMATION("read() EINTR we ignore this (returned:%d, errno:%d)\n", readResult, errno);
      set_errno(0);   // Clear EINTR
      continue;
    }
    
    // Treat all errors the same, drop the connection and try again.
    // Some errors are better handled with a delay before retrying.
    if (errno == ENOTCONN || errno == ENOTSOCK || errno == ENETDOWN)
    {
      // Host connection dropped - calling read will only repeat the error. So,
      // delay for a bit.
      return (true);    // Delay retry
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-HCOM recv error:%d, errno:%d\n", thisFile, __LINE__, readResult, errno);
      return(false);    // No retry delay
    }

    // Should only get this far if shutdown true
    return false; // Establish a new connection and repeat
  }   // while(!_shutting_down)

  return false;    // No retry delay on shutdown
}
