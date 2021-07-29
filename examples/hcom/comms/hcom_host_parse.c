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
static int hcom_host_parse_pull_all_packets_from_buffer(void);

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
    hcom_logging_syslog(LOG_ERR, "%s@%d-cir buf alloc\n", thisFile, __LINE__);
    return -1;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE,
          HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE);
  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_cirbuf_init\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
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
// Add the received data to the circular buffer. It can be added byte by byte
// or several messages at once.
int hcom_host_parse_save_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add raw data to the buffer until no more will fit
  for (;;)
  {
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    if(result == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-%d bytes added to cir buf\n", thisFile, __LINE__, recvByteCnt);

      // In all valid cases pull all full packets and process them
      break;
    }
    else if (result == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this data
      hcom_logging_syslog(LOG_WARNING, "%s@%d-No room in cir buf, pull and retry\n",
              thisFile, __LINE__);
      result = hcom_host_parse_pull_all_packets_from_buffer();
      if (result == HCOM_CIR_BUF_GET_FOUND_MSG)
        continue;   // There should be room now for the failed add

      if (result == HCOM_CIR_BUF_GET_NONE_FOUND || result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-pull packets from cir buf. Result:%d\n",
                 thisFile, __LINE__, result);
        return OK;    // Report and throw data away.
      }
    }
    else if (result == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // Bad argument
      hcom_logging_syslog(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
      return OK; // Report and throw data away and keep going
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown cir buf add err:%d\n", thisFile, __LINE__, result);
      return OK; // Report and throw data away and keep going
    }
  }

  // This could be on a separate thread
  result = hcom_host_parse_pull_all_packets_from_buffer();
  return result;
}

//====================================================================
// Pull and process all the complete packets from the circular buffer
int hcom_host_parse_pull_all_packets_from_buffer()
{
  int result;

  for (;;)
  {
    size_t packetLength;
    // If buffer too small packetLength will contain the desired size
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, _packet_dest_buf, _max_packet_size, &packetLength);

    if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
      return OK; // Return to receive more data

    DEBUGASSERT(result != HCOM_CIR_BUF_GET_DEST_NO_ROOM);
    DEBUGASSERT(result == HCOM_CIR_BUF_GET_FOUND_MSG);

    // Drop trailing delimiter of 0x00 (--packetLength) then decode the packet
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);

    if(decodedPacketSize == 0)
      continue;
      
    // Process the received data
    result = hcom_host_parse_process_packet(_decode_dest_buf, decodedPacketSize);
    if (result == OK)
    {
      continue; // pull next packet
    }
    else if (result < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
      return result;
      // If ever supported NAK to host to resend bad data
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-unknown value %d\n",
              thisFile, __LINE__, result);
      return result;
    }
  }
}

//====================================================================
// Parse and process received packet as sent by host
// 1) Grab the sequence number
// 2) Remove sequence number and process as needed
int hcom_host_parse_process_packet(const uint8_t *packet, const size_t packetSize)
{
  // All messages contains the sequence number
  HcomProtocolDataMessage_t *hcomDataMsg = (HcomProtocolDataMessage_t *) packet;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->dataHeader.seqNumber, packetSize);

  // The sequence number determines if this message is a command or data
  if (hcomDataMsg->dataHeader.seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // A non-data i.e. command  message
    hcom_host_route_request_by_cmd_type((HcomProtocolCmdMessage_t *) packet, packetSize);
  }
  else
  {
    // Must be a Data Packet (sequence number != 0) 
    hcom_file_dnld_proc_recvd_file_data(hcomDataMsg, packetSize);
  }

  return OK;
}
