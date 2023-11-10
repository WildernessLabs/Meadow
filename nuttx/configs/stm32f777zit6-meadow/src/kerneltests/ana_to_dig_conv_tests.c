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
#include <nuttx/kthread.h>
#include "chip/stm32f74xx77xx_adc.h"
#include "chip/stm32f76xx77xx_rcc.h"
#include "chip/stm32f76xx77xx_dma.h"
#include "chip/stm32f76xx77xx_memorymap.h"
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_syscall_support.h>
#include "hcom_nx/hcom_nx_common.h"

#if defined (CONFIG_ADC_TESTS)

#pragma message "(--) ana_to_dig_conv_tests.c included"

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

// Of these PA4 and PA5 are the only ones that can be used for DAC
#define GPIO_V2_A00_IN4_PA4         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A01_IN5_PA5         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN5)
#define GPIO_V2_A02_IN3_PA3         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN3)
#define GPIO_V2_A03_IN8_PB0         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN0)
#define GPIO_V2_A04_IN9_PB1         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN1)
#define GPIO_V2_A05_IN10_PC0        (GPIO_ANALOG|GPIO_PORTC|GPIO_PIN0)

// This is NOT a valid analog input pin. It can be used for testing
#define GPIO_V2_A0x_INx_PA9         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN9)

// This is fixed for 2 bytes for the ADC results
#define ADC_TESTS_DMA_BYTES_PER_CONVERSION (2)

// Adjust for different tests. Values from 1 to 16 are valid
#define ADC_TESTS_MAX_GPIO_COUNT (16)

#define ADC_TESTS_RESULT_BUFFER_SIZE (ADC_TESTS_MAX_GPIO_COUNT * \
                        ADC_TESTS_DMA_BYTES_PER_CONVERSION)

/************************************************************************************
 * Private Data
 ************************************************************************************/

static double _voltageResultBuf[ADC_TESTS_RESULT_BUFFER_SIZE];
static bool _userData;
static uint32_t _numbGpioActive;

enum
{
  unknown           = 0,
  configure1Gpio    = 1,    // Config 1 GPIO
  configure8Gpio    = 2,    // Config 8 GPIOs
  configure16Gpio   = 3,    // Config 16 GPIOs
  readGpioAnaOnce   = 4,
  readTempBatOnce   = 5,
  readGpioAnaOften  = 6,
  readTempBatOften  = 7,
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void adc_test_initialize(uint32_t numberGpio);
static int adc_test_create_testing_thread(void);
static void *adc_test_kthread_func(int argc, char *argv[]);
static void show_all_data_in_buffer(char *headerText, double dataBuffer[],
              uint32_t dataBufElements);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// ADC tests enter here
void meadow_kt_adc_tests(uint32_t userData)
{
  int ret;
  double batteryVoltage;
  double temperatureValue;

  _userData = userData;

  syslog(2, "%s@%d-Entered meadow_kt_adc_tests, userData:%lu\n", __FILE__, __LINE__, userData);
  usleep(20 * 1000);
  
  switch(userData)
  {
    case configure1Gpio:
      // Initialize test code
      syslog(2, "--------------- configure-1-Gpio ---------------\n");usleep(20 * 1000);
      adc_test_initialize(1);
      _numbGpioActive = 1;
      break;

    case configure8Gpio:
      // Initialize test code
      syslog(2, "--------------- configure-8-Gpio ---------------\n");usleep(20 * 1000);
      adc_test_initialize(8);
      _numbGpioActive = 8;
      break;

    case configure16Gpio:
      // Initialize test code
      syslog(2, "--------------- configure-16-Gpio ---------------\n");usleep(20 * 1000);
      adc_test_initialize(ADC_TESTS_MAX_GPIO_COUNT);
      _numbGpioActive = ADC_TESTS_MAX_GPIO_COUNT;
      break;

      // Read the data here after configuring
    case readGpioAnaOnce:
      syslog(2, "--------------- readGpioAnaOnce ---------------\n");usleep(20 * 1000);
      ret = meadow_adc_read_values();
      if(ret < 0)
      {
        syslog(LOG_ERR, "Error:GPIO conversion, ret:%d\n", ret);
      }
      // Show information in the buffer
      show_all_data_in_buffer("TestApp", _voltageResultBuf, _numbGpioActive);
      break;

    case readTempBatOnce:
      syslog(2, "--------------- readTempBatOnce ---------------\n");
      // Read the internal values of battery and temperature once
      ret = meadow_adc_read_temp_vbat(&batteryVoltage, &temperatureValue);
      if(ret < 0)
      {
        syslog(LOG_ERR, "Error:Internal vbat and temp conversion, ret:%d\n", ret);
      }
      syslog(2, "TestApp:Vbat:%.3f, Temp:%.3f\n", batteryVoltage, temperatureValue);
      break;
    
    case readGpioAnaOften:
      syslog(2, "--------------- readGpioAnaOften ---------------\n");usleep(20 * 1000);
      // Create a thread to test standard GPIO ADC operation often
      adc_test_create_testing_thread();
      break;

    case readTempBatOften:
      syslog(2, "--------------- readTempBatOften ---------------\n");usleep(20 * 1000);
      // Create a thread to test getting temperature & Vbat often
     adc_test_create_testing_thread();
      break;

    default:
      syslog(2, "Undefined test for meadow_kt_adc_tests, userData:%lu\n", userData);
      break;
  }
}

//================================================================
// This part of the code could be called > 1 time when it supports more
// than ADC1 for debugging. However the ADC reset done via RCC will only
// need to be done once.
// nuttx/arch/arm/src/stm32f7/chip/stm32f74xx77xx_adc.h
void adc_test_initialize(uint32_t numberGpio)
{
  int ret;
  uint8_t gpioList[ADC_TESTS_MAX_GPIO_COUNT];   // Might as well prepare for max

  syslog(2, "--> Entered adc_test_initialize()\n"); usleep(20 * 1000);

  // To test need to prepare a few things
  // A list of input points. Note points can be used more than once

  // These need to be configured here for testing, but not for non-testing
  // they'll be configured by Meadow.Core
  stm32_configgpio(GPIO_V2_A00_IN4_PA4);
  stm32_configgpio(GPIO_V2_A01_IN5_PA5);
  stm32_configgpio(GPIO_V2_A02_IN3_PA3);
  stm32_configgpio(GPIO_V2_A03_IN8_PB0);
  stm32_configgpio(GPIO_V2_A04_IN9_PB1);
  stm32_configgpio(GPIO_V2_A05_IN10_PC0);

  // We'll use the first of these based on the ADC_TESTS_MAX_GPIO_COUNT
  // value. If it is 1 then only used the first entry. It it is 6, we'll use
  // the first 6 entries. For more than 6 we reuse the previous analog GPIOs
  //
  // The Nuttx GPIO Config holds the Port in bits 7:4 and the Pin in bits 3:0
  // this is all we need to setup the ADC
  gpioList[0]  = GPIO_V2_A00_IN4_PA4  & 0x000000ff;
  gpioList[1]  = GPIO_V2_A01_IN5_PA5  & 0x000000ff;
  gpioList[2]  = GPIO_V2_A02_IN3_PA3  & 0x000000ff;
  gpioList[3]  = GPIO_V2_A03_IN8_PB0  & 0x000000ff;
  gpioList[4]  = GPIO_V2_A04_IN9_PB1  & 0x000000ff;
  gpioList[5]  = GPIO_V2_A05_IN10_PC0 & 0x000000ff;
  // Note: because the ADC sequence registers 3 and 2 both hold 6 GPIOs and
  // the Meadow has 6 GPIOs for ADC the pattern will repeat itself in these
  // registers.
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

  // syslog(2, "----- gpioList contains -----\n");
  // hcom_nx_diag_print_buffer(gpioList, 16, 1);
  
  // Calling configuration API to set things up
  ret = meadow_adc_configure(gpioList,
                            numberGpio,   // Determines how many GPIOs
                            _voltageResultBuf);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:meadow_adc_configure() returned:%d, errno:%d\n",
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
  double batteryVoltage;
  double temperatureValue;

  // Run the test
  for(int chkCnt = 0; chkCnt < 1000000; chkCnt++)
  {
    if(_userData == readGpioAnaOften)
    {
      // Calling meadow_adc.c API to indicate conversion needed
      // This thread will wait until buffer is full
      ret = meadow_adc_read_values();
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-Call to meadow_adc_read_values() failed\n",
                  __FILE__, __LINE__);
      }

      // Show information in the buffer
      char textBuf[64];
      snprintf_chk(textBuf, sizeof(textBuf), "%04d-%s", chkCnt + 1, "Test App");

      show_all_data_in_buffer(textBuf, _voltageResultBuf, _numbGpioActive);
    }
    else if(_userData == readTempBatOften)
    {
      // Read the internal values of battery and temperature
      ret =  meadow_adc_read_temp_vbat(&batteryVoltage, &temperatureValue);
      if(ret < 0)
      {
        syslog(LOG_ERR, "Error:Internal vbat and temp conversion, ret:%d\n", ret);
      }
      
      syslog(2, "TestApp: Vbat:%f, Temp:%f\n", batteryVoltage, temperatureValue);
    }

    // Just keep looping
    usleep(3000 * 1000);
  }
  return NULL;
}

//==========================================================================
// Output the information from the data buffer
void show_all_data_in_buffer(char *headerText, double dataBuffer[],
                            uint32_t dataBufElements)
{
#define DMA_ISR_DISP_MAX_PER_ROW (4)    // elements / row
#define DMA_ISR_DISP_VALUE_LEN (10)     // Doubles values take 5-9 char
#define DMA_ISR_DISP_LEADER_LEN (9)     // Addr takes 9 chars

  uint32_t dmaBuffOff = 0;
  int remainingElements = dataBufElements;
  int columnCnt;
  int lineBuffOff;
  int disp_max_per_row = DMA_ISR_DISP_MAX_PER_ROW;

  if(disp_max_per_row > dataBufElements)
    disp_max_per_row = dataBufElements;

  int disp_char_per_row = (disp_max_per_row * DMA_ISR_DISP_VALUE_LEN);
  int disp_total_line_len = disp_char_per_row + DMA_ISR_DISP_LEADER_LEN;

  char *lineBuff = malloc(disp_total_line_len);    // Room for NULL

  // Fill unused spaces with space
  memset(lineBuff, 0x20, disp_total_line_len);

  do
  {
    // Add address of offset to beginning of line
    lineBuffOff = snprintf(lineBuff, disp_total_line_len, "%08x ", dmaBuffOff);

    // Build a full row of data then print it
    for(columnCnt = 0; columnCnt < disp_max_per_row; columnCnt++)
    {
      // This will add a terminating NULL in the buffer
      int lineLen = snprintf(&lineBuff[lineBuffOff],
                disp_char_per_row - (columnCnt * DMA_ISR_DISP_VALUE_LEN),   // Buffer size
                "%.3f ", dataBuffer[dmaBuffOff++]);

      // On the last entry don't override the NULL, it's terminating the line
      if((columnCnt+ 1) != disp_max_per_row)
        lineBuff[lineBuffOff + lineLen] = 0x20;    // Overwrite unwanted NULL

      lineBuffOff += DMA_ISR_DISP_VALUE_LEN;    // Fixed spacing
      remainingElements--;
    }

    // Display this line of text
    syslog(2, "%s:%s\n", headerText, lineBuff);

    // Line by line show entire buffer
  } while (remainingElements > 0);
  
  free(lineBuff);
}
#endif    // #if defined (CONFIG_ADC_TESTS)
