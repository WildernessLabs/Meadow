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
static sem_t _waitSendSem;    /* Implements event waiting */

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int hcom_host_msg_bldr_send_build_header(uint16_t requestType, uint16_t ctrlData,
        uint32_t userData, uint8_t *msgBuff);
static int hcom_host_msg_bldr_send_completed_msg(uint8_t * message, size_t messageLength);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_msg_builder_setup()
{
  _lastMessageWasBlocked = false;
  _encodedBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);
  nxsem_init(&_waitSendSem, 0, 1);

  return OK;
}

//--------------------------------------------------------------------
void hcom_host_msg_builder_shutdown()
{
  free(_encodedBuff);
  nxsem_destroy(&_waitSendSem);

  _shutting_down = true;
}

//===================================================================================
// Wait for the thread writing to exit
static void hcom_send_msg_takesem(void)
{
  int ret;

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = nxsem_wait(&_waitSendSem);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//=====================================================================
int hcom_host_msg_bldr_send_short_text_msg(uint16_t ctrlData, uint32_t userData, char *shortText)
{
  uint8_t *message = malloc(HCOM_PACKET_MAX_SIZE);
  int ret;
  int textLength = strlen(shortText);
  int msgLength = textLength + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;
  DEBUGASSERT(msgLength <= HCOM_PACKET_MAX_SIZE);
  
  // Uses the first part of message for header
  ret = hcom_host_msg_bldr_send_build_header(HCOM_HOST_REQUEST_SIMPLE_TEXT_MESSAGE,
      ctrlData, userData, message);
  if(ret < 0)
  {
    free(message);
    return ret;
  }

  memcpy(message + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH, shortText, textLength);
  ret = hcom_host_msg_bldr_send_completed_msg((uint8_t *) message, msgLength);
  
  free(message);
  return ret;
}

//=====================================================================
// Just sends a header message
int hcom_host_msg_bldr_send_information_msg(uint16_t ctrlData, uint32_t userData)
{
  int ret;
  struct HcomProtocolHeader_s hcomHdr;

  ret = hcom_host_msg_bldr_send_build_header(HCOM_HOST_REQUEST_SIMPLE_MESSAGE,
          ctrlData, userData, (uint8_t *)&hcomHdr);
  if(ret < 0)
  {
    return ret;
  }

  ret = hcom_host_msg_bldr_send_completed_msg((uint8_t *) &hcomHdr, HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);
  if(ret < 0)
  {
    return ret;
  }
  return OK;
}

//=====================================================================
// Build the header
int hcom_host_msg_bldr_send_build_header(uint16_t requestType,
        uint16_t ctrlData, uint32_t userData, uint8_t *message)
{
  int xmitReturn;

  hcom_send_msg_takesem();

  // Because, usually, no receiver is consuming these messages, they eventually will
  // be blocked. To workaround this, once we get a -EAGAIN error (i.e. blocked) we'll
  // attempt to send 0x00 before every message. This way when the CLI begins to consume
  // messages again our 0x00 will be the first thing to arrive after whatever nuttx has
  // buffered. This will cause the CLI to assume that this is an End of Messsage.
  // Therefore, the message after the blockage is removed can be sent successfully and
  // be properly parsed.
  if(_lastMessageWasBlocked)
  {
    f7syslog(LOG_DEBUG, "%s() - Attempting to send \0 to test host.\n", __func__);

    // Send a dummy single byte message that the host can ignore.
    xmitReturn = hcom_usb_acm_transmit_to_host((uint8_t *)"\0", 1);
    if(xmitReturn == -EAGAIN)
      return xmitReturn;    // Still blocked
  }

  _lastMessageWasBlocked = false;

  // Populate the header
  struct HcomProtocolHeader_s *hdr = (struct HcomProtocolHeader_s *) message;

  // populate the header
  hdr->seqNumber = HCOM_PROTOCOL_REQUEST_HEADER_SEQ_NUMBER;
  hdr->version = HCOM_PROTOCOL_CURRENT_VERSION_NUMBER;
  hdr->control = ctrlData;
  hdr->rqstType = requestType;
  hdr->userData = userData;

  return OK;
}

//=====================================================================
// Send the completed message
int hcom_host_msg_bldr_send_completed_msg(uint8_t * message, size_t messageLength)
{
  int ret;

  syslog(0, "-----Message before encoding-----\n");
  hcom_diag_print_buffer(message, messageLength, 0);

  // Encode
  size_t encodedSize = hcom_com_support_cobs_encoder(message, 0, messageLength, _encodedBuff);

  // Encoded message needs a terminating delimiter for COBS
  DEBUGASSERT(encodedSize < HCOM_SAFE_PACKET_BUF_SIZE - 1);
  _encodedBuff[encodedSize] = HCOM_PROTOCOL_PACKET_TERMINATING_VALUE;

  ret = hcom_usb_acm_transmit_to_host(_encodedBuff, encodedSize + 1);
  _lastMessageWasBlocked = (ret == -EAGAIN);

  if(_lastMessageWasBlocked)
    f7syslog(LOG_INFO, "%s() - The last message was blocked.\n", __func__);

  nxsem_post(&_waitSendSem);
  return ret;
}
