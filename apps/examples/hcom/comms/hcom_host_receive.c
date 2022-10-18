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

#include <fcntl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

static int _comms_read_fd;
static uint8_t *_recvDataBuffer;
static bool _firstTimeToConnect;
static const char *deviceName;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool hcom_host_recv_received_data(void);
static int hcom_host_recv_open_connection(void);
static int hcom_host_recv_restart_concluded(void);
static int hcom_host_recv_create_thread(void);
static FAR void *hcom_host_recv_pthread(FAR void *arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_recv_setup()
{
  _shutting_down = false;
  _comms_read_fd = -1;

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

  /* If we detect that we are booting into QEMU, then use serial comms
     instead of the configured device name (USB ACM) */

  // if (hcom_utils_boot_time_qemu_check())
  //   deviceName = "/dev/ttyS1";   // UART 4

  return hcom_host_recv_create_thread();
}

//=======================================================================
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

  // Release startup thread to call shutdown
  hcom_startup_mgr_release_sem();
}

//=============================================================
// Create thread to run hcom receive. This thread is central to
// all CLI command processing and notification.
int hcom_host_recv_create_thread()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

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

//=================================================================
// This thread receives all stdout messages received from mono
FAR void *hcom_host_recv_pthread(FAR void *arg)
{
  int ret;
  bool wait_before_retry = false;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_HCOM_RECEIVE);
#endif

  // Allow startup thread to continue working
  hcom_startup_mgr_release_sem();

  // Never exit this loop
  while(! _shutting_down)
  {
    // Is this the best solution?
    if(wait_before_retry)
      sleep(1);    // Delay, thus limiting wasted CPU cycles and error messages
      
    // Attempt to establish the connection
    ret = hcom_host_recv_open_connection();
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-connection not made, error:%d\n", thisFile, __LINE__, ret);
      sleep(1);
      continue;   // Can't leave this loop or all hcom will stop
    }

    if(_firstTimeToConnect)
    {
      _firstTimeToConnect = false;
      hcom_host_recv_restart_concluded();
    }

    // Begin reading data. Some errors need a delay.The receiver determines
    // if a delay needed.
    wait_before_retry = hcom_host_recv_received_data();
    
    // Close the connection before looping for a re-connect
    close(_comms_read_fd);
    _comms_read_fd = -1;
  }

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
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
  }

  return OK;
}

//=======================================================================
// Make a connection to the host
int hcom_host_recv_open_connection()
{
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  if(_comms_read_fd >= 0)
    return OK;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-usb open read %s\n", thisFile, __LINE__,
        HCOM_COMMUNICATIONS_DEVICE_NAME);
#endif

  while(!_shutting_down)
  {
    // Open reader
    _comms_read_fd = open(deviceName, O_RDONLY);
    if(_comms_read_fd >= 0)
    {
      break;
    }

    // Is this really necessary? Could this be based on the ret/errno?
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

  return OK;
}

//========================================================================
// This thread receives all host data and may call transmit to responsed as
// needed. This thread is the only thread receiving via USB serial data.
bool hcom_host_recv_received_data()
{
#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Waiting for '%s' message\n",
            thisFile, __LINE__, HCOM_COMMUNICATIONS_DEVICE_NAME);
#endif

  // Stay in this loop forever
  while (!_shutting_down)
  {
    bool delayBeforeRetry;

    // This is a blocking read. read() will return:
    // (1) readReturn > 0 and readReturn is amount of data in buffer
    // (2) readReturn == 0 on end of file
    // (3) readReturn < 0 on a read error or interruption by a signal, value in errno
    ssize_t readResult = read(_comms_read_fd, _recvDataBuffer, HCOM_PROTOCOL_PACKET_MAX_SIZE);

    // Return > 0 valid data received and this is the length
    if (readResult > 0)
    {
      // We've received some data. Next step is to write it into a circular
      // buffer and return, allowing the processing thread to read and
      // process the message.
      int result = hcom_host_parse_save_raw_data(_recvDataBuffer, readResult);
      if (result < 0)
      {
        hcom_logging_syslog(LOG_WARNING, "%s@%d-received result:%d \n", thisFile, __LINE__, result);
      }
      continue;
    }

    // readResult == 0 (end-of-file). Host PC probably dropped connection
    if (readResult == 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-HCOM received EOF\n", thisFile, __LINE__);
      continue;
    }

    // If we get this far, we have an interrupt or an error (the value in
    // errno tells us which).

    // EINTR (Error Interrupt) is not an error... it simply means that this read was
    // interrupted by a signal before it obtained data. The signal may be SIGALRM
    // indicating an timeout condition. We will know this case because the signal handler
    if (errno == EINTR) 
    {
      continue;
    }

    // Treat all real errors result in dropping the connection and try again.
    // Some errors are better handled with a delay before retrying.
    if (errno == -ENOTCONN || errno == -ENOTSOCK || errno == -ENETDOWN)
    {
      // Host connection dropped - calling read will only repeat the error. So,
      // delay for a bit.
      delayBeforeRetry = true;    // Delay retry
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-HCOM recv error:%d, errno:%d\n", thisFile, __LINE__, readResult, errno);
      delayBeforeRetry = false;    // No retry delay
    }

    return delayBeforeRetry; // Establish a new connection and repeat
  }   // while(!_shutting_down)

  return false;    // No retry delay on shutdown
}
