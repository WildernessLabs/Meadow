/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/comms/hcom_nx_host_send.c
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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

// This module is a proxy for \apps\examples\hcom\comms\hcom_host_send.c. It
// makes it possible for Meadow nuttx code to send messages directly to the
// CLI.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static send_host_std_msg_data _hostCallback;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_host_send_setup()
{
  _hostCallback = NULL;

  return OK;
}

//==========================================================================
// When setting up, the app side code in /apps/examples/hcom/comms/hcom_host_send.c
// will call this function via hcom_nx_udp() to establish the callback to use
// to route the messages to host.
int hcom_nx_host_send_set_send_callback(send_host_std_msg_data hostCallback)
{
  _hostCallback = hostCallback;

  return OK;
}


//==========================================================================
// Routes message to apps side for sending to host (e.g. CLI).
// /apps/examples/hcom/comms/hcom_host_send.c/hcom_host_send_std_msg_data()
// for transmission to the Host application (e.g. CLI).
int hcom_nx_host_send_std_msg_data(HcomProtoHdrMsg_t *hdrMsg,
          size_t totalMsgLen, char *sourceFileName, int sourceLineNumber)
{
  int ret = OK;

  if(_hostCallback == NULL)
  {
    return -ENODEV; // No Device
  }
  
  ret = _hostCallback(hdrMsg, totalMsgLen, sourceFileName, sourceLineNumber);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Host send callback returned:%d\n", thisFile, __LINE__, ret);
  }
  return ret;
}
