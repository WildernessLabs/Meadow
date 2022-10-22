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
#include <meadow/hcom_dnld_shared.h>

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

// (--) NEEDS TO BE REMOVED WHILE HACKING
// static hcom_dnld_shared_t *_dnldShared;

// static timer_t _processWdogTimerId;
// static bool _hcom_host_process_wdog_timedout;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_process_route_packet(const uint8_t *packet, const size_t packetSize);
static FAR void *hcom_host_proc_pthread(FAR void *arg);
static int hcom_host_proc_create_thread(void);
// static void hcom_file_process_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
// static int hcom_host_proc_handle_wdog_timeout(size_t *haveValidMsgSize);
// static int hcom_host_proc_read_all_cir_buf_msg(void);
// static int hcom_host_dnld_shared_init(uint32_t userData);
static bool hcom_file_dnld_stm32f7_is_active(void);

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

  // (--) TEMPORARY
  // _dnldShared = NULL;
  
  struct sched_param sparam;
  sparam.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
  sched_setparam(0, &sparam);

  hcom_host_proc_pthread(NULL);
}

//=============================================================
// Create thread to preocess hcom received messages. This thread processes
// all CLI commands and notifications.
// int hcom_host_proc_create_thread()
// {
//     int ret;
//     pthread_t thread;
//     pthread_attr_t attr;
//     struct sched_param param;

//     param.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
//     (void)pthread_attr_init(&attr);
//     (void)pthread_attr_setschedparam(&attr, &param);
//     (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_HCOM_PROCESS);

//     ret = pthread_create(&thread, &attr, hcom_host_proc_pthread, NULL);
//     if (ret < 0)
//     {
//       hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
//                 thisFile, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret, errno);
//       return ret;
//     }

//   return OK;
// }

//====================================================================
void hcom_host_process_shutdown()
{
  _shutting_down = true;
  free(_decode_dest_buf);
  free(_packet_dest_buf);
}

//==========================================================================
// Are we involved in some download activity?
// bool hcom_file_dnld_stm32f7_is_active()
// {
//   // (--) HACKING
//   return true;

//   // if(_dnldShared == NULL)
//   // {
//   //   return false;
//   // }

//   // return (_dnldShared->dnldCurrentState != HcomStm32F7DnldStateNone);
// }

// //=======================================================================
// (--) THIS ISN'T A PROBLEM FOR PROCESSING
// // The receive thread calls here to add the received data to the circular
// // buffer. It can be added byte-by-byte or several messages at once. The
// // data will be pulled from the circular buffer in packets to be processed.
// int hcom_host_process_save_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
// {
//   int ret;
//   int result;
//   int semCount;

//   if (recvByteCnt == 0)
//     return OK;

//   // This loop is used to add received data to the buffer until no more will
//   // fit. The received data is assumed to not be received in packet sized
//   // chuncks.
//   for (;;)
//   {
//     // Only 3 possible results of this call, HCOM_CIR_BUF_ADD_SUCCESS,
//     // HCOM_CIR_BUF_ADD_WONT_FIT or HCOM_CIR_BUF_ADD_BAD_ARG
//     result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
//     switch(result)
//     {
//       case HCOM_CIR_BUF_ADD_SUCCESS:
//       // Notify proc thread that there's work to do by adding 1 to semaphore
//       // count. We don't want to over-post (allowing count to get > 1).
//       ret = sem_getvalue(&_processWaitRecvSem, &semCount);
//       if (ret == OK && semCount <= 0)
//       {
//         sem_post(&_processWaitRecvSem);
//       }

// #if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
//         hcom_logging_syslog(LOG_DEBUG, "%s@%d-%d bytes added to cir buf\n", thisFile, __LINE__, recvByteCnt);
// #endif
//         return OK;

//       case HCOM_CIR_BUF_ADD_WONT_FIT:
//         // If the buffer is full we must wait for the proc thread to empty it.
//         // This is a common occurrance when downloading large files. This
//         // sleep value is arbitrary, too short and waste CPU, too long and
//         // download is slowed.
//         usleep(30 * 1000);
//         break;    // Try again to add msg to buffer

//       case HCOM_CIR_BUF_ADD_BAD_ARG:
//         // Message being added has zero length
//         hcom_logging_syslog(LOG_ERR, "%s@%d-Message with length of 0 ignored\n", thisFile, __LINE__);
//         return OK;

//       default:
//         hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown cir buf err:%d\n", thisFile, __LINE__, result);
//         return OK; // Report and keep going
//     }
//   }
//   return OK;
// }

//=================================================================
// This thread processes all the messages the receive thread has written to
// the circular buffer.
// (--) THIS IS TO BE REPLACED BY USING THE MAIN TASK THREAD USED TO INIT EVERYTHING
FAR void *hcom_host_proc_pthread(FAR void *arg)
{
  int ret;
  size_t packetLength;
  
  syslog(1, "$-proc-@%d hcom_host_proc_pthread running\n", __LINE__);
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
  return NULL;
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
        //     // hcom_host_proc_handle_wdog_timeout(&haveValidMsgSize);
            
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
// This function will read all valid message from circular buffer and
// routes them to the proper destination.
// int hcom_host_proc_read_all_cir_buf_msg()
// {
//   int result;
//   size_t packetLength;

  // Pull next message packet from cir buf and route the packet. Loop until
  // buffer is empty of complete message packets.
  // do
  // {
  //   // This can only return one of these 3 values, HCOM_CIR_BUF_GET_FOUND_MSG,
  //   // HCOM_CIR_BUF_GET_NONE_FOUND or HCOM_CIR_BUF_GET_DELETED_TOO_BIG
  //   result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);
  //   if(result == HCOM_CIR_BUF_GET_FOUND_MSG)
  //   {
  //     // We pulled a good packet. Drop trailing delimiter using --packetLength,
  //     // then decode the packet and route it.
  //     size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);

  //     if(decodedPacketSize == 0)
  //       continue;

  //     // Process the received & decoded packet
  //     result = hcom_host_process_route_packet(_decode_dest_buf, decodedPacketSize);
  //     if (result < 0)
  //     {
  //       // If ever supported, NAK host to resend bad packet
  //       hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
  //     }
  //   }
  //   else if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
  //   {
  //     // Nothing in the buffer, this his not an error, there's just a partially
  //     // received message in the buffer. So, we'll leave this loop and wait to
  //     // be notified when the rest of the message is received.
  //     break;
  //   }
  //   else
  //   {
  //     // The only remaining return value is HCOM_CIR_BUF_GET_DELETED_TOO_BIG. So,
  //     // if we've been careful to provide a large enough buffer this will
  //     // never happen. But, just in case output a syslog message.
  //     // If the buffer is too small packetLength will contain the needed size.
  //     hcom_logging_syslog(LOG_ERR, "%s@%d-Dest buffer too small. Need:%d bytes\n",
  //               __FILE__, __LINE__, packetLength);
  //     sleep(1);         // Make sure error log above is output
  //     return -ENOMEM;   // This will terminate this thread
  //   }

  // } while(!_shutting_down); // loops till buffer empty

  // Return and wait to be notified again
  // return OK;
// }

//====================================================================
// Parse and process received decoded packets as sent by host (CLI).
// Grab the sequence number, using it to determine if data or command.
int hcom_host_process_route_packet(const uint8_t *decodedPacket, const size_t decodedSize)
{
// (--) WHILE HACKING, TEMPORARY OLD CODE
//-------------------------------------------------
  // Parse and process received packet as sent by host
  // 1) Grab the sequence number
  // 2) Remove sequence number and process as needed
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  // The sequence number determines if this message is a command or data
  if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // A non-data i.e. command  message
    hcom_host_route_request_by_cmd_type((HcomProtoHdrMsg_t *) decodedPacket, decodedSize);
  }
  else
  {
    // Must be a Data Packet (sequence number != 0) 
    hcom_file_dnld_proc_recvd_file_data(hcomDataMsg, decodedSize);
  }

  return OK;
}

//-------------------------------------------------
// (--) NEW CODE BUT NOT YET
/*
    // All messages contains the sequence number
    HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
              thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

    // The sequence number determines if this packet is a command or data
    if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
    {
      // Only commands (non-data packets) have the hcom protocol header
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
      const uint16_t requestType = hdrMsg->stdHeader.rqstType;
      const uint32_t userData = hdrMsg->stdHeader.userData;

      // For downloading or deleting need the file name both original and posix
      if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
        requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
        requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
      {
        // Initialize the struct containing all download/delete state information
        hcom_host_dnld_shared_init(userData);
      
        // We'll do a little work here so it doesn't need to be done in multiple
        // places.
        size_t fileNameLength = decodedSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
        _dnldShared->dnldOrigFileName = malloc(fileNameLength + 1);
        if(_dnldShared->dnldOrigFileName == NULL)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
          return -ENOMEM;
        }

        HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
        memcpy(_dnldShared->dnldOrigFileName, fileMsg->fileInfo.fileName, fileNameLength);
        _dnldShared->dnldOrigFileName[fileNameLength] = '\0';

        // Build the full path plus file name string (e.g. /mnt0/FileName.ext)
        size_t fullFileNameLen = strlen(_dnldShared->dnldOrigFileName) + \
                  strlen(HCOM_FILE_MOUNT_POINT_TARGET) + 3; // Add '/', Partition Id and NULL

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
      if(hcom_file_dnld_stm32f7_is_active())
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
*/

//============================================================
// Free any memory that needs freeing in struct hcom_dnld_shared_s.
// The intent is any function can call this and be assured that all the
// memory is released.
int hcom_host_dnld_shared_free()
{
  // Free any strings etc.
  // if(_dnldShared->dnldOrigFileName != NULL)
  // {
  //   free(_dnldShared->dnldOrigFileName);
  // }

  // if(_dnldShared->dnldFullFileName != NULL)
  // {
  //   free(_dnldShared->dnldFullFileName);
  // }

  // free(_dnldShared);  // Free the structure holding the information
  // _dnldShared = NULL;

  return OK;
}

//============================================================
// Basic initialization
// int hcom_host_dnld_shared_init(uint32_t userData)
// {
//   // Allocate the struct used to support this download/delete.
// //   if(_dnldShared != NULL)
// //   {
// //     hcom_host_dnld_shared_free();
// //   }

// //   _dnldShared = malloc(sizeof(hcom_dnld_shared_t));
// //   memset(_dnldShared, 0, sizeof(hcom_dnld_shared_t));

// //   // This is a nuttx configuration about partitioning 
// // #ifdef CONFIG_MTD_PARTITION
// //   _dnldShared->dnldFilePartId = userData;
// // #else
// //   _dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
// // #endif

// //   _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

//   return OK;
// }

//=================================================================
// The remaining code deals with a download failure
//=================================================================
// Encountered a watchdog timeout and this function gets called from our
// pthread main loop while waiting for the semaphore.
// int hcom_host_proc_handle_wdog_timeout(size_t *haveValidMsgSize)
// {
  // int ret;
  // int result;

  // *haveValidMsgSize = 0;

  // // Setting the download state to inactive directs future downloads.
  // _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  // // Delete the download wdog timer that got us here
  // ret = hcom_file_process_dnld_timer_delete();
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
// void hcom_file_process_timeout_expired(int signo, FAR siginfo_t *info,
//           FAR void *context)
// {
//   _hcom_host_process_wdog_timedout = true;
// }

// //=================================================================
// // Start, restart, or stop the timer
// // This gets called a lot when downloading
// int hcom_file_process_dnld_timer_set_delay(time_t delayInSeconds)
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
// int hcom_file_process_dnld_timer_initialize()
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
//   act.sa_sigaction = hcom_file_process_timeout_expired;
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
// int hcom_file_process_dnld_timer_delete()
// {
//   int ret;
  
//   ret = hcom_file_process_dnld_timer_set_delay(0);
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
