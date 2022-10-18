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
#include "misc/hcom_config_manager.h"

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
static int _semaphoreRet;
static sem_t _startupWaitSem;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

//===========================================================================
// Wait for the thread holding the semaphore to release it
static int hcom_startup_mgr_takesem(sem_t *semaphore)
{
  int ret;
  _semaphoreRet = OK;
  
  do
  {
    ret = sem_wait(semaphore);    // Take the semaphore (perhaps waiting)
  }
  while (ret == -EINTR);
  
  return _semaphoreRet;
}

//===========================================================================
// Called by setup code in different modules to release this startup thread
// to continue the setup
void hcom_startup_mgr_release_sem()
{
  hcom_startup_mgr_release_sem_err(OK);
}

//---------------------------------------------------------------------------
// Called to report an error
void hcom_startup_mgr_release_sem_err(int semaphoreRet)
{
  _semaphoreRet = semaphoreRet;

  sem_post(&_startupWaitSem);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This is the hcom tasks main thread, created by Nuttx when it has finished
// initializating and starting the OS. This thread will do all the following
// initialization of hcom, then becomes the thread that receives CLI messages.
// Other hcom threads are created by this thread or one of it's child threads.
// This means that they are all in the same "task group." See the following
// https://cwiki.apache.org/confluence/display/NUTTX/Tasks+vs.+Threads+FAQ
// https://cwiki.apache.org/confluence/pages/viewpage.action?pageId=158862687
int hcom_main(int argc, char *argv[])
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
syslog(2, "hcom_main() running\n"); usleep(10 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 1\n"); usleep(10 * 1000);
#endif

  // To better control the startup sequence a semaphore is used.
  // This thread will wait for those setup routines that immediately
  // create a thread to complete before continuing with the startup.
  // Initialize value to 0 for a 'signaling' semaphore to block
  // the calling thread (this one) until it is okay for it to proceed.
  sem_init(&_startupWaitSem, 0, 0);
  // Special non-standard nuttx function required for signaling semaphores
  sem_setprotocol(&_startupWaitSem, SEM_PRIO_NONE);

  // Allocates memory for moving reading ramlog. Nothing to wait for.
  ret = hcom_diag_logging_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup logging utils:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 2\n"); usleep(20 * 1000);
#endif

  // Note: This needs to be early because all battery backed register
  // (BBR) access needs this (e.g. hcom_logging_syslog_mask_init).
  // Opens the nuttx upd driver to allow nuttx access.
  ret = hcom_via_nx_upd_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom nx access:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 3\n"); usleep(20 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 4\n"); usleep(20 * 1000);
#endif

  // Restores previous syslog mask from the battery backed register (BBR).
  // Note: This needs to be third because all hcom_logging_syslog
  // calls are filtered by the results of this call. Nothing to wait for.
  ret = hcom_logging_syslog_mask_init();
  if(ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-hcom_logging_syslog_mask_init failed:%d\n", thisFile, __LINE__, ret);
    return ret; 
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 5\n"); usleep(20 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 6\n"); usleep(20 * 1000);
#endif

  // Does nothing
  ret = hcom_common_utils_setup();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom utils:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 7\n"); usleep(20 * 1000);
#endif

  // Does nothing
  ret = hcom_diag_misc_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Only sets a bool
  ret = hcom_diag_nsh_support_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-nsh setup %d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 8\n"); usleep(20 * 1000);
#endif

  // Does nothing
  ret = hcom_misc_rqst_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 9a\n"); usleep(20 * 1000);
#endif

  // Sets internal variable state
  ret = hcom_file_dnld_proc_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup file download %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 9b\n"); usleep(20 * 1000);
#endif

  // Sets internal variable state
  ret = hcom_file_upld_proc_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup file upload %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 10\n"); usleep(20 * 1000);
#endif

  // Allocates memory and sets a few internal variable states
  ret = hcom_file_write_del_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup file cmds %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 11\n"); usleep(20 * 1000);
#endif

  // Allocates memory and initializes hcom circular buffer
  ret = hcom_host_enq_deq_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host request %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 12\n"); usleep(20 * 1000);
#endif

  // Sets one variable
  ret = hcom_host_route_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host request %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 13\n"); usleep(20 * 1000);
#endif

#if defined(CONFIG_HCOM_MONO_STDERR_STDOUT)
  // Creates a fifo and a thread to receive stdout
  ret = hcom_mono_stdout_read_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono stdout fifo %d\n", thisFile, __LINE__, ret);
    return ret;
  }
  ret = hcom_startup_mgr_takesem(&_startupWaitSem);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono stdout fifo %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 14\n"); usleep(20 * 1000);
#endif

  // Creates a fifo and a thread to receive stderr
  ret = hcom_mono_stderr_read_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono stderr fifo %d\n", thisFile, __LINE__, ret);
    return ret;
  }
  ret = hcom_startup_mgr_takesem(&_startupWaitSem);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono stderr fifo %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 15\n"); usleep(20 * 1000);
#endif

#if defined(CONFIG_HCOM_MONO_REMOTE_DEBUGGING)
  // Sets a few variables
  ret = hcom_mono_remote_dbg_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup remote dbg %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 16\n"); usleep(20 * 1000);
#endif

#if defined (CONFIG_HCOM_ESP32_COMMS)
  // Sets a few internal variables
  ret = hcom_esp32_uart_comms_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup esp32 comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 17\n"); usleep(20 * 1000);
#endif

  // Allocates memory and sets some internal variables
  ret = hcom_host_send_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup host msg builder:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 18a\n"); usleep(20 * 1000);
#endif

  // Creates a thread to run hcom USB serial receive
  ret = hcom_host_recv_setup();  // Handle CLI commands
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup Host comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }  // Wait, hcom_host_recv_setup creates a new thread that must start before we continue
  ret = hcom_startup_mgr_takesem(&_startupWaitSem);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup Host comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 18b\n"); usleep(20 * 1000);
#endif

  // This is for transporting syslog messages to CLI. But, only if BBR requests it.
  ret = hcom_trace_to_cli_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-provide cli access %d\n", thisFile, __LINE__, ret);
    return ret;
  }


#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 18c\n"); usleep(20 * 1000);
#endif

  // This is for transporting text messages to Host.
  ret = hcom_host_text_transport_setup();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-provide text transport %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 19\n"); usleep(20 * 1000);
#endif

  // Minor setup, configures blue led as output
  ret = hcom_mono_ctrl_mono_main_setup();  // Handle CLI commands
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-setup mono main %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 20\n"); usleep(20 * 1000);
#endif

  // Last stop, start mono
  hcom_mono_ctrl_start_mono_main();

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 21\n"); usleep(20 * 1000);
#endif

#if HCOM_DIAG_INCLUDE_STARTUP_SYSLOG > 0
  syslog(2, "Startup Manager 22-Exit\n"); usleep(20 * 1000);
#endif

  sem_destroy(&_startupWaitSem);

  // Say good bye to the HCOM's task main thread
  return OK;
}

//=========================================================================
// Notify all interested children that we are shutting down. This closes the
// comms file descriptor which will cause the worker thread to exit.
// At present this is never called
void hcom_manager_shutdown()
{  
  hcom_host_recv_shutdown();
  hcom_mono_stdout_read_shutdown();
  hcom_mono_stderr_read_shutdown();
  hcom_common_utils_shutdown();
  hcom_diag_logging_shutdown();
  hcom_host_route_shutdown();  
  hcom_host_enq_deq_shutdown();
  hcom_host_send_shutdown();
  hcom_file_write_del_shutdown();
  hcom_mono_remote_dbg_shutdown();
#if defined (CONFIG_HCOM_ESP32_COMMS)
  hcom_esp32_uart_comms_shutdown();
#endif
}
