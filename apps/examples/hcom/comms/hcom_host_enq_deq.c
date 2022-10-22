/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_enq_deq.c
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

// This file is mostly about saving undelimited data, buffering it and
// pulling packetized data and forwarding it to be routed.

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
static sem_t _lockCirBufSem;
static sem_t _runProcSem;
static sem_t _runRecvSem;
static bool _FBFlag;
static host_com_cir_buffer_t *_hcom_cbuf;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_enq_deq_setup()
{
  _shutting_down = false;

  // Protects circular buffer
  sem_init(&_lockCirBufSem, 0, 1);
  sem_setprotocol(&_lockCirBufSem, SEM_PRIO_NONE);
  
  // Notifies proc thread a message has been queued
  sem_init(&_runProcSem, 0, 0);
  sem_setprotocol(&_runProcSem, SEM_PRIO_NONE);

  // Notifies recv that it should retry to add past message to circular buffer
  sem_init(&_runRecvSem, 0, 0);
  sem_setprotocol(&_runRecvSem, SEM_PRIO_NONE);

  _hcom_cbuf = (host_com_cir_buffer_t *)malloc(sizeof(host_com_cir_buffer_t));

  if (_hcom_cbuf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-One of 2 allocations failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE,
          HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE);
  if (result == HCOM_CIR_BUF_ALLOC_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Buffer allocation failed\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
}

//====================================================================
//
void hcom_host_enq_deq_shutdown()
{
  _shutting_down = true;

  hcom_cirbuf_release_memory(_hcom_cbuf);
  free(_hcom_cbuf);

 sem_destroy(&_lockCirBufSem);
 sem_destroy(&_runProcSem);
 sem_destroy(&_runRecvSem);
}

//=======================================================================
// Add the received data is put into the circular buffer. It is added byte by
// byte or several messages at once.
// TODO: during sem_wait a signal will wake up this thread
int hcom_host_enq_deq_enqueue_rcvd_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;

  if (recvByteCnt == 0)
    return OK;
  do
  {
    // Gain exclusive access to circular buffer
    syslog(1, "EQ-Ownership cirbuf @%d\n", __LINE__); usleep(20 * 1000);
    sem_wait(&_lockCirBufSem);

    // Only possible return values: HCOM_CIR_BUF_ADD_SUCCESS,
    // HCOM_CIR_BUF_ADD_WONT_FIT and HCOM_CIR_BUF_ADD_BAD_ARG
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    switch(result)
    {
      case HCOM_CIR_BUF_ADD_SUCCESS:
        syslog(1, "EQ-Release cirbuf (Add success) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        syslog(1, "EQ-Awake proc (data added) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_runProcSem);         // Notify proc of message
        return OK;                      // Return to read more data

      case HCOM_CIR_BUF_ADD_WONT_FIT:
        _FBFlag = true;                 // Set Full Buffer Flag then free cir buff
        syslog(1, "EQ-Release cirbuf (Won't Fit) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        syslog(1, "EQ-Awake proc (need room) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_runProcSem);         // Notify proc to read messages
        syslog(1, "EQ-Wait for recv post [check cirbuf again] @%d\n", __LINE__); usleep(10 * 1000);
        sem_wait(&_runRecvSem);         // Wait for a message to be removed
        syslog(1, "EQ-Wokeup recv (data removed) @%d\n", __LINE__); usleep(10 * 1000);
        continue;                       // Try again to add message

      case HCOM_CIR_BUF_ADD_BAD_ARG:
        // Report error and return. The message is lost.
        syslog(1, "EQ-Release cirbuf (Bad Arg)? @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
        return OK;

      default:
        syslog(1, "EQ-Release cirbuf (default)? @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown return from hcom_cirbuf_add_bytes():%d\n",
                    thisFile, __LINE__, result);
        break;
    }
  } while (! _shutting_down);

  return OK;
}

//====================================================================
// TODO: during sem_wait a signal will wake up this thread
// Proc calls here to get the next message
int hcom_host_enq_deq_dequeue_packet(uint8_t *packet_dest_buf,
          size_t *packetLength)
{
  int result;

  do
  {
    // Gain exclusive access to circular buffer
    syslog(1, "DQ-Ownership cirbuf @%d\n", __LINE__); usleep(20 * 1000);
    sem_wait(&_lockCirBufSem);

    // Only HCOM_CIR_BUF_GET_FOUND_MSG, HCOM_CIR_BUF_GET_NONE_FOUND and
    // HCOM_CIR_BUF_GET_DELETED_TOO_BIG can be returned
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, packet_dest_buf,
              HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE, packetLength);
    switch(result)
    {
      case HCOM_CIR_BUF_GET_FOUND_MSG:
        if(_FBFlag)
        {
          _FBFlag = false;              // Full buffer flag did it's work
          syslog(1, "DQ-Awake recv (Found msg, buffer full) @%d\n", __LINE__); usleep(10 * 1000);
          sem_post(&_runRecvSem);       // Allow recv to retry to add
        }
        syslog(1, "DQ-Release cirbuf (Found msg) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        return OK;                      // Return to process message

      case HCOM_CIR_BUF_GET_NONE_FOUND:
        syslog(1, "DQ-Release cirbuf (none found) @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        syslog(1, "DQ-Wait for proc post [wait for data add] @%d\n", __LINE__); usleep(10 * 1000);
        sem_wait(&_runProcSem);         // Wait for a message to be added
        syslog(1, "DQ-Wokeup proc (data added) @%d\n", __LINE__); usleep(10 * 1000);
        break;                          // Loop again to check for new message

      case HCOM_CIR_BUF_GET_DELETED_TOO_BIG:
        // The message was too big and the circular buffer code has removed it.
        // Report error and return.
        syslog(1, "DQ-Release cirbuf (too big)? @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Message too big, deleted, size:%d\n",
                  thisFile, __LINE__, packetLength);
        break;                          // Try again to get the next message

      default:
        syslog(1, "DQ-Release cirbuf (default)? @%d\n", __LINE__); usleep(10 * 1000);
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown return from hcom_cirbuf_get_next_packet():%d\n",
                  thisFile, __LINE__, result);
        break;
    }
    syslog(1, "DQ-Looping to read next message @%d\n", __LINE__); usleep(10 * 1000);

  } while (! _shutting_down);
  
  return OK;
}

// //====================================================================
// // Pull and process all the complete packets from the circular buffer
// // (--) THIS NEEDS A COMPLETE RE-WRITE TO CONSIDER THE PROC THREAD
// int hcom_host_enq_deq_pull_all_packets_from_buffer()
// {
//   int result;

//   for (;;)
//   {
//     size_t packetLength;
//     // If buffer too small packetLength will contain the desired size
//     result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf,
//               HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE, &packetLength);

//     if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
//       return OK; // Return to receive more data

//     if (result == HCOM_CIR_BUF_GET_DELETED_TOO_BIG)
//     {
//       syslog(LOG_ERR, "%s@%d-Dest buffer too small. Need:%d\n",
//                 __FILE__, __LINE__, packetLength);
//       return result;
//     }

//     // Must be HCOM_CIR_BUF_GET_FOUND_MSG
//     // Drop trailing delimiter of 0x00 (--packetLength) then decode the packet
//     size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);

//     if(decodedPacketSize == 0)
//       continue;
      
//     // Process the received data
//     // (--) SET THE PROPER SEMAPHORE TO WAKE UP THE PROCESSING THREAD
//     result = hcom_host_enq_deq_dequeue_packet(_decode_dest_buf, decodedPacketSize);
//     if (result == OK)
//     {
//       continue; // pull next packet
//     }
//     else if (result < 0)
//     {
//       hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
//       return result;
//       // If ever supported NAK to host to resend bad data
//     }
//     else
//     {
//       hcom_logging_syslog(LOG_ERR, "%s@%d-unknown value %d\n",
//               thisFile, __LINE__, result);
//       return result;
//     }
//   }
// }

// // (--) THIS WILL BE CALLED FROM THE NEW PROCESSING THREAD
// // //====================================================================
// // // Parse and process received packet as sent by host
// // // 1) Grab the sequence number
// // // 2) Remove sequence number and process as needed
// int hcom_host_enq_deq_dequeue_packet(const uint8_t *packet, const size_t packetSize)
// {
// //   // All messages contains the sequence number
// //   HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) packet;

// //   hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
// //             thisFile, __LINE__, hcomDataMsg->seqNumber, packetSize);

// //   // The sequence number determines if this message is a command or data
// //   if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
// //   {
// //     // A non-data i.e. command  message
// //     hcom_host_route_request_by_cmd_type((HcomProtoHdrMsg_t *) packet, packetSize);
// //   }
// //   else
// //   {
// //     // Must be a Data Packet (sequence number != 0) 
// //     hcom_file_dnld_proc_recvd_file_data(hcomDataMsg, packetSize);
// //   }

//   return OK;
// }
