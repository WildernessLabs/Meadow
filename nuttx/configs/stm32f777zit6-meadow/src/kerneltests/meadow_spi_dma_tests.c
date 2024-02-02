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

#define MEADOW_SPI_TESTING_SPI_FREQ (25000000)   // Want 24MHz for testing
#define MEADOW_SPI_TESTING_TX_ALIGN_OFF (7)   // Want 24MHz for testing
#define MEADOW_SPI_TESTING_RX_ALIGN_OFF (3)   // Want 24MHz for testing

/************************************************************************************
 * Private Data
 ************************************************************************************/
static struct work_s spi_test_work;

struct SPITestingOptions_s
{
  struct spi_dev_s *spiDev;
  uint32_t repeatExchange;
  uint32_t spiNumber;
  size_t msgBits;
  size_t bufferSize;
  bool isMemAligned;
};
typedef struct SPITestingOptions_s SPITestingOptions;

SPITestingOptions SpiTestOps;
SPITestingOptions *_testOps = &SpiTestOps;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int spi_initiate_loopback_test(SPITestingOptions *testOps);
static void spi_test_main_work_function(FAR void *arg);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// developer -d 17 comes here
void meadow_kt_spi_dma_tests(uint32_t userData)
{
  memset(_testOps, 0, sizeof(SPITestingOptions));
  // Default values SPI3, aligned 2k buffer, 8 byte message, no repeat
  _testOps->repeatExchange = 1;
  _testOps->spiNumber = 3;
  _testOps->isMemAligned = true;
  _testOps->bufferSize = 2048;
  _testOps->msgBits = 8;

  switch(userData)
  {
    case 0:      // 4k buffer
      _testOps->bufferSize = 4096;
      break;

    case 1:      // 3k buffer
      _testOps->bufferSize = 3072;
      break;

    case 2:      // 2k buffer (default)
      break;

    case 3:      // 1k buffer
      _testOps->bufferSize = 1024;
      break;

    case 4:      // 512 buffer
      _testOps->bufferSize = 512;
      break;

    case 5:      // 256 buffer
      _testOps->bufferSize = 256;
      break;

    case 6:      // 128 buffer
      _testOps->bufferSize = 128;
      break;

    case 7:      // 64 buffer
      _testOps->bufferSize = 64;
      break;

    case 8:      // 32 buffer
      _testOps->bufferSize = 32;
      break;

    case 9:      // 16 buffer
      _testOps->bufferSize = 16;
      break;

    case 10:      // 8 buffer
      _testOps->bufferSize = 8;
      break;

    case 11:      // 4 buffer
      _testOps->bufferSize = 4;
      break;

    case 12:      // 2 buffer
      _testOps->bufferSize = 2;
      break;

    case 13:      // 1 buffer
      _testOps->bufferSize = 1;
      break;

    case 14:      // 100k buffer
      _testOps->bufferSize = 102400;
      break;

    case 21:      // Misaligned 4k buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 4096;
      break;

    case 22:      // Misaligned 2k buffer (default)
      _testOps->isMemAligned = false;
      break;
      
    case 23:      // Misaligned 1k buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 1024;
      break;

    case 24:      // Misaligned 512 buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 512;
      break;

    case 25:      // Misaligned 256 buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 256;
      break;

    case 26:      // Misaligned 128 buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 128;
      break;

    case 27:      // Misaligned 64 buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 64;
      break;

    case 28:      // Misaligned 32 buffer
      _testOps->isMemAligned = false;
      _testOps->bufferSize = 32;
      break;

    default:
      syslog(2, "Undefined test meadow_kt_spi_dma_tests, userData:%lu\n", userData);
      return;
  }

  spi_initiate_loopback_test(_testOps);
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
int spi_initiate_loopback_test(SPITestingOptions *testOps)
{
  int ret;

  // Also sets GPIO low
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_A04_PB1);

  testOps->spiDev = NULL;
  testOps->spiDev = stm32_spibus_initialize(testOps->spiNumber);  // Nuttx function
  if(testOps->spiDev == NULL)
  {
    syslog(2, "%s@%d-SPI%d failed to initialize\n", __FILE__, __LINE__, 3);
    return -ENODEV;
  }

  SPI_SETFREQUENCY(testOps->spiDev, MEADOW_SPI_TESTING_SPI_FREQ);

  // Set the mode 0-4
  // SPIDEV_MODE0: /* CPOL=0; CPHA=0 */
  // SPIDEV_MODE1: /* CPOL=0; CPHA=1 */
  // SPIDEV_MODE2: /* CPOL=1; CPHA=0 */
  // SPIDEV_MODE3: /* CPOL=1; CPHA=1 */
  SPI_SETMODE(testOps->spiDev, SPIDEV_MODE0);

  // Set the number of bits per word. 4 - 32 is legal
  SPI_SETBITS(testOps->spiDev, testOps->msgBits);

#if defined (CONFIG_STM32F7_SPI_DMA)
  syslog(2, "%s@%d-DMA test, SPI%lu, allocate %lu bytes %sligned\n",
          __FILE__, __LINE__, testOps->spiNumber,
          testOps->bufferSize, testOps->isMemAligned ? "A" : "Una");
#else
  syslog(2, "%s@%d-Non-DMA test, SPI%lu, allocate %lu bytes %sligned\n",
          __FILE__, __LINE__, testOps->spiNumber,
          testOps->bufferSize, testOps->isMemAligned ? "A" : "Una");
#endif

  ret = work_queue(HPWORK, &spi_test_work, spi_test_main_work_function, testOps, 0);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error work_queue returned with error:%d\n", __FILE__, __LINE__, ret); usleep(30 * 1000);
  }

  return ret;
}

//==================================================================
// This function is responsible for sending and receiving data to the SPI bus
void spi_test_main_work_function(FAR void *arg)
{
  SPITestingOptions *testOps = (SPITestingOptions *)arg;

  size_t bufOff;
  uint8_t *txBuff;
  uint8_t *rxBuff;
  int score = 0;

  if(testOps->isMemAligned)
  {
    txBuff = memalign(ARMV7M_DCACHE_LINESIZE, testOps->bufferSize);
    if(txBuff == NULL)
    {
      syslog(2, "%s@%d-Couldn't allocate mem for txBuff\n", __FILE__, __LINE__);
      usleep(20 * 1000);
      return;
    }

    rxBuff = memalign(ARMV7M_DCACHE_LINESIZE, testOps->bufferSize);
    if(rxBuff == NULL)
    {
      syslog(2, "%s@%d-Couldn't allocate mem for rxBuff\n", __FILE__, __LINE__);
      return;
    }
  }
  else
  {
    // Allocate a bit too much
    txBuff = malloc(testOps->bufferSize + 16);
    if(txBuff == NULL)
    {
      syslog(2, "%s@%d-Couldn't allocate mem for txBuff\n", __FILE__, __LINE__);
      usleep(20 * 1000);
      return;
    }

    rxBuff = malloc(testOps->bufferSize + 16);
    if(rxBuff == NULL)
    {
      syslog(2, "%s@%d-Couldn't allocate mem for rxBuff\n", __FILE__, __LINE__);
      return;
    }

    // Insure memory is not aligned to any reasonable boundary
    txBuff += MEADOW_SPI_TESTING_TX_ALIGN_OFF;
    rxBuff += MEADOW_SPI_TESTING_RX_ALIGN_OFF;
  }

  syslog(2, "%s@%d- 24 MHz, %lu bytes in buffers, testOps:%p, txBuff:%p, rxBuff:%p\n",
            __FILE__, __LINE__, testOps->bufferSize,
            testOps, txBuff, rxBuff); usleep(20 * 1000);

  // Fill send buffer with pseudo "data"
  for(bufOff = 0; bufOff < testOps->bufferSize; bufOff++)
  {
    // 0x00-0xff and repeat pattern
    txBuff[bufOff] = bufOff & 0xff;
  }

  // Send repeatedly
  for(int loopCnt = 0; loopCnt < testOps->repeatExchange; loopCnt++)
  {
    // Fill the receive buffer with different pattern from txBuff
    memset(rxBuff, 0x5a, testOps->bufferSize);

    DEBUG_SET_HIGH(DEBUG_PIN_CCM_A04_PB1);
    SPI_EXCHANGE(testOps->spiDev, txBuff, rxBuff, testOps->bufferSize);
    DEBUG_SET_LOW(DEBUG_PIN_CCM_A04_PB1);

    // Compare data sent with data received
    int cmpResult = memcmp(txBuff, rxBuff, testOps->bufferSize);
    if(cmpResult == 0)
    {
      score++;
    }
  }

  syslog(2, "Successful transfered:%d of %d\n", score, testOps->repeatExchange);

#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
  syslog(2, "------------------------ txBuff ---------------------------\n");
  hcom_nx_diag_print_buffer(txBuff, testOps->bufferSize, 1);
  syslog(2, "------------------------ rxBuff ---------------------------\n");
  hcom_nx_diag_print_buffer(rxBuff, testOps->bufferSize, 1);
  usleep(30 * 1000);
#endif

  if(testOps->isMemAligned)
  {
    free(txBuff);
    free(rxBuff);
    // syslog(2, "Aligned memory freed\n"); usleep(10 * 1000);
  }
  else
  {
    free(txBuff -= MEADOW_SPI_TESTING_TX_ALIGN_OFF);
    free(rxBuff -= MEADOW_SPI_TESTING_RX_ALIGN_OFF);
    // syslog(2, "Unaligned memory freed\n"); usleep(10 * 1000);
  }
}

#endif  // #if defined(CONFIG_SPI_DMA_TESTS)
