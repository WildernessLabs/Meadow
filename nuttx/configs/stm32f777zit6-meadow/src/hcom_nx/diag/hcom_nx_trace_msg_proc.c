/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\hcom_nx\diag\hcom_nx_trace_msg_proc.c
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

#if defined (CONFIG_RAMLOG_SYSLOG)
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
#endif

static char *thisFile = __FILE__;

// This covers most of this file
#if defined (CONFIG_RAMLOG_SYSLOG)

static bool _shutting_down;
static host_com_cir_buffer_t *_ramlog_cbuf;
// This buffer is where each messages is placed before being sent out
// via uart and/or to CLI. When we must send to CLI a semaphore protects
// this shared memory so we don't need to copy it's data while being sent
// It would be nice if we could just copy the data to a different buffer
// but then there would be a risk that messages would arrive faster than
// CLI can consume them.
static uint8_t *_syslogMsgBuf;
static int _ramlog_fd;
static int _uart1_fd;
static bool _trace_kthread_running;
static bool _trace_log_to_host;
static bool _trace_log_to_uart1;
static bool _profiler_log_to_uart1;
static size_t _cliMsgLength;
static sem_t _startupSem;
static sem_t _readNxtSem;
static sem_t _sendCliSem;
static int _ramlog_reader_kthread_pid;
static uint32_t _txRxReCfgStopTime;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void *hcom_nx_trace_msg_kthread(int argc, char *argv[]);
static void hcom_nx_trace_read_bbreg_config(void);
static int hcom_nx_trace_msg_make_thread(void);
static int hcom_nx_trace_msg_open_ramlog(void);
static int hcom_nx_trace_msg_lazy_initialization(void);
static void hcom_nx_trace_msg_close_and_delay(bool ramLogClose);
static int hcom_nx_trace_msg_read_ramlog_loop(uint8_t *readBuf);
static int hcom_nx_trace_msg_save_recvd_data(uint8_t readBuf[], const ssize_t recvByteCnt);
static int hcom_nx_trace_msg_pull_all_packets_from_buffer(void);
static int hcom_nx_trace_msg_route_trace_text(void);
static int hcom_nx_trace_msg_send_msg_to_uart1(const char *toUartBuf, size_t numbBytes);
static int hcom_nx_trace_msg_open_uart1_serial_port(void);
static void hcom_nx_trace_msg_wait_sem(sem_t *semaphore);
static void hcom_nx_trace_kthread_exit_initiate(void);
static void hcom_nx_trace_kthread_exit_cleanup(void);
static void hcom_nx_trace_msg_sig_recv(int signo, FAR siginfo_t *info, FAR void *context);
static void hcom_nx_uart1_direct(int priority, const char *outputMsg, ...);

//=========================================================================
// Returns the current time as a 32-bit number representing millisec time.
static uint32_t hcom_nx_trace_get_time_ms(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return(((uint32_t)ts.tv_sec * MSEC_PER_SEC) + ts.tv_nsec / NSEC_PER_MSEC);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_trace_msg_proc_setup()
{
  _trace_kthread_running = false;
  _ramlog_reader_kthread_pid = 0;
  _txRxReCfgStopTime = 0;
  sem_init(&_startupSem, 0, 1);

  // Check the BBR to determine if initialization is needed, but only
  // during startup. After startup, this is controlled by CLI command
  hcom_nx_trace_read_bbreg_config();

  // Follow through with initialization if needed
  if((_trace_log_to_host || _trace_log_to_uart1) && !_profiler_log_to_uart1)
    hcom_nx_trace_msg_lazy_initialization();
  return OK;
}

//==========================================================================
// This function reads the battery backed register and sets appropriate flags
void hcom_nx_trace_read_bbreg_config()
{
  int bbrRegValue;

  // If started after nuttx start time recheck the battery
  // backed registers as it could have been long ago.
  bbrRegValue = getreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
  if((HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT & bbrRegValue) > 0)
    _trace_log_to_uart1 = true;
  else
    _trace_log_to_uart1 = false;

#if HCOM_FORCE_SYSLOG_MASK_AND_OUTPUT_TO_UART1 > 0
  _trace_log_to_uart1 = true;
#endif

  if((HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT & bbrRegValue) > 0)
    _trace_log_to_host = true;
  else
    _trace_log_to_host = false;

    if((HCOM_BBREG_ROUTE_PROFILER_BINS_TO_UART1_BIT & bbrRegValue) > 0)
    _profiler_log_to_uart1 = true;
  else
    _profiler_log_to_uart1 = false;
}

//==========================================================================
// Called when mono starts
int hcom_nx_trace_msg_mono_started()
{
  // When this message is received we know mono has begun running. When mono does
  // begin it will reconfigured all of the GPIOs, including those needed by this
  // code to send messages to the UART. Therefore, we must reconfigures these GPIOs
  // We don't want to do this all the time because there's no reason. So, we'll
  // grab the current time and calculate a reasonable reconfiguration stop time.
  _txRxReCfgStopTime = hcom_nx_trace_get_time_ms();
  _txRxReCfgStopTime += ((uint32_t)HCOM_TRACE_RAMLOG_RECONFIG_TIMEOUT * MSEC_PER_SEC);

  return OK;
}

//==========================================================================
// Because this feature is rarely used we initialize only when requested
// May be called at start or from CLI command or both
int hcom_nx_trace_msg_lazy_initialization()
{
  int ret;

  // Need to guard the thread running flag otherwise may get
  // 2 ramlog reader threads
  hcom_nx_trace_msg_wait_sem(&_startupSem);
  if(_trace_kthread_running)  // Already running? Don't restart
    return OK;

  _ramlog_fd = -1;
  _uart1_fd = -1;
  _cliMsgLength = 0;
  _ramlog_reader_kthread_pid = 0;

  // When trace logging is started and uart1 is able to output messages via
  // uart we'll output one message very early. This message will not be output
  // using syslog but, directly via the uart. This will indicate that Meadow
  // has started. Also, since executed at startup, if the OS crashes, we
  // should still see this message which gives us a clue why no other trace
  // messages follow.
  if(_trace_log_to_uart1 && !_profiler_log_to_uart1)
  {
    struct tm tmNow;
    char timeBuf[64];

    ret = up_rtc_getdatetime(&tmNow);
    if(ret < 0)
    {
      return -EINVAL;
    }
    
    snprintf_chk(timeBuf, 64, "%02d:%02d:%02d", tmNow.tm_hour, tmNow.tm_min,
              tmNow.tm_sec);

    hcom_nx_uart1_direct(0, "\n" HCOM_DEVICE_INFO_PRODUCT " initialization has begun at %s UTC Meadow time.\n", timeBuf);

    // Close uart port because the file descriptor is open by a different thread
    // than the one that will normally handle trace processing.
    close(_uart1_fd);
    _uart1_fd = -1;
  }

  // These semaphores are needed for sending trace to CLI
  sem_init(&_readNxtSem, 0, 0);
  sem_setprotocol(&_readNxtSem, SEM_PRIO_NONE);

  sem_init(&_sendCliSem, 0, 0);
  sem_setprotocol(&_sendCliSem, SEM_PRIO_NONE);

  // Create a circular buffer instance to manage messages read from ramlog
  _ramlog_cbuf = (host_com_cir_buffer_t *)malloc(sizeof(struct host_com_cir_buffer_s));
  if (_ramlog_cbuf == NULL)
  {
    hcom_nx_uart1_direct(LOG_ERR, "%s@%d-cir buf malloc\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Initialize the internal circular buffer and use 0x0a as the delimiter.
  // Therefore, every syslog string must be terminate by a new line
  // (i.e '\n' or 0x0a) since they are all the same value 0x0a.
  ret = hcom_cirbuf_init(_ramlog_cbuf, HCOM_TRACE_LOCAL_SYSLOG_CIR_BUF_SIZE, 0x0a);
  if (ret == HCOM_CIR_BUF_ALLOC_FAILED)
  {
    hcom_nx_uart1_direct(LOG_ERR, "%s@%d-Cir buf alloc failed\n", thisFile, __LINE__);
    return -1;
  }

  // Message buffer containing full messages pulled from the ramlog
  _syslogMsgBuf = malloc(HCOM_TRACE_RAMLOG_ASSUME_LARGEST_SYSLOG);
  if (_syslogMsgBuf == NULL)
  {
    hcom_nx_uart1_direct(LOG_ERR, "%s@%d-cir buf malloc\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Create a thread to read the ramlog
  ret = hcom_nx_trace_msg_make_thread();
  if (ret < 0)
  {
    hcom_nx_uart1_direct(LOG_ERR, "%s@%d-thread create, errno:%d\n",
              thisFile, __LINE__, errno);

    free(_syslogMsgBuf);
    _syslogMsgBuf = NULL;
    return -1;
  }

  return OK;
}

//=============================================================
// Create a kthread to use as a syslog message reader
int hcom_nx_trace_msg_make_thread()
{
  _ramlog_reader_kthread_pid = kthread_create(HCOM_THREAD_NAME_TRACE_RAMLOG,
                                   HCOM_THREAD_PRIORITY_TRACE_RAMLOG,
                                   HCOM_THREAD_STACKSIZE_TRACE_RAMLOG,
                                   (main_t) hcom_nx_trace_msg_kthread,
                                   (char *const *) NULL);
  if (_ramlog_reader_kthread_pid <= 0)
  {
      return -ENOEXEC;
  }

  return OK;
}

//=================================================================
// This thread reads all ramlog messages received via syslog from
// the ramlog buffer and routes them to the UART1 and CLI. This
// thread once started is not stopped until F7 reset.
void *hcom_nx_trace_msg_kthread(int argc, char *argv[])
{
  int ret;
  uint8_t *readBuf;
  struct sigaction act;

  _trace_kthread_running = true;
  _shutting_down = false;
  sem_post(&_startupSem);

  readBuf = malloc(HCOM_TRACE_RAMLOG_READ_BUF_SIZE);
  if(readBuf == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return NULL;
  }

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_TRACE_RAMLOG);
#endif

  // Attach a signal handler to catch the kill command
  act.sa_sigaction = hcom_nx_trace_msg_sig_recv;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  ret = sigaction(SIGALRM, &act, NULL);
  if (ret < 0)
  {
    _trace_kthread_running = false;
    free(readBuf);
    return NULL;
  }

  //-------------------------------------------
  // Only leave this loop when shutting down
  while(!_shutting_down)
  {
    // Must have ramlog, it's the source of all trace info
    ret = hcom_nx_trace_msg_open_ramlog();
    if(_shutting_down) break;
    if(ret < 0)
    {
      hcom_nx_trace_msg_close_and_delay(false);
      continue;
    }

    // Open uart1 if it's requested
    if(_trace_log_to_uart1 && !_profiler_log_to_uart1)
    {
      ret = hcom_nx_trace_msg_open_uart1_serial_port();
      if(_shutting_down) break;
      if(ret < 0)
      {
        hcom_nx_trace_msg_close_and_delay(false);
        continue;
      }
    }

    // Attempt to read from the ramlog buffer
    ret = hcom_nx_trace_msg_read_ramlog_loop(readBuf);
    if(_shutting_down) break;
    if(ret < 0)
    {
      hcom_nx_trace_msg_close_and_delay(true);
    }
  }
  //-------------------------------------------

  // Time for this thread to exit
  hcom_nx_trace_kthread_exit_cleanup();

  _trace_kthread_running = false;
  free(readBuf);

  return NULL;
}

//=================================================================
// Need a signal handler or the default Nuttx behavior kicks in
void hcom_nx_trace_msg_sig_recv(int signo, FAR siginfo_t *info, FAR void *context)
{
  return;
}

//==========================================================================
// Initiate the kthread exit.
void hcom_nx_trace_kthread_exit_initiate()
{
  // A pthread cannot kill a kthread so we must indirectly kill the thread
  // by setting _shutting_down true, closing connections and waiting
  _shutting_down = true;

  // Make sure the semaphores won't block the exiting of either thread
  sem_post(&_readNxtSem);
  sem_post(&_sendCliSem);

  // Signal the kthread so _shutting_down will be tested, forcing it to exit
  kill(_ramlog_reader_kthread_pid, SIGALRM);

  // Nuttx kthreads have no 'join' functionality. We stay in this loop
  // until the exiting kthread has cleaned up it's resources.
  while(_trace_kthread_running)
  {
    usleep(10 * 1000);
  }

  sem_destroy(&_readNxtSem);
  sem_destroy(&_sendCliSem);
}

//==========================================================================
// This function is called by the kthread just before it exits
// and is used to reclaim resources
void hcom_nx_trace_kthread_exit_cleanup()
{
  _trace_log_to_uart1 = false;
  _trace_log_to_host = false;
  _profiler_log_to_uart1 = false;

  close(_ramlog_fd);
  _ramlog_fd = -1;

  close(_uart1_fd);
  _uart1_fd = -1;

  if(_ramlog_cbuf != NULL)
  {
    hcom_cirbuf_release_memory(_ramlog_cbuf);
    free(_ramlog_cbuf);
    _ramlog_cbuf = NULL;
  }

  if(_syslogMsgBuf != NULL)
  {
    free(_syslogMsgBuf);
    _syslogMsgBuf = NULL;
  }
}

//=================================================================
void hcom_nx_trace_msg_close_and_delay(bool ramLogClose)
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
int hcom_nx_trace_msg_open_ramlog()
{
  if(_ramlog_fd > -1)
  {
    close(_ramlog_fd);
    _ramlog_fd = -1;
  }

  _ramlog_fd = open(HCOM_TRACE_RAMLOG_DEVICE_NAME, O_RDONLY);
  if(_shutting_down) return OK;
  if (_ramlog_fd < 0)
  {
    hcom_nx_uart1_direct(LOG_ERR, "%s@%d-open %s, errno:%d\n",
            thisFile, __LINE__, HCOM_TRACE_RAMLOG_DEVICE_NAME, errno);
    return -1;
  }

  return OK;
}

//=================================================================
int hcom_nx_trace_msg_open_uart1_serial_port()
{
  if(_uart1_fd > -1)
  {
    close(_uart1_fd);
    _uart1_fd = -1;
  }

  _uart1_fd = open(HCOM_TRACE_RAMLOG_SERIAL_NAME, O_WRONLY);
  if(_shutting_down) return OK;
  if (_uart1_fd < 0)
  {
    _uart1_fd = -1;
    return -1;
  }
  return OK;
}

//=================================================================
// This function reads the data put into the ramlog by Nuttx
// Returning on error will close the uart connection
int hcom_nx_trace_msg_read_ramlog_loop(uint8_t *readBuf)
{
  ssize_t readReturn;
  int ret;

  while(!_shutting_down)
  {
    // Read ramlog
    readReturn = read(_ramlog_fd, readBuf, HCOM_TRACE_RAMLOG_READ_BUF_SIZE);
    if(_shutting_down) break;
    if (readReturn < 0 )
    {
      // Error
      hcom_nx_uart1_direct(LOG_ERR, "%s@%d ramlog read readReturn:%d errno:%d\n",
              thisFile, __LINE__, readReturn, errno);
      return -errno;    // Close connection, wait and try again
    }
    else if (readReturn == 0)
    {
      // EOF
      hcom_nx_uart1_direct(LOG_WARNING, "%s@%d ramlog read EOF read returned:%d errno:%d\n",
              thisFile, __LINE__, readReturn, errno);
      return -errno;    // Close connection, wait and try again
    }
    else
    {
      // Successful ramlog message read. Put message into circular buffer
      // unless all logging has been turned off
      if(_trace_log_to_host || _trace_log_to_uart1)
      {
        ret = hcom_nx_trace_msg_save_recvd_data(readBuf, readReturn);
        if (ret < 0 )
        {
          hcom_nx_uart1_direct(LOG_WARNING, "%s@%d hcom_nx_trace_msg_save_recvd_data() returned:%d\n",
                    thisFile, __LINE__, ret);
          return ret;    // Close connection, wait and try again
        }
      }
    }
  }   // while (!_shutting_down)

  return OK;
}

//=======================================================================
// Add the data read from ramlog into a circular buffer. It can be added
// byte by byte or several bytes at once.
// We assume that the message "unit" (aka line) will be delimited with
// 0x0a (AKA 'line feed' or '\n')
int hcom_nx_trace_msg_save_recvd_data(uint8_t readBuf[], const ssize_t recvByteCnt)
{
  int addResult;
  int pullResult;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add received data and read all available data to/from
  // the buffer until no more will fit
  while(!_shutting_down)
  {
    // Can return HCOM_CIR_BUF_ADD_SUCCESS, HCOM_CIR_BUF_ADD_WONT_FIT, HCOM_CIR_BUF_ADD_BAD_ARG,
    addResult = hcom_cirbuf_add_bytes(_ramlog_cbuf, readBuf, recvByteCnt);
    if(_shutting_down) break;
    if(addResult == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      break;    // Leave loop and check for data ready to read
    }
    else if (addResult == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // The buffer doesn't have room for these bytes. We need to pull messages
      // and retry to add this data Only returns -error, HCOM_CIR_BUF_GET_NONE_FOUND
      // or HCOM_CIR_BUF_GET_DELETED_TOO_BIG
      pullResult = hcom_nx_trace_msg_pull_all_packets_from_buffer();
      if(_shutting_down) break;
      if (pullResult == HCOM_CIR_BUF_GET_FOUND_MSG)
      {
        // Pulled and processed some message so attempt to add again
        continue;
      }

      if (pullResult == HCOM_CIR_BUF_GET_NONE_FOUND)
      {
        // This makes no sense. Like a buffer full of garbage and no delimiter
        hcom_cirbuf_clear_buffer(_ramlog_cbuf);

        hcom_nx_uart1_direct(LOG_ERR, "%s@%d-buffer corrupted or messages w/o linefeed. Deleted data.\n",
                 thisFile, __LINE__);
        
        return HCOM_CIR_BUF_GET_NONE_FOUND;    // Reported so throw data away.
      }

      if (pullResult == HCOM_CIR_BUF_GET_DELETED_TOO_BIG)
      {
        // The message was too long for the allocated buffer and has been deleted.
        hcom_nx_uart1_direct(LOG_ERR, "%s@%d-pull packets from cir buf, msg too long, deleted\n",
                 thisFile, __LINE__);
        return pullResult;    // Reported and deleted.
      }
    }
    else if (addResult == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // A bad argument is never expected
      hcom_nx_uart1_direct(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
      return addResult; // Report, throw data away and keep going
    }
    else
    {
      // Undefined error????
      hcom_nx_uart1_direct(LOG_ERR, "%s@%d-Unknown cir buf add err:%d\n", thisFile, __LINE__, addResult);
      return addResult; // Report, throw data away and keep going
    }
  }   // while(!_shutting_down)

  // Check if there's more complete message(s) buffered
  pullResult = hcom_nx_trace_msg_pull_all_packets_from_buffer();
  return pullResult;
}

//====================================================================
// Pull and process all the complete packets from the circular buffer
// Returns -error or one of the circular buffer get return values
int hcom_nx_trace_msg_pull_all_packets_from_buffer()
{
  int ret;
  size_t packetLength;

  while(!_shutting_down)
  {
    // If buffer too small for the found message packetLength will contain
    // the needed buffer size.
    ret = hcom_cirbuf_get_next_packet(_ramlog_cbuf, _syslogMsgBuf,
            HCOM_TRACE_RAMLOG_ASSUME_LARGEST_SYSLOG, &packetLength);
    if(_shutting_down) break;

    // Any messages available?
    if (ret == HCOM_CIR_BUF_GET_NONE_FOUND)
    {
      return ret; // Buffer empty, return to get more data
    }

    if(ret == HCOM_CIR_BUF_GET_DELETED_TOO_BIG)
    {
      // The message was too long.
      // Probably corrupted data or no linefeed at end of messages
      hcom_cirbuf_clear_buffer(_ramlog_cbuf);

      hcom_nx_uart1_direct(LOG_ERR, "%s@%d-message %d long or w/o linefeed, deleted\n",
                thisFile, __LINE__, packetLength);

      return ret; // _syslogMsgBuf too small, throw away data and keep going 
    }
    
    // Circular Buffer ret must have been HCOM_CIR_BUF_GET_FOUND_MSG
    _cliMsgLength = packetLength;

    // Route the isolated message
    ret = hcom_nx_trace_msg_route_trace_text();
    if(_shutting_down) break;
    if (ret == OK)
    {
      continue; // pull next packet, if there is one
    }
    else
    {
      hcom_nx_uart1_direct(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, ret);
      return HCOM_CIR_BUF_GET_NONE_FOUND;   // Assume buffer empty
    }
  }   // while(!_shutting_down)
  
  return HCOM_CIR_BUF_GET_FOUND_MSG;
}

//=================================================================
// Ship the ramlog text message to uart and/or host CLI
// If this function cannot send it, the message is lost....
int hcom_nx_trace_msg_route_trace_text(void)
{
  int ret;

  if(_trace_log_to_uart1)
  {
    // Route to uart1
    ret = hcom_nx_trace_msg_send_msg_to_uart1((char *) _syslogMsgBuf, _cliMsgLength);
    if(ret < 0)
    {
      return ret;
    }
  }

  if(_trace_log_to_host)
  {
    // Route to CLI
    // Release CLI pthread to return the  message in the buffer it provided,
    // to the CLI
    sem_post(&_sendCliSem);

    // Wait for message to be sent before getting next message. Why? Because
    // the _syslogMsgBuf buffer is a shared resource.
    hcom_nx_trace_msg_wait_sem(&_readNxtSem);
  }

  return OK;
}

//==========================================================================
// This is called via udp and provides a pthread, and the buffer for the
// trace message to CLI. On every call it waits for the next message.
// When a message is found it returns and calls the code to send
// the trace message to CLI and returns again.
size_t hcom_nx_trace_cli_trace_transport(char *buff, size_t buffLen)
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
    // The caller of this function has returned, indicating that the message
    // has been sent to the CLI. Therefore, allow the ramlog reader thread
    // to get the next message
    sem_post(&_readNxtSem);
  }

  // Wait for the next messge to be provided for CLI
  hcom_nx_trace_msg_wait_sem(&_sendCliSem);
  if(_shutting_down)
    return 0;

  // Truncate the message if too long for caller's buffer
  if(_cliMsgLength > buffLen)
    msgLength = buffLen;
  else
    msgLength = _cliMsgLength;

  // Currently the 2 threads run in a ping-pong fashion, only one can run
  // at a time. With some effort, once the data has been copied to the userland
  // buffer, the ramlog reader could be allowed to read the next message.
  // But, at this time the effort doesn't seem to be worth the benefit.
  memcpy(buff, _syslogMsgBuf, msgLength);

  // Return to userland with the message
  return msgLength;
}

//==========================================================================
// There's really isn't a good way to handle errors with ramlog because errors
// cannot be written to syslog or we'll end up with an infinite loop. So,
// the best we can do is send them to UART1, and hope it's being watched.
//--------------------------------------------------------------------------
// Send a string directly to uart1. The priority value is not used but makes
// this function signature like syslog so that this can be used as a substitute
// for syslog.
// Note:This funcion can ONLY be called from a kthread.
// Note:Calling from a userland pthread will CRASH Nuttx.
void hcom_nx_uart1_direct(int priority, const char *fmt, ...)
{
#define HCOM_NX_UART1_DIRECT_BUF_LEN (256)
  va_list ap;
  int stringLen;

  char *completeStr;
  completeStr = malloc(HCOM_NX_UART1_DIRECT_BUF_LEN);
  if(completeStr == NULL)
  {
    // Can't do anything else at this point
    return;
  }

  // Create the complete message
  va_start(ap, fmt);

  // Returned value is the string length, including '\0'. But, if buffer is too
  // small the returned value is the needed buffer length excluding terminating
  // '\0'. However, sending direct to uart means a '\0' is not needed anyway.
  stringLen = vsnprintf(completeStr, HCOM_NX_UART1_DIRECT_BUF_LEN, fmt, ap);
  va_end(ap);

  // We need the actual length. So if trucated we'll use the buffer's length
  if(stringLen > HCOM_NX_UART1_DIRECT_BUF_LEN)
    stringLen = HCOM_NX_UART1_DIRECT_BUF_LEN;

  hcom_nx_trace_msg_send_msg_to_uart1(completeStr, stringLen);
  
  // Need a carrage return which we usually don't add
  hcom_nx_trace_msg_send_msg_to_uart1("\r", 1);
  free(completeStr);
}

//==========================================================================
// Forward the message to the uart1 for transmission
int hcom_nx_trace_msg_send_msg_to_uart1(const char *toUartBuf, size_t numbBytes)
{
  // If uart1 not opened do this now
  if(_uart1_fd < 0 && !_profiler_log_to_uart1)
  {
    int ret = hcom_nx_trace_msg_open_uart1_serial_port();
    if(ret < 0)
    {
      return ret;
    }
  }

  // Because the mono/meadow runtime may reconfigure all the GPIOs used by the board
  // we need to reconfigure them after mono starts. The problem is we don't know when
  // this is is. Therefore, we execute this code for a predetermined amount of time,
  // starting when mono has started running.
  if(_txRxReCfgStopTime != 0)
  {
    // Reconfigure uart1 (takes about 32usec to do all 4 commands)
    stm32_unconfiggpio(GPIO_USART1_TX); // PB14
    stm32_configgpio(GPIO_USART1_TX);
    stm32_unconfiggpio(GPIO_USART1_RX); // PH13
    stm32_configgpio(GPIO_USART1_RX);
    
    // Run until stop time is exceeded then stop the process
    if(_txRxReCfgStopTime < hcom_nx_trace_get_time_ms())
    {
      _txRxReCfgStopTime = 0;      // Past so timeout no longer needed
    }
  }

  // Write string out the uart
  ssize_t nbytes = write(_uart1_fd, toUartBuf, numbBytes);
  if (nbytes < 0)
  {
    return nbytes;
  }

  return OK;
}

//===========================================================================
// Wait for the thread holding the semaphore to release it
void hcom_nx_trace_msg_wait_sem(sem_t *semaphore)
{
  int ret;  

  do
  {
    ret = sem_wait(semaphore);    // Take the semaphore (perhaps waiting)
  }
  while (ret == -EINTR);
}

#endif    // #if defined (CONFIG_RAMLOG_SYSLOG)

//======================================================================================
// The following functions are always built. They are used to process
// hcom commands received from CLI that control the tracing feature.
//======================================================================================
//
// Called from Meadow.CLI to enable tracing to host.
int hcom_nx_exec_trace_do_send_to_host(struct hcom_nx_cmd_data *cmdData)
{
  char *sendMsgToHost;
  
  // Set the appropriate battery backed register bit
  modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER,
              0, HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT);

  // If ramlog configured, need to init ramlog now. This insures
  // that Meadow.CLI is listening
#if defined (CONFIG_RAMLOG_SYSLOG)
  if(_trace_log_to_host)
  {
    sendMsgToHost = "No change. Trace logs already sent to CLI";
  }
  else
  {
    _trace_log_to_host = true;

    // Initialize as needed
    hcom_nx_trace_msg_lazy_initialization();

    if(_trace_log_to_uart1)
      sendMsgToHost = "Trace logs now sent to CLI and UART1";
    else
      sendMsgToHost = "Trace logs now sent to CLI";
  }
#else
  sendMsgToHost = "Trace logging not available";
#endif

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
          thisFile, __LINE__);
  return OK;
}

//======================================================================================
// Called from Meadow.CLI for ramlog output to host
int hcom_nx_exec_trace_do_not_send_to_host(struct hcom_nx_cmd_data *cmdData)
{
  char *sendMsgToHost;

  // Clear the appropriate battery backed register bit
  modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER,
              HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT, 0);

#if defined (CONFIG_RAMLOG_SYSLOG)
  if(_trace_log_to_host)
  {
    // Disable ramlogs to host
    _trace_log_to_host = false;

    if(!_trace_log_to_uart1)
    {
      // Since no longer needed, terminate the main ramlog reader thread
      // and clean up it's resources
      hcom_nx_trace_kthread_exit_initiate();
      sendMsgToHost = "Trace logging will be terminated";
    }
    else
    {
      // When CLI sending stops we need to do a bit more cleanup.
      sem_post(&_sendCliSem);     // Allow userland pthread to return and exit
      sem_post(&_readNxtSem);     // Allow ramlog thread to leave
                                  // 'if(_trace_log_to_host)' block.

      sendMsgToHost = "Will no longer send trace logs to CLI";
    }
  }
  else
  {
    sendMsgToHost = "No change. Trace logs still not sent to CLI";
  }
#else
  sendMsgToHost = "Trace logging not available";
#endif

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
          thisFile, __LINE__);

  return OK;
}

//======================================================================================
// Called from Meadow.CLI to enable tracing to uart1.
int hcom_nx_exec_trace_forward_to_uart1(struct hcom_nx_cmd_data *cmdData)
{
  char *sendMsgToHost;
  
   // Set the appropriate battery backed register bit
  modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER,
              0, HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT);

  // If ramlog configured, need to init ramlog now. This insures
  // that Meadow.CLI is listening
#if defined (CONFIG_RAMLOG_SYSLOG)
  if(_trace_log_to_uart1)
  {
    sendMsgToHost = "No change. Trace logs already sent to UART1";
  }
  else
  {
    _trace_log_to_uart1 = true;

    // Initialize if needed
    hcom_nx_trace_msg_lazy_initialization();

    if(_trace_log_to_host)
      sendMsgToHost = "Trace logs now sent to UART1 and CLI";
    else
      sendMsgToHost = "Trace logs now sent to UART1";
  }
#else
  sendMsgToHost = "Trace logging not available";
#endif

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
          thisFile, __LINE__);

  return OK;
}

//======================================================================================
// Called from Meadow.CLI for ramlog output to uart1
int hcom_nx_exec_trace_do_not_send_to_uart1(struct hcom_nx_cmd_data *cmdData)
{
  char *sendMsgToHost;

  // Clear the appropriate battery backed register bit
  modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER,
              HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT, 0);

#if defined (CONFIG_RAMLOG_SYSLOG)
  if(_trace_log_to_uart1)
  {
    _trace_log_to_uart1 = false;

    if(!_trace_log_to_host)
    {
      // Since no longer needed, terminate the main ramlog reader thread
      // and clean up it's resources
      hcom_nx_trace_kthread_exit_initiate();

      sendMsgToHost = "Trace logging will be terminated";
    }
    else
    {
      sendMsgToHost = "Will no longer send trace logs to UART1";
    }
  }
  else
  {
    sendMsgToHost = "No change. Trace logs still not sent to UART1";
  }
#else
  sendMsgToHost = "Trace logging not available";
#endif
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            sendMsgToHost, thisFile, __LINE__);
  
  return OK;
}

//======================================================================================
// Called by meadow configuration after it has started. Once the meadow configuration
// has been parsed this method is called if it determines tracing should be enabled.
void hcom_nx_trace_insure_correct_config(bool uartTracing, bool cliTracing, bool uartProfiling)
{
#if defined (CONFIG_RAMLOG_SYSLOG)

  if (uartProfiling)
  {
    _profiler_log_to_uart1 = true;
    _trace_log_to_uart1 = false;
    _trace_log_to_host = false; // TODO: Check it
    return;
  }

  bool needToInit = false;

  if(uartTracing && (!_trace_log_to_uart1))
  {
    _trace_log_to_uart1 = true;
    needToInit = true;
  }

  if(cliTracing && (!_trace_log_to_host))
  {
    _trace_log_to_host = true;
    needToInit = true;
  }

  // At startup this initialization would not have been called so do it now
  if(needToInit)
  {
    hcom_nx_trace_msg_lazy_initialization();
  }

#endif
}
