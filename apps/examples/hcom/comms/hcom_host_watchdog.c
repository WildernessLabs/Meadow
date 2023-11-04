/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_watchdog.c
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

// This file consists of a watchdog timer to insure that file downloads don't
// stall.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "../hcom_common.h"
#include <meadow/hcom_dnld_shared.h>
#include <meadow/meadow_cirbuf.h>
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static hcom_dnld_shared_t *_dnldShared;
static timer_t _processWdogTimerId;
static bool _hcom_host_process_wdog_timedout;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_host_watchdog_timeout_expired(int signo, FAR siginfo_t *info, FAR void *context);
static void hcom_host_watchdog_cleanup_wdog_timeout(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from hcom_host_process.c
int hcom_host_watchdog_initialize(hcom_dnld_shared_t *dnldShared)
{
  // Need this to provide file name on failure.
  _dnldShared = dnldShared;

  _hcom_host_process_wdog_timedout = false;  
  return OK;
}

//=================================================================
void hcom_host_watchdog_stopping()
{
  _shutting_down = true;
}

//=================================================================
// Check if watchdog is indicating that we must cleanup. This is called by
// the process thread from hcom_host_enq_deq.c as the process thread is waiting
// for data to be written.
int hcom_host_watchdog_check_execute_if_expired()
{
  if(_hcom_host_process_wdog_timedout)
  {
    _hcom_host_process_wdog_timedout = false;

    // Cleanup download state information
    hcom_host_watchdog_cleanup_wdog_timeout();
    return -ETIME;   // Watchdog timed out
  }
  return OK;
}

//=================================================================
// Encountered a watchdog timeout. This function gets called from our
// pthread main loop while waiting for the semaphore.
void hcom_host_watchdog_cleanup_wdog_timeout()
{
  int ret;

  // Delete the download wdog timer that got us here
  ret = hcom_host_watchdog_dnld_timer_delete();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Stop/delete timer failed, ret:%d, errno:%d\n",
             thisFile, __LINE__, ret, get_errno());
  }

  // Close the partially downloaded file
  ret = hcom_file_write_close_active_file(_dnldShared);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-close failed for '%s', ret:%d, errno:%d\n",
             thisFile, __LINE__, _dnldShared->dnldOrigPathName, ret, get_errno());
  }

  // Delete the partially downloaded file
  ret = hcom_file_delete_stm32f7_file_by_name_internal(_dnldShared);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-delete failed for '%s', ret:%d, errno:%d\n",
             thisFile, __LINE__, _dnldShared->dnldOrigPathName, ret, get_errno());
  }

  // Clear the receive data buffer queue
  if(! hcom_host_enq_deq_clear_buffer())
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-download failed, clearing buff failed '%s'\n",
             thisFile, __LINE__, _dnldShared->dnldOrigPathName);
  }

  // Tell CLI to restart the download
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "File '%s' download failed, resend", _dnldShared->dnldOrigPathName);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_DNLD_FAIL_RESEND, 0, hostMsg,
        thisFile, __LINE__);

  // Setting the download state to inactive allows future downloads.
  _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
  
  return ret;
}

//=================================================================
// Callback on watchdog timer expiration. Set a flag so we know that when
// EINTR is detected, it was this timeout that caused it. This will trigger
// the download state cleanup.
void hcom_host_watchdog_timeout_expired(int signo, FAR siginfo_t *info,
          FAR void *context)
{
  // Only set a flag. The signal sent to the processing thread will detect
  // this the next time it waits for a semaphore.
  // Note: If the cleanup is executed from here on return the processing thread
  // terminates.
  _hcom_host_process_wdog_timedout = true;  
}

//=================================================================
// Start, restart, or stop the timer
// This gets called a lot when downloading
int hcom_host_watchdog_dnld_timer_set_delay(time_t delayInSeconds)
{
  struct itimerspec todelay;
  int ret;

  // Start, restart, or stop the timer
  todelay.it_interval.tv_sec = 0; // Nonrepeating
  todelay.it_interval.tv_nsec = 0;
  todelay.it_value.tv_sec = delayInSeconds;
  todelay.it_value.tv_nsec = 0;
  
  ret = timer_settime(_processWdogTimerId, 0, &todelay, NULL);
  if (ret < 0)
  {
    int errorcode = errno;
    hcom_logging_syslog(LOG_ERR, "%s@%d-setting timer, errno:%d\n", thisFile, __LINE__, errorcode);
    return -errorcode;
  }
  return OK;
}

//=================================================================
// Create the POSIX timer for detecting download failures
int hcom_host_watchdog_dnld_timer_initialize()
{
  struct sigevent toevent;
  struct sigaction act;
  int ret;

  _processWdogTimerId = NULL;

  // Create a POSIX timer to handle timeouts
  toevent.sigev_notify = SIGEV_SIGNAL;
  toevent.sigev_signo = SIGALRM;
  toevent.sigev_value.sival_ptr = NULL;  // Carry value to 'context' in callback

  ret = timer_create(CLOCK_REALTIME, &toevent, &_processWdogTimerId);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-create timer errno:%d\n", thisFile, __LINE__, errno);
    return -errno;
  }

  // Attach a callback to catch the timeout
  act.sa_sigaction = hcom_host_watchdog_timeout_expired;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  ret = sigaction(SIGALRM, &act, NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-attach signal errno:%d\n", thisFile, __LINE__, errno);
    return -errno;
  }
  return OK;
}

//=================================================================
// Delete the POSIX timer for detecting download failures
int hcom_host_watchdog_dnld_timer_delete()
{
  int ret;
  
  ret = hcom_host_watchdog_dnld_timer_set_delay(0);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set delay = 0, errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  ret = timer_delete(_processWdogTimerId);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  return ret;
}
