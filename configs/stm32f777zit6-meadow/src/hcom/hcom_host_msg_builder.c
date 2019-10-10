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
static bool _lastMessageWasBlocked;
static pid_t _creator_pid;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static bool hcom_host_msg_bldr_is_send_blocked(void);
static void hcom_host_msg_bldr_build_msg_header(uint16_t requestType, uint16_t ctrlData,
        uint32_t userData, uint8_t *xmitBuffer);
static int hcom_host_msg_bldr_encode_and_send_msg(uint8_t * message, size_t messageLength);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_msg_builder_setup()
{
  _lastMessageWasBlocked = false;
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

  if(hcom_host_msg_bldr_is_send_blocked())
    return OK;   // Throw the message away. What else can be done?

  hcom_host_msg_bldr_build_msg_header(HCOM_HOST_REQUEST_SIMPLE_MESSAGE,
          ctrlData, userData, (uint8_t *)&hcomHdr);

  hcom_host_msg_bldr_encode_and_send_msg((uint8_t *) &hcomHdr, HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);  
  // ret not used because error already reported 
  return OK;
}

//=====================================================================
int hcom_host_msg_bldr_send_short_text_msg(uint16_t ctrlData, uint32_t userData, char *shortText)
{
  uint8_t *message;
  int textLength;
  int msgLength;

  if(hcom_host_msg_bldr_is_send_blocked())
    return OK;   // Throw the message away. What else can be done?

  message = malloc(HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN);
  textLength = strlen(shortText);
  msgLength = textLength + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;

  if(msgLength > HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN)
  {
    // Truncate text to fit
    msgLength = HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN;
    textLength = HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;
  }

  // Uses the first part of message buffer for header
  hcom_host_msg_bldr_build_msg_header(HCOM_HOST_REQUEST_SIMPLE_TEXT_MESSAGE,
      ctrlData, userData, message);

  // Copy the rest of the message
  memcpy(message + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH, shortText, textLength);
  hcom_host_msg_bldr_encode_and_send_msg((uint8_t *) message, msgLength);
  // ret not used because error already reported 

  free(message);
  return OK;
}

//=====================================================================
// Because, usually, no receiver is consuming messages, they eventually will be
// blocked. To workaround this, once we get a -EAGAIN error (i.e. blocked) we'll
// attempt to send 0x00 before every message. This way when the CLI begins to consume
// messages again our 0x00 will be the first thing to arrive after whatever nuttx has
// buffered. This will cause the CLI to assume that this is an End of Messsage.
// Therefore, the message after the blockage is removed can be sent successfully and
// be properly parsed.
bool hcom_host_msg_bldr_is_send_blocked()
{
  if(_lastMessageWasBlocked)
  {
    int ret;
    f7syslog_x(LOG_DEBUG, "%s() - Attempting to send \0 to test host.\n", __func__);

    // Send a dummy single byte message that the host can ignore.
    ret = hcom_usb_acm_transmit_to_host((uint8_t *)"\0", 1);
    if(ret == -EAGAIN)
      return true;    // Still blocked

    _lastMessageWasBlocked = false;
  }

  return false;
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
  _encodedBuff[encodedSize] = HCOM_PROTOCOL_PACKET_TERMINATING_VALUE;

  ret = hcom_usb_acm_transmit_to_host(_encodedBuff, encodedSize + 1);
  if(ret < 0)
  {
    if(ret == -EAGAIN)
    {
      _lastMessageWasBlocked = true;
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
