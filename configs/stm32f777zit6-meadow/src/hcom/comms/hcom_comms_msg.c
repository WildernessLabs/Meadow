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

#include "../hcom_common.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;
static pid_t _creator_pid;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static void hcom_comms_build_msg_header(uint16_t requestType, uint16_t protocolCtrl,
        uint32_t userData, uint8_t *xmitBuffer);
static int hcom_comms_send_message(uint8_t * message, size_t messageLength);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_comms_msg_builder_setup()
{
  _creator_pid = getpid();
  return OK;
}

//--------------------------------------------------------------------
void hcom_comms_msg_builder_shutdown()
{
  _shutting_down = true;
}

//=====================================================================
// Just sends a header message
int hcom_comms_send_header_msg(uint16_t requestType, uint32_t userData)
{
  hcom_comms_send_simple_buffer_msg(requestType, 0, userData, NULL, 0);
  // ret not used because error already reported 
  return OK;
}

//=====================================================================
// Prepare a string for transmission
int hcom_comms_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText)
{
  // Need to remove any trailing cr/lf. If none found strcspn() finds terminating '\0'
  // returning its offset
  size_t trueDataLen = strcspn(shortText, "\r\n");

  int ret = hcom_comms_send_simple_buffer_msg(requestType, 0, userData, (uint8_t*) shortText, trueDataLen);
  return ret;
}

//=====================================================================
// Prepare a string for transmission and allow any character
int hcom_comms_send_raw_string_msg(uint16_t requestType, uint32_t userData, char *shortText, size_t msgLength)
{
  int ret = hcom_comms_send_simple_buffer_msg(requestType, 0, userData, (uint8_t*) shortText, msgLength);
  return ret;
}

//=====================================================================
// This will prepare and send a simple message, as an extention to the header
int hcom_comms_send_simple_buffer_msg(uint16_t requestType, uint16_t protocolCtrl,
       uint32_t userData, uint8_t *origMsg, size_t msgLen)
{
  int ret;

  if(hcom_comms_was_host_xmit_blocked())
  {
    // This is a normal occurance since the host is usually not connected
    return OK;   // Throw the message away. What else can be done?
  }
  
  int fullMsgLen = msgLen + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;

 // DEBUGASSERT(fullMsgLen <= HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  if(fullMsgLen > HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN)
  {
    // Truncate to fit
    // todo - is this a good idea?
    fullMsgLen = HCOM_PROTOCOL_PACKET_MAX_SIZE;
  }

  if(msgLen > 0)
  {
    // Unique buffer for each thread
    uint8_t *xmitBuffer = malloc(fullMsgLen);

    // Uses the first part of message buffer for header
    hcom_comms_build_msg_header(requestType, protocolCtrl, userData, xmitBuffer);
    // Copy the body of the message
    memcpy(xmitBuffer + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH, origMsg, fullMsgLen - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);

    // Send the message
    ret = hcom_comms_send_message(xmitBuffer, fullMsgLen);
    free(xmitBuffer);
  }
  else
  {
    DEBUGASSERT(msgLen == 0);
    uint8_t headerOnlyMsg[HCOM_PROTOCOL_REQUEST_HEADER_LENGTH];

    // Uses the first part of message buffer for header
    hcom_comms_build_msg_header(requestType, protocolCtrl, userData, headerOnlyMsg);

    // Send the message
    ret = hcom_comms_send_message(headerOnlyMsg, fullMsgLen);
  }

  return ret;
}

//=====================================================================
// Build the header
void hcom_comms_build_msg_header(uint16_t requestType,
        uint16_t protocolCtrl, uint32_t userData, uint8_t *xmitBuffer)
{
  // Populate the header
  struct HcomProtocolHeader_s *hdr = (struct HcomProtocolHeader_s *) xmitBuffer;

  hdr->seqNumber = HCOM_PROTOCOL_REQUEST_HEADER_SIMPLE_SEQ_NUMBER;
  hdr->version = HCOM_PROTOCOL_CURRENT_VERSION_NUMBER;
  hdr->control = protocolCtrl;
  hdr->rqstType = requestType;
  hdr->userData = userData;
}

//=====================================================================
// Send the completed message
int hcom_comms_send_message(uint8_t *message, size_t messageLength)
{
  int ret;

  ret = hcom_comms_transmit_to_host(message, messageLength);
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
