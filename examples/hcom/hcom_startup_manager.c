/****************************************************************************
 * \apps\examples\hcom\hcom_startup_manager.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

// This module is where everything starts. The thread created by nuttx for
// running apps enters here and hcom begins.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"
#include <meadow/hcom_shared_common.h>

#if defined (CONFIG_HCOM_ESP32_COMMS)
#include "esp32/hcom_esp32_comms.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

extern int hcom_main (int argc, char* argv[]);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_manager_shutdown(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This is the hcom tasks main thread, created by Nuttx when it has finished
// starting the OS. This thread will do all the following initialization of
// hcom, then becomes the thread that receives CLI messages.
// All other threads are created by this thread or one of it's child threads.
// This means that they are all in the same "task group." See the following
// https://cwiki.apache.org/confluence/display/NUTTX/Tasks+vs.+Threads+FAQ
// https://cwiki.apache.org/confluence/pages/viewpage.action?pageId=158862687
int hcom_main(int argc, char *argv[])
{
  int ret;

  // Allocates memory for moving reading ramlog
  // Note: Therefore, this should be first because hcom_logging_syslog
  // needs this buffer to move stuff to syslog and syslog writes it to
  // an internal circular buffer.
  ret = hcom_diag_logging_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup logging utils:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Opens the  nuttx interface driver to allow nuttx access.
  // Note: This needs to be second because all battery backed register
  // (BBR) access needs this (e.g. hcom_logging_syslog_mask_init).
  ret = hcom_via_nx_access_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom nx access:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  #if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
    // This is a almost never needed diagnostic.
    ret = hcom_diag_gpio_setup();
    if (ret < 0)
    {
      syslog(LOG_CRIT, "%s@%d-setup diag gpio:%d\n", thisFile, __LINE__, ret);
      return ret;
    }
  #endif

  // Restores previous syslog mask from the battery backed register (BBR).
  // Note: This needs to be third because all hcom_logging_syslog
  // calls are filtered by the results of this call.
  ret = hcom_logging_syslog_mask_init();
  if(ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-hcom_logging_syslog_mask_init failed:%d\n", thisFile, __LINE__, ret);
    return ret; 
  }

 #if defined (CONFIG_RAMLOG_SYSLOG)
  // Sets up some basic initialization for ramlog, but usually does not create
  // the ramlog read thread etc. This is because this feature is not usually
  // needed. It will create the ramlog read thread if the BBR indicates its
  // needed. Otherwise, this is postponed until a request is received.
  // Note: must follow hcom_via_nx_access_setup because it access BBR.
  ret = hcom_diag_trace_ramlog_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup log tracing %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  // Does nothing
  ret = hcom_common_utils_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom utils:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Only sets a bool
  ret = hcom_diag_misc_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
  // Does nothing
  ret = hcom_misc_rqst_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Sets internal variable state
  ret = hcom_file_dnld_proc_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup file download %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Sets a few internal variable states
  ret = hcom_file_write_del_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup file cmds %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Allocates memory for circular buffer
  ret = hcom_host_parse_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host request %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_host_route_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host request %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if defined(CONFIG_HCOM_MONO_OUTPUT_PIPE)
  // Creates a pipe and a receiving thread.
  ret = hcom_mono_stdout_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono pipe %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if defined(HCOM_VS_REMOTE_DEBUGGING_INCLUDE_IN_BUILD)
  // Creates a unix domain socket and creates a receiving thread.
  ret = hcom_mono_remote_dbg_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup remote dbg %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if defined (CONFIG_HCOM_ESP32_COMMS)
  ret = hcom_esp32_uart_comms_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup esp32 comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  ret = hcom_host_send_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host msg builder:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Handle CLI commands
  ret = hcom_host_recv_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup Host comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Last we start mono, if it should be started
  hcom_mono_ctrl_start_mono_main();

  //-------------------------------------------------------
  // This thread will now run hcom CLI receive. It only
  // returns on shutdown or serious error
  //-------------------------------------------------------  
  ret = hcom_host_recv_receiving_loop();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-%s main-thread exited:%d\n",
            thisFile, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret);
  }

  hcom_manager_shutdown();
  return OK;
}

//=========================================================================
// Notify all interested children that we are shutting down. This closes the
// comms file descriptor which will cause the worker thread to exit.
void hcom_manager_shutdown()
{  
  hcom_host_recv_shutdown();
  hcom_mono_stdout_shutdown();
  hcom_common_utils_shutdown();
  hcom_diag_logging_shutdown();
  hcom_host_route_shutdown();  
  hcom_host_parse_shutdown();
  hcom_host_send_shutdown();
  hcom_file_write_del_shutdown();
  hcom_mono_remote_dbg_shutdown();
#if defined (CONFIG_HCOM_ESP32_COMMS)
  hcom_esp32_uart_comms_shutdown();
#endif
#if defined (CONFIG_RAMLOG_SYSLOG)
  hcom_diag_trace_ramlog_shutdown();
#endif
}
