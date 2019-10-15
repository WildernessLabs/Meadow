/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_transmitter.c
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
static uint8_t *_encodedBuff;
static pid_t _creator_pid;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static void hcom_host_msg_bldr_build_msg_header(uint16_t requestType, uint16_t ctrlData,
        uint32_t userData, uint8_t *xmitBuffer);
static int hcom_host_msg_bldr_encode_and_send_msg(uint8_t * message, size_t messageLength);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_msg_builder_setup()
{
  _encodedBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);
  _creator_pid = getpid();
  return OK;
}

//--------------------------------------------------------------------
void hcom_host_msg_builder_shutdown()
{
  free(_encodedBuff);
  _shutting_down = true;
}

//=====================================================================
// Just sends a header message
int hcom_host_msg_bldr_send_information_msg(uint16_t ctrlData, uint32_t userData)
{
  struct HcomProtocolHeader_s hcomHdr;

  if(hcom_usb_acm_was_host_xmit_blocked())
  {
    // This is a normal occurance since the host is usually not connected
    return OK;   // Throw the message away. What else can be done?
  }

  hcom_host_msg_bldr_build_msg_header(HCOM_HOST_REQUEST_SIMPLE_MESSAGE,
          ctrlData, userData, (uint8_t *)&hcomHdr);

  hcom_host_msg_bldr_encode_and_send_msg((uint8_t *) &hcomHdr, HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);  
  // ret not used because error already reported 
  return OK;
}

//=====================================================================
int hcom_host_msg_bldr_send_short_str_msg(uint16_t ctrlData, uint32_t userData, char *shortText)
{
  // Need to remove any trailing cr/lf. If none found strcspn() finds terminating '\0'
  // and returns its offset
  size_t trueDataLen = strcspn(shortText, "\r\n");
  int ret = hcom_host_msg_bldr_send_short_buffer_msg(ctrlData, userData, (uint8_t*) shortText, trueDataLen);
  return ret;
}

//=====================================================================
// At this time this function could be made local to this file
int hcom_host_msg_bldr_send_short_buffer_msg(uint16_t ctrlData, uint32_t userData,
      uint8_t *origMsg, size_t msgLen)
{
  int ret;

  if(hcom_usb_acm_was_host_xmit_blocked())
  {
    // This is a normal occurance since the host is usually not connected
    return OK;   // Throw the message away. What else can be done?
  }
  
  int fullMsgLen = msgLen + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;
  if(fullMsgLen > HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN)
  {
    // Truncate string to fit
    fullMsgLen = HCOM_PROTOCOL_PACKET_MAX_SIZE;
  }

  uint8_t *xmitBuffer = malloc(fullMsgLen);

  // Uses the first part of message buffer for header
  hcom_host_msg_bldr_build_msg_header(HCOM_HOST_REQUEST_SIMPLE_TEXT_MESSAGE,
      ctrlData, userData, xmitBuffer);

  // Copy the data of the message
  memcpy(xmitBuffer + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH, origMsg, fullMsgLen - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);
  ret = hcom_host_msg_bldr_encode_and_send_msg((uint8_t *) xmitBuffer, fullMsgLen);

  free(xmitBuffer);
  return ret;
}

//=====================================================================
// Build the header
void hcom_host_msg_bldr_build_msg_header(uint16_t requestType,
        uint16_t ctrlData, uint32_t userData, uint8_t *xmitBuffer)
{
  // Populate the header
  struct HcomProtocolHeader_s *hdr = (struct HcomProtocolHeader_s *) xmitBuffer;

  hdr->seqNumber = HCOM_PROTOCOL_REQUEST_HEADER_SEQ_NUMBER;
  hdr->version = HCOM_PROTOCOL_CURRENT_VERSION_NUMBER;
  hdr->control = ctrlData;
  hdr->rqstType = requestType;
  hdr->userData = userData;
}

//=====================================================================
// Send the completed message
int hcom_host_msg_bldr_encode_and_send_msg(uint8_t *message, size_t messageLength)
{
  int ret;

  // Encode
  size_t encodedSize = hcom_com_support_cobs_encoder(message, 0, messageLength, _encodedBuff);

  // Encoded message needs a terminating delimiter for COBS
  DEBUGASSERT(encodedSize < HCOM_SAFE_PACKET_BUF_SIZE - 1);
  _encodedBuff[encodedSize] = HCOM_PROTOCOL_PACKET_DELIMITER_VALUE;

  ret = hcom_usb_acm_transmit_to_host(_encodedBuff, encodedSize + 1);
  if(ret < 0)
  {
    if(ret == -EAGAIN)
    {
      f7syslog_x(LOG_INFO, "%s() - The last message was blocked.\n", __func__);
      return OK;
    }
    else
    {
      f7syslog_x(LOG_ERR, "%s/%s() @%d Error (%d).\n", __FILE__, __func__, __LINE__, ret);
    }
  }
  
  return ret;
}
