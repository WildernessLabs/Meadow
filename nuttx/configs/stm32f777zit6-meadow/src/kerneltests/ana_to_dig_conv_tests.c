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
#include "chip/stm32f76xx77xx_dma.h"
#include <meadow/hcom_shared_common.h>
// nuttx/arch/arm/src/common/up_arch.h
// #include "chip.h"
// #include "stm32_rcc.h"
// #include "stm32_tim.h"
// #include "stm32_adc.h"

// FOR TESTING BBR
// #include "chip/stm32_rtcc.h"

#if defined (CONFIG_ADC_TESTS)
#warning "(--) Hacking ana_to_dig_conv_tests.c"

#ifndef CONFIG_STM32F7_DMA2
#error "Meadow ADC with DMA requires CONFIG_STM32F7_DMA2"
#endif

// Using DMA? This may be temporary
#define ADC_TESTS_USE_DMA_TRANSFER (1)

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

// These PA4 and PA5 are available for DAC in a future test
#define GPIO_V2_A00_IN4_PA4         (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A01_IN5_PA5         (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTA|GPIO_PIN5)
#define GPIO_V2_A02_IN3_PA3         (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A03_IN8_PB0         (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTB|GPIO_PIN0)
#define GPIO_V2_A04_IN9_PB1         (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTB|GPIO_PIN1)
#define GPIO_V2_A05_IN10_PC0        (GPIO_ANALOG|GPIO_PULLDOWN|GPIO_PORTC|GPIO_PIN0)

#define ADC_SMPR_DEFAULT    ADC_SMPR_112
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
#define ADC_TESTS_DMA_DATA_BUFFER_SIZE (16)

/************************************************************************************
 * Private Data
 ************************************************************************************/

  DMA_HANDLE _dmaHandle;
  uint16_t _dmaDataBuffer[ADC_TESTS_DMA_DATA_BUFFER_SIZE];

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

static int adc_test_config_adc(int adc_numb, uint32_t baseADCAddr);
static void adc_test_initialize_adc(int adcNumb);

static int adc_test_create_testing_thread(void);
static void *adc_test_kthread_func(int argc, char *argv[]);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// ADC tests
void meadow_kt_adc_tests(uint32_t userData)
{
  static int firstTime = true;

  syslog(1, "%s@%d-Entered meadow_kt_adc_tests, userData:%lu\n", __FILE__, __LINE__, userData);

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D01);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D02);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D03);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D04);
  
  DEBUG_SET_LOW(DEBUG_PIN_V2_D01);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D02);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D03);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D04);

  switch(userData)
  {
    case 1:
      if(firstTime)
      {
        firstTime = false;
        // Initialize only ADC-1 to start with
        adc_test_initialize_adc(1);
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

/************************************************************************************
 * Private Functions
 ************************************************************************************/
//================================================================
// Only for TESTING
static void adc_test_display_basic_adc_regs(uint32_t baseADCAddr)
{
  syslog(1, "SR:   0x%08x CR1:  0x%08x CR2:  0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SR_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET));

  syslog(1, "SQR1: 0x%08x SQR2: 0x%08x SQR3: 0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET));

  syslog(1, "CCR:  0x%08x\n", getreg32(STM32_ADC_CCR));
}

#if ADC_TESTS_USE_DMA_TRANSFER > 0

//==========================================================================
// DMA ISR
static void adc_dma_interrupt_handler_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  // uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;
  // static int execCnt = 0;

  DEBUG_SET_HIGH(DEBUG_PIN_V2_D03);

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
    // syslog(1, "DMA ISR reason:Half Transfer\n");
  }

  //------------------------------------------------------
  // Stream Transfer Complete flag
  if((status & DMA_STREAM_TCIF_BIT) != 0)
  {
    // syslog(1, "DMA ISR reason:Transfer Complete\n");

// #define DMA_ISR_DISP_MAX_PER_ROW (8)    // 8 elements / row
// #define DMA_ISR_DISP_VAL_LEN (5)        // Data values take 5 char
// #define DMA_ISR_DISP_LEADER_LEN (9)     // Addr takes 9 chars

//     int disp_max_per_row = DMA_ISR_DISP_MAX_PER_ROW;
//     if(disp_max_per_row > ADC_TESTS_DMA_DATA_BUFFER_SIZE)
//       disp_max_per_row = ADC_TESTS_DMA_DATA_BUFFER_SIZE;
//     int disp_char_per_row = (disp_max_per_row * DMA_ISR_DISP_VAL_LEN);
//     int disp_total_line_len = disp_char_per_row + DMA_ISR_DISP_LEADER_LEN;
//     int lineBuffOff;
//     uint32_t dmaBuffOff = 0;
//     int columnCnt;
//     char lineBuff[disp_total_line_len + 1];    // Room for NULL

//     do
//     {
//       lineBuffOff = 0;
//       snprintf(&lineBuff[lineBuffOff], disp_total_line_len, "%08x ", dmaBuffOff);
//       lineBuffOff = DMA_ISR_DISP_LEADER_LEN;

//       // Build a full row of data then print it
//       for(columnCnt = 0; columnCnt < disp_max_per_row; columnCnt++)
//       {
//         snprintf(&lineBuff[lineBuffOff],
//                   disp_char_per_row - (columnCnt * DMA_ISR_DISP_VAL_LEN),
//                   "%04u ", _dmaDataBuffer[dmaBuffOff++]);
//         lineBuffOff += DMA_ISR_DISP_VAL_LEN;
//       }

//       lineBuff[(lineBuffOff) + 1] = '\0';
//       syslog(1, "%s\n", lineBuff);

//       // Show entire buffer
//     } while (dmaBuffOff < ADC_TESTS_DMA_DATA_BUFFER_SIZE);
  }

  //---------------------------------------------------
  // Invalidate the entire data buffer
  up_invalidate_dcache((uintptr_t)_dmaDataBuffer,
                (uintptr_t)_dmaDataBuffer + ADC_TESTS_DMA_DATA_BUFFER_SIZE);

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
  
  DEBUG_SET_LOW(DEBUG_PIN_V2_D03);
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
  // uint32_t regval;
  uint32_t baseADCAddr = (uint32_t)arg;
  // static int isrCount = 0;

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
    // Reinitialze DMA

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

    // // ADC output is 12-bit
    // uint32_t adcValue = getreg32(baseADCAddr + STM32_ADC_DR_OFFSET);

    // // Just for testing
    // isrCount++;
    // if((isrCount % 25) == 0)
    // {
    //   syslog(1, "%d-ADC ISR value:%lu\n", isrCount, adcValue);
    // }
  }

  // Clear any interrupts
  pendingInterrupts &= ~ADC_ALL_POSSIBLE_ADC_INTERRUPTS;
  putreg32(pendingInterrupts, baseADCAddr + STM32_ADC_SR_OFFSET);

  // Without SCAN mode (ADC_CR1_SCAN) this will restart the conversion
  // Per Ref Man 15.8.2 this may need to be here
  // (--) THIS SEEMS LIKE THE WRONG WAY TO INSURE THAT THE CONVERSION IS
  // CONTINUOUS 
  // regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  // regval |= ADC_CR2_SWSTART;
  // putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // Don't leave ISR until the above have fully finished
  // asm volatile ("dsb");

  DEBUG_SET_LOW(DEBUG_PIN_V2_D04);
  return OK;
}

//================================================================
// Entry point
  // (--) This part of the code could be called > 1 time when it supports more
  // than ADC1 for debugging. However the ADC reset done via RCC will only
  // need to be done once.
  // nuttx/arch/arm/src/stm32f7/chip/stm32f74xx77xx_adc.h
void adc_test_initialize_adc(int adc_numb)
{
  int ret;
  static bool firstTime = true;
  irqstate_t flags;
  uint32_t regval;
  uint32_t baseADCAddr;
  uint32_t adcRccClkEnable;
  
  if(firstTime)
  {
    firstTime = false;
  }
  else
  {
    return;
  }

  syslog(1, "--> Entered adc_test_initialize_adc() ADC is:%d (1-3 valid)\n", adc_numb); usleep(20 * 1000);

  // Configure Analog Inputs for Testing
  stm32_configgpio(GPIO_V2_A02_IN3_PA3);
  stm32_configgpio(GPIO_V2_A00_IN4_PA4);    // DAC capable
  stm32_configgpio(GPIO_V2_A01_IN5_PA5);    // DAC capable
  stm32_configgpio(GPIO_V2_A03_IN8_PB0);
  stm32_configgpio(GPIO_V2_A04_IN9_PB1);
  stm32_configgpio(GPIO_V2_A05_IN10_PC0);

  // Find the base address for the ADC being configured
  // [--] THIS ASSUMES USING ADC2 AND ADC3 WHICH ARE ONLY NEEDED WHEN DOING
  // DUAL AND TRIPLE ADC CONVERSION MODES.
  switch(adc_numb)
  {
  case 1:
    baseADCAddr = STM32_ADC1_BASE;
    adcRccClkEnable = RCC_APB2ENR_ADC1EN;
    break;
  case 2:
    baseADCAddr = STM32_ADC2_BASE;
    adcRccClkEnable = RCC_APB2ENR_ADC2EN;
    break;
  case 3:
    baseADCAddr = STM32_ADC3_BASE;
    adcRccClkEnable = RCC_APB2ENR_ADC3EN;
    break;
  default:
    syslog(LOG_ERR, "%s@%d-The adc_numb:%d is invalid\n", __FILE__, __LINE__,
              adc_numb);
    return;
  }

  // Some RCC registers need to be accessed
  flags = enter_critical_section();

#if ADC_TESTS_USE_DMA_TRANSFER > 0
  _dmaHandle = NULL;
#endif

  // Reset all the ADCs via Reset and Clock Control (RCC). For the STM32F7
  // there is a single bit for all ADCs. Other MCUs have a bit for each ADC.
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval |= RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Restore ADC from reset state
  regval &= ~RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
  regval = getreg32(STM32_RCC_APB2ENR);
  regval |= adcRccClkEnable;
  putreg32(regval, STM32_RCC_APB2ENR);

  leave_critical_section(flags);

  // Setup the ADC Interrupt handler
  ret = irq_attach(STM32_IRQ_ADC, adc_conversion_interrupt_handler_isr,
            (void *)baseADCAddr);
  if(ret < 0)
  {
    syslog(1, "Error calling irq_attach\n");
  }

  // Configure ADC and start it converting
  syslog(1, "-->Configuring ADC\n"); usleep(20 * 1000);
  ret = adc_test_config_adc(adc_numb, baseADCAddr);
  if(ret < 0)
  {
    syslog(1, "Error calling adc_test_config_adc\n");
  }

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);

  syslog(1, "--> Exiting ADC config\n"); usleep(20 * 1000);
}

//==================================================================
// This code configures one specific ADC
static int adc_test_config_adc(int adc_numb, uint32_t baseADCAddr)
{
  irqstate_t flags;
  uint32_t regval;

  // SHOW REGISTER VALUES INITIALLY
  adc_test_display_basic_adc_regs(baseADCAddr); usleep(50 * 1000);

  flags = enter_critical_section();

  //------------------------------------------------------------------
  // Insure the ADC is off while setting up
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_ADON;      // 0=A/D Converter off (turned on later)
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // Set the ADC watchdog high and low threshold to max and min
  putreg32(0x00000fff, baseADCAddr + STM32_ADC_HTR_OFFSET);
  putreg32(0x00000000, baseADCAddr + STM32_ADC_LTR_OFFSET);

  //------------------------------------------------------------
  // ADC Common Control Register
  regval = getreg32(STM32_ADC_CCR);
  // regval &= ~ADC_CCR_TSVREFE;       // 0=disable temperature sensor channel
  // regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel

  // ADCPRE - Calculation based on PCLK2=96MHz (with 192MHz clock). Per Data
  // Sheet 5.3.24 pp 165, max ADC clock is 36MHz. Therefore, divide by 4
  // (96/4=24MHz) is the highest freq. For clock  details see Meadow's board.h
  regval &= ~ADC_CCR_ADCPRE_MASK;   // Clear any bits in ADC prescaler
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

  //---------------------------------------------------
  // Get the ADC Control Register 1 register. This register controls a lot of
  // options. I put the following in the same order as the Ref Man 15.13.2
  // This is mostly interrupt configuration
  regval = getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET);
  // regval &= ~ADC_CR1_OVRIE;       // 0=Disable Overrun interrupt
  regval &= ~ADC_CR1_RES_MASK;    // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;    // Set resolution 12, 10, 8 or 6 bits
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
  putreg32(regval, baseADCAddr + STM32_ADC_CR1_OFFSET);
  
  //---------------------------------------------------
  // ADC CR2 Configuration
  // Note:fields not defined in header file have been ignored
  // Missing fields: SWSTART, EXTSEL, JSWSTART, JEXTEN, JEXTSEL, DDS & EOCS
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_EXTEN_MASK;  // Clear bits
  regval |= ADC_CR2_EXTEN_NONE;   // Set to none channels (i.e. no trigger)
  regval &= ~ADC_CR2_ALIGN;       // 0=Right alignment (1=left alignment)
  // EOCS - End Of Conversion Selection. When should the EOC bit be set?
  regval |= ADC_CR2_EOCS;         // 1=End of each conversion, 0=End of sequence

#if ADC_TESTS_USE_DMA_TRANSFER > 0
  // DDS = 1 - DMA requests are issued as long as data converted and DMA=1
  regval |= ADC_CR2_DDS;          // DDS
  regval |= ADC_CR2_DMA;          // 1=Enable DMA
#endif
  // Enable continous conversion
  regval |= ADC_CR2_CONT;         // 1=Enable continuous mode
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

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
  // TESTING-Set all channels to the same default ADC_SMPR_DEFAULT (currently 112)
  regval = getreg32(baseADCAddr + STM32_ADC_SMPR1_OFFSET);
  // This #define will set all sample times to the same value. Here
  // ADC_SMPR_DEFAULTs are used to set ADC conversion the same.
  regval &= ~0xf8000000;      // Clear all valid fields
  regval |= ADC_SMPR1_DEFAULT;
  putreg32(regval, baseADCAddr + STM32_ADC_SMPR1_OFFSET);

  // Set sample time for channels 0-9
  regval = getreg32(baseADCAddr + STM32_ADC_SMPR2_OFFSET);
  regval &= ~0xc0000000;      // Clear all valid fields
  regval |= ADC_SMPR2_DEFAULT;
  // Leave all at 3 cycles
  // regval |= (ADC_SMPR_480 << ADC_SMPR2_SMP0_SHIFT);
  putreg32(regval, baseADCAddr + STM32_ADC_SMPR2_OFFSET);

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
  // I'm going to hardcode the 4 GPIOs that represent A02, A03, A04 and A05,
  // skipping A00 and A01 because these are the only GPIOs that can output DAC.
  // STM32_ADC_SQR3 Holds channel sequence 1 - 6. So, its the only one that
  // needs this channel information.
  regval = getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET);
  regval &= ~ADC_SQR3_RESERVED;   // Clear all SQR Bits
  
  // Initial attempt
  // regval |= (3  << ADC_SQR3_SQ1_SHIFT);   // Channel 3  - A02 [PA3]->ADC123_IN3
  // regval |= (8  << ADC_SQR3_SQ2_SHIFT);   // Channel 8  - A03 [PB0]->ADC12_IN8
  // regval |= (9  << ADC_SQR3_SQ3_SHIFT);   // Channel 9  - A04 [PB1]->ADC12_IN9
  // regval |= (10 << ADC_SQR3_SQ4_SHIFT);   // Channel 10 - A05 [PC0]->ADC123_IN10

  // FOR TESTING - Each slot is the next ADC INx
  // SQR3 has first 6 
  regval |= (1  << ADC_SQR3_SQ1_SHIFT);   // Channel 1
  regval |= (2  << ADC_SQR3_SQ2_SHIFT);   // Channel 2
  regval |= (3  << ADC_SQR3_SQ3_SHIFT);   // Channel 3
  regval |= (4  << ADC_SQR3_SQ4_SHIFT);   // Channel 4
  regval |= (5  << ADC_SQR3_SQ5_SHIFT);   // Channel 5
  regval |= (6  << ADC_SQR3_SQ6_SHIFT);   // Channel 6
  putreg32(regval, baseADCAddr + STM32_ADC_SQR3_OFFSET);

  // SQR2 has first 7 - 12
  regval = getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET);
  regval &= ~ADC_SQR2_RESERVED;            // Clear all SQR Bits
  regval |= (7  << ADC_SQR2_SQ7_SHIFT);    // Channel 7
  regval |= (8  << ADC_SQR2_SQ8_SHIFT);    // Channel 8
  regval |= (9  << ADC_SQR2_SQ9_SHIFT);    // Channel 9
  regval |= (10  << ADC_SQR2_SQ10_SHIFT);  // Channel 10
  regval |= (11  << ADC_SQR2_SQ11_SHIFT);  // Channel 11
  regval |= (12  << ADC_SQR2_SQ12_SHIFT);  // Channel 12
  putreg32(regval, baseADCAddr + STM32_ADC_SQR2_OFFSET);

  // And SQR1 has last 13 - 16 and the length (total conversions in sequence)
  regval = getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET);
  regval &= ~ADC_SQR1_RESERVED;            // Clear all SQR Bits
  regval |= (13 << ADC_SQR1_SQ13_SHIFT);   // Channel 13
  regval |= (14 << ADC_SQR1_SQ14_SHIFT);   // Channel 14
  regval |= (15 << ADC_SQR1_SQ15_SHIFT);   // Channel 15
  regval |= (16 << ADC_SQR1_SQ16_SHIFT);   // Channel 16

  regval |= (15 << ADC_SQR1_L_SHIFT);      // A 15 will convert 16 inputs
  // regval |= (3 << ADC_SQR1_L_SHIFT);  // A 3 will convert 4 inputs
  putreg32(regval, baseADCAddr + STM32_ADC_SQR1_OFFSET);

#if ADC_TESTS_USE_DMA_TRANSFER > 0
  //------------------------------------------------------------
  // (--) PLAN B - Stop using Nuttx DMA foundation
  // ADC1 DMA can be found here:DMA2, DMA_STREAM0, DMA_CHAN0
  regval = getreg32(STM32_DMA2_S0CR);
  regval &= ~DMA_SCR_EN;      // Clear the enable bit
  putreg32(regval, STM32_DMA2_S0CR);

  // Per Ref Man 8.3.18 must wait till 0
  while ((getreg32(STM32_DMA2_S0CR) & DMA_SCR_EN) != 0);

  // DMA2 Channel 0 Peripheral Register
  regval = getreg32(STM32_DMA2_S0PAR);
  regval = baseADCAddr + STM32_ADC_DR_OFFSET;
  putreg32(regval, STM32_DMA2_S0PAR);

  // DMA2 Channel 0 Memory Address Register
  regval = getreg32(STM32_DMA2_S0M0AR);
  regval = _dmaDataBuffer;
  putreg32(regval, STM32_DMA2_S0M0AR);

  // Offset to DMA2 Stream 0 Control Register
  regval = getreg32(STM32_DMA2_S0CR);
  // Select the Channel 0 by clearing (0000)
  regval &= ~DMA_SCR_CHSEL_MASK;
  // Set the memory size (MSIZE) is 16-bits 01
  regval &= ~DMA_SCR_MSIZE_MASK;    // Clear both bits
  regval |= DMA_SCR_MSIZE_16BITS;   // Set size
  // Set the peripheral size (PSIZE) is 16-bits 01 
  regval &= ~DMA_SCR_PSIZE_MASK;    // Clear both bits
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

  // Configure the DMA
  // DMA2 Channel 0 Number of Data Register
  regval = getreg32(STM32_DMA2_S0NDTR);
  regval &= ~0x0000ffff;      // Clear 15:0
  regval |= ADC_TESTS_DMA_DATA_BUFFER_SIZE;
  putreg32(regval, STM32_DMA2_S0NDTR);

  // Enable DMA Stream. Waited till finished because if this DMA stream is
  // enabled register values are read-only
  regval = getreg32(STM32_DMA2_S0CR);
  regval |= DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);
  
  // Per Ref Man 8.3.18 must wait
  while ((getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET) & ADC_CR2_ADON) != 0);

//   //------------------------------------------------------------
//   // Using Nuttx DMA module to handle DMA setup
//   if(_dmaHandle != NULL)
//   {
//     // Needed only if previous dma being modified.
//     stm32_dmastop(_dmaHandle);
//     stm32_dmafree(_dmaHandle); 
//   }

//   // DMAMAP_ADC1_1 got DMA2, DMA_STREAM0, DMA_CHAN0
// // STM32_DMA1_CHAN1, DMAMAP_ADC1_1, ADC1_DMA_CHAN, DMAMAP_ADC1_1
//   _dmaHandle = stm32_dmachannel(DMAMAP_ADC1_1);

//   // Configure the SCR (Stream Control Register) values
//   regval =  DMA_SCR_MSIZE_16BITS;   // Size of memory transfer
//   regval |= DMA_SCR_PSIZE_16BITS;   // Size of peripheral transfer
//   // Memory increment mode. 0=mem addr is fixed, 1=mem addr increments
//   regval |= DMA_SCR_MINC;           // Increment
//   regval |= DMA_SCR_CIRC;           // Circular mode 1=enabled
//   regval |= DMA_SCR_DIR_P2M;        // Direction 0=Perph->Mem

//   // SxNDTR is set by Nuttx
//   stm32_dmasetup(_dmaHandle,
//                  baseADCAddr + STM32_ADC_DR_OFFSET, // Peripheral addr
//                  (uint32_t)_dmaDataBuffer,          // Mem addr
//                  ADC_TESTS_DMA_DATA_BUFFER_SIZE,    // number of transfers
//                  regval);

//   // Provide DMA callback
//   // void *arg will be returned via callback to ISR
//   // true/false for half buffer callback as well as full buffer.
//   stm32_dmastart(_dmaHandle, adc_dma_interrupt_handler_isr,
//             (void *)baseADCAddr, true);
#endif    // #if ADC_TESTS_USE_DMA_TRANSFER > 0

  //------------------------------------------------------------
  // Return to ADC CR2
// (--) Make the following code snipets into 2 unique functions
// NEW FUNCTION - ENABLE ADC
  // Set ADON to turn on this ADC
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_ADON;
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // Wait a bit
  usleep(20);

// START FUNCTION - START ADC
  // Clear the status register's fields
  regval  = getreg32(baseADCAddr + STM32_ADC_SR_OFFSET);
  regval = 0;
  putreg32(regval, baseADCAddr + STM32_ADC_SR_OFFSET);

  // Start conversion
  regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_SWSTART;
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);
// START FUNCTION - START ADC

  adc_test_display_basic_adc_regs(baseADCAddr); usleep(50 * 1000);

  // Restore the IRQ state
  leave_critical_section(flags);

  return OK;
}

//===================================================================
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
  // Check the ADC data register periodically
  // Assume ADC1
  // uint32_t baseADCAddr = STM32_ADC1_BASE;

  for(int chkCnt = 0; chkCnt < 1000000; chkCnt++)
  {
    // Show the DMA  data in the buffer
    // syslog(1, "%04d-ADC: 00   01   02   03     04   05   06   07     08   09   10   11     12   13   14   15\n");
    syslog(1, "%04d-ADC:%04u %04u %04u %04u | %04u %04u %04u %04u | %04u %04u %04u %04u | %04u %04u %04u %04u\n",
              chkCnt,
              _dmaDataBuffer[0], _dmaDataBuffer[1], _dmaDataBuffer[2], _dmaDataBuffer[3],
              _dmaDataBuffer[4], _dmaDataBuffer[5], _dmaDataBuffer[6], _dmaDataBuffer[7],
              _dmaDataBuffer[8], _dmaDataBuffer[9], _dmaDataBuffer[10], _dmaDataBuffer[11],
              _dmaDataBuffer[12], _dmaDataBuffer[13], _dmaDataBuffer[14], _dmaDataBuffer[15]);

    // syslog(1, "%04d-ADC beginning values:%04u %04u %04u %04u | %04u %04u %04u %04u | %04u %04u %04u %04u | Last:%04u %04u %04u %04u\n",
    //           chkCnt,
    //           // Very beginning
    //           _dmaDataBuffer[0], _dmaDataBuffer[1], _dmaDataBuffer[2], _dmaDataBuffer[3],
    //           _dmaDataBuffer[4], _dmaDataBuffer[5], _dmaDataBuffer[6], _dmaDataBuffer[7],
    //           _dmaDataBuffer[8], _dmaDataBuffer[9], _dmaDataBuffer[10], _dmaDataBuffer[11],
    //           _dmaDataBuffer[12], _dmaDataBuffer[13], _dmaDataBuffer[14], _dmaDataBuffer[15]);
              // Much later
              // _dmaDataBuffer[512], _dmaDataBuffer[513], _dmaDataBuffer[514], _dmaDataBuffer[515],
              // _dmaDataBuffer[516], _dmaDataBuffer[517], _dmaDataBuffer[518], _dmaDataBuffer[519],
              // _dmaDataBuffer[520], _dmaDataBuffer[521], _dmaDataBuffer[522], _dmaDataBuffer[523],
              // _dmaDataBuffer[524], _dmaDataBuffer[525], _dmaDataBuffer[526], _dmaDataBuffer[527]);

    // For converting 4 values
    // syslog(1, "%04d-ADC values:%04u %04u %04u %04u\n", chkCnt,
    //           _dmaDataBuffer[0], _dmaDataBuffer[1],
    //           _dmaDataBuffer[2], _dmaDataBuffer[3]);
    
    usleep(1000 * 1000);
  }
  return NULL;
}

#endif  // #if defined (CONFIG_ADC_TESTS)
