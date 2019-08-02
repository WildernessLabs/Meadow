/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_receiver.c
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
static struct host_com_cir_buffer_s *_hcom_cbuf;
static size_t _max_packet_size = HCOM_SAFE_PACKET_BUF_SIZE;
static int _dbgNumbDataReads;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int hcom_recv_process_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt);
static ssize_t hcom_recv_wait_until_change(uint8_t *recvBuffer, time_t readTimeout);
static int hcom_recv_pull_all_packets_from_buffer(void);

static void hcom_recv_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static int hcom_receive_timerstart(timer_t timerid, time_t sec);
static int hcom_recv_timerInit(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_receiver_setup(int fd)
{
  _shutting_down = false;
  _hcom_connection_fd = fd;

  _hcom_cbuf = (struct host_com_cir_buffer_s *)malloc(sizeof(struct host_com_cir_buffer_s));
  if (_hcom_cbuf == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: circular buffer allocation failed\n", __func__);
    return -1;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE);
  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_cirbuf_init failed\n", __func__);
    return -1;
  }

  hcom_recv_timerInit();
  return OK;
}

//--------------------------------------------------------------------
// Called before hcom mgr closes hcom_fd which, forces a receive error which,
// causes the thread to return.
void hcom_receiver_shutdown()
{
  _shutting_down = true;
}

//========================================================================
// Dedicated thread enters here. It receives all host data and calls transmit
// to responsed as needed.
void hcom_receiver_receive_data_thread()
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
      if (result == OK)
      {
        _dbgNumbDataReads++;
      }
      else
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
          // TODO - ACTION TBD
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
          // Host dropped connection - quickly calling read will only repeat the error
          f7syslog(LOG_NOTICE, "Host dropped USB connection. Will retry shortly.\n");
          sleep(2); // Wait and try again
        }
        f7syslog(LOG_ERR, "%s() ERROR: HCOM receive, unexpected error: %d\n", __func__, readResult);
        // ACTION TBD
      }
    }
    else
    {
      // readResult must be 0 (end-of-file). Not sure this can be detected for a serial connection
      f7syslog(LOG_WARNING, "%s() WARNING: HCOM receive, Received End-Of-File indication\n", __func__);
      // TODO - ACTION TBD
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

  // Read a block of data, read() will return:
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
      // If handshake is added send nak to host
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

//=======================================================================
// Add the received data to the circular buffer
int hcom_recv_process_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add messages to the buffer until no more will fit
  for (;;)
  {
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    if (result == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this message
      f7syslog(LOG_WARNING, "%s() WARNING: No room in circular buffer, will pull and try again\n", __func__);
      result = hcom_recv_pull_all_packets_from_buffer();
      if (result == HCOM_CIR_BUF_GET_FOUND_MSG)
        continue;

      if (result == HCOM_CIR_BUF_GET_NONE_FOUND || result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        f7syslog(LOG_ERR, "%s() ERROR: Unexpected error from attempt to pull all packets from circular buffer %d\n",
                 __func__, result);
        return OK;
      }
    }
    else if (result == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // Bad argument
      f7syslog(LOG_ERR, "%s() ERROR: Bad argument passed to circular buffer\n", __func__);
      return OK; // Throw message away and keep going
    }
    else //if(result == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      // In all valid cases pull all full packets and process them
      f7syslog(LOG_DEBUG, "%d bytes added to circular buffer\n", recvByteCnt);
      break; // break to pull more messages
    }
  }

  // This could be on a separate thread if greater performance is needed
  result = hcom_recv_pull_all_packets_from_buffer();
  return result;
}

//====================================================================
// Pull and process all the complete packets from the circular buffer
int hcom_recv_pull_all_packets_from_buffer()
{
  int result;
  static uint8_t *packet_dest_buf = NULL;
  static uint8_t *decode_dest_buf = NULL;

  if (packet_dest_buf == NULL)
  {
    packet_dest_buf = (uint8_t *)malloc(_max_packet_size);
    decode_dest_buf = (uint8_t *)malloc(_max_packet_size);
  }

  for (;;)
  {
    size_t packetLength = 0;
    // If buffer too small packetLength will contain the desired size
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, packet_dest_buf, _max_packet_size, &packetLength);
    if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
      return OK; // Return to receive more data

    if (result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
    {
      // WARNING: THIS ISN'T SAFE. THE SIZE OF THE CIRCULAR BUFFER IS FIXED.
      // TOO MUCH EXPANSION WILL BE SERIOUS.
      // Packet size bigger than packet parsing buffer so allocate space
      f7syslog(LOG_WARNING, "%s() WARNING: Packet parsing buffer too small, will increase from %d to %d bytes\n",
               __func__, _max_packet_size, packetLength);

      // The buffer needs to be expanded
      _max_packet_size = packetLength;
      free(packet_dest_buf);
      free(decode_dest_buf);
      packet_dest_buf = (uint8_t *)malloc(_max_packet_size);
      decode_dest_buf = (uint8_t *)malloc(_max_packet_size);
      continue; // Try again
    }

    // Fall through when result == HCOM_CIR_BUF_GET_FOUND_MSG

    // Ignore trailing delimiter (0x00) and decode the packet
    size_t decodedPacketSize = hcom_com_support_cobs_decoder(packet_dest_buf, --packetLength, decode_dest_buf);

    // Process the received data
    result = hcom_parse_request_and_process(decode_dest_buf, decodedPacketSize);
    if (result == OK)
    {
      continue; // pull next packet
    }
    else if (result < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: processing data failed: %d\n", __func__, result);
      return result;
      // When supported NEED TO SEND NAK TO HOST TO RESEND BAD DATA
    }
    else
    {
      f7syslog(LOG_ERR, "%s() ERROR: unknown value returned from processing data: %d\n", __func__, result);
      return result;
    }
  }
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
