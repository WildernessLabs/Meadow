/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_uart_comms.c
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

#include "../hcom_common.h"
#include "hcom_esp32_comms.h"

#include <nuttx/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// WARNING: This code makes the assumption that the UART connected to the 
// esp32 chip is UART5 and has been registered with Nuttx as /dev/ttyS2.
// See ...nuttx\arch\arm\src\stm32f7\stm32_serial.c @ 3100 and following
// for where this assignment is made.

// A few common defines
#define HCOM_ESP32_FLASH_UART_DEV_NAME "/dev/ttyS2"   // UART 5
#define HCOM_ESP32_FLASH_UART_READ_BUF_SIZE 512

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static pthread_t _esp32_recv_thread;
static int _esp32_read_fd;
static int _esp32_write_fd;
static sem_t _initalizeWaitSem;    /* Implements event waiting */

static uint8_t *esp32_read_buffer;
static bool _esp_uart_initialized;
static bool _init_failed;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *hcom_esp32_uart_comms_pthread(FAR void *arg);

static int hcom_esp32_uart_comms_make_thread(void);
static int hcom_esp32_uart_open_serial_ports(void);
static int hcom_esp32_uart_comms_read_serial_loop(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// We'll do a minimum of initialization because this is a rarely used feature.
// The first time it's used we'll finish the initialization.
int hcom_esp32_uart_comms_setup()
{
  _shutting_down = false;
  _esp_uart_initialized = false;
  _esp32_write_fd = -1;
  _esp32_read_fd = -1;

  return OK;
}

//====================================================================
// This is called after most/all commands have finished. It stops the
// receive thread and prepares the system to restart.
// Why? Because the ESP32 constantly sends text when not communicating
// in binary data. We cannot afford such an expensive operation for
// the F7 MCU. And this functionality will only be used < 0.0001% of
// the time.
void hcom_esp32_stop_and_prep_for_restart()
{
  usleep(20 * 1000);
  hcom_esp32_uart_comms_shutdown();
  usleep(20 * 1000);
}

//====================================================================
// Both hcom_esp32_stop_and_prep_for_restart() and Startup Manager call
// here
void hcom_esp32_uart_comms_shutdown()
{
  int ret;

  _shutting_down = true;

  hcom_esp32_recv_shutdown();
  hcom_esp32_xmit_shutdown();
  hcom_esp32_exec_shutdown();
  hcom_esp32_util_shutdown();

  // Kill esp32 receive thread
  pthread_cancel(_esp32_recv_thread);

  ret = pthread_join(_esp32_recv_thread, NULL);
  if(ret != 0)
  {
    syslog(LOG_ERR, "%s@%d-pthread join failed:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return;
  }

  // Close everything open
  if(_esp32_read_fd > -1)
  {
    close(_esp32_read_fd);
    _esp32_read_fd = -1;
  }

  if(_esp32_write_fd > -1)
  {
    close(_esp32_write_fd);
    _esp32_write_fd = -1;
  }

  if(esp32_read_buffer != NULL)
     free(esp32_read_buffer);

  _esp_uart_initialized = false;
  _init_failed = 0;
  _shutting_down = false;
}

//===================================================================================
// Wait for the thread holding semaphore to release it
static void hcom_esp32_uart_takesem(sem_t *semaphore)
{
  int ret;
  DEBUGASSERT(semaphore != NULL);

  do
  {
    ret = sem_wait(semaphore);    // Take the semaphore (perhaps waiting)
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
    hcom_logging_syslog(LOG_ERR, "%s@%d-mem alloc for esp read buffer. errno:%d\n",
              thisFile, __LINE__, errno);
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
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed init esp32 recv %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_xmit_setup_lazy();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed init esp32 xmit %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_util_setup_lazy();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed init esp32 xmit %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_esp32_exec_setup_lazy();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed init esp32 util %d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Create a thread to do the ESP32 Uart reads.
  // Note: the thread being created will eventually wake up this thread 
  ret = hcom_esp32_uart_comms_make_thread();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-thread create, ret:%d\n", thisFile, __LINE__, ret);
    _init_failed = ret;
    return ret;
  }

  // Wait for the new thread to finish its initialization before continuing.
  hcom_esp32_uart_takesem(&_initalizeWaitSem);

  // This semaphore has done it's job
  sem_destroy(&_initalizeWaitSem);

  return OK;
}

//=============================================================
int hcom_esp32_uart_comms_make_thread()
{
    int ret;
    pthread_attr_t attr;
    struct sched_param param;

    param.sched_priority = HCOM_THREAD_PRIORITY_ESP32_RECEIVE;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setschedparam(&attr, &param);
    (void)pthread_attr_setstacksize(&attr, HCOM_THREAD_STACKSIZE_ESP32_RECEIVE);

    ret = pthread_create(&_esp32_recv_thread, &attr, hcom_esp32_uart_comms_pthread, NULL);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed to create %s thread. Error:%d\n",
                thisFile, __LINE__, HCOM_THREAD_NAME_ESP32_RECEIVE, ret);
    }
    return ret;
}

//=================================================================
// This thread receives all UART messages received from ESP32
FAR void *hcom_esp32_uart_comms_pthread(FAR void *arg)
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New pthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_ESP32_RECEIVE);
#endif

  // Note: only one chance to open serial port. Should this be in the main
  // receiving loop?
  ret = hcom_esp32_uart_open_serial_ports();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open serial port failed\n", thisFile, __LINE__);
    _init_failed = ret;
    sem_post(&_initalizeWaitSem);  // Allow hcom thread to proceed
    return NULL; //  This will terminate this thread
  }

  // Thread created and initialization successful
  sem_post(&_initalizeWaitSem);  // Allow hcom thread to proceed

  // Allow hcom thread to start before using this thread. Without
  // this delay this thread start receiving ascii from ESP32 before
  // we are ready to do anything with it.
  // Consider something more deterministic?
  usleep(500 * 1000);

  while(!_shutting_down)
  {
    ret = hcom_esp32_uart_comms_read_serial_loop();
    if(_shutting_down)
    {
      break;
    }

    if(ret < 0)
    {
      usleep(100 * 1000);   // prevent tight loops
    }
  }

  return NULL;
}

//=============================================================
// This initialiation is done by the thread created to receive from ESP32
int hcom_esp32_uart_open_serial_ports()
{
  // Open the uart for read
  _esp32_read_fd = open(HCOM_ESP32_FLASH_UART_DEV_NAME, O_RDONLY);
  if (_esp32_read_fd == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open of %s for read failed. errno:%d\n", thisFile, __LINE__,
        HCOM_ESP32_FLASH_UART_DEV_NAME, errno);
    return -errno;
  }

  // Open the uart for send
  _esp32_write_fd = open(HCOM_ESP32_FLASH_UART_DEV_NAME, O_WRONLY);
  if (_esp32_write_fd == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open of %s for write failed. errno:%d\n", thisFile, __LINE__,
        HCOM_ESP32_FLASH_UART_DEV_NAME, errno);
    return -errno;
  }

  return OK;
}

//====================================================================
// Read from the esp32's serial port
int hcom_esp32_uart_comms_read_serial_loop()
{
  ssize_t readReturn;

  // Read uart
  while (!_shutting_down)
  {
    readReturn = read(_esp32_read_fd, esp32_read_buffer, HCOM_ESP32_FLASH_UART_READ_BUF_SIZE);
    if(_shutting_down)
    {
      break;
    }

    if (readReturn < 0 )
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-esp read ret:%d, errno:%d\n", thisFile, __LINE__,
        readReturn, errno);
      return -errno;
    }
    
    if (readReturn == 0)    // EOF
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d-UART read=0 (EOF)\n", thisFile, __LINE__);
      sleep(1);
      continue;
    }
    else
    {
      int ret = hcom_esp32_recv_handle_data(esp32_read_buffer, readReturn);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 recvd data not processed:%d\n",
                thisFile, __LINE__, ret);
      }
    }
  }   // while (!_shutting_down)

  if(_shutting_down)
    return -1;

  return OK;
}

//====================================================================
// Write to esp32 serial port
int hcom_esp32_uart_comms_write_serial(uint8_t* espWriteBuf, size_t espWriteSize)
{
  size_t remainingBytes = espWriteSize;
  size_t toWriteOffset = 0;
  ssize_t writeRet = 0;

  // Insure all bytes get written
  while (remainingBytes > 0)
  {
    writeRet = write(_esp32_write_fd, &espWriteBuf[toWriteOffset], remainingBytes);
    if(writeRet >= 0)
    {
      remainingBytes -= writeRet;   // Note: if remainingBytes == 0 will exit while loop
      toWriteOffset += writeRet;
      hcom_logging_syslog(LOG_DEBUG, "SUCCESS-Wrote to ESP ret:%d, total wrote:%d bytes, remaining:%d (uart_comms)\n",
                writeRet, toWriteOffset, remainingBytes);
      continue;
    }
    
    if(remainingBytes == 0)
      return toWriteOffset;

    hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 write errno:%d\n", thisFile, __LINE__, errno);
  }
  return writeRet;
}
