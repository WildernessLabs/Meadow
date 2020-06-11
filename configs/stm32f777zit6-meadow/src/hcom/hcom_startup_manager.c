/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_startup_manager.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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
#if defined (CONFIG_HCOM_ESP32_COMMS)
#include "esp32/hcom_esp32_comms.h"
#endif

#include <nuttx/kthread.h>
#include <assert.h>
#include "task/task.h"

#if HCOM_TASK_SHOW_CREATED_TASK_PID_NAME > 0
#include <nuttx/sched.h>
#include <../sched/sched/sched.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _hcom_pid;
static int _syslog_mask;
static int _syslog_mask_old;
static bool _power_on_restart;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_manager_shutdown(void);
static int hcom_manager_create_worker_thread(void);

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_comms_recv_worker_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_comms_recv_worker_pthread(FAR void *arg);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#if defined(CONFIG_STM32F7_PWR)
int hcom_manager_syslog_mask_init()
{
  // Check if this is a reboot or a power-on restart. The MCU on Power-on
  // restart clears all 32 battery backed registers to 0.
  if(hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_SYSLOG_MASK) == 0)
  {
    // Power-on restart
    _power_on_restart = true;

    // Set and save the syslog level to the default value
    _syslog_mask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
               LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING);
    hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_SYSLOG_MASK, _syslog_mask);
  }
  else
  {
    // Rebooted - it's safe to use the battery backed registers and SRAM values
    _power_on_restart = false;
    _syslog_mask = hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_SYSLOG_MASK);
  }

  // Save for emergency debugging :-)
  // _syslog_mask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
  //             LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_NOTICE) | 
  //             LOG_MASK(LOG_INFO);  // | LOG_MASK(LOG_DEBUG);


  // Sets new mask and returns the previous syslog_mask
  _syslog_mask_old = setlogmask(_syslog_mask);
  if (_syslog_mask_old < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setlogmask err:%d\n", thisFile, __LINE__, _syslog_mask_old);
    return _syslog_mask_old;
  }

  return OK;
}
#endif

//=============================================================
int hcom_manager_setup(FAR struct mtd_dev_s *mtd)
{
  int ret;

  if (mtd == NULL)
  {
    return ERROR;
  }
  
  ret = hcom_utils_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup hcom utils:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Determine if there's any special action required by mono_main. This sets up
  // variables within mono_main.c before it is started by nuttx. When mono_main is started
  // it checks if special action is necessary.
  hcom_utils_boot_time_mono_check();

#if defined (CONFIG_RAMLOG_SYSLOG)
  // Sets up some basic initialization for ramlog, but does not
  // create the receive thread etc. as this is not a commonly
  // needed feature.
  ret = hcom_ramlog_trace_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup log tracing %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  // Sets the MTD for the file system
  // Note: the calling thread is the nuttx startup thread. Any activity here may delay the
  //  remainder of nuttx from starting, which may be determined to be a good thing.
  ret = hcom_exec_flash_fs_setup(mtd);
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup F/S %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Sets a few internal variable states
  ret = hcom_exec_rqst_misc_setup(mtd);
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
  // Sets a few internal variable states
  ret = hcom_exec_rqst_download_file_rqst_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup file download %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Sets a few internal variable states
  ret = hcom_file_commands_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup file cmds %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Allocates memory for the circular buffer
  ret = hcom_save_parse_request_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup host request %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_fs_setup(mtd);
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup F/S helper %d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if defined(CONFIG_HCOM_MTD_STRESS_TEST)
  ret = hcom_exec_rqst_testing_setup(mtd);
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup testing %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  ret = hcom_comms_recv_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup Host comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_comms_send_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup host msg builder:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if defined(CONFIG_HCOM_FILESYSTEM_INIT)
  // This will format the file system as needed. This will prevent the nuttx OS from
  // starting which includes mono. Therefore, mono cannot start until the file system
  // is at least initialized. This is the desired behavior since mono starting before
  // the file system would be a problem.
  ret = hcom_fs_init_file_system();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup F/S helper %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if defined(CONFIG_HCOM_MONO_OUTPUT_PIPE)
  // Creates a named pipe (fifo) and creates a receiving thread.
  ret = hcom_mono_pipe_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup mono pipe %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if defined(CONFIG_HCOM_MONO_DEBUG_PIPE)
  // Creates a unix domain socket and creates a receiving thread.
  ret = hcom_remote_dbg_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup remote dbg %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if defined (CONFIG_HCOM_ESP32_COMMS)
  ret = hcom_esp32_uart_comms_setup();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-setup esp32 comms %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  // Do this last! - Create a thread to handle receiving and responding to received messages
  ret = hcom_manager_create_worker_thread();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-create hcom thread %s\n", thisFile, __LINE__, ret);
    return ret;
  }

  return OK;
}

//===============================================================
// Create a thread to handle the work. It may create a pthread
// or kernel thread depending on the build configuration
int hcom_manager_create_worker_thread()
{
#ifdef CONFIG_BUILD_PROTECTED
  // Note: I've seen the reported stack size at 0x17e4 (6116)
  _hcom_pid = kthread_create(HCOM_THREAD_NAME_HCOM_RECEIVE,
    HCOM_THREAD_PRIORITY_HCOM_RECEIVE,
    8192, (main_t)hcom_comms_recv_worker_kthread,
    (FAR char * const *)  NULL);
  if(_hcom_pid <= 0)
  {
    return -ENOEXEC;
  }
#else
  int ret;
  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;

  param.sched_priority = 100;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setschedparam(&attr, &param);

  // Note: earlier a stack size of 2048 had trouble
  // doubling solved problem. Alot of the things that where on the
  // stack have been removed. 2048 may now be good enough (peter 4Jun19)
  (void)pthread_attr_setstacksize(&attr, 4096);

  //int pthread_create(FAR pthread_t *thread, FAR const pthread_attr_t *attr,
  //             pthread_startroutine_t start_routine, pthread_addr_t arg)
  ret = pthread_create(&thread, &attr, hcom_comms_recv_worker_pthread, NULL);
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-%s thread err:%d\n", thisFile, __LINE__,
            HCOM_THREAD_NAME_HCOM_RECEIVE, ret);
    return ret;
  }
#endif

  // Let the Nuttx startup thread continue with its work
  return OK;
}

//============================================================================
// hcom main thread
#ifdef CONFIG_BUILD_PROTECTED
int hcom_comms_recv_worker_kthread(int argc, char *argv[])
#else
FAR void *hcom_comms_recv_worker_pthread(FAR void *arg)
#endif
{
  int ret;

#if HCOM_TASK_SHOW_CREATED_TASK_PID_NAME > 0
  struct tcb_s *rtcb = this_task();
  hcom_utils_f7syslog(LOG_NOTICE, "PID:%d is '%s'\n", getpid(), rtcb->name);
#endif

  char *traceCombo[] = {
    "none",
    "Host",
    "UART1",
    "Host+UART1"};
  // Assumes host and uart1 are bits 1 & 2
  uint32_t value = hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_BIT_FLAGS);
  value >>= 1;
  char *traceDest = traceCombo[value & 0x00000003];

  // Provide some information that may be useful
  hcom_utils_f7syslog(LOG_INFO, "Meadow %s (%s@%s) %s, Mono:%s, Trace level:0x%02x(was 0x%02x), to:%s, type:%s\n",
        HCOM_DEVICE_INFO_MEADOW_OS_VERSION, __DATE__, __TIME__, 
        _power_on_restart ? "power-on restart" :"rebooted",
        hcom_utils_is_mono_disabled() ? "Disabled" : "Enabled",
        _syslog_mask, _syslog_mask_old, traceDest,
#if defined CONFIG_RAMLOG_SYSLOG
        "ramlog");
#else
        "syslog");
#endif

  //-------------------------------------------------------
  // Main thread only returns on shutdown or serious error
  //-------------------------------------------------------  
  ret = hcom_comms_recv_thread_loop();
  if (ret < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-%s thread exit:%d\n", thisFile, __LINE__, HCOM_THREAD_NAME_HCOM_RECEIVE, ret);
  }

  hcom_manager_shutdown();

  hcom_utils_f7syslog(LOG_INFO, "%s thread exit'\n", HCOM_THREAD_NAME_HCOM_RECEIVE);

#ifdef CONFIG_BUILD_PROTECTED
  return OK;      // Thread exit
#else
  return NULL;    // Keeps compiler happy
#endif
}

//=========================================================================
// Notify all interested children that we are shutting down. This closes the
// comms file descriptor which will cause the worker thread to exit.
void hcom_manager_shutdown()
{
  // todo - confirm that all functions that need shutdown are called
  hcom_comms_recv_shutdown();
  hcom_mono_pipe_shutdown();
  hcom_utils_shutdown();
  hcom_save_parse_request_shutdown();
  hcom_comms_send_msg_shutdown();
  hcom_file_commands_shutdown();
  hcom_fs_shutdown();
  hcom_remote_dbg_shutdown();
#if defined (CONFIG_HCOM_ESP32_COMMS)
  hcom_esp32_uart_comms_shutdown();
#endif
#if defined (CONFIG_RAMLOG_SYSLOG)
  hcom_ramlog_trace_shutdown();
#endif
}
