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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

#include <meadow/hcom_protocol.h>
#include <meadow/hcom_bbreg_defn.h>
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_TRACE_RAMLOG_ASSUME_LARGEST_SYSLOG (384)
#define HCOM_TRACE_RAMLOG_READ_BUF_SIZE (256)
#define HCOM_TRACE_LOCAL_SYSLOG_CIR_BUF_SIZE (HCOM_TRACE_RAMLOG_READ_BUF_SIZE * 5)
#define HCOM_TRACE_RAMLOG_SERIAL_NAME ("/dev/ttyS0")    // UART 1
#define HCOM_TRACE_RAMLOG_RECONFIG_TIMEOUT (30)   // Seconds to reconfigure
#define HCOM_TRACE_SHARED_SYSLOG_CIR_BUF_SIZE HCOM_TRACE_LOCAL_SYSLOG_CIR_BUF_SIZE

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

// static char *thisFile = __FILE__;

static uint8_t *_sharedMsgBuff;
static size_t _sharedMsgLen;
static uint16_t _sharedRqstType;

static sem_t _readNxtSem;
static sem_t _sendCliSem;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int hcom_nx_route_to_host_route_trace_text(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_route_to_host_setup()
{  
  // These semaphores are needed for sending trace to CLI
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
// Ship the ramlog text message to uart and/or host CLI
// If this function cannot send it, the message is lost....
int hcom_nx_route_text_to_host(uint16_t requestType, char *msgBuff,
          size_t msgLen)
{
  // Route to Host
  // Copy the message to the shared buffer
  memcpy(_sharedMsgBuff, msgBuff, msgLen);
  _sharedMsgLen = msgLen;
  _sharedRqstType = requestType;

  // Release pthread to return the message in the shared buffer to the host
  sem_post(&_sendCliSem);

  // Wait for message to be sent before returning to caller. Why? Because
  // the _sharedMsgBuff and _sharedMsgLen are shared by the caller and the
  // caller which is a kthread.
  hcom_nx_route_text_wait_sem(&_readNxtSem);
}

//==========================================================================
// This is called via udp and provides a pthread, and the buffer for the
// message to CLI. On every call it waits for the next message.
// When a message is found it returns and calls the code to send
// the message to CLI and returns again.
size_t hcom_nx_to_host_cli_message_transport(uint16_t *requestType,
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
    // The caller of this function has entered, indicating that the message
    // has been sent to the host. Therefore, allow the caller to get the
    // next message
    sem_post(&_readNxtSem);
  }

  // Now we wait for the next message to arrive
  hcom_nx_route_text_wait_sem(&_sendCliSem);

  // Truncate the message if too long for caller's buffer
  if(_sharedMsgLen > buffLen)
    msgLength = buffLen;
  else
    msgLength = _sharedMsgLen;

  *requestType = _sharedRqstType;

  // Currently, the 2 threads run in a ping-pong fashion, only one can run
  // at a time. With some effort, once the data has been copied to the userland
  // buffer, the ramlog reader could be allowed to read the next message.
  // But, at this time the effort doesn't seem to be worth the benefit.
  // Note: the pthread must be the one to copy the data into buff.
  memcpy(buff, _sharedMsgBuff, msgLength);

  // Return to userland with the message and its length
  return msgLength;
}
