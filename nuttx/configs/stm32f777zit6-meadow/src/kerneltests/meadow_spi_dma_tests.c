/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\spi_dma_tests.c
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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

// This file contains SPI test developed when adding DMA to SPI for Meadow.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx/hcom_nx_common.h"
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>
#include <nuttx/kthread.h>
#include <nuttx/spi/spi.h>
#include "stm32_spi.h"
#include <up_arch.h>

#include <nuttx/spi/spi.h>
#include "stm32_spi.h"
#include <up_arch.h>

// Only build if configured
#if defined(CONFIG_SPI_DMA_TESTS)
#pragma message "(--) spi_dma_tests.c"

// Diagnostic always as this is test code
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#if defined (USE_MEADOW_DEBUG_HELPERS)
#include "stm32_gpio.h"
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

<<<<<<< HEAD
#define MEADOW_SPI_TEST_ECHO_SPI_FREQ (25000000)   // Want 24MHz for testing
#define MEADOW_SPI_TEST_ECHO_BUF_SIZE (2048)
=======
#define MEADOW_SPI_TEST_ECHO_SPI_FREQ (400000)   // 400KHz
#define MEADOW_SPI_TEST_ECHO_BUF_SIZE (32)
>>>>>>> 35abb627e1f4e8ee766293b718fc64b24a4f5b5e
#define MEADOW_SPI_TEST_ECHO_LOOP_CNT (1)

/************************************************************************************
 * Private Data
 ************************************************************************************/
<<<<<<< HEAD
struct spi_dev_s *_spiDev3 = NULL;
static struct work_s spi_test_work;
=======
static bool _canLoopback;
struct spi_dev_s *_spiDev3 = NULL;
>>>>>>> 35abb627e1f4e8ee766293b718fc64b24a4f5b5e

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void spi_loopback_test(uint32_t userData);
static int spi_dma_tests_set_bus_params(struct spi_dev_s **spiDev, int bus, bool isPeriph);
<<<<<<< HEAD
static void spi_test_main_work_function(FAR void *arg);
=======
static void *spi_test_main_thread_func(int argc, char *argv[]);
>>>>>>> 35abb627e1f4e8ee766293b718fc64b24a4f5b5e
static void execute_loopback_test(void);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// set developer -d 17 come here
void meadow_kt_spi_dma_tests(uint32_t userData)
{
  syslog(2, "SPI DMA tests received 'set developer -d 17 -v %lu'.\n", userData);

  switch(userData)
  {
    case 1:
      spi_loopback_test(userData);
      break;

    default:
      syslog(2, "Undefined test for meadow_kt_spi_dma_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
void spi_loopback_test(uint32_t userData)
{
#if defined(CONFIG_STM32F7_SPI_DMA)
  syslog(2, "CONFIG_STM32F7_SPI_DMA configured. Running test using DMA\n");
#else
  syslog(2, "CONFIG_STM32F7_SPI_DMA not configured. Running test without DMA\n");
#endif

#if !defined(CONFIG_STM32F7_SPI3)
  syslog(2, "%s@%d-SPI3 not defined, cannot run test\n", __FILE__, __LINE__); usleep(30 * 1000);
  return;
#endif

  work_queue(HPWORK, &spi_test_work, spi_test_main_work_function, NULL, 0);
}

//===================================================================
// This thread will manage the test and create a secondary thread to handle
// echoing the data
static void spi_test_main_work_function(FAR void *arg)
{
  int ret;
  _spiDev3 = NULL;

  // Also set GPIO low
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_A04_PB1);

  // We want to measure the time to do a SPI data exchange
  syslog(2, "%s@%d-Work thread executing\n", __FILE__, __LINE__); usleep(30 * 1000);

#if defined(CONFIG_STM32F7_SPI3)
  // Initialize SPI3
  syslog(2, "%s@%d-Executing config for SPI 3\n", __FILE__, __LINE__); usleep(30 * 1000);
  ret = spi_dma_tests_set_bus_params(&_spiDev3, 3, false);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not setup bus%d\n", __FILE__, __LINE__, 3);
    return NULL;
  }

  if(_spiDev3 == NULL)
  {
    syslog(2, "%s@%d-Setup returned spiDev:%p\n", __FILE__, __LINE__, _spiDev3);
    return NULL;
  }
#endif

#if defined (CONFIG_STM32F7_SPI_DMA)
  // DMA testing
#else
  // No DMA Testing
#endif

  syslog(2, "%s@%d-Executing config for SPI 3\n", __FILE__, __LINE__); usleep(30 * 1000);
  execute_loopback_test();

  syslog(2, "%s@%d-worker queue exiting\n", __FILE__, __LINE__); usleep(30 * 1000);
  return NULL;    // Thread exit
}

//=====================================================================
int spi_dma_tests_set_bus_params(struct spi_dev_s **spiDev, int bus, bool isPeriph)
{
  *spiDev = stm32_spibus_initialize(bus);  // Nuttx function
  if(*spiDev == NULL)
  {
    syslog(2, "%s@%d-SPI%d failed to initialize\n", __FILE__, __LINE__, bus);
    return -ENODEV;
  }

  SPI_SETFREQUENCY(*spiDev, MEADOW_SPI_TEST_ECHO_SPI_FREQ);

  // Set the mode 0-4
  // SPIDEV_MODE0: /* CPOL=0; CPHA=0 */
  // SPIDEV_MODE1: /* CPOL=0; CPHA=1 */
  // SPIDEV_MODE2: /* CPOL=1; CPHA=0 */
  // SPIDEV_MODE3: /* CPOL=1; CPHA=1 */
  SPI_SETMODE(*spiDev, SPIDEV_MODE0);

  // Set the number of bits per word. 4 - 32 is legal
  SPI_SETBITS(*spiDev, 8);
  return OK;
}

//==================================================================
// This function is responsible for sending and receiving as a Controller
void execute_loopback_test()
{
  int bufOff;
  uint8_t *txBuff3;
  uint8_t *rxBuff3;
  int score = 0;

  syslog(2, "%s@%d-About to allocate buffer memory.\n", __FILE__, __LINE__); usleep(30 * 1000);
  txBuff3 = memalign(ARMV7M_DCACHE_LINESIZE, MEADOW_SPI_TEST_ECHO_BUF_SIZE);
  if(txBuff3 == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for txBuff3\n", __FILE__, __LINE__); usleep(30 * 1000);
    return;
  }
  rxBuff3 = memalign(ARMV7M_DCACHE_LINESIZE, MEADOW_SPI_TEST_ECHO_BUF_SIZE);
  if(rxBuff3 == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for rxBuff3\n", __FILE__, __LINE__);
    return;
  }
  syslog(2, "%s@%d-memory allocated, txBuff3:%p, rxBuff3:%p\n",
            __FILE__, __LINE__, txBuff3, rxBuff3); usleep(30 * 1000);

  // Fill the initial send buffer with "data"
  for(bufOff = 0; bufOff < MEADOW_SPI_TEST_ECHO_BUF_SIZE; bufOff++)
  {
    // 0x00-0xff and repeat pattern
    txBuff3[bufOff] = bufOff & 0xff;
  }

  // Fill the final receive buffer with a different pattern
  // memset?
  for(bufOff = 0; bufOff < MEADOW_SPI_TEST_ECHO_BUF_SIZE; bufOff++)
  {
    rxBuff3[bufOff] = 0x55;
  }

  // Send repeatedly
  for(int loopCnt = 0; loopCnt < MEADOW_SPI_TEST_ECHO_LOOP_CNT; loopCnt++)
  {
    syslog(2, "%s@%d-Executing test.\n", __FILE__, __LINE__); usleep(30 * 1000);
    DEBUG_SET_HIGH(DEBUG_PIN_CCM_A04_PB1);

    SPI_EXCHANGE(_spiDev3, txBuff3, rxBuff3, MEADOW_SPI_TEST_ECHO_BUF_SIZE);

    DEBUG_SET_LOW(DEBUG_PIN_CCM_A04_PB1);

    // Compare data sent with data received
    int cmpResult = memcmp(txBuff3, rxBuff3, MEADOW_SPI_TEST_ECHO_BUF_SIZE);
    if(cmpResult == 0)
    {
      score++;
    }
  }

  syslog(2, "Successful transfered:%d of %d\n", score, MEADOW_SPI_TEST_ECHO_LOOP_CNT);

#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
  syslog(2, "------------------------ txBuff3 ---------------------------\n");
  hcom_nx_diag_print_buffer(txBuff3, MEADOW_SPI_TEST_ECHO_BUF_SIZE, 1);
  syslog(2, "------------------------ rxBuff3 ---------------------------\n");
  hcom_nx_diag_print_buffer(rxBuff3, MEADOW_SPI_TEST_ECHO_BUF_SIZE, 1);
#endif
  usleep(30 * 1000);

  free(txBuff3);
  free(rxBuff3);

  syslog(2, "Aligned memory freed\n"); usleep(30 * 1000);
}

#endif  // #if defined(CONFIG_SPI_DMA_TESTS)
