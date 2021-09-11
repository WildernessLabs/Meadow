/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_host_text_transport.c
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

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

#if defined(HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD)
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static pthread_t _host_text_pthread;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_host_text_transport_pthread(FAR void *arg);
static void hcom_host_text_transport_create_thread(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called during startup
int hcom_host_text_transport_setup()
{
  hcom_host_text_transport_create_thread();

  return OK;
}

//================================================================
// Create a thread that can be used to transport the syslog message
// from k-land to here in userland.
// This thread calls via hcom_via_nx_access into kernelland and lives
// there until the a message is ready to be sent to the host text.
void hcom_host_text_transport_create_thread()
{
  int ret;
  pthread_attr_t attr;
  struct sched_param param;

  param.sched_priority = HCOM_THREAD_PRIORITY_HOST_TRANSPORT;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_HOST_TRANSPORT);

  ret = pthread_create(&_host_text_pthread, &attr, hcom_host_text_transport_pthread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
              __FILE__, __LINE__, HCOM_THREAD_NAME_HOST_TRANSPORT, ret, errno);
  }
  return;
}

//================================================================
FAR void *hcom_host_text_transport_pthread(FAR void *arg)
{
#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_HOST_TRANSPORT);
#endif

  size_t stringLen;
  uint16_t requestType;
  
  char *hostTextMsgBuf = (char *)malloc(HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
  if(hostTextMsgBuf == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", __FILE__, __LINE__);
    return NULL;
  }

  // Stay in this loop
  while(true)
  {
    // Call into kernelland to get the next host text message. This thread will
    // wait in kernelland until the next message or terminated, at either point
    // it will return.
    stringLen = hcom_via_nx_provide_host_text_transport(&requestType,
          hostTextMsgBuf, HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);

    if(stringLen <= 0 || hostTextMsgBuf == NULL)
    {
      // In case something goes wrong we don't want to be in a tight loop
      usleep(10 * 1000);
      continue;
    }

    // Forward to host_text and wait again for next message
    hcom_host_send_simple_string_msg(requestType, 0,
           hostTextMsgBuf, __FILE__, __LINE__);
  }

  free(hostTextMsgBuf);

  // pthread dies here
  return NULL;
}

#else

int hcom_host_text_transport_setup()
{
  return OK;
}

#endif // #if defined(HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD)
