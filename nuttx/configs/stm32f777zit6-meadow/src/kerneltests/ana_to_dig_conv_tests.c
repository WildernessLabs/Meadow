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
// nuttx/arch/arm/src/common/up_arch.h
#include <nuttx/arch.h>   // up_enable_irq
#include "up_arch.h"      // getreg32 & putreg32
#include <arch/stm32f7/chip.h>
#include "stm32_gpio.h"
#include <meadow/hcom_shared_common.h>

// #include "chip.h"
// #include "stm32_rcc.h"
// #include "stm32_tim.h"
// #include "stm32_dma.h"
// #include "stm32_adc.h"

// FOR TESTING BBR
#include "chip/stm32_rtcc.h"

// #if defined (CONFIG_DAC_TESTS)
#if defined (CONFIG_ADC_TESTS)
#warning "(--) Hacking adc_dac_tests.c"

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#include "chip/stm32f74xx77xx_adc.h"
#include "chip/stm32f76xx77xx_rcc.h"
#include "chip/stm32f76xx77xx_memorymap.h"

#define ADC_ALL_POSSIBLE_INTERRUPT_TYPES (ADC_SR_OVR | ADC_SR_STRT | \
          ADC_SR_JSTRT | ADC_SR_JEOC | ADC_SR_EOC | ADC_SR_AWD)

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/
// For reference
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
static int adc_dac_tests_config_adc(int adc_numb, uint32_t baseADCAddr);
static void adc_dac_test_initialize_adc(int adcNumb);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
void meadow_kt_dac_tests(uint32_t userData)
{

}
// ADC and DAC tests
void meadow_kt_adc_tests(uint32_t userData)
{
  static int onlyOnce = false;

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
      if(onlyOnce)
      {
        syslog(1, "Only once\n");
      }
      else
      {
        onlyOnce = true;
        // Initialize only ADC1 to start with
        adc_dac_test_initialize_adc(1);
      }
      break;

    case 2:
      break;

    default:
      syslog(1, "Undefined test for meadow_kt_adc_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
static int adc_test_isr(int irq, FAR void *context, FAR void *arg)
{
  uint32_t statusReg;
  uint32_t baseADCAddr = (uint32_t)arg;
  uint32_t pendingInterrupts;
  uint32_t regval;

  static int isrCount = 0;
  DEBUG_SET_HIGH(DEBUG_PIN_V2_D04);

  statusReg = getreg32(baseADCAddr + STM32_ADC_SR_OFFSET);
  pendingInterrupts = statusReg & ADC_ALL_POSSIBLE_INTERRUPT_TYPES;
  if(pendingInterrupts == 0)
    return OK;

  if ((pendingInterrupts & ADC_SR_AWD) != 0)
  {
    syslog(1, "Analog WD\n");
  }

  if ((pendingInterrupts & ADC_SR_OVR) != 0)
  {
    syslog(1, "Over Run\n");

    // Per Ref Man 15.8.2 this may need to be here
    regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
    regval |= ADC_CR2_SWSTART;
    putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);
  }

  // End of conversion - got a value?
  if ((pendingInterrupts & ADC_SR_EOC) != 0)
  {
    // ADC output is 12-bit
    // uint32_t adcValue = (uint16_t)(getreg32(baseADCAddr + STM32_ADC_DR_OFFSET) & 0x00000fff);
    uint32_t adcValue = getreg32(baseADCAddr + STM32_ADC_DR_OFFSET);
    // Just for testing
    // running at about 15/msec is too fast for processing
    if((isrCount % 10) == 0)
    {
      isrCount++;
      syslog(1, "%d-ADC ISR value:%lu\n", isrCount, adcValue);
    }

    // Per Ref Man 15.8.2 this may need to be here
    // regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
    // regval |= ADC_CR2_SWSTART;
    // putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);
  }

  // THIS SHOULD NOT WORK-and it doesn't
  // regval  = getreg32(baseADCAddr + STM32_ADC_SR_OFFSET);
  // regval |= ADC_SR_STRT;
  // putreg32(regval, baseADCAddr + STM32_ADC_SR_OFFSET);

  // Clear all interrupts
  statusReg &= ~ADC_ALL_POSSIBLE_INTERRUPT_TYPES;
  putreg32(statusReg, baseADCAddr + STM32_ADC_SR_OFFSET);

  // Don't leave ISR until the above have fully finished
  // asm volatile ("dsb");

  DEBUG_SET_LOW(DEBUG_PIN_V2_D04);
  return OK;
}

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

//================================================================
// Entry point
  // (--) This part of the code could be called > 1 time when it supports more
  // than ADC1 for debugging. However the ADC reset done via RCC will only
  // need to be done once.
void adc_dac_test_initialize_adc(int adc_numb)
{
  int ret;
  static bool firstTime = true;
  irqstate_t flags;
  uint32_t regval;
  uint32_t baseADCAddr;
  uint32_t adcRccClkEnable;
  
  syslog(1, "-->Entered adc_dac_test_initialize_adc() ADC is:%d (1-3 valid)\n", adc_numb); usleep(20 * 1000);

  // Initialize a GPIO for analog use
  // #define GPIO_ADC1_IN4         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN4)
  // #define GPIO_ADC1_IN5         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN5)

  ret = stm32_configgpio(GPIO_ADC1_IN4);
  if(ret < 0)
  {
    syslog(1, "Error#1 in adc_dac_test_initialize_dac_1.\n ret:%d errno:%d\n", ret, errno);
  }

  // Find the base address for the ADC being configured
  switch (adc_numb)
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
    syslog(LOG_ERR, "%s@%d-The adc_numb:%d is invalid\n", __FILE__, __LINE__, adc_numb);
    return;
  }

  // Some RCC registers need to be accessed
  flags = enter_critical_section();

  if(firstTime)
  {
    firstTime = false;

    // Reset all the ADCs via Reset and Clock Control (RCC). For the STM32F7
    // there is a single bit for all ADCs. Other MCUs have a bit for each ADC.
    regval = getreg32(STM32_RCC_APB2RSTR);
    regval |= RCC_APB2RSTR_ADCRST;
    putreg32(regval, STM32_RCC_APB2RSTR);

    // Restore ADC from reset state
    regval &= ~RCC_APB2RSTR_ADCRST;
    putreg32(regval, STM32_RCC_APB2RSTR);
  }

  // Insure the correct ADC clock is on. If not enabled then it is on possible
  // to write values into some ADC configuration registers.
  regval = getreg32(STM32_RCC_APB2ENR);
  regval |= adcRccClkEnable;
  putreg32(regval, STM32_RCC_APB2ENR);

  leave_critical_section(flags);

  // Setup the Interrupt handler
  ret = irq_attach(STM32_IRQ_ADC, adc_test_isr, (void *)baseADCAddr);
  if(ret < 0)
  {
    syslog(1, "Error calling irq_attach\n");
  }

  // Configure ADC and start it converting
  syslog(1, "-->Configuring ADC\n"); usleep(20 * 1000);
  ret = adc_dac_tests_config_adc(adc_numb, baseADCAddr);
  if(ret < 0)
  {
    syslog(1, "Error calling adc_dac_tests_config_adc\n");
  }

  // Enable interrupt generation
  syslog(1, "-->Enabling IRQ\n"); usleep(20 * 1000);
  up_enable_irq(STM32_IRQ_ADC);

  // Start conversion on regular channels. This bit only stays 1 until the
  // conversion begins.
  regval  = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_SWSTART;
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);
}

//==================================================================
// This code configures one ADC
static int adc_dac_tests_config_adc(int adc_numb, uint32_t baseADCAddr)
{
  irqstate_t flags;
  uint32_t regval;

  // ANY REGISTER VALUES INITIALLY?
  adc_test_display_basic_adc_regs(baseADCAddr);

  //------------------------------------------------------------------
  flags = enter_critical_section();

  // Insure the ADC is off while setting up
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_ADON;      // 0=A/D Converter off (turned on later)
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // Set the ADC watchdog high and low threshold of the ADC to max and min
  putreg32(0x00000fff, baseADCAddr + STM32_ADC_HTR_OFFSET);
  putreg32(0x00000000, baseADCAddr + STM32_ADC_LTR_OFFSET);

   // [TEST] - Will experiment with this value to better understand its effect.
  // There are a total of 18 channels. We'll only set channel 0 to maximum
  // and it to the maximum sample time of 480 as a test.
 // Sample timer registers
  // How many clock cycles should each conversion last? See Ref Man sec 15.5.
  // 000: 3 cycles
  // 001: 15 cycles
  // 010: 28 cycles
  // 011: 56 cycles
  // 100: 84 cycles
  // 101: 112 cycles
  // 110: 144 cycles
  // 111: 480 cycles
  // Initially we'll use 000 with continous conversion + DMA to move data to
  // a buffer for processing.
  // SMPR1 is for channels 10-18 which will be zero
  regval = getreg32(baseADCAddr + STM32_ADC_SMPR1_OFFSET);
  regval &= ~0xf8000000;    // Clear fields 26:0
  putreg32(regval, baseADCAddr + STM32_ADC_SMPR1_OFFSET);

  regval = getreg32(baseADCAddr + STM32_ADC_SMPR2_OFFSET);
  regval &= ~0x0c000000;      // Clear fields 28:0
  // Set channel 1 to 480 for testing
  regval |= (ADC_SMPR_480 << ADC_SMPR2_SMP0_SHIFT);
  putreg32(regval, baseADCAddr + STM32_ADC_SMPR2_OFFSET);

  //---------------------------------------------------
  // Get the ADC Control Register 1 register. This register controls a lot of
  // options. I put the following in the same order as the Ref Man 15.13.2
  // This is mostly interrupt configuration
  regval = getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET);
  regval |= ADC_CR1_OVRIE;       // 1=Enable Overrun interrupt
  regval &= ~ADC_CR1_RES_MASK;    // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;    // Set resolution 12, 10, 8 or 6 bits
  regval &= ~ADC_CR1_AWDEN;       // 0=Disable Analog watchdog on regular channels
  regval &= ~ADC_CR1_JAWDEN;      // 0=Disable Analog watchdog on injected
  regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  regval &= ~ADC_CR1_JDISCEN;     // 0=Disable discontinuous mode on injected channels
  regval &= ~ADC_CR1_DISCEN;      // 0=Disable discontinuous mode on regular channels
  regval &= ~ADC_CR1_JAUTO;       // 0=Automatic Injected Group conversion
  regval &= ~ADC_CR1_AWDSGL;      // 0=Disable watchdog on single channel in scan mode 
  regval |= ADC_CR1_SCAN;         // 1=Scan mode (Needed for DMA)
  regval &= ~ADC_CR1_JEOCIE;      // 0=Disable interrupt for injected channels
  regval &= ~ADC_CR1_AWDIE;       // 0=Analog Watchdog interrupt enable
  regval |= ADC_CR1_EOCIE;       // 1=Enable interrupt for EOC
  // regval &= ~ADC_CR1_EOCIE;       // 0=Disable interrupt for EOC
  // Not used by set as if using IN4 (PA4)
  regval |= (4 << ADC_CR1_AWDCH_SHIFT);  // 00100=Channel 4 analog watchdog select bits
  putreg32(regval, baseADCAddr + STM32_ADC_CR1_OFFSET);

  //---------------------------------------------------
  // ADC CR2 Configuration
  // Note:fields not defined in header file have been ignored
  // ADC_CR2_ADON not here as already cleared for A/D off
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_EXTEN_MASK;  // 0000=Tim1Ch1 External trigger
  regval |= ADC_CR2_EXTEN_NONE;   // Set to none channels
  // Several fields not defined left 0
  regval &= ~ADC_CR2_ALIGN;       // 0=Right alignment (1=left)
  
  // EOCS End Of Conversion Selection. When should the EOC bit be set?
  // EOCS = 0 End of each sequence,
  // regval &= ~ADC_CR2_EOCS;         // 1=End of each conversion, 0=End of selection
  regval |= ADC_CR2_EOCS;         // 1=End of each conversion, 0=End of selection

  // When DDS is set to 0 DMA transfer is stopped. Setting to 1 restarts DMA
  regval &= ~ADC_CR2_DDS;         // 0=No DMA issued as long as data converted
  regval &= ~ADC_CR2_DMA;         // 0=Disable Direct Memory Access mode
  // regval |= ADC_CR2_DDS;         // 1=DMA issued as long as data converted
  // regval |= ADC_CR2_DMA;         // 1=Enable Direct Memory Access mode
  regval |= ADC_CR2_CONT;         // 1=Enable continuous mode
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  //---------------------------------------------------
  // ADC_SQR1, ADC_SQR2 and ADC_SQR3 are used to configure the sequence of the
  // conversions. If there are no entries then nothing will be converted. So,
  // for a single conversion we need to set ADC_SQR3's ls field (4:0).
  //
  // The values put in this field are found by looking at the target hardware.
  // For the FeatherV2, A00 is PA4. Next look at the data sheet
  // 'Table 11. STM32F777xx, STM32F778Ax and STM32F779xx pin and ball
  // definitions.' In the 'Pin name' column find PA4. Then look in the
  // 'Additional functions' column for the possible analog inputs for the ADC
  // being used. In this case there are 2 possible ADC1_IN4, and ADC2_IN4.
  // Since we're using ADC1 the value is '4'.
  //
  // Length is the number in the sequence which is 1 ( in register is 0).
  regval = getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET) & ADC_SQR3_RESERVED;
  // [TEST] HARDCODE ADC_IN4 (A00 - PA4) AS ONLY CHANNEL TO CONVERT
  regval |= 4 << ADC_SQR3_SQ1_SHIFT;
  putreg32(regval, baseADCAddr + STM32_ADC_SQR3_OFFSET);
  
  regval = getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET) & ADC_SQR2_RESERVED;
  putreg32(regval, baseADCAddr + STM32_ADC_SQR2_OFFSET);

  // And set the length which is the number of conversions in the sequence
  regval = getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET) & ADC_SQR1_RESERVED;
  // [TEST] HARDCODED LENGTH AS 1 conversion FOR DEMO
  // For 1 conversion the length value is 0000 into bits 23:20.
  regval |= 0 << ADC_SQR1_L_SHIFT;   // This will convert 1 input
  putreg32(regval, baseADCAddr + STM32_ADC_SQR1_OFFSET);

  //---------------------------------------------------
  // ADC Common Control Register
  regval = getreg32(STM32_ADC_CCR);
  // Clear all fields
  regval &= ~(ADC_CCR_MULTI_MASK | ADC_CCR_DELAY_MASK | ADC_CCR_DDS | ADC_CCR_DMA_MASK |
              ADC_CCR_ADCPRE_MASK | ADC_CCR_VBATE | ADC_CCR_TSVREFE);

  // ADCPRE - Calculation based on PCLK2=96MHz (with 192MHz clock). Per Data
  // Sheet 5.3.24 pp 165, max ADC clock is 36MHz. Therefore, must divide by 4
  // (96/4=24MHz). Too bad can't divide by 3, only 2, 4, 6, 8
  regval |=  (ADC_CCR_MULTI_NONE | ADC_CCR_DMA_DISABLED | ADC_CCR_ADCPRE_DIV4);
  // // regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor channel
  // // regval &= ~ADC_CCR_VBATE;       // 0=disable vbat channel
  // // regval &= ~ADC_CCR_ADCPRE_MASK;   // Clear any bits
  // regval |= ADC_CCR_ADCPRE_DIV4;    // 01=ADC prescaler PCLK2 divided by 4

  // // [DMA] Future
  // regval |= ADC_CCR_DMA_DISABLED;   // 00 = DMA Mode disabled
  // // regval &= ~ADC_CCR_DDS;           // 0=No new DMA requests
  // // regval &= ~ADC_CCR_DELAY_MASK;    // 0000=5*Tadcclk (only used for dual/triple)
  // regval |= ADC_CCR_MULTI_NONE;     // 00000=Independent mode
  putreg32(regval, STM32_ADC_CCR);

  // Set ADON to turn on this ADC
  regval = getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_ADON;
  putreg32(regval, baseADCAddr + STM32_ADC_CR2_OFFSET);

  // Restore the IRQ state
  leave_critical_section(flags);

  adc_test_display_basic_adc_regs(baseADCAddr);

  return OK;
}

#endif  // #if defined (CONFIG_ADC_TESTS)
