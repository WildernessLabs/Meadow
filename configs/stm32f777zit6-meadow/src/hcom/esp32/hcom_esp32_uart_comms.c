/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_esp32_uart_comms.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
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
#include "hcom_esp32_comms.h"

#include <nuttx/board.h>
#include <arch/board/board.h>
#include "stm32_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// WARNING: This code makes the assumption that the UART connected to the 
// esp32 chip is UART5 and has been registered with Nuttx as /dev/ttyS2.
// See ...nuttx\arch\arm\src\stm32f7\stm32_serial.c @ 3100 and following
// for where this assignment is made.
#define HCOM_ESP32_FLASH_UART_DEV_NAME "/dev/ttyS2"
#define HCOM_ESP32_FLASH_UART_READ_BUF_SIZE 512

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static bool _is_esp32_read_open;
static FAR struct file _esp32_read_fd;
static int _is_esp32_write_open;
static FAR struct file _esp32_write_fd;
static sem_t _initalizeWaitSem;    /* Implements event waiting */

static uint8_t *esp32_read_buffer;
static bool _esp_uart_initialized;
static bool _init_failed;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_BUILD_PROTECTED
static int hcom_esp32_uart_comms_kthread(int argc, char *argv[]);
#else
static FAR void *hcom_esp32_uart_comms_pthread(FAR void *arg);
#endif

static int hcom_esp32_uart_comms_make_thread(void);
static int hcom_esp32_uart_phase2_initialization(void);
static int hcom_esp32_uart_comms_read_serial_loop(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// We'll do a minimum of initialization because this is a rarely used feature
int hcom_esp32_uart_comms_setup()
{
  int ret;

  _shutting_down = false;
  _esp_uart_initialized = false;

  _is_esp32_read_open = false;
  _is_esp32_write_open = false;

  ret = stm32_configgpio(MEADOW_ESP32_ONBOARD_BOOT_PIN);
  if(ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Config GPIO D10 failed ret:%d\n", thisFile, __LINE__, ret);
    return -1;
  }

  ret = stm32_configgpio(MEADOW_ESP32_ONBOARD_RESET_PIN);
  if(ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Config GPIO D7 failed ret:%d\n", thisFile, __LINE__, ret);
    return -1;
  }

  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_BOOT_PIN, DIGITAL_OUTPUT_STATE_HIGH);
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_RESET_PIN, DIGITAL_OUTPUT_STATE_HIGH);

  return OK;
}

//====================================================================
void hcom_esp32_uart_comms_shutdown()
{
  _shutting_down = true;

  hcom_esp32_recv_shutdown();
  hcom_esp32_xmit_shutdown();
  hcom_esp32_exec_shutdown();
  hcom_esp32_util_shutdown();

  if(_is_esp32_read_open == true)
    file_close(&_esp32_read_fd);

  if(_is_esp32_write_open == true)
    file_close(&_esp32_write_fd);

  if(esp32_read_buffer != NULL)
    free(esp32_read_buffer);
}

//===================================================================================
// Wait for the thread holding semaphore to release it
static void hcom_esp32_uart_takesem(void)
{
  int ret;
  do
  {
    ret = sem_wait(&_initalizeWaitSem);    // Take the semaphore (perhaps waiting)
    // The only case that an error should occur here is if the wait was awakened by a signal
    DEBUGASSERT(ret == OK || ret == -EINTR);
  }
  while (ret == -EINTR);
}

//====================================================================
// This initialize is done by the hcom caller
// Since this functionality is seldom used we're using lazy
// initialization.
int hcom_esp32_uart_lazy_initialization()
{
  int ret;
  
  // Already initialized?
  if(_esp_uart_initialized)
  {
    if(_init_failed < 0)
      return _init_failed;
    return OK;
  }

  _esp_uart_initialized = true;

  esp32_read_buffer = malloc(HCOM_ESP32_FLASH_UART_READ_BUF_SIZE);
  if(esp32_read_buffer == NULL)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:mem alloc for esp read buffer. errno:%d\n", thisFile, __LINE__, errno);
    _init_failed = -ENOMEM;
    return -ENOMEM;
  }
    
  // Initialize value to 0 for a 'signaling' semaphore to block
  // initially the calling thread until it is safe for it to proceed.
  sem_init(&_initalizeWaitSem, 0, 0);
  // Special non-standard nuttx function required for signaling semaphores
  sem_setprotocol(&_initalizeWaitSem, SEM_PRIO_NONE);

  // Setup all the subordinate modules
  ret = hcom_esp32_recv_setup_lazy();
  if (ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Error:Failed init esp32 recv %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_xmit_setup_lazy();
  if (ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Error:Failed init esp32 xmit %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_util_setup_lazy();
  if (ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Error:Failed init esp32 xmit %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_exec_setup_lazy();
  if (ret < 0)
  {
    f7syslog(LOG_CRIT, "%s@%d-Error:Failed init esp32 util %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Create a thread to do the ESP32 Uart reads.
  // Note: the thread being created will eventually wake up this thread 
  ret = hcom_esp32_uart_comms_make_thread();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:thread create, ret:%d\n", thisFile, __LINE__, ret);
    _init_failed = ret;
    return ret;
  }

  // Wait for the new thread to finish its initialization before continuing.
  hcom_esp32_uart_takesem();

  // This semaphore has done it's job
  sem_destroy(&_initalizeWaitSem);
  return OK;
}

//=============================================================
// This initialiation is done by the thread created to receive from ESP32
int hcom_esp32_uart_phase2_initialization()
{
  int ret;

  // Open the uart device
  ret = file_open(&_esp32_read_fd, HCOM_ESP32_FLASH_UART_DEV_NAME, O_RDONLY);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:open of %s for read failed. errno:%d\n", thisFile, __LINE__,
        HCOM_ESP32_FLASH_UART_DEV_NAME, errno);
    return ret;
  }
  _is_esp32_read_open = true;

  ret = file_open(&_esp32_write_fd, HCOM_ESP32_FLASH_UART_DEV_NAME, O_WRONLY);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:open of %s for write failed. errno:%d\n", thisFile, __LINE__,
        HCOM_ESP32_FLASH_UART_DEV_NAME, errno);
    return ret;
  }
  _is_esp32_write_open = true;
  
  return OK;
}

//=============================================================
int hcom_esp32_uart_comms_make_thread()
{
  #ifdef CONFIG_BUILD_PROTECTED
    int pid = kthread_create(
      HCOM_THREAD_NAME_ESP32_RECEIVE,
      HCOM_THREAD_PRIORITY_ESP32_RECEIVE,
      2048, (main_t)hcom_esp32_uart_comms_kthread,
      (FAR char * const *)  NULL);
    if(pid <= 0)
    {
      return -ENOEXEC;
    }
    return OK;
  #else
    int ret;
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = 120;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, 1024);

    ret = pthread_create(&thread, &attr, hcom_esp32_uart_comms_pthread, NULL);
    if (ret < 0)
    {
      f7syslog(LOG_CRIT, "%s@%d-Error:Failed to create thread. Error %s\n", thisFile, __LINE__, ret);
    }
    return ret;
  #endif
}

//=================================================================
// This thread receives all UART messages received from ESP32
#ifdef CONFIG_BUILD_PROTECTED
int hcom_esp32_uart_comms_kthread(int argc, char *argv[])
#else
FAR void *hcom_esp32_uart_comms_pthread(FAR void *arg)
#endif
{
  int ret;

  ret = hcom_esp32_uart_phase2_initialization();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:phase 2 initialization failed\n", thisFile, __LINE__);
    // Error noted
    _init_failed = ret;
    sem_post(&_initalizeWaitSem);  // Allow hcom thread to proceed
    return ret; //  This will kill this thread
  }

  // Thread created and initialization successful
  sem_post(&_initalizeWaitSem);  // Allow hcom thread to proceed

  // Allow hcom thread to start before using this thread
  usleep(500 * 1000);

  while(!_shutting_down)
  {
    ret = hcom_esp32_uart_comms_read_serial_loop();
    if(ret < 0)
    {
      sleep(1);   // prevent tight loops
    }
  }

#ifdef CONFIG_BUILD_PROTECTED
  return OK;
#else
  return NULL;    // Keeps compiler happy
#endif
}

//====================================================================
// Read from the esp32's serial port
int hcom_esp32_uart_comms_read_serial_loop()
{
  ssize_t readReturn;

  // Read uart
  while (!_shutting_down)
  {
    readReturn = file_read(&_esp32_read_fd, esp32_read_buffer, HCOM_ESP32_FLASH_UART_READ_BUF_SIZE);
    if (readReturn < 0 )
    {
      f7syslog(LOG_ERR, "%s@%d-Error:esp read ret:%d, errno:%d\n", thisFile, __LINE__,
        readReturn, errno);
      return -errno;
    }
    
    if (readReturn == 0)    // EOF
    {
      f7syslog(LOG_WARNING, "%s@%d-UART read=0 (EOF)\n", thisFile, __LINE__);
      sleep(1);
      continue;
    }
    else
    {
      int ret = hcom_esp32_recv_handle_data(esp32_read_buffer, readReturn);
      if(ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:ESP32 recvd data not processed:%d\n",
                thisFile, __LINE__, ret);
      }
    }
  }   // while (!_shutting_down)

  return OK;
}

//====================================================================
// Write to esp32 serial port
int hcom_esp32_uart_comms_write_serial(uint8_t* espWriteBuf, size_t espWriteSize)
{
  size_t remainingBytes = espWriteSize;
  size_t toWriteOffset = 0;
  ssize_t writeRet;

  // Insure all bytes get written
  while (remainingBytes > 0)
  {
    writeRet = file_write(&_esp32_write_fd, &espWriteBuf[toWriteOffset], remainingBytes);
    if(writeRet >= 0)
    {
      remainingBytes -= writeRet;   // Note: if remainingBytes == 0 will exit while loop
      toWriteOffset += writeRet;
      continue;
    }
    
    if(remainingBytes == 0)
      return toWriteOffset;

    f7syslog(LOG_ERR, "%s@%d-Error:ESP32 write errno %d\n", thisFile, __LINE__, errno);

    return writeRet;
  }
  return writeRet;
}

