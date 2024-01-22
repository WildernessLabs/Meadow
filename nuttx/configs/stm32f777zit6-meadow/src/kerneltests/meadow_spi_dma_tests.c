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

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void spi_dma_tests_no_dma_loopback(uint32_t userData);
static int spi_dma_tests_set_bus_params(struct spi_dev_s *spiDev, int bus);
static void spi_dma_tests_send_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, size_t txSize);
static void spi_dma_tests_recv_data_via_spi(struct spi_dev_s *spiDev,
          void *rxBuf, size_t rxSize);
static void spi_dma_tests_xchg_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, void *rxBuf, size_t xchgSize);
static void *spi_dma_test_kthread_func(int argc, char *argv[]);

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
    int ret;
    struct spi_dev_s *spiDev3 = NULL;
    struct spi_dev_s *spiDev5 = NULL;

#if defined(CONFIG_STM32F7_SPI_DMA)
    syslog(2, "Cannot run no DMA SPI test if CONFIG_STM32F7_SPI_DMA is defined\n");
#endif

#if defined(CONFIG_STM32F7_SPI3)
    syslog(2, "Without CONFIG_STM32F7_SPI3 defined, cannot run SPI DMA loopback test\n");
#endif
#if defined(CONFIG_STM32F7_SPI5)
    syslog(2, "Without CONFIG_STM32F7_SPI5 defined, cannot run SPI DMA loopback test\n");
#endif

  // Initialize SPI3 and SPI5
  ret = spi_dma_tests_set_bus_params(spiDev3, 3);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not setup bus%d\n", __FILE__, __LINE__, 3);
    return;
  }

  ret = spi_dma_tests_set_bus_params(spiDev5, 5);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not setup bus%d\n", __FILE__, __LINE__, 5);
    return;
  }

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
  // We want to measure the time to do an exchange between SPI3 and SPI5
  // First without DMA and then with DMA
#if defined (CONFIG_STM32F7_SPI_DMA)
  // DMA testing
#else
  // No DMA Testing
#endif
  return NULL;
}

//=====================================================================
int spi_dma_tests_set_bus_params(struct spi_dev_s *spiDev, int bus)
{
  int ret;
  struct spi_dev_s *spiDevCfg;
  uint32_t desiredFreq = 8000000UL;   // 8MHz
  // uint32_t actualFreq;
  
  spiDev = NULL;
  
  spiDevCfg = stm32_spibus_initialize(bus);
  if(spiDevCfg == NULL)
  {
    syslog(2, "%s@%d-SPI%d failed to initialize\n", __FILE__, __LINE__, bus);
    return -ENODEV;
  }

  // Lock the bus for access
  ret = SPI_LOCK(spiDevCfg, true);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not lock bus\n", __FILE__, __LINE__);
    return ret;
  }

  SPI_SETFREQUENCY(spiDevCfg, desiredFreq);

  // Set the mode 0-4
  // SPIDEV_MODE0: /* CPOL=0; CPHA=0 */
  // SPIDEV_MODE1: /* CPOL=0; CPHA=1 */
  // SPIDEV_MODE2: /* CPOL=1; CPHA=0 */
  // SPIDEV_MODE3: /* CPOL=1; CPHA=1 */
  SPI_SETMODE(spiDevCfg, SPIDEV_MODE0);

  // Set the number of bits per word. 4 - 32 is legal
  SPI_SETBITS(spiDevCfg, 8);

  // Unlock the bus, we're done
  ret = SPI_LOCK(spiDevCfg, false);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Could not unlock bus\n", __FILE__, __LINE__);
    return ret;
  }

  spiDev = spiDevCfg;
  return OK;
}

//==================================================================
// Send
void spi_dma_tests_send_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, size_t txSize)
{
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
void spi_dma_tests_xchg_data_via_spi(struct spi_dev_s *spiDev,
          void *txBuf, void *rxBuf, size_t xchgSize)
{
  SPI_EXCHANGE(spiDev, txBuf, rxBuf, xchgSize);
}

#endif  // #if defined(CONFIG_SPI_DMA_TESTS)
