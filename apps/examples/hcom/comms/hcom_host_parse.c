/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_parse.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
static host_com_cir_buffer_t *_hcom_cbuf;
static size_t _max_packet_size = HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_parse_process_packet(const uint8_t *packet, const size_t packetSize);
static FAR void *hcom_host_proc_pthread(FAR void *arg);
static int hcom_host_proc_create_thread(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_parse_setup()
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
  
  // Create the processing thread
  return hcom_host_proc_create_thread();
}

//====================================================================
void hcom_host_parse_shutdown()
{
  _shutting_down = true;

  free(_packet_dest_buf);
  free(_decode_dest_buf);
  hcom_cirbuf_release_memory(_hcom_cbuf);
  free(_hcom_cbuf);
}

//=======================================================================
// The receive thread calls here to add the received data to the circular
// buffer. It can be added byte by byte or several messages at once. The
// data will be pulled from the  circular buffer in packets to be processed.
int hcom_host_parse_save_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;
// syslog(1, "-->Saving %d bytes to circular buffer\n", recvByteCnt);

  // int tempLIMIT_CNT = 0;
  // int tempLIMIT_WAIT = 50;   // 5 SECONDS  WITH  100 MS SLEEP

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add raw data to the buffer until no more will fit
  for (;;)
  {
    // Only 3 possible results of this call, HCOM_CIR_BUF_ADD_SUCCESS,
    // HCOM_CIR_BUF_ADD_WONT_FIT or HCOM_CIR_BUF_ADD_BAD_ARG
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    switch(result)
    {
      case HCOM_CIR_BUF_ADD_SUCCESS:
// syslog(1, "-->Saved to circular buffer, Success\n");
#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
        hcom_logging_syslog(LOG_DEBUG, "%s@%d-%d bytes added to cir buf\n", thisFile, __LINE__, recvByteCnt);
#endif
        return OK;

      case HCOM_CIR_BUF_ADD_WONT_FIT:
// syslog(1, "-->Saved to circular buffer. But, Msg Won't Fit\n");
        // Wasn't possible to put these bytes in the buffer. We need to process
        // a few packets and then retry to add this data. This happens when
        // there is a request being processed and other CLI commands keep being
        // received. I'm not sure this can even happen.
        // tempLIMIT_CNT++;
        // if(tempLIMIT_CNT > tempLIMIT_WAIT)
        {
          // Periodically report as warning
          // tempLIMIT_CNT = 0;
          // hcom_logging_syslog(LOG_WARNING, "%s@%d-No room in cir buf for %d bytes, waiting and retry\n",
          //         thisFile, __LINE__, recvByteCnt);
        }

        // if(!hcom_file_dnld_stm32f7_is_active())
        usleep(1 * 1000);

        break;    // keep looping

      case HCOM_CIR_BUF_ADD_BAD_ARG:
// syslog(1, "-->Saved to circular buffer, Msg had 0 length\n");
        // Message being added has zero length
        hcom_logging_syslog(LOG_ERR, "%s@%d-Message with length of 0 ignored\n", thisFile, __LINE__);
        return OK;

      default:
// syslog(1, "-->Saved to circular buffer, Unknow result\n");
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown cir buf err:%d\n", thisFile, __LINE__, result);
        return OK; // Report and keep going
    }
  }
  return OK;
}

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

//=================================================================
// This thread processes all the messages the receive thread after they
// are put into the circular buffer
FAR void *hcom_host_proc_pthread(FAR void *arg)
{
  int result;
  size_t packetLength;

  // Pull and process all the complete packets from the circular buffer
  // int hcom_host_parse_pull_all_packets_from_buffer()
  while (!_shutting_down)
  {
    // Wait to be notified that there's something in the buffer
    // TEMPORARY UNTIL HANDSHAKE SEMAPHORE ADDED
    // if(!hcom_file_dnld_stm32f7_is_active())
    usleep(1 * 1000);

    // There are 3 possible return values, HCOM_CIR_BUF_GET_FOUND_MSG,
    // HCOM_CIR_BUF_GET_NONE_FOUND and HCOM_CIR_BUF_GET_DEST_NO_ROOM

    // If buffer too small packetLength will contain the desired size
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);
    if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
    {
      // Nothing in the buffer, this is happens 99.99% of the time. Wait and
      // try again when notified.
      continue;
    }
  
    if (result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
    {
// syslog(1, "-->Pulled from circular buffer. Result:No ROOM for %d bytes (buffer too small)\n", packetLength);
      // This should NEVER happen, the supplied buffer is too small
      hcom_logging_syslog(LOG_ERR, "%s@%d-Dest buffer too small. Need:%d\n",
                __FILE__, __LINE__, packetLength);
      continue;
    }

    // Must be HCOM_CIR_BUF_GET_FOUND_MSG
    // Drop trailing delimiter of 0x00 (--packetLength) here, then decode the packet
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);

    if(decodedPacketSize == 0)
      continue;
      
    // Process the received/decoded packet
    result = hcom_host_parse_process_packet(_decode_dest_buf, decodedPacketSize);
    if (result < 0)
    {
      // If ever supported, NAK host to resend bad data
      hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
    }
    continue;
  }

  return NULL;    // Will terminate thread
}

//====================================================================
// Parse and process received packet as sent by host
// 1) Grab the sequence number
// 2) Remove sequence number and process as needed
int hcom_host_parse_process_packet(const uint8_t *packet, const size_t packetSize)
{
  // All messages contains the sequence number
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) packet;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, packetSize);

  // The sequence number determines if this message is a command or data
  if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // A non-data i.e. command  message
    hcom_host_route_request_by_cmd_type((HcomProtoHdrMsg_t *) packet, packetSize);
  }
  else
  {
    // Must be a Data Packet because sequence number != 0. But, is it for
    // external flash or ESP32?
    if(hcom_file_dnld_stm32f7_is_active())
    {
      hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, packetSize);
    }
    else if(hcom_file_dnld_esp32_is_active())
    {
      hcom_file_dnld_esp32_recvd_file_data(hcomDataMsg, packetSize);
    }
    else
    {
      // CLI must be confused
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data received but no active download\n",
                thisFile, __LINE__);
    }
  }

  return OK;
}
