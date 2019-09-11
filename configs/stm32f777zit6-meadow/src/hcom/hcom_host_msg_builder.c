/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_transmitter.c
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

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_msg_builder_setup()
{
  return OK;
}

//--------------------------------------------------------------------
void hcom_host_msg_builder_shutdown()
{
  _shutting_down = true;
}

//-----------------------------------------------------------------------
// Send text to host
int hcom_host_msg_bldr_send_text(FAR char xmitBuffer[], size_t xmitLength)
{
  static bool _lastMessageBlocked = false;
  int xmitReturn;

  // Note the xmitLength does not include the trailing '\0' since 
  // This requirement is temporary, until real message can be sent to host
  DEBUGASSERT(xmitBuffer[xmitLength] == '\0');

  // At this time this function is the only caller to hcom_usb_acm_transmit_to_host.
  // Because, usually, no receiver is consuming these messages, they eventually will
  // be blocked. To work around this, if we get a -EAGAIN error (i.e., blocked) we'll
  // try to send cr/lf before every message if last time a -EAGAIN was returned. This
  // way when the CLI is consuming messages our cr/lf will be the first thing to
  // arrive after, anything buffered, and allow the CLI to assume that this
  // is the EOM and this message can be sent successfully and properly parsed.
  if(_lastMessageBlocked)
  {
    f7syslog(LOG_DEBUG, "%s() - Attempting to send cr/lf to test host.\n", __func__);
    // Attempt to send cr/lf
    xmitReturn = hcom_usb_acm_transmit_to_host((uint8_t *)"\r\n", 2);
    if(xmitReturn == -EAGAIN)
      return xmitReturn;    // Still blocked
  }

  // Appending cr/lf to the end of every text messages
  char *tempBuff;
  tempBuff = malloc(xmitLength + 2);
  memcpy(tempBuff, xmitBuffer, xmitLength);
  tempBuff[xmitLength] = '\r';
  tempBuff[xmitLength + 1] = '\n';

  xmitReturn = hcom_usb_acm_transmit_to_host((uint8_t *)tempBuff, xmitLength + 2);
  _lastMessageBlocked = (xmitReturn == -EAGAIN);

  if(_lastMessageBlocked)
    f7syslog(LOG_DEBUG, "%s() - The last message was blocked.\n", __func__);

  return xmitReturn;
}
