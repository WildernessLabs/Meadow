/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_trace_to_cli.c
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
#include <meadow/hcom_bbreg_defn.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static bool _trace_log_to_host;
static bool _thread_running;
static pthread_t _cli_pthread;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_trace_to_cli_transport_pthread(FAR void *arg);
static void hcom_trace_to_cli_create_thread(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called during startup
int hcom_trace_to_cli_setup()
{
  _thread_running = false;

  // Check if the battery backed register indicates that the current
  // user wants syslog messages routed to CLI
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT))
  {
    _trace_log_to_host = true;
    hcom_trace_to_cli_create_thread();
  }
  else
  {
    _trace_log_to_host = false;
  }

  return OK;
}

//================================================================
// Create a thread that can be used to transport the syslog message
// from k-land to here in userland.
// This thread calls via hcom_via_nx_access into kernelland and lives
// there until the a message is ready to be sent to the CLI.
// Note: This can be called during startup or from a CLI command
void hcom_trace_to_cli_create_thread()
{
  int ret;
  pthread_attr_t attr;
  struct sched_param param;

  if(_thread_running)
    return;

  param.sched_priority = HCOM_THREAD_PRIORITY_CLI_TRANSPORT;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);
  (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_CLI_TRANSPORT);

  ret = pthread_create(&_cli_pthread, &attr, hcom_trace_to_cli_transport_pthread, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-create thread %s, ret:%d, errno:%d\n",
              __FILE__, __LINE__, HCOM_THREAD_NAME_CLI_TRANSPORT, ret, errno);
  }
  return;
}

//================================================================
// This thread is only needed when we are requested to send messages to CLI.
FAR void *hcom_trace_to_cli_transport_pthread(FAR void *arg)
{
#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_CLI_TRANSPORT);
#endif

  size_t stringLen;
  char *cliMsgBuf = (char *)malloc(HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
  if(cliMsgBuf == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", __FILE__, __LINE__);
    return NULL;
  }

  _thread_running = true;

  // Stay in this loop until told to stop
  while(_trace_log_to_host)
  {
    // Call into kernelland to get the next CLI syslog message. This thread will
    // wait in kernelland until the next message or terminated, at either point
    // it will return.
    stringLen = hcom_via_nx_provide_cli_trace_transport(cliMsgBuf,
              HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
    if(stringLen <= 0 || cliMsgBuf == NULL)
    {
      if(!_trace_log_to_host)
        break;

      // In case something goes wrong we don't want to be in a tight loop
      usleep(10 * 1000);
      continue;
    }

    // Forward to CLI and wait again for next message
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0,
           cliMsgBuf, __FILE__, __LINE__);
  }

  free(cliMsgBuf);

  // pthread dies here
  _thread_running = false;
  return NULL;
}

//================================================================
// Call by a CLI command that first calls into kernelland then makes
// this call. It starts the thread that transports the syslog message
// from k-land to userland.
// Note: The battery backed register setting is updated in k-land
void hcom_trace_to_cli_enable_command(uint32_t userData)
{
  _trace_log_to_host = true;
  hcom_trace_to_cli_create_thread();
}

//================================================================
// The following 2 functions are called by the same command from CLI that
// stops syslog messages going to the CLI.
// Here is the order:
// 1. This function is called via CLI command to prepare for the thread to return.
// 2. The k-land code is called via CLI and will cause our pthread to return.
// 3. hcom_trace_to_cli_disable_cleanup() is called last to wait for the pthread
//  to terminate
// Note: The battery backed register setting is updated in kernelland
void hcom_trace_to_cli_disable_command(uint32_t userData)
{
  // Prepare our pthread to exit when k-land sends it back
  _trace_log_to_host = false;
}

//================================================================
// Call by a command from CLI. It will stop the thread that transports
// messages to/from kernelland.
void hcom_trace_to_cli_disable_cleanup(uint32_t userData)
{
  // Wait for our pthread to exit
  pthread_addr_t exitVal;
  pthread_join(_cli_pthread, &exitVal);
}
