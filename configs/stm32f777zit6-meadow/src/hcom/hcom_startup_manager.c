/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_startup_manager.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
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
#include <nuttx/kthread.h>
#include <assert.h>
#include "task/task.h"
// #include <nuttx/sched.h>
// #include <../sched/sched/sched.h>

#include <nuttx/userspace.h>  // TESTING

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct mtd_dev_s *_flash_mtd;
static int _hcom_pid;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_manager_shutdown(void);
static int hcom_manager_create_worker_thread(void);

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_receive_worker_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_receive_worker_pthread(FAR void *arg);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_manager_setup(FAR struct mtd_dev_s *flash_mtd)
{
  static bool initialized = false;
  int ret;

  // struct tcb_s *rtcb = this_task();
  // pid_t pid = getpid();
  // syslog(0, "%s() -->> Startup task = %d, name = '%s'\n", __func__, pid, rtcb->name);

  if (flash_mtd == NULL)
    return -1;

  _flash_mtd = flash_mtd;
  
  // Check if we have already initialized
  if (!initialized)
  {
    // First determine if there's any special action required by mono_main. This sets up
    // variables within mono_main.c before it is started by nuttx. When it is started it
    // checks if special action is necessary.
    hcom_boot_time_mono_check();

    // Note: the calling thread is the nuttx startup thread. Any activity here may delay the
    //  remainder of nuttx from starting, which may be determined to be a good thing.

    // Sets the mtd for the flash file system
    ret = hcom_exec_flash_fs_setup(_flash_mtd);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize flash file system setup %d\n", __func__, ret);
      return ret;
    }

    // Sets a few internal variable states
    ret = hcom_exec_rqst_misc_setup(_flash_mtd);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize request action setup %d\n", __func__, ret);
      return ret;
    }

    // Sets a few internal variable states
    ret = hcom_exec_rqst_download_file_rqst_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file download setup %d\n", __func__, ret);
      return ret;
    }

    // Does nothing
    ret = hcom_host_msg_builder_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize Host message builder setup %d\n", __func__, ret);
      return ret;
    }

    // Sets a few internal variable states
    ret = hcom_file_commands_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file processing setup %d\n", __func__, ret);
      return ret;
    }

    // Allocates memory for the circular buffer
    ret = hcom_save_parse_request_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize host request setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_fs_helper_setup(_flash_mtd);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file system helper setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_exec_rqst_testing_setup(_flash_mtd);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize testing setup %d\n", __func__, ret);
      return ret;
    }
    
    ret = hcom_usb_acm_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize Host communications setup %d\n", __func__, ret);
      return ret;
    }

    // Creates a named pipe (fifo) and starts the receiving thread.
    ret = hcom_mono_pipe_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize Appliction debug message pipe setup %d\n", __func__, ret);
      return ret;
    }

    // This call may not return for several minutes. It will format the file system if needed.
    // This will prevent the nuttx OS from starting which includes mono. Therefore, mono cannot
    // start until the file system is at least initialized. This is the desired behavior since
    // mono starting before the file system could be a problem.
    ret = hcom_fs_helper_init_file_system();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file system helper setup %d\n", __func__, ret);
      return ret;
    }

    // Do this last! - Create a thread to handle receiving and responding to received messages
    ret = hcom_manager_create_worker_thread();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to create worker thread %s\n", __func__, ret);
      return ret;
    }

    initialized = true;
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
  _hcom_pid = kthread_create("hcom thread",
    120, 8192, (main_t)hcom_receive_worker_kthread,
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
  ret = pthread_create(&thread, &attr, hcom_receive_worker_pthread, NULL);
  if (ret != OK)
  {
    f7syslog(LOG_CRIT, "%s() ERROR: Failed to create thread. Error %s\n", __func__, ret);
    return ret;
  }
#endif

  // Let the Nuttx startup thread continue with its work
  return OK;
}

//============================================================================
// hcom main thread
#ifdef CONFIG_BUILD_PROTECTED
int hcom_receive_worker_kthread(int argc, char *argv[])
#else
FAR void *hcom_receive_worker_pthread(FAR void *arg)
#endif
{
  int ret;

  //-------------------------------------------------------
  // Main thread only returns on shutdown or serious error
  //-------------------------------------------------------
  ret = hcom_usb_acm_recv_thread_loop();
  if (ret < 0)
  {
    f7syslog(LOG_CRIT, "%s() ERROR: Host communications thread exited unexpectedly. ret = %d\n", __func__, ret);
  }

  hcom_manager_shutdown();

  f7syslog(LOG_INFO, "Hcom receive worker thread exiting'\n");

#ifdef CONFIG_BUILD_PROTECTED
  return OK;   // Thread exit
#else
  return NULL;    // Keeps compiler happy
#endif
}

//=========================================================================
// Notify all interested children that we are shutting down. This closes the
// comms file descriptor which will cause the worker thread to exit.
void hcom_manager_shutdown()
{
  hcom_usb_acm_shutdown();
  hcom_mono_pipe_shutdown();  
  hcom_save_parse_request_shutdown();
  hcom_host_msg_builder_shutdown();
  hcom_file_commands_shutdown();
  hcom_fs_helper_shutdown();
}
