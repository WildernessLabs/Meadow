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

// This #define is needed to time the SPI DMA send/receive time
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#if defined (USE_MEADOW_DEBUG_HELPERS)
#include "stm32_gpio.h"
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
// The actual frequency is related to the F7's PCLK frequency and multiples
// thereof. So this choice will be around 24 or 48MHz. SPI2 and SPI3 have a
// maximum of 24MHz and all others 48MHz.
#define MEADOW_SPI_TESTING_SPI_FREQ (50000000)

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
  // Default values SPI3/5, aligned 2k buffer, 8 byte message, no repeat
  _testOps->repeatExchange = 1;
  _testOps->spiNumber = 5;
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

    case 14:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 10;
      break;

    case 15:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 20;
      break;

    case 16:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 40;
      break;

    case 17:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 50;
      break;

    case 18:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 60;
      break;

    case 19:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 70;
      break;

    case 20:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 80;
      break;

    case 21:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 90;
      break;

    case 22:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 100;
      break;

    case 23:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 110;
      break;

    case 24:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 120;
      break;

    case 25:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 130;
      break;

    case 26:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 140;
      break;

    case 27:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 150;
      break;

    case 28:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 160;
      break;

    case 29:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 170;
      break;

    case 30:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 180;
      break;

    case 31:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 190;
      break;
      
    case 32:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 1024 * 200;
      break;
    
    // The following are for testing the workaround needed to exceed the F7's
    // DMA limit of 65535 bytes. For more information see Ref Man section
    // 8.3.6 and 8.3.16 the last bullet,"This means that a maximum of 65535
    // data items can be managed by the DMA in a single transaction."

    case 33:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 65534;
      break;

    case 34:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 65535;
      break;

    case 35:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 65536;
      break;

    case 36:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 65537;
      break;

    case 37:
      // _testOps->repeatExchange = 20;
      _testOps->bufferSize = 65538;
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
  syslog(2, "%s@%d-DMA test, SPI%lu, allocate %lu bytes\n",
          __FILE__, __LINE__, testOps->spiNumber,
          testOps->bufferSize);
#else
  syslog(2, "%s@%d-Non-DMA test, SPI%lu, allocate %lu bytes\n",
          __FILE__, __LINE__, testOps->spiNumber,
          testOps->bufferSize);
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

  // Allocate buffers
  txBuff = malloc(testOps->bufferSize);
  if(txBuff == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for txBuff\n", __FILE__, __LINE__);
    usleep(20 * 1000);
    return;
  }

  rxBuff = malloc(testOps->bufferSize);
  if(rxBuff == NULL)
  {
    free(txBuff);
    syslog(2, "%s@%d-Couldn't allocate mem for rxBuff\n", __FILE__, __LINE__);
    return;
  }

  // syslog(2, "%s@%d- 24 MHz, sending:%lu (0x%08x) bytes, testOps:%p, txBuff:%p, rxBuff:%p\n",
  //           __FILE__, __LINE__, testOps->bufferSize, testOps->bufferSize,
  //           testOps, txBuff, rxBuff); usleep(20 * 1000);

  // Fill send buffer with pseudo "data"
  for(bufOff = 0; bufOff < testOps->bufferSize; bufOff++)
  {
    // Use a prime number to create a pattern that repeats
    txBuff[bufOff] = bufOff % 11;
    // // 0x00-0xff and repeat pattern. This pattern didn't find the 65535
    // byte DMA limit, because it landed on a boundry 
    // txBuff[bufOff] = bufOff & 0xff;
  }

  int firstMismatch[testOps->repeatExchange];
  int failCount[testOps->repeatExchange];
  int successCount[testOps->repeatExchange];
  int loopOff;

  for(loopOff = 0; loopOff < testOps->repeatExchange; loopOff++)
  {
    firstMismatch[loopOff] = 0;
    failCount[loopOff] = 0;
    successCount[loopOff] = 0;
  }
  
  // Send repeatedly
  for(loopOff = 0; loopOff < testOps->repeatExchange; loopOff++)
  {
    // Refill the entire receive buffer
    memset(rxBuff, 0x5a, testOps->bufferSize);

    DEBUG_SET_HIGH(DEBUG_PIN_CCM_A04_PB1);

    // Note: There is code like the following in meadow-upd.c. This is a workaround for the 65535 byte SPI DMA limit.
    if(testOps->bufferSize <= 0xffff)
    {
      SPI_EXCHANGE(testOps->spiDev, txBuff, rxBuff, testOps->bufferSize);
    }
    else
    {
      // There is also a requirement that the number be mulitple of 4,
      // in some cases.
      uint32_t numbToSend = testOps->bufferSize;
      uint8_t *txTempBuf = txBuff;
      uint8_t *rxTempBuf = rxBuff;

      while(numbToSend > 65532)
      {
        // syslog(1, "->Send Loop-to send %lu bytes, tx:%p->rx:%p\n",
        //           numbToSend, txTempBuf, rxTempBuf);
        SPI_EXCHANGE(testOps->spiDev, txTempBuf, rxTempBuf, 65532);
        txTempBuf += 65532;
        rxTempBuf += 65532;
        numbToSend -= 65532;
      }
      // syslog(1, "->Send Last-%lu bytes, tx:%p->rx:%p\n",
      //             numbToSend, txTempBuf, rxTempBuf);
      SPI_EXCHANGE(testOps->spiDev, txTempBuf, rxTempBuf, numbToSend);
    }
    DEBUG_SET_LOW(DEBUG_PIN_CCM_A04_PB1);

    // Compare data sent with data received
    for(bufOff = 0; bufOff < testOps->bufferSize; bufOff++)
    {
      // Quit on error and show buffers
      if(txBuff[bufOff] != rxBuff[bufOff])
      {
        syslog(2, "-->Loop:%02d - Error sent:%lu(0x%08x) bytes, err@bufOff:%lu(0x%08x), txBuff:%p, rxBuff:%p [txBuff:0x%02x(%p) != rxBuff:0x%02x(%p)]\n",
                loopOff,
                testOps->bufferSize, testOps->bufferSize,
                bufOff, bufOff,
                txBuff, rxBuff,
                txBuff[bufOff], &txBuff[bufOff],
                rxBuff[bufOff], &rxBuff[bufOff]);

// #if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
//         // Show bytes before and after error
//         syslog(2, "------------------------ txBuff ---------------------------\n");
//         hcom_nx_diag_print_buffer((&txBuff[bufOff]) - 24, 48, 1);
//         syslog(2, "------------------------ rxBuff ---------------------------\n");
//         hcom_nx_diag_print_buffer((&rxBuff[bufOff]) - 24, 48, 1);
//         syslog(2, "-------------------- End of rxBuff ------------------------\n");
//         hcom_nx_diag_print_buffer(rxBuff + (testOps->bufferSize - 16), 16, 1);
        
//         usleep(30 * 1000);
// #endif
        // Count the error
        if(firstMismatch[loopOff] == 0)
          firstMismatch[loopOff] = bufOff;

        failCount[loopOff]++;

        break;        // Continue with outer loop
      }
      else
      {
        // Success count
        successCount[loopOff]++;
      }
    }
  }

  // Final results
  for(loopOff = 0; loopOff < testOps->repeatExchange; loopOff++)
  {
    if(failCount[loopOff] == 0)
    {
      syslog(2, "Loop:%02d - Successful transfer of:%d bytes\n", loopOff + 1, successCount[loopOff]);
    }
    else
    {
      syslog(2, "Loop:%02d - Transfer failed. First offset:%d, total errors:%d, total success:%d\n",
                loopOff, firstMismatch[loopOff], failCount[loopOff], successCount[loopOff]);
    }
  }

  // syslog(2, "Successful transfered:%d of %d\n", score, testOps->repeatExchange);

  free(txBuff);
  free(rxBuff);
  // syslog(2, "Memory freed\n"); usleep(10 * 1000);
}

#endif  // #if defined(CONFIG_SPI_DMA_TESTS)
