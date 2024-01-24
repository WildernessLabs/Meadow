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

// This file contains SPI test and were developed when adding DMA to SPI for
// Meadow. Some of these test use 2 F7 SPIs and therefore require a Project
// Lab V3.x or equal. As this makes SPI3 and SPI5 available.

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

// Only build if configured
#if defined(CONFIG_SPI_DMA_TESTS)
#pragma message "(--) spi_dma_tests.c"

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/
static bool _canLoopback;
struct spi_dev_s *_spiDev3 = NULL;
struct spi_dev_s *_spiDev5 = NULL;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void spi_dma_tests_no_dma_loopback(uint32_t userData);
static int spi_dma_tests_set_bus_params(struct spi_dev_s **spiDev, int bus, bool isPeriph);
static void spi_dma_tests_send_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, size_t txSize);
static void spi_dma_tests_recv_data_via_spi(struct spi_dev_s *spiDev,
          void *rxBuf, size_t rxSize);
// static void spi_dma_tests_xchg_data_via_spi(struct spi_dev_s *spiDev,
//           void *txBuf, void *rxBuf, size_t xchgSize);
static void *spi_dma_test_kthread_func(int argc, char *argv[]);
static void execute_loopback_test(int numbLoops);

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
      spi_dma_tests_no_dma_loopback(userData);
      break;

    default:
      syslog(2, "Undefined test for meadow_kt_spi_dma_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
void spi_dma_tests_no_dma_loopback(uint32_t userData)
{
#if defined(CONFIG_STM32F7_SPI_DMA)
  syslog(2, "CONFIG_STM32F7_SPI_DMA configured. Can only run SPI tests using DMA\n");
#else
  syslog(2, "CONFIG_STM32F7_SPI_DMA not configured. Can only run non-DMA SPI test\n");
#endif

#if defined(CONFIG_STM32F7_SPI3) && (CONFIG_STM32F7_SPI5)
  _canLoopback = true;
  syslog(2, "%s@%d-Both SPI3 and SPI5 configured\n", __FILE__, __LINE__); usleep(30 * 1000);
#else
  _canLoopback = false;
  syslog(2, "%s@%d-Either SPI3 or SPI 5 not defined\n", __FILE__, __LINE__); usleep(30 * 1000);
#endif

  // Create a thread to do testing 
  int thread_id = kthread_create("SPI DMA Test",
                                100,      // Priority
                                2048,     // Stack
                                (main_t) spi_dma_test_kthread_func,
                                (char *const *) NULL);
  if (thread_id <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              __FILE__, __LINE__, "ADC Test Thread");
    return;
  }
}

//===================================================================
void *spi_dma_test_kthread_func(int argc, char *argv[])
{
  int ret;
  _spiDev3 = NULL;
  _spiDev5 = NULL;

  // We want to measure the time to do an exchange between SPI3 and SPI5
  // First without DMA and then with DMA
  
  syslog(2, "%s@%d-kthread started\n", __FILE__, __LINE__); usleep(30 * 1000);

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

#if defined(CONFIG_STM32F7_SPI5)
  // Initialize SPI5
  syslog(2, "%s@%d-Executing config for SPI 5\n", __FILE__, __LINE__); usleep(30 * 1000);
  ret = spi_dma_tests_set_bus_params(&_spiDev5, 5, true);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not setup bus%d\n", __FILE__, __LINE__, 5);
    return NULL;
  }
#endif

#if defined (CONFIG_STM32F7_SPI_DMA)
  // DMA testing
#else
  // No DMA Testing
#endif

  if(_canLoopback)
  {
    syslog(2, "%s@%d-kthread starting loopback\n", __FILE__, __LINE__); usleep(30 * 1000);
    execute_loopback_test(1);
  }
  else
  {
    syslog(2, "%s@%d-kthread cannot run loopback test, exiting\n", __FILE__, __LINE__); usleep(30 * 1000);
  }

  // spi_close(_spiDev3);
  // spi_close(_spiDev5);

  syslog(2, "%s@%d-kthread about to exit\n", __FILE__, __LINE__); usleep(30 * 1000);
  return NULL;    // Thread exit
}

//=====================================================================
int spi_dma_tests_set_bus_params(struct spi_dev_s **spiDev, int bus, bool isPeriph)
{
  // uint32_t desiredFreq = 8000000UL;   // 8MHz
  // uint32_t desiredFreq = 1000000UL;   // 1MHz
  uint32_t desiredFreq    = 400000;   // 400KHz

  *spiDev = stm32_spibus_initialize(bus);  // Nuttx function
  if(*spiDev == NULL)
  {
    syslog(2, "%s@%d-SPI%d failed to initialize\n", __FILE__, __LINE__, bus);
    return -ENODEV;
  }

  // Lock the bus for access
  // ret = SPI_LOCK(spiDev, true);
  // if(ret < 0)
  // {
  //   syslog(2, "%s@%d-Could not lock bus\n", __FILE__, __LINE__);
  //   return ret;
  // }

  SPI_SETFREQUENCY(*spiDev, desiredFreq);

  // Set the mode 0-4
  // SPIDEV_MODE0: /* CPOL=0; CPHA=0 */
  // SPIDEV_MODE1: /* CPOL=0; CPHA=1 */
  // SPIDEV_MODE2: /* CPOL=1; CPHA=0 */
  // SPIDEV_MODE3: /* CPOL=1; CPHA=1 */
  SPI_SETMODE(*spiDev, SPIDEV_MODE0);

  // Set the number of bits per word. 4 - 32 is legal
  SPI_SETBITS(*spiDev, 8);

  // Unlock the bus, we're done
  // ret = SPI_LOCK(spiDev, false);
  // if(ret < 0)
  // {
  //   syslog(2, "%s@%d-Could not unlock bus\n", __FILE__, __LINE__);
  //   return ret;
  // }

  // (--) THIS IS UNTESTED!!
  if(isPeriph)
  {
    // Modify the configuration to make this not a controller but a peripheral
    // by clearing the master bit
    uint16_t setbits = 0;
    uint16_t clrbits = SPI_CR1_MSTR;
    uint16_t cr1;

    cr1 = getreg16(STM32_SPI_CR1_OFFSET);
    cr1 &= ~clrbits;
    cr1 |= setbits;
    putreg16(cr1, STM32_SPI_CR1_OFFSET);
    
    // from /nuttx/arch/arm/src/stm32f7/stm32_spi.c line 1962
    // clrbits = SPI_CR1_CPHA | SPI_CR1_CPOL | SPI_CR1_BR_MASK | SPI_CR1_LSBFIRST |
    //           SPI_CR1_RXONLY | SPI_CR1_BIDIOE | SPI_CR1_BIDIMODE;
    // setbits = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM;
    // spi_modifycr1(priv, setbits, clrbits);
  }
  return OK;
}

//==================================================================
//
void execute_loopback_test(int numbLoops)
{
  size_t exchangeSize = 32;
  uint8_t *txBuff3;
  uint8_t *rxBuff3;
  uint8_t *echoBuff5;
  int score = 0;

  syslog(2, "%s@%d-About to allocate buffer memory.\n", __FILE__, __LINE__); usleep(30 * 1000);
  txBuff3 = malloc(exchangeSize);
  if(txBuff3 == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for txBuff3\n", __FILE__, __LINE__);
    return;
  }
  rxBuff3 = malloc(exchangeSize);
  if(rxBuff3 == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for rxBuff3\n", __FILE__, __LINE__);
    return;
  }
  echoBuff5 = malloc(exchangeSize);
  if(echoBuff5 == NULL)
  {
    syslog(2, "%s@%d-Couldn't allocate mem for echoBuff5\n", __FILE__, __LINE__);
    return;
  }

  syslog(2, "%s@%d-All memory allocated, _spiDev3:%p, _spiDev5:%p \n",
            __FILE__, __LINE__, _spiDev3, _spiDev5); usleep(30 * 1000);

  // Fill final buffer with "data"
  int bufOff;
  for(bufOff = 0; bufOff < exchangeSize; bufOff++)
  {
    // 0-0xff 
    txBuff3[bufOff] = bufOff & 0xff;
  }

  for(bufOff = 0; bufOff < exchangeSize; bufOff++)
  {

    echoBuff5[bufOff] = 0xaa;
  }

  for(bufOff = 0; bufOff < exchangeSize; bufOff++)
  {
    rxBuff3[bufOff] = 0x55;
  }

  // Send repeatedly
  for(int loopCnt = 0; loopCnt < numbLoops; loopCnt++)
  {
    syslog(2, "%s@%d-Loop:%d\n", __FILE__, __LINE__, loopCnt); usleep(30 * 1000);
    spi_dma_tests_send_data_via_spi(_spiDev3, txBuff3, exchangeSize);

    spi_dma_tests_recv_data_via_spi(_spiDev5, echoBuff5, exchangeSize);

    spi_dma_tests_send_data_via_spi(_spiDev5, echoBuff5, exchangeSize);

    spi_dma_tests_recv_data_via_spi(_spiDev3, rxBuff3, exchangeSize);

    // Compare data sent with data received
    int cmpResult = memcmp(txBuff3, rxBuff3, exchangeSize);
    if(cmpResult == 0)
    {
      score++;
    }
  }
  syslog(2, "Successful transfers:%d of %d\n",score, numbLoops);
  
  syslog(2, "------------------------ txBuff3 ---------------------------\n");
  hcom_nx_diag_print_buffer(txBuff3, exchangeSize, 1);
  syslog(2, "------------------------ rxBuff3 ---------------------------\n");
  hcom_nx_diag_print_buffer(rxBuff3, exchangeSize, 1);
  syslog(2, "------------------------ echoBuff5 ---------------------------\n");
  hcom_nx_diag_print_buffer(echoBuff5, exchangeSize, 1);

  syslog(2, "Successful transfers:%d of %d\n",score, numbLoops);

  free(txBuff3);
  free(rxBuff3);
  free(echoBuff5);
}

//==================================================================
// Send
void spi_dma_tests_send_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, size_t txSize)
{
  syslog(2, "%s@%d-Sending %d bytes, spiDev:%p\n", __FILE__, __LINE__, txSize, spiDev); usleep(30 * 1000);
  
  SPI_EXCHANGE(spiDev, txBuf, NULL, txSize);
}

//==================================================================
// Receive
void spi_dma_tests_recv_data_via_spi(struct spi_dev_s *spiDev,
          void *rxBuf, size_t rxSize)
{
  SPI_EXCHANGE(spiDev, NULL, rxBuf, rxSize);
}

//==================================================================
// Exchange
// void spi_dma_tests_xchg_data_via_spi(struct spi_dev_s *spiDev,
//           void *txBuf, void *rxBuf, size_t xchgSize)
// {
//   // Select ignored /nuttx/configs/stm32f777zit6-meadow/src/stm32_spi.c
//   // We don't support multiply peripheral devices on a bus.
//   SPI_EXCHANGE(spiDev, txBuf, rxBuf, xchgSize);
// }

#endif  // #if defined(CONFIG_SPI_DMA_TESTS)
