/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\ana_to_dig_conv_tests.c
 * 
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

#include <nuttx/config.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include <nuttx/arch.h>   // up_enable_irq
#include "up_arch.h"      // getreg32 & putreg32
#include <arch/stm32f7/chip.h>
#include "stm32_gpio.h"
#include "stm32_dma.h"
#include <nuttx/kthread.h>
#include "chip/stm32f74xx77xx_adc.h"
#include "chip/stm32f76xx77xx_rcc.h"
#include "chip/stm32f76xx77xx_memorymap.h"
#include <meadow/hcom_shared_common.h>
#include "hcom_nx/hcom_nx_common.h"
// nuttx/arch/arm/src/common/up_arch.h
// #include "chip.h"
// #include "stm32_rcc.h"
// #include "stm32_tim.h"
// #include "stm32_adc.h"
// #include "chip/stm32_rtcc.h"   // FOR TESTING BBR

// Using DMA? This may be temporary
#define ADC_TESTS_USE_DMA_TRANSFER (1)

#if ADC_TESTS_USE_DMA_TRANSFER > 0
#include "chip/stm32f76xx77xx_dma.h"
#endif

#if defined (CONFIG_ADC_TESTS)
#warning "(--) Hacking ana_to_dig_conv_tests.c"

#ifndef CONFIG_STM32F7_DMA2
#error "Meadow ADC with DMA requires CONFIG_STM32F7_DMA2"
#endif

// Diagnostic always as this is test code
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// Of these PA4 and PA5 are available for DAC
#define GPIO_V2_A00_IN4_PA4         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A01_IN5_PA5         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN5)
#define GPIO_V2_A02_IN3_PA3         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN3)
#define GPIO_V2_A03_IN8_PB0         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN0)
#define GPIO_V2_A04_IN9_PB1         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN1)
#define GPIO_V2_A05_IN10_PC0        (GPIO_ANALOG|GPIO_PORTC|GPIO_PIN0)

// ADJUST THESE FOR DIFFERENT TESTS
#define ADC_TESTS_DMA_GPIO_COUNT (6)
#define ADC_TESTS_DMA_SAMPLE_COUNT (1)

#define ADC_TESTS_DMA_BUFFER_SIZE (ADC_TESTS_DMA_GPIO_COUNT * ADC_TESTS_DMA_SAMPLE_COUNT)

// Maybe someday???
#define ADC_TESTS_USE_DOUBLE_BUFFERING (0)

/************************************************************************************
 * Private Data
 ************************************************************************************/

#if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  // 2-buffers in one. This is a Nuttx DMA requirement, not the STM32F7
  uint16_t _dmaDataBuffer1[ADC_TESTS_DMA_BUFFER_SIZE * 2];
  uint16_t *_dmaDataBuffer2 = _dmaDataBuffer1 + ADC_TESTS_DMA_BUFFER_SIZE;
#else
  uint16_t _dmaDataBuffer1[ADC_TESTS_DMA_BUFFER_SIZE];
  #endif

#endif

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void adc_test_initialize(void);
static int adc_test_create_testing_thread(void);
static void *adc_test_kthread_func(int argc, char *argv[]);
static void show_all_data_in_buffer(char *headerText, uint16_t dataBuffer[], uint32_t dataBufSize);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// Only for TESTING
// static void adc_test_display_basic_adc_regs(uint32_t baseADCAddr)
// {
//   syslog(1, "SR:  0x%08x CR1:  0x%08x CR2:  0x%08x\n",
//         getreg32(baseADCAddr + STM32_ADC_SR_OFFSET),
//         getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET),
//         getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET));

//   syslog(1, "SQR1: 0x%08x SQR2: 0x%08x SQR3: 0x%08x\n",
//         getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET),
//         getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET),
//         getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET));

//   syslog(1, "CCR:  0x%08x\n", getreg32(STM32_ADC_CCR));
// }
// //-----------------------------------------------------------------------
// static void adc_test_display_basic_dma_regs(void)
// {
//   syslog(1, "S0CR:  0x%08x  S0NDTR: 0x%08x\n",
//         getreg32(STM32_DMA2_S0CR),
//         getreg32(STM32_DMA2_S0NDTR));

//   syslog(1, "S0PAR: 0x%08x  S0M0AR: 0x%08x S0M1AR: 0x%08x\n",
//         getreg32(STM32_DMA2_S0PAR),
//         getreg32(STM32_DMA2_S0M0AR),
//         getreg32(STM32_DMA2_S0M1AR));
// }

//==========================================================================
// ADC tests Enter here
void meadow_kt_adc_tests(uint32_t userData)
{
  static int firstTime = true;

  syslog(1, "%s@%d-Entered meadow_kt_adc_tests, userData:%lu\n", __FILE__, __LINE__, userData);

  switch(userData)
  {
    case 1:
      if(firstTime)
      {
        firstTime = false;
        
        DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D01);
        DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D02);
        DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D03);
        DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D04);

        DEBUG_SET_LOW(DEBUG_PIN_V2_D01);
        DEBUG_SET_LOW(DEBUG_PIN_V2_D02);
        DEBUG_SET_LOW(DEBUG_PIN_V2_D03);
        DEBUG_SET_LOW(DEBUG_PIN_V2_D04);

        // Initialize test code
        adc_test_initialize();
      }
      else
      {
        syslog(1, "Only first time\n");
      }
      break;

    case 2:
      // Create a thread to test operation
      adc_test_create_testing_thread();
      break;

    default:
      syslog(1, "Undefined test for meadow_kt_adc_tests, userData:%lu\n", userData);
      break;
  }
}

//================================================================
// Entry point
  // (--) This part of the code could be called > 1 time when it supports more
  // than ADC1 for debugging. However the ADC reset done via RCC will only
  // need to be done once.
  // nuttx/arch/arm/src/stm32f7/chip/stm32f74xx77xx_adc.h
void adc_test_initialize()
{
  int ret;
  uint8_t gpioList[16];   // Might as well prepare for max

  syslog(1, "--> Entered adc_test_initialize()\n"); usleep(20 * 1000);

  // To test need to prepare a few things
  // A list of input points. Note points can be used more than once

  // These need to be configured for here testing, but not for actual use as
  // they'll be configured by Meadow.Core
  stm32_configgpio(GPIO_V2_A00_IN4_PA4);
  stm32_configgpio(GPIO_V2_A01_IN5_PA5);
  stm32_configgpio(GPIO_V2_A02_IN3_PA3);
  stm32_configgpio(GPIO_V2_A03_IN8_PB0);
  stm32_configgpio(GPIO_V2_A04_IN9_PB1);
  stm32_configgpio(GPIO_V2_A05_IN10_PC0);
  
  // Populate gpioList for maximum size
  // The Nuttx GPIO Config - Port (bits 7:4) and Pin (bits 3:0)
  gpioList[0]  = GPIO_V2_A00_IN4_PA4  & 0x000000ff;
  gpioList[1]  = GPIO_V2_A01_IN5_PA5  & 0x000000ff;
  gpioList[2]  = GPIO_V2_A02_IN3_PA3  & 0x000000ff;
  gpioList[3]  = GPIO_V2_A03_IN8_PB0  & 0x000000ff;
  gpioList[4]  = GPIO_V2_A04_IN9_PB1  & 0x000000ff;
  gpioList[5]  = GPIO_V2_A05_IN10_PC0 & 0x000000ff;

  gpioList[6]  = GPIO_V2_A00_IN4_PA4  & 0x000000ff;
  gpioList[7]  = GPIO_V2_A01_IN5_PA5  & 0x000000ff;
  gpioList[8]  = GPIO_V2_A02_IN3_PA3  & 0x000000ff;
  gpioList[9]  = GPIO_V2_A03_IN8_PB0  & 0x000000ff;
  gpioList[10] = GPIO_V2_A04_IN9_PB1  & 0x000000ff;
  gpioList[11] = GPIO_V2_A05_IN10_PC0 & 0x000000ff;

  gpioList[12] = GPIO_V2_A00_IN4_PA4  & 0x000000ff;
  gpioList[13] = GPIO_V2_A01_IN5_PA5  & 0x000000ff;
  gpioList[14] = GPIO_V2_A02_IN3_PA3  & 0x000000ff;
  gpioList[15] = GPIO_V2_A03_IN8_PB0  & 0x000000ff;

  syslog(1, "----- gpioList contains -----\n");
  hcom_nx_diag_print_buffer(gpioList, 16, 1);
  
  // Calling API
  // ret = meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
  //         uint16_t dataBuffer[], uint32_t bufferConvSlots)
  // Only needs to execute once
  syslog(1, "--- Test - Address of buffer is:%p\n", _dmaDataBuffer1);

  ret = meadow_adc_configure(gpioList,
                            ADC_TESTS_DMA_GPIO_COUNT,   // Determines how many GPIOs
                            _dmaDataBuffer1,
                            ADC_TESTS_DMA_BUFFER_SIZE);
  if(ret < 0)
  {
    syslog(1, "%s@%d-Error:meadow_adc_configure() returned:%d, errno:%d\n",
              __FILE__, __LINE__, ret, errno);
    usleep(20 * 1000);
  }
}

//===================================================================
// Create a thread for testing
static int adc_test_create_testing_thread(void)
{
  int thread_id = kthread_create("ADC Test",
                                100,      // Priority
                                4096,     // Stack
                                (main_t) adc_test_kthread_func,
                                (char *const *) NULL);
  if (thread_id <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              __FILE__, __LINE__, "ADC Test Thread");
    return -ENOEXEC;
  }
  return OK;
}

//===================================================================
void *adc_test_kthread_func(int argc, char *argv[])
{
  int ret;
#if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  uint32_t regval;
#endif

  for(int chkCnt = 0; chkCnt < 1000000; chkCnt++)
  {
    // Calling meadow_adc.c API
    // This thread will wait until buffer is full
    // syslog(1, "Calling meadow_adc_read_conversions\n");
    ret = meadow_adc_read_conversions();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Call to meadow_adc_read_conversions() failed\n",
                __FILE__, __LINE__);
    }
    // syslog(1, "Returned from meadow_adc_read_conversions call\n");

    // Show information in the buffer
    // This call requires a byte count so, ADC_TESTS_DMA_BUFFER_SIZE*2
    // hcom_nx_diag_print_buffer((uint8_t*)_dmaDataBuffer1, ADC_TESTS_DMA_BUFFER_SIZE*2, 1);

    // Works from 16-bit size, so it's okay as is
    // show_all_data_in_buffer("Test App  ", _dmaDataBuffer1, ADC_TESTS_DMA_BUFFER_SIZE);
    usleep(1000 * 1000);
  }
  return NULL;
}

//==========================================================================
void show_all_data_in_buffer(char *headerText, uint16_t dataBuffer[], uint32_t dataBufSize)
{
#define DMA_ISR_DISP_MAX_PER_ROW (8)    // 8 elements / row
#define DMA_ISR_DISP_VAL_LEN (5)        // Data values take 5 char
#define DMA_ISR_DISP_LEADER_LEN (9)     // Addr takes 9 chars

  uint32_t dmaBuffOff = 0;
  int columnCnt;
  int lineBuffOff;

  int disp_max_per_row = DMA_ISR_DISP_MAX_PER_ROW;
  if(disp_max_per_row > dataBufSize)
    disp_max_per_row = dataBufSize;

  int disp_char_per_row = (disp_max_per_row * DMA_ISR_DISP_VAL_LEN);
  int disp_total_line_len = disp_char_per_row + DMA_ISR_DISP_LEADER_LEN;
  char lineBuff[disp_total_line_len + 1];    // Room for NULL

  do
  {
    lineBuffOff = 0;
    snprintf(&lineBuff[lineBuffOff], disp_total_line_len, "%08x ", dmaBuffOff);
    lineBuffOff = DMA_ISR_DISP_LEADER_LEN;

    // Build a full row of data then print it
    for(columnCnt = 0; columnCnt < disp_max_per_row; columnCnt++)
    {
      snprintf(&lineBuff[lineBuffOff],
                disp_char_per_row - (columnCnt * DMA_ISR_DISP_VAL_LEN),
                "%04u ", dataBuffer[dmaBuffOff++]);
      lineBuffOff += DMA_ISR_DISP_VAL_LEN;
    }

    lineBuff[(lineBuffOff) + 1] = '\0';
    syslog(1, "%s-%s\n", headerText, lineBuff);

    // Line by line show entire buffer
  } while (dmaBuffOff < dataBufSize);
}
