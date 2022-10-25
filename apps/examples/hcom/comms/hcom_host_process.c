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

// This file does processing of all request. For file downloads it allocates
// and populates a struct that contains file specific information.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#warning "Peter working here (--)"
#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/meadow_cirbuf.h>
#include <meadow/hcom_dnld_shared.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

// (--) NEEDS TO BE REMOVED WHILE HACKING
static hcom_dnld_shared_t *_dnldShared;

// static timer_t _processWdogTimerId;
// static bool _hcom_host_process_wdog_timedout;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_process_route_packet(const uint8_t *packet, const size_t packetSize);
static int hcom_host_process_run(void);

// static FAR void *hcom_host_process_run(FAR void *arg);
// static int hcom_host_proc_create_thread(void);
static int hcom_host_process_init_dnld_share(uint32_t partitionId);
static bool hcom_host_process_is_stm32f7_dnld_active(void);
// static void hcom_host_process_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
// static int hcom_host_process_handle_wdog_timeout(size_t *haveValidMsgSize);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_host_process_setup()
{
  _shutting_down = false;

  _packet_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_packet_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Packet buffer allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }
  
  _decode_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Decoded allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  _dnldShared = NULL;
  
  struct sched_param sparam;
  sparam.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
  sched_setparam(0, &sparam);
  // (--) HOW TO DO THIS?
  // HCOM_THREAD_STACKSIZE_HCOM_PROCESS

  // hcom_host_process_run(NULL);
  int ret = hcom_host_process_run();
 
  // This return is only reached on shutddown
  return ret;
}

//====================================================================
void hcom_host_process_shutdown()
{
  _shutting_down = true;
  free(_decode_dest_buf);
  free(_packet_dest_buf);
}

//==========================================================================
// Are we involved in some download activity?
bool hcom_host_process_is_stm32f7_dnld_active()
{
  if(_dnldShared == NULL)
  {
    return false;
  }

  return (_dnldShared->dnldCurrentState != HcomStm32F7DnldStateNone);
}

//=================================================================
// This thread processes all the messages the receive thread has written to
// the circular buffer.
// (--) USING THE MAIN TASK THREAD USED TO INIT EVERYTHING
// FAR void *hcom_host_process_run(FAR void *arg)
int hcom_host_process_run()
{
  int ret;
  size_t packetLength;
  
  syslog(1, "$-proc-@%d hcom_host_process_run running\n", __LINE__);
  while (!_shutting_down)
  {
    // Get the next received packet
    // (--) syslog(1, "$-proc-@%d Calling DeQueue\n", __LINE__);
    ret = hcom_host_enq_deq_dequeue_packet(_packet_dest_buf, &packetLength);
    // (--) syslog(1, "$-proc-@%d Returned from DeQueue\n", __LINE__);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Failed to dequeue message\n", thisFile, __LINE__);
      continue;
    }

    // We have a good packet. Drop trailing delimiter using --packetLength,
    // then decode the packet and route it.
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);
    if(decodedPacketSize == 0)
      continue;

    // (--) TEMPORARY USING OLD CODE - Process the received & decoded packet
    ret = hcom_host_process_route_packet(_decode_dest_buf, decodedPacketSize);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n",
                thisFile, __LINE__, ret);
    }
  }

  // Thread is exiting
  return OK;
}

    // do
    // {
      // Wait for more data to be written or more work
      // if(ret < 0)
      // {
        // Been notified by error
// (--) IGNORE WHILE HACKING WATCHDOG WHILE BRINGING UP
        // if(errno == EINTR)
        // {
        //   if (_hcom_host_process_wdog_timedout)
        //   {
        //     size_t haveValidMsgSize;

        //     // _hcom_host_process_wdog_timedout = false;
        //     // hcom_host_process_handle_wdog_timeout(&haveValidMsgSize);
            
        //     // If while draining the circular buffers messages a valid HCOM
        //     // message is read, we need to make sure it gets processed.
        //     if(haveValidMsgSize > 0)
        //     {
        //       int result;
        //       result = hcom_host_process_route_packet(_decode_dest_buf, haveValidMsgSize);
        //       if (result < 0)
        //       {
        //         // If ever supported, request host to resend bad packet here
        //         hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
        //       }

        //       // Resume waiting for next message
        //       continue;
        //     }
        //   }
        // }
      // }
    // }
    // // If interrupt continue
    // while (errno == EINTR);

    // We have the semaphore so, pull and process all the complete packets
    // from the circular buffer.
    
//     ret = hcom_host_proc_read_all_cir_buf_msg();
//     if(ret < 0)
//     {
//       // Terminate thread, error already logged.
//       return NULL;
//     }

//     // (--) NEXT STEP IS TO PROCESS THE RECEIVED DATA

//   }
//   return NULL;
// }

//====================================================================
// Parse and process received decoded packets as sent by host (CLI).
// Grab the sequence number, using it to determine if data or command.
int hcom_host_process_route_packet(const uint8_t *decodedPacket, const size_t decodedSize)
{
  uint16_t requestType;
  uint32_t userData;

  // All messages contain a sequence number field. The sequence number
  // determines if this packet is a command or data.
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // Only commands (non-data packets) have the full HCOM header
    const HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) decodedPacket;

#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
    hcom_diag_decode_recvd_message_type(hdrMsg, decodedSize);
    usleep(100 * 1000);
#endif

    if(hdrMsg->stdHeader.version != (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER)
    {
      char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
            "Meadow is expecting a newer CLI Protocol version. Please update Meadow.CLI on your connecting computer." \
            " (version received::%04x required:%04x).",
            hdrMsg->stdHeader.version, (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER);

      hcom_logging_syslog(LOG_ERR, "%s\n", hostMsg);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
              thisFile, __LINE__);
      return -ENOTSUP;
    }

    // Pull out important values
    requestType = hdrMsg->stdHeader.rqstType;
    userData = hdrMsg->stdHeader.userData;

    // For downloading or deleting files need more information and require
    // HCOM to keep this activity state alive while downloading. These
    // commands are those that need the file's name and may need to establish
    // a temporary state while being processed.
    if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
      requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
      requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
    {
      // Initialize the struct containing all download/delete state information
      hcom_host_process_init_dnld_share(userData);
    
      // We'll do a little work here so it doesn't need to be done in multiple
      // places.
      size_t fileNameLength = decodedSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
      _dnldShared->dnldOrigFileName = malloc(fileNameLength + 1);
      if(_dnldShared->dnldOrigFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }

      // File name
      HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
      memcpy(_dnldShared->dnldOrigFileName, fileMsg->fileInfo.fileName, fileNameLength);
      _dnldShared->dnldOrigFileName[fileNameLength] = '\0';

      // Build the full path plus file name string (e.g. /mnt0/FileName.ext)
      size_t fullFileNameLen = strlen(_dnldShared->dnldOrigFileName) + \
                strlen(HCOM_FILE_MOUNT_POINT_TARGET) + 3; // Room for '/', partition Id, NULL

      _dnldShared->dnldFullFileName = malloc(fullFileNameLen + 1);
      if(_dnldShared->dnldFullFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }

#ifdef CONFIG_MTD_PARTITION
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s%d/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldFilePartId,
                                _dnldShared->dnldOrigFileName);
#else
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldOrigFileName);
#endif
    }

    // Route the command message
    hcom_host_route_request_by_cmd_type(hdrMsg, decodedSize, userData,
              requestType, _dnldShared);
  }
  else
  {
    // Must be a Data Packet because sequence number != 0. Is it for external
    // flash or ESP32?
    if(hcom_host_process_is_stm32f7_dnld_active())
    {
      hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, decodedSize, _dnldShared);
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

//============================================================
// Free any memory that needs freeing in struct hcom_dnld_shared_s.
// The intent is any function can call this and be assured that all the
// memory is released.
int hcom_host_process_free_dnld_share()
{
  // Free any strings etc.
  if(_dnldShared->dnldOrigFileName != NULL)
  {
    free(_dnldShared->dnldOrigFileName);
  }

  if(_dnldShared->dnldFullFileName != NULL)
  {
    free(_dnldShared->dnldFullFileName);
  }

  free(_dnldShared);  // Free the structure holding the information
  _dnldShared = NULL;

  return OK;
}

//============================================================
// Basic initialization
int hcom_host_process_init_dnld_share(uint32_t partitionId)
{
  // Allocate the struct used to support this download/delete.
  if(_dnldShared != NULL)
  {
    hcom_host_process_free_dnld_share();
  }

  _dnldShared = malloc(sizeof(hcom_dnld_shared_t));
  memset(_dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // This is a nuttx configuration about partitioning 
#ifdef CONFIG_MTD_PARTITION
  _dnldShared->dnldFilePartId = partitionId;
#else
  _dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
#endif

  _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  return OK;
}

//=================================================================
// The remaining code deals with a download failure
//=================================================================
// Encountered a watchdog timeout and this function gets called from our
// pthread main loop while waiting for the semaphore.
// int hcom_host_process_handle_wdog_timeout(size_t *haveValidMsgSize)
// {
  // int ret;
  // int result;

  // *haveValidMsgSize = 0;

  // // Setting the download state to inactive directs future downloads.
  // _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  // // Delete the download wdog timer that got us here
  // ret = hcom_host_process_dnld_timer_delete();
  // if (ret < 0)
  // {
  //   hcom_logging_syslog(LOG_ERR, "%s@%d-Stop/delete timer failed, ret:%d, errno:%d\n",
  //            thisFile, __LINE__, ret, get_errno());
  // }

  // // Close the partially downloaded file
  // ret = hcom_file_write_close_active_file(_dnldShared);
  // if (ret < 0)
  // {
  //   hcom_logging_syslog(LOG_ERR, "%s@%d-close failed for '%s', ret:%d, errno:%d\n",
  //            thisFile, __LINE__, _dnldShared->dnldOrigFileName, ret, get_errno());
  // }

  // // Delete the file 
  // hcom_file_delete_stm32f7_file_by_name(_dnldShared);

  // // Send a message to CLI to stop sending data
  // char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  // snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
  //       "File '%s' download failed, resend (AKA:%s)", _dnldShared->dnldOrigFileName,
  //       _dnldShared->dnldFullFileName);

  // hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_DNLD_FAIL_RESEND, 0, hostMsg,
  //       thisFile, __LINE__);

  // // This loop will remove any download data still in the cir buf. Care must
  // // be taken because we don't want to throw away good data that could have
  // // already been sent.
  // // There's a potential problem here. As soon as we start to remove data from
  // // the cir buffer, the receive thread starts adding more stuff to it.
  // while(true)
  // {
  //   size_t packetLength;
  //   result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);
  //   if(result == HCOM_CIR_BUF_GET_FOUND_MSG)
  //   {
  //     // We pulled a good packet so decode it.
  //     size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);
  //     if(decodedPacketSize == 0)
  //     {
  //       continue;
  //     }

  //     // Only care about header info. All packets have this header
  //     HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) _decode_dest_buf;

  //     // The sequence number determines if this packet is a command or data
  //     if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  //     {
  //       // Only non-data packets have the hcom protocol header
  //       HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) _decode_dest_buf;
  //       uint16_t requestType = hdrMsg->stdHeader.rqstType;
  //       if(requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER)
  //       {
  //         // This is the CLI's final message of the file download. So we need
  //         //  to stop emptying the buffer.
  //         break;
  //       }
  //       else
  //       {
  //         // There must not have been a File Download End message. Set the flag
  //         // indicating a valid message has been read and must be processed
  //         // before processing any other messages are read.
  //         *haveValidMsgSize = decodedPacketSize;
  //         break;
  //       }
  //     }
  //     else
  //     {
  //       // Only other type is a data packet, so we keep pulling data.
  //       continue;
  //     }
  //   }
  //   else
  //   {
  //     // Assume HCOM_CIR_BUF_GET_NONE_FOUND indicating that the buffer is
  //     // empty.
  //     break;
  //   }
  // }

//   return OK;
// }

// //=================================================================
// // Callback on watchdog timer expiration. Set a flag so we know that when
// // EINTR is detected, it was this timeout that caused it. This will trigger
// // the download state cleanup.
// void hcom_host_process_timeout_expired(int signo, FAR siginfo_t *info,
//           FAR void *context)
// {
//   _hcom_host_process_wdog_timedout = true;
// }

// //=================================================================
// // Start, restart, or stop the timer
// // This gets called a lot when downloading
// int hcom_host_process_dnld_timer_set_delay(time_t delayInSeconds)
// {
//   struct itimerspec todelay;
//   int ret;

//   // Start, restart, or stop the timer
//   todelay.it_interval.tv_sec = 0; // Nonrepeating
//   todelay.it_interval.tv_nsec = 0;
//   todelay.it_value.tv_sec = delayInSeconds;
//   todelay.it_value.tv_nsec = 0;
  
//   ret = timer_settime(_processWdogTimerId, 0, &todelay, NULL);
//   if (ret < 0)
//   {
//     int errorcode = errno;
//     hcom_logging_syslog(LOG_ERR, "%s@%d-setting timer, errno:%d\n", thisFile, __LINE__, errorcode);
//     return -errorcode;
//   }
//   return OK;
// }

// //=================================================================
// // Create the POSIX timer for detecting download failures
// int hcom_host_process_dnld_timer_initialize()
// {
//   struct sigevent toevent;
//   struct sigaction act;
//   int ret;

//   _processWdogTimerId = NULL;

//   // Create a POSIX timer to handle timeouts
//   toevent.sigev_notify = SIGEV_SIGNAL;
//   toevent.sigev_signo = SIGALRM;
//   toevent.sigev_value.sival_ptr = NULL;  // Carry value to 'context' in callback

//   ret = timer_create(CLOCK_REALTIME, &toevent, &_processWdogTimerId);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s@%d-create timer errno:%d\n", thisFile, __LINE__, errno);
//     return -errno;
//   }

//   // Attach a callback to catch the timeout
//   act.sa_sigaction = hcom_host_process_timeout_expired;
//   act.sa_flags = SA_SIGINFO;
//   sigemptyset(&act.sa_mask);

//   ret = sigaction(SIGALRM, &act, NULL);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s@%d-attach signal errno:%d\n", thisFile, __LINE__, errno);
//     return -errno;
//   }
//   return OK;
// }

// //=================================================================
// // Delete the POSIX timer for detecting download failures
// int hcom_host_process_dnld_timer_delete()
// {
//   int ret;
  
//   ret = hcom_host_process_dnld_timer_set_delay(0);
//   if(ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set delay = 0, errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
//   }

//   ret = timer_delete(_processWdogTimerId);
//   if(ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
//   }

//   return ret;
// }
