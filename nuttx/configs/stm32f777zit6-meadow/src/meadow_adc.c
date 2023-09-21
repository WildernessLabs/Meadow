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
#define ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME (1)

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

#if ADC_TESTS_USE_DMA_TRANSFER > 0
#define ADC_TESTS_USE_DOUBLE_BUFFERING (0)
  DMA_HANDLE _dmaHandle;
#endif

// Sequence Registers hold different number of inputs
#define MEADOW_ADC_SEQ_3_REGISTER_TOTAL (6)
#define MEADOW_ADC_SEQ_2_REGISTER_TOTAL (6)
#define MEADOW_ADC_SEQ_1_REGISTER_TOTAL (4)

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

// /************************************************************************************
//  * Private Data
//  ************************************************************************************/
// From data sheet - GPIO to ADC1 channel input map
// Entries represent the STM32F7's valid GPIOs the can be used for ADC
static uint8_t _gpioAdcChanMap[] =
{
  0x00,      // Chan 0 = PA0
  0x01,      // Chan 1 = PA1
  0x02,      // Chan 2 = PA2
  0x03,      // Chan 3 = PA3
  0x04,      // Chan 4 = PA4
  0x05,      // Chan 5 = PA5
  0x06,      // Chan 6 = PA6
  0x07,      // Chan 7 = PA7
  0x10,      // Chan 8 = PB0
  0x11,      // Chan 9 = PB1
  0x20,      // Chan 10 = PC0
  0x21,      // Chan 11 = PC1
  0x22,      // Chan 12 = PC2
  0x23,      // Chan 13 = PC3
  0x24,      // Chan 14 = PC4
  0x25,      // Chan 15 = PC5
};
#define MEADOW_ADC_GPIO_CHAN_MAP_LENGTH (sizeof(_gpioAdcChanMap))

#if ADC_TESTS_USE_DOUBLE_BUFFERING > 0
  // 2-buffers in one is required by Nuttx dma code
#else
 volatile uint16_t *_dmaDataBuf;
 uint16_t *_userDataBuf;
  static uint32_t _adcBufSzBytes;
#endif
  static uint32_t _gpioTransferCount;
  static uint8_t *_gpioList;
  static bool _meadowAdcInit = false;

#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME > 0
  static sem_t _waitTillDoneSem;
#endif

  static int _totalChkd;
  static int _noChangeCnt;
  static bool _neverChkd;
  static bool _previousA00High;
  
// /************************************************************************************
//  * Private Function Prototypes
//  ************************************************************************************/

static int get_in_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan);
static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegMaxGpios,
          uint32_t initRegShift);
static int meadow_adc_initialize(void);

static void show_all_data_in_buffer(char *headerText, uint16_t dataBuffer[],
                            uint32_t dataBufElements);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME > 0
static void meadow_adc_buffer_takesem(sem_t *semaphore)
{
  int ret;
  do
  {
    ret = sem_wait(semaphore);
  }
  while (ret == -EINTR);
}
#endif

//==========================================================================
// Find ADC input channel (0-16) from the provided GPIO input port/pin
static int get_in_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan)
{
  int mapOff;

  // Look for match
  for(mapOff = 0; mapOff < MEADOW_ADC_GPIO_CHAN_MAP_LENGTH; mapOff++)
  {
    if(pinId == _gpioAdcChanMap[mapOff])
    {
      *adcInputChan = mapOff;
      return OK;
    }
  }

  // Return error since no GPIO matched any valid analog input GPIOs
  return -EBADSLT;    // 55 - Invalid Slot
}

//======================================================================
// Helps fill the sequence registers
static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegMaxGpios,
          uint32_t initRegShift)
{
  int ret;
  uint32_t regCnt;
  uint32_t adcInputChan;

  // Cycle through all the slots in this register
  for(regCnt = 0; regCnt < seqRegMaxGpios; regCnt++)
  {
    // For entry x what 'ADC input' channel
    ret = get_in_chan_from_pinid(_gpioList[regCnt], &adcInputChan);
    if(ret < 0)
      return ret;    // Error

    *regval |= (adcInputChan << (initRegShift + (regCnt * 5)));
  }
  return OK;
}


#if ADC_TESTS_USE_DMA_TRANSFER > 0
//==========================================================================
// DMA ISR
static void adc_dma_interrupt_handler_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;
  // static int execCnt = 0;

  // Without this the buffer's data is often not updated
  up_invalidate_dcache((uintptr_t)_dmaDataBuf,
                       (uintptr_t)_dmaDataBuf + _adcBufSzBytes);

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
    // CTEIF
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
    static bool toggleD04High = true;

    // syslog(1, ">>>>> DMA:Transfer complete\n");

    // With a 2k2 resistor connected to DEBUG_PIN_CCM_D04_PB9 and from this
    // junction a 4k7 connected to ground and another 4k7 to 3v3, then each
    // time DEBUG_PIN_CCM_D04_PB9 changes, the voltage at GPIO_V2_A00_IN4_PA4
    // will toggle about 1 volt above and 1 volt below mid-way between ground
    // and 3v3.
    
    // Look at the ADC value for GPIO_V2_A00_IN4_PA4. It should
    // change on every DMA transfer complete interrupt.
    _totalChkd++;
    if(_dmaDataBuf[0] > 2048)
    {
      // Voltage above the mid point "high"
      DEBUG_SET_LOW(DEBUG_PIN_CCM_D04_PB9);
      if(_previousA00High)
      {
        _noChangeCnt++;             // Still high
      }
      else
      {
        _previousA00High = true;    // Changed
      }
    }
    else
    {
      // Voltage below the mid-point "low"
      DEBUG_SET_HIGH(DEBUG_PIN_CCM_D04_PB9);
      if(_previousA00High)
      {
        _previousA00High = false;   // Changed
      }
      else
      {
        _noChangeCnt++;             // Still low
      }
    }

#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME > 0
// (--) Needed?
    // Restore the transfer count
    regval = getreg32(STM32_DMA2_S0NDTR);
    if(regval == 0)
    {
      regval &= ~0x0000ffff;      // Clear 15:0
      regval = _gpioTransferCount;
      putreg32(regval, STM32_DMA2_S0NDTR);
      // regval = getreg32(STM32_DMA2_S0NDTR);
      // syslog(1, "DMA:Transfer complete. SxNDTR was 0 now:%lu\n", regval);
    }

    // Now DMA2's SxCR register's will need to be re-enabled after each
    // conversion. This is because it's cleared whenever an a DMA transfer
    // has been completed.
// (--) Needed?
    regval  = getreg32(STM32_DMA2_S0CR);
    regval |= DMA_SCR_EN;
    putreg32(regval, STM32_DMA2_S0CR);

    // From Ref Man 15.8.1
    // At the end of the last DMA transfer (number of transfers configured in the
    // DMA controller’s DMA_SxNTR register):
    // • No new DMA request is issued to the DMA controller if the DDS bit is
    // cleared to 0 in the ADC_CR2 register (this avoids generating an overrun
    // error). However the DMA bit is not cleared by hardware. It must be written
    // to 0, then to 1 to start a new transfer.
    // • Requests can continue to be generated if the DDS bit is set to 1. This
    // allows configuring the DMA in double-buffer circular mode.
    // (--) Needed?
regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
    regval &= ~ADC_CR2_DMA;
    putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    regval |= ADC_CR2_DMA;
    putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    // Wakeup callers thread
    sem_post(&_waitTillDoneSem);
#endif
  }
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
    // 1. Reinitialize the DMA (adjust destination address and NDTR counter). This is DMA
    // 2. Clear the ADC OVR bit in ADC_SR register (below)  This is ADC
    // 3. Trigger the ADC to start the conversion (below)   This is ADC
    //
    // (--) #1 above - there doesn't appear to be in the stm32f7/stm32_dma.c
    // code a function to "Reinitialize the DMA".
    // See nuttx/arch/arm/src/stm32f7/stm32_dma.c @657-673 for code that would
    // do the above requirement (I think).
#endif
  }

  // End of conversion - got a value?
  if ((pendingInterrupts & ADC_SR_EOC) != 0)
  {
    syslog(1, "-- ADC ISR-End of Conversion --\n");
    // (--) What to do with the following comments and code?
    // // With a 2k2 resistor connected to DEBUG_PIN_CCM_D04_PB9 and from this
    // // junction a 4k7 connected to ground and another 4k7 to 3v3, then each
    // // time DEBUG_PIN_CCM_D04_PB9 changes, the voltage at GPIO_V2_A00_IN4_PA4
    // // will toggle about 1 volt above and 1 volt below mid-way between ground
    // // and 3v3.
    
    // // Look at the ADC value for GPIO_V2_A00_IN4_PA4. It should
    // // change on every DMA transfer complete interrupt.
    // _totalChkd++;
    // if(_dmaDataBuf[0] > 2048)
    // {
    //   // Voltage above the mid point "high"
    //   DEBUG_SET_LOW(DEBUG_PIN_CCM_D04_PB9);
    //   if(_previousA00High)
    //   {
    //     _noChangeCnt++;             // Still high
    //   }
    //   else
    //   {
    //     _previousA00High = true;    // Changed
    //   }
    // }
    // else
    // {
    //   // Voltage below the mid-point "low"
    //   DEBUG_SET_HIGH(DEBUG_PIN_CCM_D04_PB9);
    //   if(_previousA00High)
    //   {
    //     _previousA00High = false;   // Changed
    //   }
    //   else
    //   {
    //     _noChangeCnt++;             // Still low
    //   }
    // }

    // // // Now DMA2's SxCR register's will need to be re-enabled after each
    // // // conversion. This is because it's cleared whenever an a DMA transfer
    // // // has been completed.
    // // regval  = getreg32(STM32_DMA2_S0CR);
    // // regval |= DMA_SCR_EN;
    // // putreg32(regval, STM32_DMA2_S0CR);

    // // // And per Ref Man 15.8.1 - Must clear and reset DMA
    // // regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
    // // regval &= ~ADC_CR2_DMA;
    // // putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    // // regval |= ADC_CR2_DMA;
    // // putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    // // Wakeup callers thread
    // sem_post(&_waitTillDoneSem);
  }

  // Clear any interrupts
  pendingInterrupts &= ~ADC_ALL_POSSIBLE_ADC_INTERRUPTS;
  putreg32(pendingInterrupts, baseADCAddr + STM32_ADC_SR_OFFSET);

  return OK;
  // END IF ADC ISR
}

//======================================================================
// Assumes ADC 1
static int adc_initialize (void)
{
  uint32_t regval;

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_D00_PI9);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_D01_PH13);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_D02_PH10);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_D03_PB8);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_CCM_D04_PB9);

  // adc_test_display_basic_adc_regs(STM32_ADC1_BASE);

  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
  // (--) NEEDED ?
  regval = getreg32(STM32_RCC_APB2ENR);
  regval |= RCC_APB2ENR_ADC1EN;
  putreg32(regval, STM32_RCC_APB2ENR);

  regval = getreg32(STM32_RCC_AHB1ENR);
  regval |= RCC_AHB1ENR_DMA2EN;
  putreg32(regval, STM32_RCC_AHB1ENR);

  // Reset all the ADCs via Reset and Clock Control (RCC). For the STM32F7
  // there is a single bit for all ADCs. Other MCUs have a bit for each ADC.
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval |= RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Restore ADC from reset state
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval &= ~RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Turn-off ADC
  // regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  // regval &= ~ADC_CR2_ADON;      // 0=A/D Converter off (turned on later)
  // putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // Set the ADC watchdog high and low threshold to max and min
  putreg32(0x00000fff, STM32_ADC1_BASE + STM32_ADC_HTR_OFFSET);
  putreg32(0x00000000, STM32_ADC1_BASE + STM32_ADC_LTR_OFFSET);

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

  //---------------------------------------------------
  // ADC Control Register 1 (CR1)
  // Get the ADC Control Register 1 register. This register controls a lot of
  // options. I put the following in the same order as the Ref Man 15.13.2
  // This is mostly interrupt configuration
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  // [--] THIS MAY BE WRONG Buffer overrun has special instructions to resume
  // conversion. See section 15.8.1 for details
  regval |= ADC_CR1_OVRIE;       // 1=Enable Overrun interrupt
  //? regval &= ~ADC_CR1_RES_MASK;    // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;      // Set resolution 00=12, 01=10, 10=8 or 11=6 bits
  // regval |= ADC_CR1_AWDEN;       // 0=Disable Analog watchdog on regular channels
  // regval &= ~ADC_CR1_JAWDEN;      // 0=Disable Analog watchdog on injected
  // regval &= ~ADC_CR1_JDISCEN;     // 0=Disable discontinuous mode on injected channels
  // regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  // regval &= ~ADC_CR1_DISCEN;      // 0=Disable discontinuous mode on regular channels
  // regval &= ~ADC_CR1_JAUTO;       // 0=Automatic Injected Group conversion
  // regval &= ~ADC_CR1_AWDSGL;      // 0=Disable watchdog on single channel in scan mode
  // In Scan mode, the inputs selected through the ADC_SQRx
  regval |= ADC_CR1_SCAN;             // 1=Scan mode (Scans channels in ADC_SQRx registers)
  // regval |= ADC_CR1_JEOCIE;        // 0=Disable interrupt for injected channels
  // regval |= ADC_CR1_AWDIE;         // 0=Analog Watchdog interrupt enable
  // EOCIE seems to have no effect when using DMA
  regval |= ADC_CR1_EOCIE;        // 1=Enable ADC interrupt for EOC (not with DMA)
  // regval &= ~ADC_CR1_AWDCH_MASK;   // Clear the watchdog channel to 00000=Chan 0

  // Set for IN4 (PA4) while testing
  // (--) NOT NEEDED ??? regval |= (4 << ADC_CR1_AWDCH_SHIFT);  // 00100=Channel 4 analog watchdog select bits
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  //---------------------------------------------------
  // ADC Control Register 2 (CR2)
  // Note:fields not defined in header file have been ignored
  // Missing fields: SWSTART, EXTSEL, JSWSTART, JEXTEN, JEXTSEL, DDS & EOCS
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_EXTEN_MASK;  // Clear bits
  regval |= ADC_CR2_EXTEN_NONE;   // No trigger from external sources
  regval &= ~ADC_CR2_ALIGN;       // 0=Right alignment (1=left alignment)
  regval &= ~ADC_CR2_EOCS;        // 1=End of each conversion, 0=End of sequence
  // NOTE: 'DDS' and 'DMA' fields exist in 2 ADC registers, CR2 and CCR
  // DDS=0 No new DMA request is issued after the last transfer
  // DDS=1 DMA requests are issued as long as data are converted and DMA=1
  // regval |= ADC_CR2_DDS;        // DDS = DMA Disable Selection
  regval &= ~ADC_CR2_DDS;
  regval |= ADC_CR2_DMA;          // 1=Enable DMA

#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME == 0
  // Enable continous conversion
  regval &= ~ADC_CR2_CONT;         // 1=Enable continuous conversion
#endif
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

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
#if(1)
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);
  regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
 
  // All Pins available
  if(_gpioTransferCount == 6)
  {
    regval |= (4  << ADC_SQR3_SQ1_SHIFT);   // Channel 4  - A00 [PA4]->ADC123_IN4
    regval |= (5  << ADC_SQR3_SQ2_SHIFT);   // Channel 5  - A01 [PA5]->ADC123_IN5
    regval |= (3  << ADC_SQR3_SQ3_SHIFT);   // Channel 3  - A02 [PA3]->ADC123_IN3
    regval |= (8  << ADC_SQR3_SQ4_SHIFT);   // Channel 8  - A03 [PB0]->ADC12_IN8
    regval |= (9  << ADC_SQR3_SQ5_SHIFT);   // Channel 9  - A04 [PB1]->ADC12_IN9
    regval |= (10 << ADC_SQR3_SQ6_SHIFT);   // Channel 10 - A05 [PC0]->ADC123_IN10
  }
  else if (_gpioTransferCount == 3)
  {
    regval |= (4  << ADC_SQR3_SQ1_SHIFT);   // Channel 4  - A00 [PA4]->ADC123_IN4
    regval |= (5  << ADC_SQR3_SQ2_SHIFT);   // Channel 5  - A01 [PA5]->ADC123_IN5
    regval |= (3  << ADC_SQR3_SQ3_SHIFT);   // Channel 3  - A02 [PA3]->ADC123_IN3
  }
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);
  //? regval &= ADC_SQR1_RESERVED;            // Clear all SQR Bits
  regval |= ((_gpioTransferCount - 1) << ADC_SQR1_L_SHIFT);    // A 5 will convert 6
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR1_OFFSET);
#else
  // BEGIN - CARRIED FROM non-working meadow_adc.c
// THE FOLLOWING HAS BEEN PARTIAL TESTED - BUT THE ABOVE IS DOING THE WORK
// WHILE TESTING  
  int ret;
  uint32_t seqRegMaxGpios;
  uint32_t remainingCnt = _gpioTransferCount;

  // Sequence Register 3 - ADC channels 1-6
  seqRegMaxGpios = MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
  if(remainingCnt < MEADOW_ADC_SEQ_3_REGISTER_TOTAL)
    seqRegMaxGpios = remainingCnt;

  regval = getreg32(STM32_ADC1_SQR3);
  regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
  ret = populate_adc_seq_channel(&regval, seqRegMaxGpios, ADC_SQR3_SQ1_SHIFT);
  if(ret < 0)
    return ret;    // Error
  putreg32(regval, STM32_ADC1_SQR3);
  remainingCnt -= MEADOW_ADC_SEQ_3_REGISTER_TOTAL;

  // Sequence Register 2 - ADC channels 7-12
  if(remainingCnt > 0)
  {
    seqRegMaxGpios = MEADOW_ADC_SEQ_2_REGISTER_TOTAL;
    if(remainingCnt < MEADOW_ADC_SEQ_2_REGISTER_TOTAL)
      seqRegMaxGpios = remainingCnt;

    regval = getreg32(STM32_ADC1_SQR2);
    regval &= ADC_SQR2_RESERVED;   // Clear all SQR Bits
    ret = populate_adc_seq_channel(&regval, seqRegMaxGpios, ADC_SQR2_SQ7_SHIFT);
    if(ret < 0)
      return ret;    // Error
    putreg32(regval, STM32_ADC1_SQR2);
    remainingCnt -= MEADOW_ADC_SEQ_2_REGISTER_TOTAL;
  }
 
  // Sequence Register 1 - ADC channels (13-16)
  if(remainingCnt > 0)
  {
    seqRegMaxGpios = MEADOW_ADC_SEQ_1_REGISTER_TOTAL;
    if(remainingCnt < MEADOW_ADC_SEQ_1_REGISTER_TOTAL)
      seqRegMaxGpios = remainingCnt;

    regval = getreg32(STM32_ADC1_SQR1);
    regval &= ADC_SQR1_RESERVED;   // Clear all SQR Bits
    ret = populate_adc_seq_channel(&regval, seqRegMaxGpios, ADC_SQR1_SQ13_SHIFT);
    if(ret < 0)
      return ret;    // Error
    putreg32(regval, STM32_ADC1_SQR1);
  }

  // Last set the size
  regval = getreg32(STM32_ADC1_SQR1);
  regval |= ((_gpioTransferCount - 1) << ADC_SQR1_L_SHIFT);
  putreg32(regval, STM32_ADC1_SQR1);
  // END - CARRIED FROM non-working meadow_adc.c
#endif

  //------------------------------------------------------------
  // ADC Common Control Register (CCR) [Common == for all ADCs]
  regval = getreg32(STM32_ADC_CCR);
  // regval &= ~ADC_CCR_TSVREFE;       // 0=disable temperature sensor channel
  // regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel

  // ADCPRE - Calculation based on PCLK2=96MHz (with 192MHz clock). Per Data
  // Sheet 5.3.24 pp 165, max ADC clock is 36MHz. Therefore, divide by 4
  // (96/4=24MHz) is the highest freq. For clock details see Meadow's board.h
  regval &= ~ADC_CCR_ADCPRE_MASK;   // Clear any bits in ADC prescaler
  regval |= ADC_CCR_ADCPRE_DIV4;    // 01=ADC prescaler PCLK2 divided by 4

  // DMA access mode for multi ADC mode
  // 00: DMA mode disabled
  // 01: DMA mode 1 enabled (2 / 3 half-words one by one - 1 then 2 then 3)
  // 10: DMA mode 2 enabled (2 / 3 half-words by pairs - 2&1 then 1&3 then 3&2)
  // 11: DMA mode 3 enabled (2 / 3 bytes by pairs - 2&1 then 1&3 then 3&2)
  regval &= ~ADC_CCR_DMA_MASK;       // Clear any bits in DMA mode (multi-ADC mode only) 
  regval |= ADC_CCR_DMA_DISABLED;    // 00 = DMA Modes (multi-ADC mode only)
  // regval |= (1 << ADC_CCR_DMA_SHIFT) // 01: DMA mode 1 enabled
  // regval |= (2 << ADC_CCR_DMA_SHIFT) // 10: DMA mode 2 enabled
  // regval |= (3 << ADC_CCR_DMA_SHIFT) // 11: DMA mode 3 enabled

  regval &= ~ADC_CCR_DDS;         // 0=No new DMA request is issued after the last transfer
  regval &= ~ADC_CCR_DELAY_MASK;    // 0000=5*Tadcclk (only used for dual/triple)
  regval &= ~ADC_CCR_MULTI_MASK;    // Clear any bits
  regval |= ADC_CCR_MULTI_NONE;    // 00000=Independent mode
  putreg32(regval, STM32_ADC_CCR);

  return OK;
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

  // // Now DMA2's SxCR register's will need to be re-enabled after each
  // // conversion. This is because it's cleared whenever an a DMA transfer
  // // has been completed.
  // regval  = getreg32(STM32_DMA2_S0CR);
  // regval |= DMA_SCR_EN;
  // putreg32(regval, STM32_DMA2_S0CR);

  // Start ADC conversion
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
  regval |=  DMA_SCR_MSIZE_16BITS;  // Size of memory transfer
  regval |= DMA_SCR_PSIZE_16BITS;   // Size of peripheral transfer
  // Memory increment mode. 0=mem addr is fixed, 1=mem addr increments
  regval |= DMA_SCR_MINC;           // Mem Increment
  // regval |= DMA_SCR_CIRC;           // Circular mode 1=enabled
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
                 (uint32_t) _dmaDataBuf,    // Destination buffer
                 _gpioTransferCount,        // number to transfers
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
  // regval |= DMA_SCR_CIRC;
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
  regval = _gpioTransferCount;
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

  // regval = getreg32(STM32_DMA2_S0CR);
  // (--) CALLING DMA ISR NEED WORK-THE CODE NEEDED TO ASSIGN THE ISR ISN'T WRITTEN
  // //       1/2 full         full          transfer err  Direct Mode err
  // regval |= DMA_SCR_HTIE | DMA_SCR_TCIE | DMA_SCR_TEIE | DMA_SCR_DMEIE;
  // putreg32(regval, STM32_DMA2_S0CR);

  // Enable DMA Stream.
  regval = getreg32(STM32_DMA2_S0CR);
  regval |= DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);

#endif    // #if ADC_TESTS_USE_DMA_TRANSFER > 0
}

// //================================================================
// // This function is probably redundant to meadow_adc_reinitialize TBD
// static int meadow_adc_unconfigure(void)
// {
//   // At least this
//   free(_dmaDataBuf);
//   _dmaDataBuf = NULL;

//   return OK;
// }

// //================================================================
// // Unconfigure the existing setup
// void meadow_adc_reinitialize(void)
// {
//   int ret;
//   _meadowAdcInit = false;

//   meadow_adc_unconfigure();

//   // Now reconfigure
//   ret = meadow_adc_initialize();
// }

//================================================================
// Call all the sub-initialization functions
int meadow_adc_initialize(void)
{
  int ret;
  static bool firstTime = true;
  syslog(1, "--> Entered meadow_adc_initialize()\n"); usleep(20 * 1000);

  if(firstTime)
  {
    firstTime = false;
    memset(_dmaDataBuf, 0, _adcBufSzBytes);

#if ADC_TESTS_USE_DMA_TRANSFER > 0
    _dmaHandle = NULL;
#endif

    _totalChkd = 0;
    _noChangeCnt = 0;
    _neverChkd = true;
  }
  else
  {
    syslog(1, "Please, only once\n");
    return -EALREADY;
  }

  // Setup the ADC Interrupt handler
  ret = irq_attach(STM32_IRQ_ADC, adc_conversion_interrupt_handler_isr,
            (void *)STM32_ADC1_BASE);
  if(ret < 0)
  {
    syslog(1, "Error calling irq_attach\n");
    return ret;
  }

  ret = adc_initialize();
  if(ret < 0)
  {
    syslog(1, "Error calling adc_initialize\n"); usleep(20 * 1000);
    return ret;
  }

  dma_initialize();

  adc_enable();

#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME == 0
  adc_start();
#endif

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);
  
  _meadowAdcInit = true;

  // ADC with DMA should be running at this point
  syslog(1, "--> Exiting ADC config\n"); usleep(20 * 1000);

  return OK;
}

#endif  // #if defined (CONFIG_ADC_TESTS)

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// As few as 1 GPIO with 1 buffer element and as many as 16 GPIOs with a buffer
// limit not specified. However. the number of GPIOs must be an even multiple
// of the buffer.
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
          uint16_t *userDataBuf, uint32_t adcBufSzBytes)
{
  syslog(1, "meadow_adc configuration. gpioCount:%lu, adcBufSzBytes:%lu, userDataBuf:%p\n",
            gpioCount, adcBufSzBytes, userDataBuf);

  // (--) SINCE API NOT TOTALLY NAILED DOWN, WON'T DO TESTING UNTIL IT'S FINALIZED
  // uint32_t gpioListOff;
  // uint32_t mapOff = 0;

  // uint32_t userBufElements;
  // if(userDataBuf == NULL)
  // {
  //   syslog(1, "%s@%d-Error:userDataBuf is NULL\n",
  //             __FILE__, __LINE__, adcBufSzBytes);
  //   return -EINVAL;   // Invalid argument
  // }
  
  // // Data elements are 2 bytes, therefore, size must be even number
  // if(adcBufSzBytes % 2)
  // {
  //   syslog(1, "%s@%d-Error:ADC values are always 16-bits and adcBufSzBytes is not even (%lu)\n",
  //             __FILE__, __LINE__, adcBufSzBytes);
  //   return -EINVAL;   // Invalid argument
  // }

  // // Each element is 2 bytes
  // userBufElements = adcBufSzBytes/2;

  // // Must have at least 1 entry per gpio
  // if(gpioCount < userBufElements)
  // {
  //   syslog(1, "%s@%d-Error:gpioCount:%lu < userBufElements:%lu\n",
  //             __FILE__, __LINE__, gpioCount, userBufElements);
  //   return -EINVAL;   // Invalid argument
  // }

  // // Verify GPIO list is valid
  // for(gpioListOff = 0; gpioListOff < gpioCount; gpioListOff++)
  // {
  //   // Look for matching port/pin in the list
  //   for(mapOff = 0; mapOff < MEADOW_ADC_GPIO_CHAN_MAP_LENGTH; mapOff++)
  //   {
  //     if(gpioList[gpioListOff] == _gpioAdcChanMap[mapOff])
  //       break;   // Found-it's connected to the ADC
  //   }

  //   // Did we go through the entire list and not find a match?
  //   if(mapOff == MEADOW_ADC_GPIO_CHAN_MAP_LENGTH)
  //   {
  //     syslog(LOG_INFO, "GPIO:0x%02x (P%c%d) not connected to ADC\n", gpioList[gpioListOff],
  //                 (gpioList[gpioListOff] >> 4) + 'A', gpioList[gpioListOff] & 0x0f);
  //     return -EINVAL;   // Invalid argument
  //   }
  // }

  // // Will the number of GPIOs exactly fill the provided buffer?
  // if(gpioCount % userBufElements != 0)
  // {
  //   // Don't allow empty buffer slots as it's likely the user made an unintended error.
  //   syslog(1, "%s@%d-Error:buffer size and the number of GPIOs not multiple.\n",
  //               __FILE__, __LINE__);
  //   return -EINVAL;   // Invalid argument
  // }

  // Populate global values
  _gpioList = gpioList;
  _gpioTransferCount = gpioCount;
  _userDataBuf = userDataBuf;
  _adcBufSzBytes = adcBufSzBytes;

#if ADC_TESTS_DO_ONE_CONVERSION_AT_A_TIME > 0
  // Signaling semaphore
  sem_init(&_waitTillDoneSem, 0, 0);
  sem_setprotocol(&_waitTillDoneSem, SEM_PRIO_NONE);  
#endif

  // Do before configuration
  // DON'T FREE the buffer's address as it's part of the DMA configuration
  _dmaDataBuf = kmm_malloc(_adcBufSzBytes);
  if(_dmaDataBuf == NULL)
  {
    syslog(1, "%s@%d-Error:Buffer space not available\n",
                __FILE__, __LINE__);
    return -ENOMEM;   // Out of memory
  }

  meadow_adc_initialize();

  return OK;
}

// //=========================================================
// // NOT USED AT PRESENT
// // Support reconfiguration with different parameters
// int meadow_adc_reconfigure(uint8_t gpioList[], uint32_t gpioCount,
//           uint16_t *userDataBuf, uint32_t adcBufSzBytes)
// {
//   // First unconfigure
//   _meadowAdcInit = false;

//   // Now new configuration
//   return meadow_adc_configure(gpioList, gpioCount, userDataBuf, adcBufSzBytes);
// }

//=========================================================
// Calling this function will initiate the ADC converstion process. It will
// run until the configured buffer is full. When the buffer is full (or error)
// the calling thread will return to the caller, signifing that the buffer
// is ready for inspection.
int meadow_adc_read_conversions(void)
{
  static int callCount = 0;

  // DEBUG_SET_LOW(DEBUG_PIN_CCM_D03_PB8);
  if(! _meadowAdcInit)
  {
    syslog(1, "%s@%d-Error:Meadow ADC not initialized.\n", __FILE__, __LINE__);
    usleep(20 * 1000);
    return -EPERM;    // Operation not permitted
  }

  // Start the conversion
  DEBUG_SET_HIGH(DEBUG_PIN_CCM_D00_PI9);
  adc_start();
  DEBUG_SET_LOW(DEBUG_PIN_CCM_D00_PI9);
  
  // Wait for conversion to finish for valid data
  DEBUG_SET_HIGH(DEBUG_PIN_CCM_D01_PH13);
  meadow_adc_buffer_takesem(&_waitTillDoneSem);
  DEBUG_SET_LOW(DEBUG_PIN_CCM_D01_PH13);

  // Note this takes about 26 ms
  callCount++;
  // if((callCount % 67) == 0)
  {
    // Show data before copy
    syslog(1, "%04d-totalChkd:%05d, noChgCnt:%05d\n", callCount, _totalChkd, _noChangeCnt);
    show_all_data_in_buffer("ADC", _dmaDataBuf, _gpioTransferCount);
  }

  // Conversion must be complete. Now time to copy collected data to user
  // buffer and return
  DEBUG_SET_HIGH(DEBUG_PIN_CCM_D02_PH10);
  memcpy(_userDataBuf, _dmaDataBuf, _adcBufSzBytes);
  DEBUG_SET_LOW(DEBUG_PIN_CCM_D02_PH10);

  return OK;
}

//==========================================================================
void show_all_data_in_buffer(char *headerText, uint16_t dataBuffer[],
                            uint32_t dataBufElements)
{
#define DMA_ISR_DISP_MAX_PER_ROW (8)    // 8 elements / row
#define DMA_ISR_DISP_VAL_LEN (5)        // Data values take 5 char
#define DMA_ISR_DISP_LEADER_LEN (9)     // Addr takes 9 chars

  uint32_t dmaBuffOff = 0;
  int columnCnt;
  int lineBuffOff;
  int disp_max_per_row = DMA_ISR_DISP_MAX_PER_ROW;

  if(disp_max_per_row > dataBufElements)
    disp_max_per_row = dataBufElements;

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
    syslog(1, "%s:%s\n", headerText, lineBuff);

    // Line by line show entire buffer
  } while (dmaBuffOff < dataBufElements);
}
