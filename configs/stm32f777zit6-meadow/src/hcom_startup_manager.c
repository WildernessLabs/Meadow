/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_startup_manager.c
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static int _hcom_communications_fd; // Used for send and receiving from host

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_receive_worker_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_receive_worker_pthread(FAR void *arg);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_manager_setup
 *
 * Description:
 *   Initialize meadow host communications.
 *
 ****************************************************************************/
int hcom_manager_setup(FAR struct mtd_dev_s *mtd)
{
  static bool initialized = false;
  int ret;

  if (mtd == NULL)
    return -1;

  // Check if we have already initialized
  if (!initialized)
  {
    ret = hcom_file_processing_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file processing setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_parse_request_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize host request setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_fs_helper_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file system helper setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_exec_utility_request_setup(mtd);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize request action setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_exec_download_file_rqst_setup();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize file download setup %d\n", __func__, ret);
      return ret;
    }

    ret = hcom_exec_flash_fs_setup(mtd); //new
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to initialize flash file system setup %d\n", __func__, ret);
      return ret;
    }

    // Do this last! - Create a thread to handle receiving and responding to received messages
    ret = hcom_manager_create_worker_thread();
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s() ERROR: Failed to create worker thread %s\n", __func__, ret);
      return -ret;
    }

    f7syslog(LOG_NOTICE, "%s() 'Thread created' SUCCESSFUL\n", __func__);
    initialized = true;
  }
  return OK;
}

//---------------------------------------------------------------
// Notify all children that we are shutting down. This closes the
// comms file descriptor which will cause the worker thread to
// exit.
void hcom_manager_shutdown()
{
  hcom_receiver_shutdown();
  hcom_transmitter_shutdown();

  hcom_file_processing_shutdown();
  hcom_fs_helper_shutdown();

  close(_hcom_communications_fd);
}

//---------------------------------------------------
// Create a thread to handle the work. It may create a pthread
// or kernel thread depending on the build configuration
int hcom_manager_create_worker_thread()
{
#ifdef CONFIG_BUILD_PROTECTED
  // Note: earlier a stack size of 2048 had trouble
  // doubling solved problem. Alot of the things that where on the
  // stack have been removed. 2048 may now be good enough (peter 4Jun19)
  // See \nuttx\sched\task\task_create.c
  // int kthread_create(FAR const char *name, int priority, int stack_size,
  //                    main_t entry, FAR char * const argv[]);
  int pid = kthread_create("hcom thread",
    100, 4096, (main_t)hcom_receive_worker_kthread,
    (FAR char * const *)  NULL);
  if(pid <= 0)
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

  return OK;
}

//-----------------------------------------------
// hcom worker thread
#ifdef CONFIG_BUILD_PROTECTED
int hcom_receive_worker_kthread(int argc, char *argv[])
#else
FAR void *hcom_receive_worker_pthread(FAR void *arg)
#endif
{
  int ret;
  FAR const char *devname = HCOM_COMMUNICATIONS_DEVICE_NAME;
  useconds_t hostConnectionAttemptCount = HCOM_CONNECTION_STARTUP_ATTEMPTS;

  // Give the Nuttx startup thread a chance to finish, at least until /dev/ttyACM0 exists
  f7syslog(LOG_DEBUG, "Attempting connection to %s\n", devname);

  for (;;)
  {
    _hcom_communications_fd = open(devname, O_RDWR);
    if (_hcom_communications_fd >= 0)
      break;

    if (hostConnectionAttemptCount > 0)
    {
      hostConnectionAttemptCount--;
    }
    usleep(hostConnectionAttemptCount > 0 ? HCOM_CONNECTION_TIMEOUT_STARTUP : HCOM_CONNECTION_TIMEOUT_RUNNING);
  }

  f7syslog(LOG_INFO, "Connection made to %s. Waiting for host communications.\n", devname);

  // Setup transmitter
  ret = hcom_transmitter_setup(_hcom_communications_fd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Failed to initialize host com transmitter setup %d\n", __func__, ret);
#ifdef CONFIG_BUILD_PROTECTED
    return ret;
#else
    return NULL;
#endif
  }

  // Setup receiver
  ret = hcom_receiver_setup(_hcom_communications_fd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Failed to initialize host com receiver setup %d\n", __func__, ret);
#ifdef CONFIG_BUILD_PROTECTED
    return ret;
#else
    return NULL;
#endif
  }

  //    // THE FOLLOWING IS ONLY INTENDED FOR JOAO AS A TEMPORARY WAY TO DUPLICATE A PROBLEM
  // #if 1 // This will automatically bulk erase the entire chip (takes about 2 minutes)
  //    hcom_exec_utility_request_flash_bulk_erase(NULL, 0, 0);
  // #endif

  // #if 1 // This will automatically create 2 partitions and call smart_initialize
  //    hcom_exec_flash_fs_partition(NULL, 0, 2);     // Create 2 partitions

  //    hcom_exec_flash_fs_initialize(NULL, 0, 0);    // Initialize partition 0
  //    hcom_exec_flash_fs_initialize(NULL, 0, 1);    // Initialize partition 1
  // #endif
  //    // THE ABOVE IS ONLY INTENDED FOR JOAO AS A TEMPORARY WAY TO DUPLICATE A PROBLEM

  // Start receiving from host.
  hcom_receiver_receive_data_thread();
  // Thread only returns on shutdown.

  hcom_manager_shutdown();
  f7syslog(LOG_INFO, "Hcom receive worker thread exited'\n");
#ifdef CONFIG_BUILD_PROTECTED
    return OK;
#else
    return NULL;    // Keeps compiler happy
#endif
}