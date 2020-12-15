/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_ramlog_trace.c
 *
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/meadow_cirbuf.h>

#include <sys/stat.h>
#include <ctype.h>
#include <poll.h>

#if defined (CONFIG_RAMLOG_SYSLOG)
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_TRACE_RAMLOG_READ_BUFF_SIZE 256
#define HCOM_TRACE_CIRCULAR_BUFFER_SIZE (HCOM_TRACE_RAMLOG_READ_BUFF_SIZE * 5)
#define HCOM_TRACE_RAMLOG_SERIAL_NAME ("/dev/ttyS0")    // UART 1

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
#endif

static char *thisFile = __FILE__;

// This covers most of this file
#if defined (CONFIG_RAMLOG_SYSLOG)

static bool _shutting_down;
static struct host_com_cir_buffer_s *_ramlog_cbuf;
static uint8_t *_singleMsgBuf;
static int _ramlog_fd;
static int _uart1_fd;
static bool _trace_ramlog_initialized;
static bool _trace_ramlog_to_host;
static bool _trace_ramlog_to_uart1;
static bool _mono_has_started;
static int _uart1_needs_reconfig;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static FAR void *hcom_diag_trace_ramlog_pthread(FAR void *arg);

static int hcom_diag_trace_ramlog_make_thread(void);
static int hcom_diag_trace_ramlog_open_ramlog(void);
static int hcom_diag_trace_ramlog_lazy_initialization(bool startup);
static void hcom_diag_trace_ramlog_close_and_delay(bool ramLogClose);
static int hcom_diag_trace_ramlog_read_ramlog_loop(void);
static int hcom_diag_trace_ramlog_save_recvd_data(uint8_t recvBuff[], const ssize_t recvByteCnt);
static int hcom_diag_trace_ramlog_pull_all_packets_from_buffer(void);
static int hcom_diag_trace_ramlog_route_trace_text(uint8_t *buffer, int readReturn);
static void hcom_diag_trace_ramlog_err_logger(int priority, FAR const IPTR char *fmt, ...);
static int hcom_diag_trace_ramlog_send_msg_to_uart1(char *sendBuff, size_t numbBytes);
static int hcom_diag_trace_ramlog_open_uart1_serial_port(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_diag_trace_ramlog_setup()
{
  _shutting_down = false;
  _ramlog_fd = -1;
  _uart1_fd = -1;
  _trace_ramlog_initialized = false;
  _mono_has_started = false;
  _uart1_needs_reconfig = 0;

  // Do we need this now?
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT))
    _trace_ramlog_to_uart1 = true;
  else
    _trace_ramlog_to_uart1 = false;

  if(hcom_via_nx_ini_cfg_get_match(NULL, MEADOW_INI_CFG_STARTUP_SECTION,
                    MEADOW_INI_CFG_DIAG_UART_KEY, MEADOW_INI_CFG_DIAG_UART_USE))
  {
    _trace_ramlog_to_uart1 = true;
  }

#if HCOM_FORCE_SYSLOG_MASK_F7_AND_UART1 > 0
  _trace_ramlog_to_uart1 = true;
#endif

  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT))
    _trace_ramlog_to_host = true;
  else
    _trace_ramlog_to_host = false;
    
  // If either enabled initialize
  if(_trace_ramlog_to_uart1 || _trace_ramlog_to_host)
  {
    // The only way to undo this initialization is to restart
    hcom_diag_trace_ramlog_lazy_initialization(true);
  }
  else
  {
    hcom_startup_mgr_release_sem();        // Release semaphore
  }

  return OK;
}

//==========================================================================
// Closing connection forces a receive error which, causes the thread to return.
void hcom_diag_trace_ramlog_shutdown()
{
  _shutting_down = true;

  if(_ramlog_fd > -1)
  {
    close(_ramlog_fd);
    _ramlog_fd = -1;
  }

  if(_uart1_fd > -1)
  {
    close(_uart1_fd);
    _uart1_fd = -1;
  }

  if(_singleMsgBuf != NULL)
    free(_singleMsgBuf);

  if(_ramlog_cbuf != NULL)
  {
    hcom_cirbuf_release_memory(_ramlog_cbuf);
    free(_ramlog_cbuf);
  }
}

//==========================================================================
// Called when mono starts
void hcom_diag_trace_ramlog_mono_started()
{
  // Because we cannot know when mono has messed with UART1's configuration
  // we'll do it for the first few trace messages after mono has started.
  _mono_has_started = true;
  if(_trace_ramlog_to_uart1)
    _uart1_needs_reconfig = 5;
}

//==========================================================================
// Because this feature is rarely used we initialize only when needed
int hcom_diag_trace_ramlog_lazy_initialization(bool startup)
{
  int ret;

  // Maybe called at start or from CLI command
  if(_trace_ramlog_initialized)
  {
    if(startup)
      hcom_startup_mgr_release_sem();
    return OK;
  }

  // Create a circular buffer to manage messages read from ramlog
  _ramlog_cbuf = (struct host_com_cir_buffer_s *)malloc(sizeof(struct host_com_cir_buffer_s));
  if (_ramlog_cbuf == NULL)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-cir buf alloc\n", thisFile, __LINE__);
    if(startup)
      hcom_startup_mgr_release_sem();
    return -1;
  }

  // Initialize the circular buffer
  int result = hcom_cirbuf_init(_ramlog_cbuf,
          HCOM_TRACE_CIRCULAR_BUFFER_SIZE, 0x0a);
  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-hcom_cirbuf_init\n", thisFile, __LINE__);
    if(startup)
      hcom_startup_mgr_release_sem();
    return -1;
  }

  // Host message buffer
  _singleMsgBuf = malloc(HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);
  if (_singleMsgBuf == NULL)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-cir buf alloc\n", thisFile, __LINE__);
    if(startup)
      hcom_startup_mgr_release_sem();
    return -1;
  }

  // Create a thread to read the ramlog
  ret = hcom_diag_trace_ramlog_make_thread();
  if (ret < 0)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-thread create, errno:%d\n",
              thisFile, __LINE__, errno);
    if(startup)
      hcom_startup_mgr_release_sem();
    return -1;
  }

  _trace_ramlog_initialized = true;
  return OK;
}

//=============================================================
int hcom_diag_trace_ramlog_make_thread()
{
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = HCOM_THREAD_PRIORITY_TRACE_RAMLOG;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_REMOTE_DBG);

    ret = pthread_create(&thread, &attr, hcom_diag_trace_ramlog_pthread, NULL);
    if (ret < 0)
    {
      hcom_diag_trace_ramlog_err_logger(LOG_CRIT, "%s@%d-Thread create %s error:%d\n",
                thisFile, __LINE__, HCOM_THREAD_NAME_TRACE_RAMLOG, ret);
      return ret;
    }

  return OK;
}

//=================================================================
// This thread reads all ramlog messages received via syslog from
// the ramlog buffer
FAR void *hcom_diag_trace_ramlog_pthread(FAR void *arg)
{
  int ret;
  
#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_TRACE_RAMLOG);
#endif

  // Release startup thread if started by it (at least one of these
  // flags will be set if startup)
  if(_trace_ramlog_to_uart1 || _trace_ramlog_to_host)
    hcom_startup_mgr_release_sem();

  // Never leave this loop until shutdown
  while(!_shutting_down)
  {
    // Must have ramlog, it's the source of all trace data
    ret = hcom_diag_trace_ramlog_open_ramlog();
    if(ret < 0)
    {
      hcom_diag_trace_ramlog_close_and_delay(false);
      continue;
    }

    // Open the uart if it's required
    if(_trace_ramlog_to_uart1)
    {
      ret = hcom_diag_trace_ramlog_open_uart1_serial_port();
      if(ret < 0)
      {
        hcom_diag_trace_ramlog_close_and_delay(false);
        continue;
      }
    }

    // Attempt to read from the ramlog buffer
    // Stay in this loop unless something goes wrong
    ret = hcom_diag_trace_ramlog_read_ramlog_loop();
    if(ret < 0)
    {
      hcom_diag_trace_ramlog_close_and_delay(true);
    }
  }

  return NULL;
}

//=================================================================
void hcom_diag_trace_ramlog_close_and_delay(bool ramLogClose)
{
  if(ramLogClose)
  {
    close(_ramlog_fd);
    _ramlog_fd = -1;
  }

  if(_uart1_fd > -1)
  {
    close(_uart1_fd);
    _uart1_fd = -1;
  }

  // Wait and try again
  if(!_shutting_down)
    usleep(100 * 1000);   // Not a special value, just prevent hard infinite looping
}

//=================================================================
int hcom_diag_trace_ramlog_open_ramlog()
{
  if(_ramlog_fd > -1)
  {
    close(_ramlog_fd);
    _ramlog_fd = -1;
  }

  _ramlog_fd = open(HCOM_TRACE_RAMLOG_DEVICE_NAME, O_RDONLY);
  if (_ramlog_fd < 0)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-open %s, errno:%d\n",
            thisFile, __LINE__, HCOM_TRACE_RAMLOG_DEVICE_NAME, errno);
    return -1;
  }

  return OK;
}

//=================================================================
int hcom_diag_trace_ramlog_open_uart1_serial_port()
{
  if(_uart1_fd > -1)
  {
    close(_uart1_fd);
    _uart1_fd = -1;
  }

  _uart1_fd = open(HCOM_TRACE_RAMLOG_SERIAL_NAME, O_WRONLY);
  if (_uart1_fd < 0)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-open %s, errno:%d\n",
            thisFile, __LINE__, HCOM_TRACE_RAMLOG_SERIAL_NAME, errno);
    _uart1_fd = -1;
    return -1;
  }
  return OK;
}

//=================================================================
// This function reads the data put into the ramlog.
int hcom_diag_trace_ramlog_read_ramlog_loop()
{
  uint8_t buffer[HCOM_TRACE_RAMLOG_READ_BUFF_SIZE];
  ssize_t readReturn;
  int ret;

  while(!_shutting_down)
  {
    // Read ramlog
    readReturn = read(_ramlog_fd, buffer, HCOM_TRACE_RAMLOG_READ_BUFF_SIZE);
    if (readReturn < 0 )
    {
      // Error
      hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d ramlog read readReturn:%d errno:%d\n",
              thisFile, __LINE__, readReturn, errno);
      return -errno;    // Close connection, wait and try again
    }
    else if (readReturn == 0)
    {
      // EOF
      hcom_diag_trace_ramlog_err_logger(LOG_WARNING, "%s@%d ramlog read EOF read returned:%d errno:%d\n",
              thisFile, __LINE__, readReturn, errno);
      return -1;
    }
    else
    {
      // Successful ramlog message read. Put message into circular buffer
      // unless logging has been turned off
      if(_trace_ramlog_to_host || _trace_ramlog_to_uart1)
      {
        ret = hcom_diag_trace_ramlog_save_recvd_data(buffer, readReturn);
        if (ret < 0 )
        {
          return ret;
        }
      }
    }
  }   // while (!_shutting_down)

  return OK;
}

//=======================================================================
// Add the received data to the circular buffer. It can be added byte by byte
// or several bytes at once.
int hcom_diag_trace_ramlog_save_recvd_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add received data to the buffer until no more will fit
  for (;;)
  {
    result = hcom_cirbuf_add_bytes(_ramlog_cbuf, recvBuff, recvByteCnt);
    if(result == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      break;
    }
    else if (result == HCOM_CIR_BUF_ADD_WONT_FIT)
    {

      // Wasn't possible to put these bytes in the buffer, it's full. We need to
      // pull packets and retry to add this data
      hcom_diag_trace_ramlog_err_logger(LOG_WARNING, "%s@%d-No room in cir buf, pull and retry\n",
              thisFile, __LINE__);

      // Pull packets
      result = hcom_diag_trace_ramlog_pull_all_packets_from_buffer();
      if (result == HCOM_CIR_BUF_GET_FOUND_MSG)
      {
        // Pull and processed some so attempt to add again
        continue;
      }
      
      if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
      {
        // This looks like a buffer full of garbage and not delimiter
        hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-pull packets from cir buf, none found\n",
                 thisFile, __LINE__);
        
        return OK;    // Report and throw data away.
      }

      if (result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        // The buffer we supplied is too small for the message found
        hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-pull packets from cir buf, no room\n",
                 thisFile, __LINE__, result);

        return OK;    // Report and throw data away.
      }
    }
    else if (result == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // A bad argument is never expected
      hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
      return OK; // Report and throw data away and keep going
    }
    else
    {
      // Undefined error????
      hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-Unknown cir buf add err:%d\n", thisFile, __LINE__, result);
      return OK; // Report and throw data away and keep going
    }
  }

  // See if there's 1 or more complete message(s)
  result = hcom_diag_trace_ramlog_pull_all_packets_from_buffer();
  return result;
}

//====================================================================
// Pull and process all the complete packets from the circular buffer
int hcom_diag_trace_ramlog_pull_all_packets_from_buffer()
{
  int result;
  size_t packetLength;

  for (;;)
  {
    // If buffer too small for the found message packetLength will contain the desired
    // size. We've sized the buffer large enough that this should never happen.
    result = hcom_cirbuf_get_next_packet(_ramlog_cbuf, _singleMsgBuf,
            HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN, &packetLength);

    if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
      return OK; // Return to receive more data - only way out

    DEBUGASSERT(result != HCOM_CIR_BUF_GET_DEST_NO_ROOM);
    DEBUGASSERT(result == HCOM_CIR_BUF_GET_FOUND_MSG);

    // Process the isolated message
    result = hcom_diag_trace_ramlog_route_trace_text(_singleMsgBuf, packetLength);
    if (result == OK)
    {
      continue; // pull next packet
    }
    else if (result == -EAGAIN)
    {
      return OK;    // transmission blocked
    }
    else
    {
      hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
      return result;
    }
  }
  return OK;
}

//=================================================================
// Ship the ramlog text directly to the host PC
int hcom_diag_trace_ramlog_route_trace_text(uint8_t *recvBuff, int numbBytes)
{
  if(_trace_ramlog_to_uart1)
  {
    // Route to UART1
    hcom_diag_trace_ramlog_send_msg_to_uart1((char *) recvBuff, numbBytes);
  }

  if(_trace_ramlog_to_host)
  {
    // Route to the host PC
    // Strip off 0x0d & 0x0a at the end and shorten length accordingly
    if(recvBuff[numbBytes - 1] == 0x0a || recvBuff[numbBytes - 1] == 0x0d)
      numbBytes--;
    if(recvBuff[numbBytes - 1] == 0x0a || recvBuff[numbBytes - 1] == 0x0d)
      numbBytes--;

    DEBUGASSERT(numbBytes < HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);

    // Send entire message, includes ctrl chararacters
    int ret = hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0, (char *) recvBuff,
            numbBytes, thisFile, __LINE__);
    if(ret == -EAGAIN)
      return ret;
  }

  return OK;
}

//==========================================================================
// Forward the message to the uart1 for transmission
int hcom_diag_trace_ramlog_send_msg_to_uart1(char *sendBuff, size_t numbBytes)
{
  // If uart1 not opened do this now
  if(_uart1_fd < 0)
  {
    int ret = hcom_diag_trace_ramlog_open_uart1_serial_port();
    if(ret < 0)
    {
      hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-uart open call:%s, errno:%d\n",
              thisFile, __LINE__, HCOM_TRACE_RAMLOG_SERIAL_NAME, errno);
      return ret;
    }
  }

  // For a few messages after mono starts reconfigures UART1
  if(_uart1_needs_reconfig)
  {
    _uart1_needs_reconfig--;

    // Reconfigure uart1
    hcom_via_nx_restore_uart_reconfig(MEADOW_RECONFIG_MISCONFIGURED_UART1);
  }

  ssize_t nbytes = write(_uart1_fd, sendBuff, numbBytes);
  if (nbytes < 0)
  {
    hcom_diag_trace_ramlog_err_logger(LOG_ERR, "%s@%d-uart failed to write:%s, errno:%d\n",
             thisFile, __LINE__, HCOM_TRACE_RAMLOG_SERIAL_NAME, errno);
    return nbytes;
  }
  return OK;
}

//==========================================================================
// There's really isn't a good way to handle errors with ramlog because errors
// cannot be written to syslog or we'll end up with an infinite loop. So,
// the best we can do is sent them to the host, and hope it's listening.
void hcom_diag_trace_ramlog_err_logger(int priority, FAR const IPTR char *fmt, ...)
{
    if(!(_trace_ramlog_to_uart1 || _trace_ramlog_to_host))
    return;

  va_list args;
  va_start(args, fmt);
  hcom_logging_safe_ramlog(priority, fmt, args);
  va_end(args);  
}

#endif    // #if defined (CONFIG_RAMLOG_SYSLOG)

// The following functions are always built. They are used to
// process hcom commands received from CLI
//======================================================================================
// Called from Meadow.CLI to enable tracing to host.
void hcom_diag_trace_forward_to_host(uint32_t userData)
{
  hcom_bbreg_set_bbr_bits(HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT);

  // If ramlog configured, need to init ramlog now. This insures
  // that Meadow.CLI is listening
#if defined (CONFIG_RAMLOG_SYSLOG)
  _trace_ramlog_to_host = true;  // Enable on ramlogs to host

  hcom_diag_trace_ramlog_lazy_initialization(false);
#endif

  char *sendMsgToHost = "Trace logs to be sent to CLI";
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);
}

//======================================================================================
// Called from Meadow.CLI for both ramlog and syslog
void hcom_diag_trace_do_not_send_to_host(uint32_t userData)
{
  hcom_bbreg_clear_bbr_bits(HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT);

#if defined (CONFIG_RAMLOG_SYSLOG)
  // Turn off ramlogs to host
  _trace_ramlog_to_host = false;
#endif

  char *sendMsgToHost = "Trace logs no longer sent to CLI";
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);
}

//======================================================================================
// Called from Meadow.CLI to enable tracing to uart1.
void hcom_diag_trace_forward_to_uart1(uint32_t userData)
{
  hcom_bbreg_set_bbr_bits(HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT);

  // If ramlog configured, need to init ramlog now. This insures
  // that Meadow.CLI is listening
#if defined (CONFIG_RAMLOG_SYSLOG)
  _trace_ramlog_to_uart1 = true;  // Enable on ramlogs to uart1

  if(_mono_has_started)
    _uart1_needs_reconfig = 1;  // Allow the first message to reconfigure UART1

  // Initialize if needed
  hcom_diag_trace_ramlog_lazy_initialization(false);
#endif

  char *sendMsgToHost = "Trace logs will be sent via UART1";
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);
}

//======================================================================================
// Called from Meadow.CLI for both ramlog and syslog
void hcom_diag_trace_do_not_send_to_uart1(uint32_t userData)
{
  hcom_bbreg_clear_bbr_bits(HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT);

#if defined (CONFIG_RAMLOG_SYSLOG)
  _trace_ramlog_to_uart1 = false;
#endif

  char *sendMsgToHost = "UART1 is available for .Net Apps";
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);
}
