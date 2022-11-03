/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_enq_deq.c
 * 
 *   Copyright (C) 2019 - 2022 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static sem_t _lockCirBufSem;
static sem_t _runProcSem;
static sem_t _runRecvSem;
static bool _FBFlag;      // State protected by _lockCirBufSem
static host_com_cir_buffer_t *_hcom_cbuf;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

int hcom_host_enq_deq_wait_for_work(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_host_enq_deq_setup()
{
  _shutting_down = false;

  // Protects circular buffer
  sem_init(&_lockCirBufSem, 0, 1);
  sem_setprotocol(&_lockCirBufSem, SEM_PRIO_NONE);
  
  // Notifies proc thread a message has been queued
  sem_init(&_runProcSem, 0, 0);
  sem_setprotocol(&_runProcSem, SEM_PRIO_NONE);

  // Notifies recv that it should retry to add past message to circular buffer
  sem_init(&_runRecvSem, 0, 0);
  sem_setprotocol(&_runRecvSem, SEM_PRIO_NONE);

  _hcom_cbuf = (host_com_cir_buffer_t *)malloc(sizeof(host_com_cir_buffer_t));

  if (_hcom_cbuf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-One of 2 allocations failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE,
          HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE);
  if (result == HCOM_CIR_BUF_ALLOC_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Buffer allocation failed\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
}

//====================================================================
//
void hcom_host_enq_deq_shutdown()
{
  _shutting_down = true;

  hcom_cirbuf_release_memory(_hcom_cbuf);
  free(_hcom_cbuf);

 sem_destroy(&_lockCirBufSem);
 sem_destroy(&_runProcSem);
 sem_destroy(&_runRecvSem);
}

//====================================================================
// Called when watchdog is cleaning up after download stopped before completion
bool hcom_host_enq_deq_clear_buffer()
{
  bool returnVal;

  sem_wait(&_lockCirBufSem);
  returnVal = (hcom_cirbuf_clear_buffer(_hcom_cbuf) == HCOM_CIR_BUF_INIT_OK);
  sem_post(&_lockCirBufSem);

  return returnVal;
}

// //====================================================================
// // Needing more information about why download would stop created this code
// // to show the state of the semaphores when the file download timer expired.
// // Visual Studio is running HcomDiagUi and reports, "The semaphore timeout
// // has expired"
// void hcom_host_enq_deq_dbg_info()
// {
//   int value_lockCirBufSem;
//   int value_runProcSem;
//   int value_runRecvSem;

//   sem_getvalue(&_lockCirBufSem, &value_lockCirBufSem);
//   sem_getvalue(&_runProcSem, &value_runProcSem);
//   sem_getvalue(&_runRecvSem, &value_runRecvSem);

//   syslog(2, "Error State: lockCirBufSem:%d, _runProcSem:%d, _runRecvSem:%d\n",
//             value_lockCirBufSem, value_runProcSem, value_runRecvSem);
// }

//=======================================================================
// Add the received data is put into the circular buffer. It is added as a
// stream.
int hcom_host_enq_deq_enqueue_rcvd_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;
  
  if (recvByteCnt == 0)
    return OK;

  do
  {
    // Gain exclusive access to circular buffer
    sem_wait(&_lockCirBufSem);

    // Only possible return values: HCOM_CIR_BUF_ADD_SUCCESS,
    // HCOM_CIR_BUF_ADD_WONT_FIT and HCOM_CIR_BUF_ADD_BAD_ARG
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    switch(result)
    {
      case HCOM_CIR_BUF_ADD_SUCCESS:
        sem_post(&_runProcSem);         // Notify proc of message
        sem_post(&_lockCirBufSem);      // Release lock on buffer
        return OK;                      // Return to read more data

      case HCOM_CIR_BUF_ADD_WONT_FIT:
        _FBFlag = true;                 // Set Full Buffer Flag then free cir buff
        sem_post(&_runProcSem);         // Notify proc to read messages
        sem_post(&_lockCirBufSem);      // Release lock on buffer

        // Read thread waits here for space in buffer
        sem_wait(&_runRecvSem);         // Wait for a message to be removed
        continue;                       // Try again to add message

      case HCOM_CIR_BUF_ADD_BAD_ARG:
        // Report error and return. The message is lost.
        sem_post(&_lockCirBufSem);      // Release lock on buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
        return OK;

      default:
        sem_post(&_lockCirBufSem);      // Release lock on buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown return from hcom_cirbuf_add_bytes():%d\n",
                    thisFile, __LINE__, result);
        break;
    }
  } while (! _shutting_down);

  return OK;
}

//====================================================================
// TODO: during sem_wait a signal will wake up this thread
// Proc calls here to get the next message
int hcom_host_enq_deq_dequeue_packet(uint8_t *packet_dest_buf,
          size_t *packetLength)
{
  int result;

  do
  {
    // Gain exclusive access to circular buffer
    sem_wait(&_lockCirBufSem);

    // Only HCOM_CIR_BUF_GET_FOUND_MSG, HCOM_CIR_BUF_GET_NONE_FOUND and
    // HCOM_CIR_BUF_GET_DELETED_TOO_BIG can be returned
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, packet_dest_buf,
              HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE, packetLength);
    switch(result)
    {
      case HCOM_CIR_BUF_GET_FOUND_MSG:
        if(_FBFlag)
        {
          _FBFlag = false;              // Full buffer flag did it's work
          sem_post(&_runRecvSem);       // Allow recv to retry to add
        }
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        return OK;                      // Return to process message

      case HCOM_CIR_BUF_GET_NONE_FOUND:
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
      
        // Thread waits to be notified that a message may be available. This
        // is also where the watchdog notification is detected.
        hcom_host_enq_deq_wait_for_work();
        break;                          // Loop again to check for new message

      case HCOM_CIR_BUF_GET_DELETED_TOO_BIG:
        // The message was too big and the circular buffer, the bogas message
        // has removed. So, report error and return.
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Message too big, deleted, size:%d\n",
                  thisFile, __LINE__, packetLength);
        break;                          // Try again to get the next message

      default:
        sem_post(&_lockCirBufSem);      // Release lock on circular buffer
        hcom_logging_syslog(LOG_ERR, "%s@%d-Unknown return from hcom_cirbuf_get_next_packet():%d\n",
                  thisFile, __LINE__, result);
        break;
    }
  } while (! _shutting_down);
  
  return OK;
}

//==========================================================
// Wait to be told that there is more data to be processed in the cir buf
// or that the watchdog timer has timedout and we must take action.
int hcom_host_enq_deq_wait_for_work()
{
  while (sem_wait(&_runProcSem) < 0)
  {
    int errcode = errno;
    if (errcode != EINTR)
    {
      continue;   // Ignore error
    }
    else
    {
      // Check if watchdog expired and if it did, update HCOM's state
      hcom_host_watchdog_check_execute_if_expired();
    }
  }

  return OK;
}
