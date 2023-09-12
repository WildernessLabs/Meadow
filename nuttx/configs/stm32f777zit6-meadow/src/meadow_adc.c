/****************************************************************************
 * configs/stm32f777zit6-meadow/src/meadow-adc.c
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

#warning "(--) Hacking meadow_adc.c"

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

#define ADC_ALL_POSSIBLE_ADC_INTERRUPTS (ADC_SR_OVR | ADC_SR_STRT | \
          ADC_SR_JSTRT | ADC_SR_JEOC | ADC_SR_EOC | ADC_SR_AWD)

// Of these PA4 and PA5 are available for DAC
#define GPIO_V2_A00_IN4_PA4         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A01_IN5_PA5         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN5)
#define GPIO_V2_A02_IN3_PA3         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN3)
#define GPIO_V2_A03_IN8_PB0         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN0)
#define GPIO_V2_A04_IN9_PB1         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN1)
#define GPIO_V2_A05_IN10_PC0        (GPIO_ANALOG|GPIO_PORTC|GPIO_PIN0)

// #define ADC_SMPR_DEFAULT    ADC_SMPR_3       // 4 usec for 6 analogs (265kHz)
#define ADC_SMPR_DEFAULT    ADC_SMPR_112     // 32 usec for 6 analogs (32kHz)
// #define ADC_SMPR_DEFAULT    ADC_SMPR_480     // 124 usec for 6 analogs
#define ADC_SMPR1_DEFAULT   ((ADC_SMPR_DEFAULT << ADC_SMPR1_SMP10_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP11_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP12_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP13_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP14_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP15_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP16_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP17_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP18_SHIFT))
#define ADC_SMPR2_DEFAULT   ((ADC_SMPR_DEFAULT << ADC_SMPR2_SMP0_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP1_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP2_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP3_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP4_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP5_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP6_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP7_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP8_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP9_SHIFT))


// [--] START - CARRIED FROM non-working meadow_adc.c
// // Sequence Register 3 holds first 6 Analog GPIOs
// #define MEADOW_ADC_SEQ_REGISTER_3_TOTAL (6)
// #define MEADOW_ADC_SEQ_REGISTER_2_TOTAL (6)
// #define MEADOW_ADC_SEQ_REGISTER_1_TOTAL (4)

// #if ADC_TESTS_USE_DMA_TRANSFER > 0
// #define ADC_TESTS_USE_DOUBLE_BUFFERING (0)
// #endif

// #define MEADOW_ADC_DEBUG_GPIO_COUNT (6)

// /************************************************************************************
//  * Private Data
//  ************************************************************************************/
// // From data sheet - GPIO to ADC1 channel input map
// // Entries represent the CPU
// static uint8_t _gpioAdcChanMap[] =
// {
//   0x00,      // Chan 0 = PA0
//   0x01,      // Chan 1 = PA1
//   0x02,      // Chan 2 = PA2
//   0x03,      // Chan 3 = PA3
//   0x04,      // Chan 4 = PA4
//   0x05,      // Chan 5 = PA5
//   0x06,      // Chan 6 = PA6
//   0x07,      // Chan 7 = PA7
//   0x10,      // Chan 8 = PB0
//   0x11,      // Chan 9 = PB1
//   0x20,      // Chan 10 = PC0
//   0x21,      // Chan 11 = PC1
//   0x22,      // Chan 12 = PC2
//   0x23,      // Chan 13 = PC3
//   0x24,      // Chan 14 = PC4
//   0x25,      // Chan 15 = PC5
// };
// #define MEADOW_ADC_GPIO_CHAN_MAP_LENGTH (sizeof(_gpioAdcChanMap))

  static uint32_t _gpioCount;
  static uint8_t *_gpioList;
  volatile uint16_t *_dmaDataBuffer;    // Points to callers buffer

//   static uint8_t _gpioChannelMap[MEADOW_ADC_GPIO_CHAN_MAP_LENGTH];
//   // static sem_t _waitTillDoneSem;

// #if ADC_TESTS_USE_DMA_TRANSFER > 0
//   DMA_HANDLE _dmaHandle;
// #endif

// BUFFER IS IN CALLER NOW
// #if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
//   // 2-buffers in one is required by Nuttx dma code
//   uint16_t _dmaDataBuffer[MEADOW_ADC_DEBUG_GPIO_COUNT * 2];
//   uint16_t *_dmaDataBuffer2 = _dmaDataBuffer + MEADOW_ADC_DEBUG_GPIO_COUNT;
// #else
//  volatile uint16_t *_dmaDataBuffer;
// #endif
// /************************************************************************************
//  * Private Function Prototypes
//  ************************************************************************************/

// // static int get_in_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan);
// // static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegCount,
// //           uint32_t initRegShift);
// static void show_all_data_in_buffer(char *headerText, uint16_t dataBuffer[], uint32_t dataBufElements);

// /************************************************************************************
//  * Private Functions
//  ************************************************************************************/
// static void meadow_adc_buffer_takesem(sem_t *semaphore)
// {
//   int ret;
//   do
//   {
//     ret = sem_wait(semaphore);
//   }
//   while (ret == -EINTR);
// }

// //==========================================================================
// // Find ADC input channel (0-16) from the provided GPIO input port/pin
// static int get_in_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan)
// {
//   int mapOff;

//   // Look for match
//   for(mapOff = 0; mapOff < MEADOW_ADC_GPIO_CHAN_MAP_LENGTH; mapOff++)
//   {
//     if(pinId == _gpioAdcChanMap[mapOff])
//     {
//       *adcInputChan = mapOff;
//       return OK;
//     }
//   }

//   // Return error
//   return -EBADSLT;    // 55 - Invalid Slot
// }

// //======================================================================
// // Helps fill the sequence registers
// static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegCount,
//           uint32_t initRegShift)
// {
//   int ret;
//   uint32_t regCnt;
//   uint32_t adcInputChan;

//   for(regCnt = 0; regCnt < seqRegCount; regCnt++)
//   {
//     // For entry x what 'ADC input' channel
//     ret = get_in_chan_from_pinid(_gpioList[regCnt], &adcInputChan);
//     if(ret < 0)
//       return ret;    // Error

//     *regval |= (adcInputChan << (initRegShift + (regCnt * 5)));
//   }
//   return OK;
// }
// [--] END - CARRIED FROM non-working meadow_adc.c

#if ADC_TESTS_USE_DMA_TRANSFER > 0
#define ADC_TESTS_DMA_DATA_BUFFER_SIZE (6)
#define ADC_TESTS_USE_DOUBLE_BUFFERING (0)

/************************************************************************************
 * Private Data
 ************************************************************************************/

  DMA_HANDLE _dmaHandle;

 #if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  // 2-buffers in one
  uint16_t _dmaDataBuffer1[ADC_TESTS_DMA_DATA_BUFFER_SIZE * 2];
  uint16_t *_dmaDataBuffer2 = _dmaDataBuffer1 + ADC_TESTS_DMA_DATA_BUFFER_SIZE;
 #else
  uint16_t _dmaDataBuffer1[ADC_TESTS_DMA_DATA_BUFFER_SIZE];
 #endif

#endif

/************************************************************************************
 * Public Data
 ************************************************************************************/
// For reference from data sheet
// Chan/INx  GPIO
//     0     PA0
//     1     PA1
//     2     PA2
//     3     PA3
//     4     PA4
//     5     PA5
//     6     PB0
//     7     PB1
//     8     PB2
//     10    PC0
//     11    PC1
//     12    PC2
//     13    PC3
//     14    PC4
//     15    PC5

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

// static int adc_test_config_adc(int adc_numb, uint32_t baseADCAddr);
static void meadow_adc_initialize(void);

/************************************************************************************
 * Public Functions
 ************************************************************************************/

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

// //==========================================================================
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

#if ADC_TESTS_USE_DMA_TRANSFER > 0
//==========================================================================
// DMA ISR
static void adc_dma_interrupt_handler_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  // uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;
  // static int execCnt = 0;

  // The DMA controller hardware can be programmed to call for the following
  // Discription        Event Flag    Enable control bit
  // -----------        ----------    ------------------
  // Half-transfer          HTIF            HTIE
  // Transfer complete      TCIF            TCIE
  // Transfer error         TEIF            TEIE
  // FIFO overrun/underrun  FEIF            FEIE
  // Direct mode error      DMEIF           DMEIE

  // Note: The interrupt has already been handled in the STM32_DMA_LISR_OFFSET
  // or STM32_DMA_HISR_OFFSET registers by the Nuttx code in stm32_dma.c This
  // function stm32_dmainterrupt() in nuttx/arch/arm/src/stm32/stm32_dma_v2.c,
  // is the function whose call that got us here.
  // The interrupt values are in the 8 bit 'status' field. The fields can be
  // found in: nuttx/arch/arm/src/stm32f7/chip/stm32f76xx77xx_dma.h

  if(status == 0)
  {
    syslog(1, "-- DMA ISR No Interrupts --\n");
    return;
  }

  // Bit 0: Stream FIFO error interrupt flag
  if((status & DMA_STREAM_FEIF_BIT) != 0)
  {
    syslog(1, "DMA ISR reason:FIFO error\n");
  }

  // Bit 2: Stream direct mode error interrupt flag
  if((status & DMA_STREAM_DMEIF_BIT) != 0)
  {
    syslog(1, "DMA ISR reason:direct mode error\n");
  }

  // Bit 3: Stream Transfer Error flag
  if((status & DMA_STREAM_TEIF_BIT) != 0)
  {
    syslog(1, "DMA ISR reason:Transfer Error\n");
  }
  
  //------------------------------------------------------
  // Stream Half Transfer flag
  if((status & DMA_STREAM_HTIF_BIT) != 0)
  {
    // syslog cannot keep up with these strings 
    // syslog(1, "DMA ISR reason:Half Transfer\n");
  }

  //------------------------------------------------------
  // Stream Transfer Complete flag
  if((status & DMA_STREAM_TCIF_BIT) != 0)
  {
    DEBUG_SET_HIGH(DEBUG_PIN_V2_D03);

    // syslog(1, "DMA ISR:Transfer Complete\n");

// (--) Future do some work here
// #if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
// #else
// #endif

    DEBUG_SET_LOW(DEBUG_PIN_V2_D03);
  }

  //---------------------------------------------------
  // Invalidate the cache
  // up_invalidate_dcache((uintptr_t)_dmaDataBuffer1,
  //               (uintptr_t)_dmaDataBuffer1 + ADC_TESTS_DMA_DATA_BUFFER_SIZE);

  // Do work here

  // Restart conversion

  // // Without SCAN mode (ADC_CR1_SCAN) this will restart the conversion
  // // Per Ref Man 15.8.2 this may need to be here
  // regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  // regval |= ADC_CR2_SWSTART;
  // putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // From Ref Man
  // At the end of the last DMA transfer (number of transfers configured in the
  // DMA controller’s DMA_SxNTR register):
  // • No new DMA request is issued to the DMA controller if the DDS bit is
  // cleared to 0 in the ADC_CR2 register (this avoids generating an overrun
  // error). However the DMA bit is not cleared by hardware. It must be written
  // to 0, then to 1 to start a new transfer.
  // • Requests can continue to be generated if the DDS bit is set to 1. This
  // allows configuring the DMA in double-buffer circular mode.

  // regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  // regval &= ~ADC_CR2_DMA;
  // putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // regval |= ADC_CR2_DMA;
  // putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);
}
#endif

//==========================================================================
// ADC conversion ISR
static int adc_conversion_interrupt_handler_isr(int irq, FAR void *context,
            FAR void *arg)
{
  // The interrupt conversion can be programmed to be called for the following
  // Discription                          Event Flag    Enable control bit
  // -----------                          ----------    ------------------
  // End of conversion of a regular group     EOC             EOCIE
  // End of conversion of an injected group   JEOC            JEOCIE
  // Analog watchdog status bit is set        AWD             AWDIE
  // Overrun                                  OVR             OVRIE
  uint32_t pendingInterrupts;
  uint32_t baseADCAddr = (uint32_t)arg;

  DEBUG_SET_HIGH(DEBUG_PIN_V2_D04);

  pendingInterrupts = getreg32(baseADCAddr + STM32_ADC_SR_OFFSET);
  if(pendingInterrupts == 0)
  {
    syslog(1, "-- ADC ISR-NO Interrupts --\n");
    return OK;
  }

  if ((pendingInterrupts & ADC_SR_AWD) != 0)
  {
    syslog(1, "-- ADC ISR-WatchDog --\n");
  }

  if ((pendingInterrupts & ADC_SR_OVR) != 0)
  {
    syslog(1, "-- ADC ISR-Over Run --\n");

#if ADC_TESTS_USE_DMA_TRANSFER > 0
    // From Ref Man 15.8.1 & 15.8.2
    // To recover the ADC from OVR when the DMA is used, follow the steps below:
    // 1. Reinitialize the DMA (adjust destination address and NDTR counter)
    // 2. Clear the ADC OVR bit in ADC_SR register (below)
    // 3. Trigger the ADC to start the conversion (below)
    //
    // (--) #1 above - there doesn't appear to be in the stm32f7/stm32_dma.c
    // code a function to "Reinitialize the DMA".
    // See nuttx/arch/arm/src/stm32f7/stm32_dma.c @657-673 for code that would
    // do the above requirement (I think).
#endif
  }

  // // End of conversion - got a value?
  if ((pendingInterrupts & ADC_SR_EOC) != 0)
  {
    syslog(1, "-- ADC ISR-End of Conversion --\n");
  }

  // Clear any interrupts
  pendingInterrupts &= ~ADC_ALL_POSSIBLE_ADC_INTERRUPTS;
  putreg32(pendingInterrupts, baseADCAddr + STM32_ADC_SR_OFFSET);

  DEBUG_SET_LOW(DEBUG_PIN_V2_D04);
  return OK;
  // END IF ADC ISR
}

//======================================================================
// Assumes ADC 1
static void adc_initialize (void)
{
  uint32_t regval;

  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
  // (--) NEEDED ?
  regval = getreg32(STM32_RCC_APB2ENR);
  regval |= RCC_APB2ENR_ADC1EN;
  putreg32(regval, STM32_RCC_APB2ENR);

  // Turn-off ADC
  // regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  // regval &= ~ADC_CR2_ADON;      // 0=A/D Converter off (turned on later)
  // putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // Set the ADC watchdog high and low threshold to max and min
  putreg32(0x00000fff, STM32_ADC1_BASE + STM32_ADC_HTR_OFFSET);
  putreg32(0x00000000, STM32_ADC1_BASE + STM32_ADC_LTR_OFFSET);

  // Reset all the ADCs via Reset and Clock Control (RCC). For the STM32F7
  // there is a single bit for all ADCs. Other MCUs have a bit for each ADC.
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval |= RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // (--) PROBABLTY NOT NEEDED
  // // Restore ADC from reset state
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval &= ~RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Now for some ADC work
  //------------------------------------------------------------
  // ADC Common Control Register
  regval = getreg32(STM32_ADC_CCR);
  // regval &= ~ADC_CCR_TSVREFE;       // 0=disable temperature sensor channel
  // regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel

  // ADCPRE - Calculation based on PCLK2=96MHz (with 192MHz clock). Per Data
  // Sheet 5.3.24 pp 165, max ADC clock is 36MHz. Therefore, divide by 4
  // (96/4=24MHz) is the highest freq. For clock  details see Meadow's board.h
  //? regval &= ~ADC_CCR_ADCPRE_MASK;   // Clear any bits in ADC prescaler
  regval |= ADC_CCR_ADCPRE_DIV4;    // 01=ADC prescaler PCLK2 divided by 4

  // 00: DMA mode disabled
  // 01: DMA mode 1 enabled (2 / 3 half-words one by one - 1 then 2 then 3)
  // 10: DMA mode 2 enabled (2 / 3 half-words by pairs - 2&1 then 1&3 then 3&2)
  // 11: DMA mode 3 enabled (2 / 3 bytes by pairs - 2&1 then 1&3 then 3&2)
  // regval &= ~ADC_CCR_DMA_MASK;       // Clear any bits in DMA mode (multi-ADC mode only) 
  // regval |= ADC_CCR_DMA_DISABLED;    // 00 = DMA Modes (multi-ADC mode only)
  // regval |= (1 << ADC_CCR_DMA_SHIFT) // 01: DMA mode 1 enabled
  // regval |= (2 << ADC_CCR_DMA_SHIFT) // 10: DMA mode 2 enabled
  // regval |= (3 << ADC_CCR_DMA_SHIFT) // 11: DMA mode 3 enabled

  // regval &= ~ADC_CCR_DELAY_MASK;    // 0000=5*Tadcclk (only used for dual/triple)
  // regval &= ~ADC_CCR_MULTI_MASK;    // Clear any bits
  // regval &= ~ADC_CCR_MULTI_NONE;     // 00000=Independent mode
  putreg32(regval, STM32_ADC_CCR);

  regval = getreg32(STM32_ADC_CCR);
  putreg32(regval, STM32_ADC_CCR);
  //---------------------------------------------------
  // Get the ADC Control Register 1 register. This register controls a lot of
  // options. I put the following in the same order as the Ref Man 15.13.2
  // This is mostly interrupt configuration
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);
  // regval &= ~ADC_CR1_OVRIE;       // 0=Disable Overrun interrupt
  //? regval &= ~ADC_CR1_RES_MASK;    // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;      // Set resolution 00=12, 01=10, 10=8 or 11=6 bits
  // regval |= ADC_CR1_AWDEN;       // Testing 0=Disable Analog watchdog on regular channels
  // regval &= ~ADC_CR1_JAWDEN;      // 0=Disable Analog watchdog on injected
  // regval &= ~ADC_CR1_JDISCEN;     // 0=Disable discontinuous mode on injected channels
  // regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  // regval &= ~ADC_CR1_DISCEN;      // 0=Disable discontinuous mode on regular channels
  // regval &= ~ADC_CR1_JAUTO;       // 0=Automatic Injected Group conversion
  // regval &= ~ADC_CR1_AWDSGL;      // 0=Disable watchdog on single channel in scan mode
  // [--] Since more than 1 ADC need to scan
  regval |= ADC_CR1_SCAN;           // 1=Scan mode (Scans channels in ADC_SQRx registers)
  // regval |= ADC_CR1_JEOCIE;      // 0=Disable interrupt for injected channels
  // regval |= ADC_CR1_AWDIE;       // 0=Analog Watchdog interrupt enable
  // regval |= ADC_CR1_EOCIE;       // 1=Enable interrupt for EOC
  // regval &= ~ADC_CR1_AWDCH_MASK;  // Clear the watchdog channel to 00000=Chan 0

  // Set for IN4 (PA4) while testing
  // (--) NOT NEEDED ??? regval |= (4 << ADC_CR1_AWDCH_SHIFT);  // 00100=Channel 4 analog watchdog select bits
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  //---------------------------------------------------
  // ADC CR2 Configuration
  // Note:fields not defined in header file have been ignored
  // Missing fields: SWSTART, EXTSEL, JSWSTART, JEXTEN, JEXTSEL, DDS & EOCS
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  //? regval &= ~ADC_CR2_EXTEN_MASK;  // Clear bits
  // regval |= ADC_CR2_EXTEN_NONE;   // No trigger from external sources
  regval &= ~ADC_CR2_ALIGN;       // 0=Right alignment (1=left alignment)
  // EOCS - End Of Conversion Selection. When should the EOC bit be set?
  regval |= ADC_CR2_EOCS;        // 1=End of each conversion, 0=End of sequence

#if ADC_TESTS_USE_DMA_TRANSFER > 0
#endif

  // Enable continous conversion
  regval |= ADC_CR2_CONT;         // 1=Enable continuous mode
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  //------------------------------------------------------------
  // ADC Sample Time Register - Determine how many clock cycles should each
  // conversion wait before beginning? See Ref Man sec 15.5.
  // With ADCCLK = 24MHz (see ADC_CCR_ADCPRE)
  // 000: 3 cycles Tconv = 3 + 12 = 15 cycles, 15/24,000,000 = 0.625 us
  // 001: 15 cycles
  // 010: 28 cycles
  // 011: 56 cycles
  // 100: 84 cycles
  // 101: 112 cycles Tconv = 112 + 12 = 124 cycles 124/24,000,000 = 4.667 us
  // 110: 144 cycles
  // 111: 480 cycles Tconv = 480 + 12 = 492 cycles 492/24,000,000 = 20.5 us
  // TESTING-Set all channels to the same default ADC_SMPR_DEFAULT
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET);
  // This #define will set all sample times to the same value. Here
  // ADC_SMPR_DEFAULTs are used to set ADC conversion the same.
  regval &= 0xf8000000;      // Clear Sample Time fields 10-18
  regval |= ADC_SMPR1_DEFAULT;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET);

  // Set sample time for channels 0-9
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SMPR2_OFFSET);
  regval &= 0xc0000000;      // Clear sample time fields 0-9
  regval |= ADC_SMPR2_DEFAULT;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SMPR2_OFFSET);

  //------------------------------------------------------------
  // ADC_SQR1, ADC_SQR2 and ADC_SQR3 are used to configure the sequence of the
  // conversions. If there are no entries then nothing will be converted.
  // Basically, which input in which order.
  //
  // The values put in this field are found by looking at the target hardware.
  // For the FeatherV2, A00 is PA4. Next look at the data sheet
  // 'Table 11. STM32F777xx, STM32F778Ax and STM32F779xx pin and ball
  // definitions.' In the 'Pin name' column find PA4. Then look in the
  // 'Additional functions' column for the possible analog inputs for the ADC
  // being used. In this case there are 2 possible ADC1_IN4, and ADC2_IN4.
  // Since for ADC1 the value is '4' (it's also 4 for ADC2 and 3).
  //
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);
  //? regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
 
  // All 6 Pins available
  regval |= (4  << ADC_SQR3_SQ1_SHIFT);   // Channel 4  - A00 [PA4]->ADC123_IN4
  regval |= (5  << ADC_SQR3_SQ2_SHIFT);   // Channel 5  - A01 [PA5]->ADC123_IN5
  regval |= (3  << ADC_SQR3_SQ3_SHIFT);   // Channel 3  - A02 [PA3]->ADC123_IN3
  regval |= (8  << ADC_SQR3_SQ4_SHIFT);   // Channel 8  - A03 [PB0]->ADC12_IN8
  regval |= (9  << ADC_SQR3_SQ5_SHIFT);   // Channel 9  - A04 [PB1]->ADC12_IN9
  regval |= (10 << ADC_SQR3_SQ6_SHIFT);   // Channel 10 - A05 [PC0]->ADC123_IN10
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);
  //? regval &= ADC_SQR1_RESERVED;            // Clear all SQR Bits
  regval |= (5 << ADC_SQR1_L_SHIFT);       // A 5 will convert 6
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);

  // [--] BEGIN - CARRIED FROM non-working meadow_adc.c
// THE FOLLOWING HAS BEEN PARTIAL TESTED - BUT THE ABOVE IS DOING THE WORK
// WHILE TESTING
  // uint32_t adcInputChan;
  // uint32_t seqRegCount;
  // uint32_t remainingCnt = _gpioCount;
  // uint32_t regCnt;

  // // Sequence Register 3 - ADC channels 1-6
  // seqRegCount = MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
  // if(remainingCnt < MEADOW_ADC_SEQ_3_REGISTER_TOTAL)
  //   seqRegCount = remainingCnt;

  // regval = getreg32(STM32_ADC1_SQR3);
  // regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
  // ret = populate_adc_seq_channel(&regval, seqRegCount, ADC_SQR3_SQ1_SHIFT);
  // if(ret < 0)
  //   return ret;    // Error
  // putreg32(regval, STM32_ADC1_SQR3);
  // remainingCnt -= MEADOW_ADC_SEQ_3_REGISTER_TOTAL;

  // // Sequence Register 2 - ADC channels 7-12
  // if(remainingCnt > 0)
  // {
  //   seqRegCount = MEADOW_ADC_SEQ_2_REGISTER_TOTAL;
  //   if(remainingCnt < MEADOW_ADC_SEQ_2_REGISTER_TOTAL)
  //     seqRegCount = remainingCnt;

  //   regval = getreg32(STM32_ADC1_SQR2);
  //   regval &= ADC_SQR2_RESERVED;   // Clear all SQR Bits
  //   ret = populate_adc_seq_channel(&regval, seqRegCount, ADC_SQR2_SQ7_SHIFT);
  //   if(ret < 0)
  //     return ret;    // Error
  //   putreg32(regval, STM32_ADC1_SQR2);
  //   remainingCnt -= MEADOW_ADC_SEQ_2_REGISTER_TOTAL;
  // }
 
  // // Sequence Register 1 - ADC channels (13-16)
  // if(remainingCnt > 0)
  // {
  //   seqRegCount = MEADOW_ADC_SEQ_1_REGISTER_TOTAL;
  //   if(remainingCnt < MEADOW_ADC_SEQ_1_REGISTER_TOTAL)
  //     seqRegCount = remainingCnt;

  //   regval = getreg32(STM32_ADC1_SQR1);
  //   regval &= ADC_SQR1_RESERVED;   // Clear all SQR Bits
  //   ret = populate_adc_seq_channel(&regval, seqRegCount, ADC_SQR1_SQ13_SHIFT);
  //   if(ret < 0)
  //     return ret;    // Error
  //   putreg32(regval, STM32_ADC1_SQR1);
  // }

  // // Last set the size
  // regval = getreg32(STM32_ADC1_SQR1);
  // regval |= ((_gpioCount - 1) << ADC_SQR1_L_SHIFT);
  // putreg32(regval, STM32_ADC1_SQR1);
  // [--] END - CARRIED FROM non-working meadow_adc.c

  // (--) WHY NOT WITH OTHER CR2 CONFIGS?
  // Enable DMA of ADC
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  // DDS may only be for single ADC mode  (may be only when not using sequence???)
  // but comments 15.8.1 ony double buffered circular mode
  regval |= ADC_CR2_DDS;          // 1=Enable DMA Disable Selection
  regval |= ADC_CR2_DMA;          // 1=Enable DMA
  regval |= ADC_CR2_CONT;         // 1=Enable continuous mode
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
}

//================================================================
// Enable ADC
static void adc_enable(void)
{
  uint32_t regval;

  // Set ADON to turn on this ADC
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_ADON;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // Wait a bit
  usleep(400);
}

//================================================================
// Start ADC
static void adc_start(void)
{
  uint32_t regval;
  
  regval  = getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);
  regval = 0;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);

  // Start conversion
  regval  = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_SWSTART;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
}

//================================================================
// Initialize DMA
static void dma_initialize(void)
{
  uint32_t regval;

// FOR DMA-SEEMS TO BE NO DIFFERENCE BETWEEN NUTTX AND DIRECT REGISTER VERSIONS
#if (1)   // Use Nuttx code
  // // Using Nuttx DMA module to handle DMA setup
  // if(_dmaHandle != NULL)
  // {
  //   // Needed only if previous dma being modified.
  //   stm32_dmastop(_dmaHandle);
  //   stm32_dmafree(_dmaHandle); 
  // }
  _dmaHandle = stm32_dmachannel(DMAMAP_ADC1_1);

  // Configure the DMA SCR (Stream Control Register) values
  regval = getreg32(STM32_DMA2_S0CR);
  regval =  DMA_SCR_MSIZE_16BITS;   // Size of memory transfer
  regval |= DMA_SCR_PSIZE_16BITS;   // Size of peripheral transfer
  // Memory increment mode. 0=mem addr is fixed, 1=mem addr increments
  regval |= DMA_SCR_MINC;           // Mem Increment
  regval |= DMA_SCR_CIRC;           // Circular mode 1=enabled
  regval |= DMA_SCR_DIR_P2M;        // Direction 0=Perph->Mem

#if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  // For double-buffered add DMA_SCR_DBM to regval, the second buffer must be
  // follow the first in memory, so a double sized buffer is needed.
  // Internal to stm32_dmasetup() it sets the second buffer memory addr
  // based on the address of the one supplied + the buffer size.
  regval |= DMA_SCR_DBM;            // Double buffered mode
#endif

  // SxNDTR is set by Nuttx
  stm32_dmasetup(_dmaHandle,
                 STM32_ADC1_BASE + STM32_ADC_DR_OFFSET, // Peripheral addr
                 (uint32_t) _dmaDataBuffer,         //  NEW BUFFER
                 ADC_TESTS_DMA_DATA_BUFFER_SIZE,    // number of transfers
                 regval);

  // Provide DMA callback
  // void *arg will be returned via callback to ISR
  // true/false for half buffer callback as well as full buffer.
  stm32_dmastart(_dmaHandle, adc_dma_interrupt_handler_isr,
            (void *)STM32_ADC1_BASE, false);
#else
//------------------------------------------------------------
  // (--) PLAN B - Stop using Nuttx DMA code
  // Use direct register code
  // ADC1 DMA can be found here:DMA2, DMA_STREAM0, DMA_CHAN0
  // regval = getreg32(STM32_DMA2_S0CR);
  // regval &= ~DMA_SCR_EN;      // Clear the enable bit
  // putreg32(regval, STM32_DMA2_S0CR);

  // // Per Ref Man 8.3.18 must wait till 0
  // while ((getreg32(STM32_DMA2_S0CR) & DMA_SCR_EN) != 0);

  // Insure the DMA clock is on.
  regval = getreg32(STM32_RCC_APB1ENR);
  regval |= RCC_AHB1ENR_DMA2EN;
  putreg32(regval, STM32_RCC_APB1ENR);

  // Offset to DMA2 Stream 0 Control Register
  regval = getreg32(STM32_DMA2_S0CR);
  
  // Clear all the bits that can be set
  regval &= 0xe0000000;
  // NOT NEEDED HERE IT'S DONE BELOW
  putreg32(regval, STM32_DMA2_S0CR);

  // Select the Channel 0 by clearing (0000)
  // DIFFERENCE?? SAMPLE USES 0x7 DMA_SCR_CHSEL_MASK IS 0xf
  regval &= ~(7<<DMA_SCR_CHSEL_SHIFT);

  // (--) Trying Double-Buffer mode
  // The CT bit reveals which buffer is being filled
  // CT = 0 -> Memory 0
  // CT = 1 -> Memory 1  
  // regval |= DMA_SCR_DBM;
  // (--) Trying Double-Buffer mode
  // Set the memory size (MSIZE) is 16-bits 01
  // regval &= ~DMA_SCR_MSIZE_MASK;    // Clear both bits
  regval |= DMA_SCR_MSIZE_16BITS;   // Set size
  // Set the peripheral size (PSIZE) is 16-bits 01 
  // regval &= ~DMA_SCR_PSIZE_MASK;    // Clear both bits
  regval |= DMA_SCR_PSIZE_16BITS;   // Set size
  // Enable Memory Address Increment (MINC)
  // PINC is left 0 to not increment
  regval |= DMA_SCR_MINC;   // For 16-bit values incrementes by 2
  // Set Circular mode
  regval |= DMA_SCR_CIRC;
  // Set direction as Peripheral to Memory
  regval &= ~DMA_SCR_DIR_MASK;    // Clear both bits
  regval |= DMA_SCR_DIR_P2M;
  putreg32(regval, STM32_DMA2_S0CR);
  // Per Ref Man 8.3.18 must wait
  // while ((getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET) & ADC_CR2_ADON) != 0);

  // DMA2 Channel 0 Number of Data Transfer Register
  regval = getreg32(STM32_DMA2_S0NDTR);
  // regval &= ~0x0000ffff;      // Clear 15:0
  // // regval |= xferSize;
  regval = ADC_TESTS_DMA_DATA_BUFFER_SIZE;
  putreg32(regval, STM32_DMA2_S0NDTR);

  // DMA2 Channel 0 Peripheral Register
  regval = getreg32(STM32_DMA2_S0PAR);
  // regval = sourceAddr;
  regval = STM32_ADC1_BASE + STM32_ADC_DR_OFFSET;
  putreg32(regval, STM32_DMA2_S0PAR);

  // DMA2 Channel 0 Memory Address Register 0
  regval = (uint32_t)_dmaDataBuffer1;
  putreg32(regval, STM32_DMA2_S0M0AR);

#if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  // Setup alternate buffer's address
  // DMA2 Channel 0 Memory Address Register 1
  regval = _dmaDataBuffer2;
  putreg32(regval, STM32_DMA2_S0M1AR);
#endif

  // (--) CALLING DMA ISR NEED WORK-THE CODE TO ASSIGN THE ISR ISN'T WRITTEN
  // //       1/2 full         full          transfer err  Direct Mode err
  // regval = getreg32(STM32_DMA2_S0CR);
  // regval |= DMA_SCR_HTIE | DMA_SCR_TCIE | DMA_SCR_TEIE | DMA_SCR_DMEIE;
  // putreg32(regval, STM32_DMA2_S0CR);

  // Enable DMA Stream.
  regval = getreg32(STM32_DMA2_S0CR);
  regval |= DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);

#endif    // #if ADC_TESTS_USE_DMA_TRANSFER > 0
}

//================================================================
// ORIGINAL ana_to_dig_conv_tests.c Entry point
  // (--) This part of the code could be called > 1 time when it supports more
  // than ADC1 for debugging. However the ADC reset done via RCC will only
  // need to be done once.
  // nuttx/arch/arm/src/stm32f7/chip/stm32f74xx77xx_adc.h
void meadow_adc_initialize(void)
{
  int ret;
  static bool firstTime = true;
  
  if(firstTime)
  {
    firstTime = false;
#if ADC_TESTS_USE_DMA_TRANSFER > 0
    _dmaHandle = NULL;
#endif
    
    // stm32_configgpio(GPIO_V2_A00_IN4_PA4);
    // stm32_configgpio(GPIO_V2_A01_IN5_PA5);
    // stm32_configgpio(GPIO_V2_A02_IN3_PA3);
    // stm32_configgpio(GPIO_V2_A03_IN8_PB0);
    // stm32_configgpio(GPIO_V2_A04_IN9_PB1);
    // stm32_configgpio(GPIO_V2_A05_IN10_PC0);
  }
  else
  {
    syslog(1, "Please, only once\n");
    return;
  }

  syslog(1, "--> Entered meadow_adc_initialize()\n"); usleep(20 * 1000);


  // Setup the ADC Interrupt handler
  ret = irq_attach(STM32_IRQ_ADC, adc_conversion_interrupt_handler_isr,
            (void *)STM32_ADC1_BASE);
  if(ret < 0)
  {
    syslog(1, "Error calling irq_attach\n");
  }

  adc_initialize();

  adc_enable();

  dma_initialize();

  adc_start();

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);

  // ADC with DMA should be running at this point
  syslog(1, "--> Exiting ADC config\n"); usleep(20 * 1000);
}

#endif  // #if defined (CONFIG_ADC_TESTS)

/*==========================================================================================*/

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// NEW ENTRY POINT
// OPTION gpioList could require an 0xff terminator instead of gpioCount
// As few as 1 GPIO with 1 buffer element and as many as 16 GPIO with a buffer
// limit not specified.
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
          volatile uint16_t *dataBuffer, uint32_t convBuffSize)
{
  _gpioList = gpioList;
  _gpioCount = gpioCount;
  _dmaDataBuffer = dataBuffer;
  
  syslog(1, "---meadow_adc configuration. gpioCount:%lu, convBufSize:%lu, BufferAddr:%p\n",
            gpioCount, convBuffSize, _dmaDataBuffer);

  // START OF ORIGINAL
  // int ret;
//   uint32_t gpioListOff;
//   uint32_t mapOff = 0;


//   // Must have at least 1 entry per gpio
//   if(gpioCount < convBuffSize)
//   {
//     syslog(1, "%s@%d-Error:gpioCount:%lu < convBuffSize:%lu\n",
//               __FILE__, __LINE__, gpioCount, convBuffSize);
//     return -EINVAL;   // Invalid argument
//   }

//   // Verify GPIO list is valid
//   for(gpioListOff = 0; gpioListOff < gpioCount; gpioListOff++)
//   {
//     // Look for match
//     for(mapOff = 0; mapOff < MEADOW_ADC_GPIO_CHAN_MAP_LENGTH; mapOff++)
//     {
//       if(gpioList[gpioListOff] == _gpioAdcChanMap[mapOff])
//         break;   // Found-it's connected to ADC
//     }
//   }

//   // Did loop check all entries with no match?
//   if(gpioListOff == gpioCount && mapOff == MEADOW_ADC_GPIO_CHAN_MAP_LENGTH)
//   {
//     syslog(1, "%s@%d-Error:GPIO not found, gpioListOff:%lu, gpioCount:%lu, mapOff:%lu, MAP_LENGTH:%lu\n",
//               __FILE__, __LINE__, gpioListOff, gpioCount,
//               mapOff, MEADOW_ADC_GPIO_CHAN_MAP_LENGTH);
//     return -EINVAL;   // Invalid argument
//   }

//   // Will the number of GPIOs exactly fill the provided buffer?
//   if(gpioCount % convBuffSize != 0)
//   {
//     // Warning there will be empty array elements in the buffer after
//     // conversion.
//     syslog(1, "%s@%d-WARNING:buffer won't be completely filled\n",
//                 __FILE__, __LINE__);
//   }

// // (--) TO DO MUST CALCULATE AND WARN AS
//   // How may cycles of conversion will fit in the provided data buffer

//   // Only after conversion has finished
//   // sem_init(&_waitTillDoneSem, 0, 1);

  meadow_adc_initialize();

// END OF ORIGINAL
  return OK;
}

//=========================================================
// NEW ENTRY POINT
// Calling this function will initiate the ADC converstion process. It will
// run until the configured buffer is full. When the buffer is full (or error)
// the calling thread will return to the caller, signifing that the buffer
// is ready for inspection.
int meadow_adc_read_conversions(void)
{
  // START OF ORIGINAL
  // syslog(1, "Entered meadow_adc_read_conversions\n");
  
  // Wait for conversion to finish
  // meadow_adc_buffer_takesem(&_waitTillDoneSem);
  // END OF ORIGINAL

  return OK;
}

