/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\hcom_nx\diag\hcom_nx_route_to_host.c
 *
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

// TODO - This is now the third way of sending a message to the host PC (CLI)
// from the kernel side of Nuttx. The others are hcom_nx_trace_msg_proc.c which
// sends syslog to host and every upd caller from HCOM carries a pointer to
// a function to send a text msg to host PC (struct hcom_nx_cmd_data).
// This should simple scheme could be the only scheme if we are willing to
// keep a pthread alive for this function. There would also be a reduction
// in duplicate code.
// At the time of this modules creation the effort and risk to consolidate
// all of these various schemes was not worth the effort.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_cirbuf.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/hcom_nuttx_shared.h>
#include "../hcom_nx_config_manager.h"

#include <nuttx/kthread.h>
#include <nuttx/kmalloc.h>

#include <sys/stat.h>
#include <ctype.h>
#include <poll.h>

#include <signal.h>

#include <arch/board/board.h>
#include "stm32_gpio.h"

// #define USE_MEADOW_DEBUG_HELPERS
// // #undef USE_MEADOW_DEBUG_HELPERS
// #include <meadow/meadow_debug_helpers.h>

// At present (Sept 2021) The only use for this feature is with ethernet
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t *_sharedMsgBuff;
static size_t _sharedMsgLen;
static uint16_t _sharedRqstType;

static sem_t _onlyOneSem;
static sem_t _readNxtSem;
static sem_t _sendCliSem;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_route_text_to_host_setup()
{  
  _sharedMsgBuff = (uint8_t *)malloc(HCOM_PROTOCOL_CURRENT_PACKET_MAX_SIZE - \
            HCOM_PROTOCOL_HEADER_MSG_LENGTH);
  if(_sharedMsgBuff == NULL)
  {
    return -ENOMEM;
  }
  
  sem_init(&_onlyOneSem, 0, 1);

  // These semaphores are needed for sending trace to CLI. Why? Because there
  // are 2 threads that must wait their turn in a ping-pong kind of way.
  sem_init(&_readNxtSem, 0, 0);
  sem_setprotocol(&_readNxtSem, SEM_PRIO_NONE);

  sem_init(&_sendCliSem, 0, 0);
  sem_setprotocol(&_sendCliSem, SEM_PRIO_NONE);

  return OK;
}

//===========================================================================
// Wait for the thread holding the semaphore to release it
static void hcom_nx_route_text_wait_sem(sem_t *semaphore)
{
  int ret;  

  do
  {
    ret = sem_wait(semaphore);    // Take the semaphore (perhaps waiting)
  }
  while (ret == -EINTR);
}

//=================================================================
// Ship the a generic text message to host
// Note: The text must end with a '\n' character.
int hcom_nx_route_text_to_host(uint16_t requestType, char *msgBuff,
          size_t msgLen)
{
  // Prevent multiple threads from stepping on each other
  hcom_nx_route_text_wait_sem(&_onlyOneSem);

  // Copy the message to the shared buffer
  memcpy(_sharedMsgBuff, msgBuff, msgLen);
  _sharedMsgLen = msgLen;
  _sharedRqstType = requestType;

  // Release pthread to return the message in the shared buffer to the host
  sem_post(&_sendCliSem);

  // Wait for message to be sent before returning to caller. since
  // the _sharedMsgBuff and _sharedMsgLen are shared by the callers
  // and the callers which is a kthread.
  hcom_nx_route_text_wait_sem(&_readNxtSem);

  sem_post(&_onlyOneSem);
  return OK;
}

//==========================================================================
// This is called via udp and provides a pthread, and the buffer for the
// message to host PC. On every call it waits for the next message.
// When a message is available it returns and calls the code to send
// the message to host PC and returns again.
size_t hcom_nx_text_to_host_transport(uint16_t *requestType,
          char *buff, size_t buffLen)
{
  static bool firstTime = true;
  size_t msgLength;

  // The first call must be ignored. Afterward we allow the reader to get the
  // to keep things in sync.
  if(firstTime)
  {
    firstTime = false;
  }
  else
  {
    // The previous message has been sent to the host. Therefore, release the
    // the allow the caller to wait for the next message.
    sem_post(&_readNxtSem);
  }

  // Now wait for the next message to arrive
  hcom_nx_route_text_wait_sem(&_sendCliSem);

  // Next message has arrived.
  // Truncate the message if too long for caller's buffer, otherwise use the
  // message's length.
  // Note: at this time the lengths of both buffers is
  // HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN which is 8180 bytes (29Jul2024)
  if(_sharedMsgLen > buffLen)
  {
    msgLength = buffLen;

    // Fixup the string in the buffer
    _sharedMsgBuff[buffLen - 1] = '\0';
    _sharedMsgBuff[buffLen - 2] = '\n';
  }
  else
  {
    msgLength = _sharedMsgLen;
  }

  *requestType = _sharedRqstType;

  // Note: Even though this code is in kernel land, the app side pthread must
  // be the one to copy the data into the app side buffer.
  memcpy(buff, _sharedMsgBuff, msgLength);

  // Return to userland with the message and its length
  return msgLength;
}

#else
int hcom_nx_route_text_to_host_setup()
{
  return OK;
}
#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
