/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_process.c
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

// This file primarily allows receive to save undelimited data. Then the
// process threads pulls packetized data and forwarding it to be routed.
// The download watchdog code is also here.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/meadow_cirbuf.h>

#if defined (CONFIG_HCOM_ESP32_COMMS)
#include "../esp32/hcom_esp32_comms.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static host_com_cir_buffer_t *_hcom_cbuf;
static size_t _max_packet_size = HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

static sem_t _processWaitRecvSem;

static timer_t _processWdogTimerId;
static bool _hcom_host_process_wdog_timedout;

// (--)
static int diagOnlyShowAFewTimes;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_process_route_packet(const uint8_t *packet, const size_t packetSize);
static FAR void *hcom_host_proc_pthread(FAR void *arg);
static int hcom_host_proc_create_thread(void);
static void hcom_file_process_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static int hcom_host_proc_handle_wdog_timeout(size_t *timeoutDecodedSize);
static int hcom_host_proc_read_all_cir_buf_msg(void);

//=============================================================
// Create thread to preocess hcom received messages. This thread is processes
// all CLI commands and notifications.
int hcom_host_proc_create_thread()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_HCOM_PROCESS);

    ret = pthread_create(&thread, &attr, hcom_host_proc_pthread, NULL);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
                thisFile, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret, errno);
      return ret;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_host_process_setup()
{
  _shutting_down = false;
  
  _packet_dest_buf = (uint8_t *)malloc(_max_packet_size);
  _decode_dest_buf = (uint8_t *)malloc(_max_packet_size);
  _hcom_cbuf = (host_com_cir_buffer_t *)malloc(sizeof(host_com_cir_buffer_t));

  if (_hcom_cbuf == NULL || _decode_dest_buf == NULL || _packet_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-One of 3 cir buf allocations failed\n",
              thisFile, __LINE__);
    if(_packet_dest_buf != NULL) free(_packet_dest_buf);
    if(_decode_dest_buf != NULL) free(_decode_dest_buf);
    if(_hcom_cbuf != NULL) free(_hcom_cbuf);
    return -ENOMEM;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE,
          HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE);
  if (result == HCOM_CIR_BUF_ALLOC_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Buffer allocation failed\n", thisFile, __LINE__);
    return -1;
  }

  // Initialize value to 0 for a 'signaling' semaphore to block
  // the calling thread (this one) until it is okay for it to proceed.
  sem_init(&_processWaitRecvSem, 0, 0);
  // Special non-standard nuttx function required for signaling semaphores
  sem_setprotocol(&_processWaitRecvSem, SEM_PRIO_NONE);

  // Create the processing thread
  return hcom_host_proc_create_thread();
}

//====================================================================
void hcom_host_process_shutdown()
{
  _shutting_down = true;

  free(_packet_dest_buf);
  free(_decode_dest_buf);
  hcom_cirbuf_release_memory(_hcom_cbuf);
  free(_hcom_cbuf);
  sem_destroy(&_processWaitRecvSem);
}

//=======================================================================
// The receive thread calls here to add the received data to the circular
// buffer. It can be added byte-by-byte or several messages at once. The
// data will be pulled from the circular buffer in packets to be processed.
int hcom_host_process_save_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int ret;
  int result;
  int semCount;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add received data to the buffer until no more will
  // fit. The received data is assumed to not be received in packet sized
  // chuncks.
  for (;;)
  {
    // Only 3 possible results of this call, HCOM_CIR_BUF_ADD_SUCCESS,
    // HCOM_CIR_BUF_ADD_WONT_FIT or HCOM_CIR_BUF_ADD_BAD_ARG
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    switch(result)
    {
      case HCOM_CIR_BUF_ADD_SUCCESS:
      // Notify proc thread that there's work to do by adding 1 to semaphore
      // count. We don't want to over-post (allowing count to get > 1).
      ret = sem_getvalue(&_processWaitRecvSem, &semCount);
      if (ret == OK && semCount <= 0)
      {
        sem_post(&_processWaitRecvSem);
      }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
        hcom_logging_syslog(LOG_DEBUG, "%s@%d-%d bytes added to cir buf\n", thisFile, __LINE__, recvByteCnt);
#endif
        return OK;

      case HCOM_CIR_BUF_ADD_WONT_FIT:
        // If the buffer is full we must wait for the proc thread to empty it.
        // This is a common occurrance when downloading large files. This
        // sleep value is arbitrary, too short and waste CPU, too long and
        // download is stalled.
        usleep(30 * 1000);
        break;    // Keep trying to add to buffer

      case HCOM_CIR_BUF_ADD_BAD_ARG:
        // Message being added has zero length
        hcom_logging_syslog(LOG_ERR, "%s@%d-Message with length of 0 ignored\n", thisFile, __LINE__);
        return OK;

      default:
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown cir buf err:%d\n", thisFile, __LINE__, result);
        return OK; // Report and keep going
    }
  }
  return OK;
}

//=================================================================
// This thread processes all the messages the receive thread after they
// are put into the circular buffer
FAR void *hcom_host_proc_pthread(FAR void *arg)
{
  int ret;

  while (!_shutting_down)
  {
    // Wait for something to do
    do
    {
      // Wait for more data to be written or more work
      ret = sem_wait(&_processWaitRecvSem);
      if(ret < 0)
      {
        if(errno == EINTR)
        {
          if (_hcom_host_process_wdog_timedout)
          {
            size_t timeoutDecodedSize;
            
            _hcom_host_process_wdog_timedout = false;
            
            syslog(1, "==> PROC loop received EINTR. Why? Timeout flag set\n");
            usleep(20 * 1000);

            hcom_host_proc_handle_wdog_timeout(&timeoutDecodedSize);
            
            // If while emptying the cir buf a valid message was read we need
            // to make sure it gets processed
            if(timeoutDecodedSize > 0)
            {
              int result;
              result = hcom_host_process_route_packet(_decode_dest_buf, timeoutDecodedSize);
              if (result < 0)
              {
                // If ever supported, NAK host to resend bad packet
                hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
              }
              // Continue to wait for next message
              continue;
            }
          }
        }
      }
    }
    while (ret == -EINTR);

    // We have the semaphore so, pull and process all the complete packets
    // from the circular buffer.
    ret = hcom_host_proc_read_all_cir_buf_msg();
    if(ret < 0)
    {
      // Terminate thread, error already logged.
      return NULL;
    }
  }
  return NULL;
}

//====================================================================
// This function will read all valid message from circular buffer and
// routes them to the proper destination.
int hcom_host_proc_read_all_cir_buf_msg()
{
  int result;
  size_t packetLength;

  // Pull next message packet from cir buf and route the packet. Loop until
  // buffer is empty of complete message packets.
  do
  {
    // This can only return one of these 3 values, HCOM_CIR_BUF_GET_FOUND_MSG,
    // HCOM_CIR_BUF_GET_NONE_FOUND or HCOM_CIR_BUF_GET_DEST_NO_ROOM
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);
    if(result == HCOM_CIR_BUF_GET_FOUND_MSG)
    {
      // We pulled a good packet. Drop trailing delimiter using --packetLength,
      // then decode the packet and route it.
      size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);

      if(decodedPacketSize == 0)
        continue;

      // Process the received & decoded packet
      result = hcom_host_process_route_packet(_decode_dest_buf, decodedPacketSize);
      if (result < 0)
      {
        // If ever supported, NAK host to resend bad packet
        hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
      }
    }
    else if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
    {
      // Nothing in the buffer, this his not an error, there's just a partially
      // received message in the buffer. So, we'll leave this loop and wait to
      // be notified of next read.
      break;
    }
    else
    {
      // The only remaining return value is HCOM_CIR_BUF_GET_DEST_NO_ROOM. So,
      // if we've been careful to provide a big enough buffer this will never
      // happen. But, just in case output a syslog message.
      // If the buffer is too small packetLength will contain the desired size.
      hcom_logging_syslog(LOG_ERR, "%s@%d-Dest buffer too small. Need:%d bytes\n",
                __FILE__, __LINE__, packetLength);
      sleep(1);         // Make sure error log above is output
      
      return -ENOMEM;   // This will terminate this thread
    }

  } while(!_shutting_down); // loops till buffer empty

  // Return and wait to be notified again
  return OK;
}

//====================================================================
// Parse and process received decoded packet as sent by host
// Grab the sequence number, using it to determine if data or request.
int hcom_host_process_route_packet(const uint8_t *decodedPacket, const size_t decodedSize)
{
  // All messages contains the sequence number
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  // The sequence number determines if this packet is a command or data
  if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // A non-data i.e. command  message
    hcom_host_route_request_by_cmd_type((HcomProtoHdrMsg_t *) decodedPacket, decodedSize);
  }
  else
  {
    // Must be a Data Packet because sequence number != 0. Is it for external
    // flash or ESP32?
    if(hcom_file_dnld_stm32f7_is_active())
    {
      hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, decodedSize);
    }
    else if(hcom_file_dnld_esp32_is_active())
    {
      hcom_file_dnld_esp32_recvd_file_data(hcomDataMsg, decodedSize);
    }
    else
    {
      // CLI must be confused, sending sequence number when no dowload is active
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data received but no active download\n",
                thisFile, __LINE__);
    }
  }

  return OK;
}

//=================================================================
// The remaining code deals with a download failure
//=================================================================
// Encountered a watchdog timeout and this function gets called from our
// pthread main loop while waiting for the semaphore.
int hcom_host_proc_handle_wdog_timeout(size_t *timeoutDecodedSize)
{
  int ret;
  int result;
  char *fullFileName = "NotAFileName";

  *timeoutDecodedSize = 0;

  syslog(1, "---> WDog timeout Enter\n"); usleep(20 * 1000);

  // Set the download state to inactive so any additional related downloads are ignored
  hcom_file_dnld_stm32f7_set_to_inactive();

  // Stop the download wdog timer
  ret = hcom_file_process_dnld_timer_delete();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Stop/delete timer failed, errno %d\n",
             thisFile, __LINE__, get_errno());
  }

  // We assume that the CLI will continue to send data, some of which will
  // be ignored by the receiver because the state has been set to inactive. It will also
  // ignore the End message for the same reason.
  syslog(1, "---> WDog timeout 2\n"); usleep(20 * 1000);

  // Close the bad file
  ret = hcom_file_write_close_active_file(&fullFileName);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-close failed for '%s', errno %d\n",
             thisFile, __LINE__, fullFileName, get_errno());
  }

  syslog(1, "---> WDog timeout 3a, file '%s'\n", fullFileName); usleep(20 * 1000);

  char *simpleFileName;
  simpleFileName = strrchr(fullFileName, '/');
  simpleFileName++;
  syslog(1, "---> WDog timeout 3b, file '%s'\n", simpleFileName); usleep(20 * 1000);

  // Delete the file from the file system
  ret = unlink(fullFileName);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-delete failed for '%s', errno %d\n",
             thisFile, __LINE__, fullFileName, get_errno());
  }

  // Send a message to CLI to stop sending data
// #if HCOM_PROTOCOL_INCLUDE_POST_RC1_REQUEST_TYPES > 0
//   char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
//   // The next call will free the simple file name so build the CLI message before
//   // setting the state to inactive.
//   snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
//         "File '%s' download failed, please resend", fullFileName);

//   // Send message to CLI
//   hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_DNLD_FAIL_RESEND, 0, hostMsg,
//         thisFile, __LINE__);
// #endif

  syslog(1, "---> WDog timeout 4\n"); usleep(20 * 1000);
  // Need to free some download memory
  hcom_file_dnld_stm32f7_free_file_name_buf();

  // Remove any download data still in the cir buf. Care must be taken because
  // we don't want to throw away good data that could have already been sent.
  // If the CLI could be trusted to honor the Completed message send after it
  // sends the File Download End message this wouldn't be necessary.
  syslog(1, "---> WDog timeout 5\n"); usleep(20 * 1000);
  while(true)
  {
    size_t packetLength;
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);
    if(result == HCOM_CIR_BUF_GET_FOUND_MSG)
    {
      syslog(1, "---> WDog timeout 5\n"); usleep(20 * 1000);

      // We pulled a good packet so decode it.
      size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);
      if(decodedPacketSize == 0)
      {
        syslog(1, "---> WDog timeout 6\n"); usleep(20 * 1000);
        continue;
      }

      // Only care about header info. All packets have this header
      HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) _decode_dest_buf;

      // The sequence number determines if this packet is a command or data
      if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
      {
        syslog(1, "---> WDog timeout 6 (Non-data packet)\n"); usleep(20 * 1000);
        // Only non-data packets have the hcom protocol header
        HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) _decode_dest_buf;
        uint16_t requestType = hdrMsg->stdHeader.rqstType;
        if(requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER)
        {
          // This is the CLI's final message of the file download. So we need
          //  to stop emptying the buffer.
          syslog(1, "---> WDog timeout 7 (Non-data packet EndOfDownload - break)\n"); usleep(20 * 1000);
          break;
        }
        else
        {
          // There must not have been a File Download End message. Set a flag
          // to let the caller know that a valid message has been read and
          // must be processed before processing any other messages.
          *timeoutDecodedSize = decodedPacketSize;
          syslog(1, "---> WDog timeout 8 (Non-data packet, Valid Packet ready - break)\n"); usleep(20 * 1000);
          break;
        }
      }
      else
      {
        syslog(1, "---> WDog timeout 9 (data packet, another data packet - continue)\n"); usleep(20 * 1000);
        continue; // Data packet, keep pulling data
      }
    }
    else
    {
      syslog(1, "---> WDog timeout 10 (data packet, buffer empty - break)\n"); usleep(20 * 1000);
      break;  // Looks like the buffer is empty
    }
  }

  syslog(1, "---> WDog timeout 11 (exiting)\n"); usleep(20 * 1000);
  return OK;
}

//=============================================================================
// Watchdog timeout occurred while doing a download. This function will delete
// the file, return the state to inactive and send a request to the CLI to
// resent the file.
int hcom_file_write_stm32f7_cleanup_on_dnld_error()
{
  int ret = OK;

  return ret;
}

//=================================================================
// Callback on watchdog timer expiration
void hcom_file_process_timeout_expired(int signo, FAR siginfo_t *info,
          FAR void *context)
{
  // (--) This is being called at the rate set by the receive thread. And it 
  // is not being called. Theory - there is only 1 callback per task group.
  //
  // This is called 
  syslog(1, "==> Proc callback - Timeout expired, setting flag for thread, timerId:%d\n", signo);
  _hcom_host_process_wdog_timedout = true;
  diagOnlyShowAFewTimes = 4;
}

//=================================================================
// Start, restart, or stop the timer
// This gets called a lot when downloading
int hcom_file_process_dnld_timer_set_delay(time_t sec)
{
  struct itimerspec todelay;
  int ret;

  // Start, restart, or stop the timer
  todelay.it_interval.tv_sec = 0; // Nonrepeating
  todelay.it_interval.tv_nsec = 0;
  todelay.it_value.tv_sec = sec;
  todelay.it_value.tv_nsec = 0;
  
  if(diagOnlyShowAFewTimes)
  {
    syslog(1, "==> PROC set delay, using timerId:%p\n", _processWdogTimerId);
    diagOnlyShowAFewTimes--;
  }

  ret = timer_settime(_processWdogTimerId, 0, &todelay, NULL);
  if (ret < 0)
  {
    int errorcode = errno;
    hcom_logging_syslog(LOG_ERR, "%s@%d-setting timer, errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }
  return OK;
}

//=================================================================
// Create the POSIX timer for detecting download failures
int hcom_file_process_dnld_timer_initialize()
{
  struct sigevent toevent;
  struct sigaction act;
  int ret;

  _processWdogTimerId = NULL;
  diagOnlyShowAFewTimes = 3;

  // Create a POSIX timer to handle timeouts
  toevent.sigev_notify = SIGEV_SIGNAL;
  toevent.sigev_signo = SIGALRM;
  toevent.sigev_value.sival_ptr = NULL;  // Carry value to 'context' in callback

  ret = timer_create(CLOCK_REALTIME, &toevent, &_processWdogTimerId);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-create timer errno:%d\n", thisFile, __LINE__, errno);
    return -errno;
  }

  syslog(1, "==> PROC initialize WDog, received timerId %p\n", _processWdogTimerId);

  // Attach a signal handler to catch the timeout
  act.sa_sigaction = hcom_file_process_timeout_expired;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  ret = sigaction(SIGALRM, &act, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-attach signal errno:%d\n", thisFile, __LINE__, errno);
    return -errno;
  }
  return OK;
}

//=================================================================
// Delete the POSIX timer for detecting download failures
int hcom_file_process_dnld_timer_delete()
{
  int ret;

  syslog(1, "==> PROC delete, set wdog timer to 0 and delete it, using timerId %p\n", _processWdogTimerId);
  
  ret = hcom_file_process_dnld_timer_set_delay(0);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set delay = 0, errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  ret = timer_delete(_processWdogTimerId);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  return ret;
}