/****************************************************************************
 * configs/stm32f777zit6-meadow/src/meadow_adc.c
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

// Note: this code uses DMA to transfer data from the ADC's data store to a
// preallocated buffer

// (--)
// 5Oct23 - Known issues when returning to adc coding
// 1. Added but not fully tested. _adcVrefInCal is populated before any other calls.
// 2. Voltage reading are 0.5 volts too high. Use meadow set developer -d 14 -v 1
//    and meadow set developer -d 14 -v 2
// 3. All calculations should use double e.g. meadow_adc_read_temp_vbat
// 4. Look for "syslog(1," and (--) in code and do general cleanup
// 5. Re-test all Analog inputs, Battery voltage, Internal MCU temperature etc.
// 6. Need to add code to disable (dispose) ADC configuration. Then test this
//    code by re-configuring and testing again

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
#include "chip/stm32f76xx77xx_dma.h"

#ifndef CONFIG_STM32F7_DMA2
#error "Meadow ADC requires CONFIG_STM32F7_DMA2 to be configured"
#endif

// Diagnostic always as this is test code
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#if defined CONFIG_ADC_TESTS
#warning "(--) Hacking meadow_adc.c"
#endif

// #pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
#define ADC_TEST_PIN_CCM_D03_PB8   (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN8)
#define ADC_TEST_PIN_CCM_D04_PB9   (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)

#if defined CONFIG_ADC_TESTS
// Of these PA4 and PA5 are available for DAC
#define GPIO_V2_A00_IN4_PA4         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN4)
#define GPIO_V2_A01_IN5_PA5         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN5)
#define GPIO_V2_A02_IN3_PA3         (GPIO_ANALOG|GPIO_PORTA|GPIO_PIN3)
#define GPIO_V2_A03_IN8_PB0         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN0)
#define GPIO_V2_A04_IN9_PB1         (GPIO_ANALOG|GPIO_PORTB|GPIO_PIN1)
#define GPIO_V2_A05_IN10_PC0        (GPIO_ANALOG|GPIO_PORTC|GPIO_PIN0)
#endif

#define ADC_ALL_POSSIBLE_ADC_INTERRUPTS (ADC_SR_OVR | ADC_SR_STRT | \
          ADC_SR_JSTRT | ADC_SR_JEOC | ADC_SR_EOC | ADC_SR_AWD)

// #define ADC_SMPR_DEFAULT    ADC_SMPR_3       // 4 usec for 6 analogs (265kHz)
#define ADC_SMPR_DEFAULT    ADC_SMPR_112     // 32 usec for 6 analogs (32kHz)
// #define ADC_SMPR_DEFAULT    ADC_SMPR_480     // 124 usec for 6 analogs
#define ADC_SMPR1_DEFAULT     ((ADC_SMPR_DEFAULT << ADC_SMPR1_SMP10_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP11_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP12_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP13_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP14_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP15_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP16_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP17_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR1_SMP18_SHIFT))
#define ADC_SMPR2_DEFAULT     ((ADC_SMPR_DEFAULT << ADC_SMPR2_SMP0_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP1_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP2_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP3_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP4_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP5_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP6_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP7_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP8_SHIFT) | \
                               (ADC_SMPR_DEFAULT << ADC_SMPR2_SMP9_SHIFT))

// The sequence Registers hold different numbers of inputs
#define MEADOW_ADC_SEQ_3_REGISTER_TOTAL (6)
#define MEADOW_ADC_SEQ_2_REGISTER_TOTAL (6)
#define MEADOW_ADC_SEQ_1_REGISTER_TOTAL (4)

// From Data Sheet-Internal reference addresses of parameter TS_CAL1, TS_CAL2
// (Table 79) and VREFINT_CAL (Table 81).
#define MEADOW_ADC_TEMPSENSOR_CAL30_ADDR ((const uint16_t*) 0x1FF0F44C)
#define MEADOW_ADC_TEMPSENSOR_CAL110_ADDR ((const uint16_t*) 0x1FF0F44E)
#define MEADOW_ADC_VREFINT_CAL_ADDR ((const uint16_t*) 0x1FF0F44A)

// From the data sheet table 82 the calibration value was read at 3.3v. We'll
// multiply by 10 to keep everything in integers
#define MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN (3.30)

// With a 12-bit ADC this is the maximum count that can be read
#define MEADOW_ADC_MAX_ADC_COUNT (4095)

#if defined CONFIG_ADC_TESTS
#define MEADOW_ADC_TEST_TOGGLE_ADC_INPUT (0)
#endif 

//-----------------------------------------------------------------------
#if defined CONFIG_ADC_TESTS
// Only for TESTING
static void adc_test_display_basic_adc_regs(uint32_t baseADCAddr)
{
  syslog(1, "SR:  0x%08x CR1:  0x%08x CR2:  0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SR_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET));

  syslog(1, "SQR1: 0x%08x SQR2: 0x%08x SQR3: 0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET));

  syslog(1, "CCR:  0x%08x\n", getreg32(STM32_ADC_CCR));
}

//-----------------------------------------------------------------------
static void adc_test_display_basic_dma_regs(void)
{
  syslog(1, "S0CR:  0x%08x  S0NDTR: 0x%08x\n",
        getreg32(STM32_DMA2_S0CR),
        getreg32(STM32_DMA2_S0NDTR));

  syslog(1, "S0PAR: 0x%08x  S0M0AR: 0x%08x S0M1AR: 0x%08x\n",
        getreg32(STM32_DMA2_S0PAR),
        getreg32(STM32_DMA2_S0M0AR),
        getreg32(STM32_DMA2_S0M1AR));
}
#endif

// /************************************************************************************
//  * Private Data
//  ************************************************************************************/
// From data sheet - GPIO to ADC1 channel input map
// Entries represent the STM32F7's valid GPIOs the can be used for ADC1 or
// ADC2. ADC3 has a few others that we'll ignore them for now.
// The following values are matched to those supplied by the caller, but here
// the position (0-16) defines the ADC multiplex position of the switch (see
// Ref Man Figure 71). Other values are not used.
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

uint16_t *_dmaDataBuf;
uint16_t *_userDataBuf;

DMA_HANDLE _dmaHandle;
static uint32_t _adcBufSzBytes;
static uint32_t _gpioTransferCount;
static uint8_t *_gpioList;
static bool _meadowAdcInit = false;
static sem_t _waitTillDoneSem;

static uint16_t _adcVrefInCal;

#if defined CONFIG_ADC_TESTS
static int _noChangeCnt;
#endif

// /************************************************************************************
//  * Private Function Prototypes
//  ************************************************************************************/

static int get_in_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan);
static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegMaxGpios,
          uint32_t initRegShift);
static int meadow_adc_initialize_dma(void);

static int meadow_adc_read_internal_temp_vref(uint16_t *adcTempReading, uint16_t *adcVrefInCal);
static int meadow_adc_read_internal_vbat(uint16_t *adcBatteryReading);
static int meadow_adc_injected_adc_cleanup(void);
static int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, uint32_t *convertedVoltage);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// DMA ISR
static void adc_dma_interrupt_handler_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;
  // static int execCnt = 0;

  // Without this call the buffer's data is usually not correct
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

#if defined CONFIG_ADC_TESTS
  if(status == 0)
  {
    syslog(1, "DMA ISR No Interrupts\n");
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
    // syslog(1, "DMA ISR reason:Half Transfer\n");
  }
#endif

  //------------------------------------------------------
  // Stream Transfer Complete flag
  if((status & DMA_STREAM_TCIF_BIT) != 0)
  {
#if defined CONFIG_ADC_TESTS
    syslog(1, "DMA ISR reason:Transfer Complete\n");
#endif

    // The following test code toggles a GPIO output to cause an ADC analog
    // input's value to also toggle above and below mid-point of Vdd.
#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
    static bool _previousA00High = false;

    // The following code is for testing using the following connections:
    // Connect one end of a 2k2 resistor to ADC_TEST_PIN_CCM_D03_PB8 and the other
    // end connected to two 4k7 resistors. Then connect the free end of one 4k7
    // resistor's to ground and connect the another 4k7 resistor's free end to
    // 3v3, the voltage at the junction, of the 3 resistors, will toggle about
    // 1 volt above and 1 volt below mid-way between ground and 3v3 when
    // ADC_TEST_PIN_CCM_D03_PB8 changes from high to low. With this junction
    // connected to an analog input pin it is easy to prove that all of the
    // ADC values are changing as desired.
    //
    // Look at the ADC value for GPIO_V2_A00_IN4_PA4. It should
    // change on every DMA transfer complete interrupt.
    if(_dmaDataBuf[0] > 2048)
    {
      // Voltage now above the mid point "high"
      if(_previousA00High)
        _noChangeCnt++;             // Still high, error
      else
        _previousA00High = true;    // Was Low, now high

      // syslog(1, "DMA:Transfer complete - setting GPIO LOW\n");
      stm32_gpiowrite(ADC_TEST_PIN_CCM_D03_PB8, false);
    }
    else
    {
      // Voltage now below the mid-point "low"
      if(! _previousA00High)
        _noChangeCnt++;             // Still low, error
      else
         _previousA00High = false;   // Was high, now low

      // syslog(1, "DMA:Transfer complete - setting GPIO HIGH\n");
      stm32_gpiowrite(ADC_TEST_PIN_CCM_D03_PB8, true);
   }
#endif

    // DMA2's SxCR register's will need to be re-enabled after each
    // conversion. This is because it's cleared whenever an a DMA transfer
    // has been completed.
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
    regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
    regval &= ~ADC_CR2_DMA;
    putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    regval |= ADC_CR2_DMA;
    putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

    // Wakeup callers thread
    sem_post(&_waitTillDoneSem);
  }
}

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

#if defined CONFIG_ADC_TESTS
  if ((pendingInterrupts & ADC_SR_AWD) != 0)
  {
    syslog(1, "-- ADC ISR-WatchDog --\n");
  }

  if ((pendingInterrupts & ADC_SR_OVR) != 0)
  {
    syslog(1, "-- ADC ISR-Over Run --\n");

  // Note: Meadow_ADC is ignoring data Overruns
  //
  // From Ref Man 15.8.1 & 15.8.2
  // To recover the ADC from OVR when the DMA is used, follow the steps below:
  // 1. Reinitialize the DMA (adjust destination address and NDTR counter).
  //    This is DMA
  // 2. Clear the ADC OVR bit in ADC_SR register (below)
  //    This is ADC
  // 3. Trigger the ADC to start the conversion (below)
  //    This is ADC
  //
  // Note: #1 above - there doesn't appear to be in NuttX stm32f7/stm32_dma.c
  // code a function to "Reinitialize the DMA".
  // See nuttx/arch/arm/src/stm32f7/stm32_dma.c @657-673 for code that would
  // do the above requirement (I think).
  }

  // End of conversion - got a value?
  if ((pendingInterrupts & ADC_SR_EOC) != 0)
  {
    syslog(1, "-- ADC ISR-End of Conversion --\n");
  }
#endif

  // Clear any interrupts
  pendingInterrupts &= ~ADC_ALL_POSSIBLE_ADC_INTERRUPTS;
  putreg32(pendingInterrupts, baseADCAddr + STM32_ADC_SR_OFFSET);

  return OK;
}

//==========================================================================
static void meadow_adc_buffer_takesem(sem_t *semaphore)
{
  int ret;
  do
  {
    ret = sem_wait(semaphore);
  }
  while (ret == -EINTR);
}

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
static int populate_adc_seq_channel(uint32_t *regval, uint32_t seqRegGpios,
          uint32_t initRegShift)
{
  int ret;
  uint32_t regCnt;
  uint32_t adcInputChan;

  // Cycle through all the slots in this register
  for(regCnt = 0; regCnt < seqRegGpios; regCnt++)
  {
    // For entry Pin/Port what 'ADC input' channel
    ret = get_in_chan_from_pinid(_gpioList[regCnt], &adcInputChan);
    if(ret < 0)
      return ret;    // Error

    // Each entry takes 5 bits
    *regval |= (adcInputChan << (initRegShift + (regCnt * 5)));
  }
  return OK;
}

//======================================================================
// Using injection for temp and vbat
int meadow_adc_read_internal_temp_vref(uint16_t *adcTempReading, uint16_t *adcVrefInCal)
{
  uint32_t regval;

  if(adcTempReading == NULL || adcVrefInCal == NULL)
  {
    return -EINVAL;   // Invalid argument
  }

  // We need adc_initialize to have been called.
  if(! _meadowAdcInit)
  {
    syslog(LOG_ERR, "%s@%d-Error:Meadow ADC not initialized.\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // CCR setup to read Temperature & Vref
  regval = getreg32(STM32_ADC_CCR);
  regval |= ADC_CCR_TSVREFE;        // 1=enable temperature sensor and reference
  regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);
  
  // Injection Sequence Register - set ADC input at 18 since ADC_IN18 is used
  // by temperature and vbat depending on eht value of TSVREFE
  // Note: for 2 injected conversions the first is from JSQ3 and the second
  // from JSQ4 (see Ref Man 15.13.12)
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= 0x003fffff;   // Clear all JSQR Bits
  regval |= (18 << ADC_JSQR_JSQ4_SHIFT);    // ADC_IN18 - Vsense (temp or vbat)
  regval |= (17 << ADC_JSQR_JSQ3_SHIFT);    // ADC_IN17 - Vrefint
  // And set the length to 1 to indicate 2 analog inputs
  regval |= (1 << ADC_JSQR_JL_SHIFT);
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

  // Start ADC via Control Register 2 (CR2) - Start the ADC
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_JSWSTART;       // Start injection ADC
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // (--) CAN THIS TIME BE SHORTENED???
  usleep(5 * 1000);    // Wait till next tick

  // Get the Vsense (temperature) and Vrefint
  *adcTempReading = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR2_OFFSET);
  *adcVrefInCal   = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR1_OFFSET);

  // Remove configuration 
  int ret = meadow_adc_injected_adc_cleanup();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
    return ret;
  }

  return ret;
}

//======================================================================
// This will read the raw ADC value for Vbat
int meadow_adc_read_internal_vbat(uint16_t *adcBatteryReading)
{
  uint32_t regval;

  // We need adc_initialize to have been called.
  if(! _meadowAdcInit)
  {
    syslog(LOG_ERR, "%s@%d-Error:Meadow ADC not initialized.\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // Injection Sequence Register - set ADC input at 18 since ADC_IN18 is used
  // by vbat
  // Setup to read Vbat
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor and ref
  regval |= ADC_CCR_VBATE;        // 1=enable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  // Note for a single conversion the data is from JSQ4 (see Ref Man 15.13.12)
  // ADC sampling time reading the temperature is 10usec (data sheet Table 78)
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= 0x003fffff;   // Clear all JSQR Bits
  regval |= (18 << ADC_JSQR_JSQ4_SHIFT);           // ADC_IN18 - Vbat
  // And set the length to 0 to indicate 1 input
  regval |= (0 << ADC_JSQR_JL_SHIFT);
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

  // Start ADC via Control Register 2 (CR2)
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_JSWSTART;       // Restart injection ADC
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // (--) CAN THIS TIME BE SHORTENED???
  usleep(5 * 1000);    // Wait till next tick

  *adcBatteryReading = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR1_OFFSET);

  // Remove configuration 
  int ret = meadow_adc_injected_adc_cleanup();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
    return ret;
  }

  return ret;
}

//======================================================================
//
int meadow_adc_injected_adc_cleanup(void)
{
  uint32_t regval;

  // Restore register values to neutral
  // Disable both Temperature and Vbat
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor channel
  regval &= ~ADC_CCR_VBATE;        // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= 0x003fffff;   // Clear all JSQR Bits
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

  return OK;
}

//======================================================================
// Assumes ADC 1
static int adc_initialize (void)
{
  uint32_t regval;

#if defined CONFIG_ADC_TESTS
  // This is used in testing it toggled high and low to switch the ADC input
  // voltage from high to low.
  syslog(1, "Configuring GPIO ADC_TEST_PIN_CCM_D03_PB8\n");

  stm32_configgpio(ADC_TEST_PIN_CCM_D03_PB8);
  stm32_gpiowrite(ADC_TEST_PIN_CCM_D03_PB8, false);
  // adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
#endif

  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
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
  // Set sample time for channels 10-18
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
  regval &= ~ADC_CR1_OVRIE;          // 0=Disable Overrun interrupt
  regval &= ~ADC_CR1_RES_MASK;      // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;      // Set resolution 00=12, 01=10, 10=8 or 11=6 bits
  regval &= ~ADC_CR1_AWDEN;         // 0=Disable Analog watchdog on regular channels
  regval &= ~ADC_CR1_JAWDEN;        // 0=Disable Analog watchdog on injected
  regval &= ~ADC_CR1_JDISCEN;       // 0=Disable discontinuous mode on injected channels
  regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  regval &= ~ADC_CR1_DISCEN;        // 0=Disable discontinuous mode on regular channels
  regval &= ~ADC_CR1_JAUTO;         // 0=Automatic Injected Group conversion
  regval &= ~ADC_CR1_AWDSGL;        // 0=Disable watchdog on single channel in scan mode
  // In Scan mode, the inputs selected through the ADC_SQRx
  regval |= ADC_CR1_SCAN;           // 1=Scan mode (Scans channels in ADC_SQRx registers)
  regval &= ~ADC_CR1_JEOCIE;        // 0=Disable interrupt for injected channels
  regval &= ~ADC_CR1_AWDIE;         // 0=Analog Watchdog interrupt enable
  // EOCIE seems to have no effect when using DMA
  regval |= ADC_CR1_EOCIE;          // 1=Enable ADC interrupt for EOC (not with DMA)
  regval &= ~ADC_CR1_AWDCH_MASK;    // Clear the watchdog channel to 00000=Chan 0
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
  // NOTE: the field names 'DDS' and 'DMA' exist in 2 ADC registers, CR2 and CCR
  // DDS=0 No new DMA request is issued after the last transfer
  // DDS=1 DMA requests are issued as long as data are converted and DMA=1
  regval &= ~ADC_CR2_DDS;
  regval |= ADC_CR2_DMA;          // 1=Enable DMA
  // Disable continous conversion
  regval &= ~ADC_CR2_CONT;         // 0=Disable continuous conversion
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  //------------------------------------------------------------
  // ADC_SQR1, ADC_SQR2 and ADC_SQR3 are used to configure the sequence of the
  // conversions when ADC_CR1_SCAN is used. If there are no entries then
  // nothing will be converted. Basically, which input in which order.
  //
  // The values put in this field are found by looking at the target hardware.
  // For the FeatherV2, A00 is PA4. Next look at the data sheet
  // 'Table 11. STM32F777xx, STM32F778Ax and STM32F779xx pin and ball
  // definitions.' In the 'Pin name' column find PA4. Then look in the
  // 'Additional functions' column for the possible analog inputs for the ADC
  // being used. In this case there are 2 possible ADC1_IN4, and ADC2_IN4.
  // Since for ADC1 the value is '4' (it's also 4 for ADC2 and 3).
  int ret;
  uint32_t regCount = 0;
  uint32_t remainingCnt = _gpioTransferCount;

  syslog(1, "SEQ_3-initial transfer count:%lu\n", remainingCnt);
  // Sequence Register 3 - ADC channels 1-6
  regval = getreg32(STM32_ADC1_SQR3);
  regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
  if(remainingCnt > 0)
  {
    if(remainingCnt >= MEADOW_ADC_SEQ_3_REGISTER_TOTAL)
    {
      regCount = MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
      remainingCnt -= MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
    }
    else
    {
      regCount = remainingCnt;
      remainingCnt = 0;
    }

    ret = populate_adc_seq_channel(&regval, regCount, ADC_SQR3_SQ1_SHIFT);
    if(ret < 0)
      return ret;    // Error
  }
  putreg32(regval, STM32_ADC1_SQR3);

  syslog(1, "SEQ_2-initial transfer count:%lu\n", regCount);
  // Sequence Register 2 - ADC channels 7-12
  regval = getreg32(STM32_ADC1_SQR2);
  regval &= ADC_SQR2_RESERVED;   // Clear all SQR Bits
  if(remainingCnt > 0)
  {
    if(remainingCnt >= MEADOW_ADC_SEQ_3_REGISTER_TOTAL)
    {
      regCount = MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
      remainingCnt -= MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
    }
    else
    {
      regCount = remainingCnt;
      remainingCnt = 0;
    }

    ret = populate_adc_seq_channel(&regval, regCount, ADC_SQR3_SQ1_SHIFT);
    if(ret < 0)
      return ret;    // Error
  }

  putreg32(regval, STM32_ADC1_SQR2);
 
  syslog(1, "SEQ_1-initial transfer count:%lu\n", remainingCnt);
  // Sequence Register 1 - ADC channels 13-16
  regval = getreg32(STM32_ADC1_SQR1);
  regval &= ADC_SQR1_RESERVED;   // Clear all SQR Bits
  if(remainingCnt > 0)
  {
    if(remainingCnt >= MEADOW_ADC_SEQ_3_REGISTER_TOTAL)
    {
      regCount = MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
      remainingCnt -= MEADOW_ADC_SEQ_3_REGISTER_TOTAL;
    }
    else
    {
      regCount = remainingCnt;
      remainingCnt = 0;
    }

    ret = populate_adc_seq_channel(&regval, regCount, ADC_SQR3_SQ1_SHIFT);
    if(ret < 0)
      return ret;    // Error
  }

  // Last set the total number of transfers, also in TM32_ADC1_SQR1
  regval |= ((_gpioTransferCount - 1) << ADC_SQR1_L_SHIFT);
  putreg32(regval, STM32_ADC1_SQR1);

  //----------------ADC_CCR_TSVREFE--------------------------------------------
  // ADC Common Control Register (CCR) [Common == for all ADCs]
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;       // 0=disable temperature sensor channel
  regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel
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
  regval &= ~ADC_CCR_DMA_MASK;      // Clear any bits in DMA mode (multi-ADC mode only) 
  regval |= ADC_CCR_DMA_DISABLED;   // 00 = DMA Modes (multi-ADC mode only)
  regval &= ~ADC_CCR_DDS;           // 0=No new DMA request is issued after the last transfer
  regval &= ~ADC_CCR_DELAY_MASK;    // 0000=5*Tadcclk (only used for dual/triple)
  regval &= ~ADC_CCR_MULTI_MASK;    // Clear any bits
  regval |= ADC_CCR_MULTI_NONE;     // 00000=Independent mode
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

  // Enable DMA
  regval  = getreg32(STM32_DMA2_S0CR);
  regval |= DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);

  // Initiate the ADC conversion
  regval  = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_SWSTART;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
}

//================================================================
// Initialize DMA
static void dma_initialize(void)
{
  uint32_t regval;

  // Using Nuttx DMA to handle ADC DMA
  _dmaHandle = stm32_dmachannel(DMAMAP_ADC1_1);

  // Configure the DMA SCR (Stream Control Register) values
  regval = getreg32(STM32_DMA2_S0CR);
  regval |=  DMA_SCR_MSIZE_16BITS;  // Size of memory transfer
  regval |= DMA_SCR_PSIZE_16BITS;   // Size of peripheral transfer
  // Memory increment mode. 0=mem addr is fixed, 1=mem addr increments
  regval |= DMA_SCR_MINC;           // Mem Increment
  regval |= DMA_SCR_DIR_P2M;        // Direction 0=Perph->Mem

  // SxNDTR is set by Nuttx
  stm32_dmasetup(_dmaHandle,
                 STM32_ADC1_BASE + STM32_ADC_DR_OFFSET, // Peripheral addr
                 (uint32_t) _dmaDataBuf,    // Destination buffer
                 _gpioTransferCount,        // number to transfers
                 regval);

  // Provides DMA callback information
  // void *arg will be returned via callback to ISR
  // true/false for half buffer callback as well as full buffer.
  // But doesn't seem to honor 'false' 1/2 callbacks are still made
  stm32_dmastart(_dmaHandle, adc_dma_interrupt_handler_isr,
            (void *)STM32_ADC1_BASE, false);
}

//================================================================
// Call all the sub-initialization functions. This function prepares the ADC
// to do analog conversion.
int meadow_adc_initialize_dma(void)
{
  int ret;
  static bool firstTime = true;

#if defined CONFIG_ADC_TESTS
  syslog(1, "Entered meadow_adc_initialize_dma()\n"); usleep(20 * 1000);
#endif

  if(firstTime)
  {
    firstTime = false;
    _dmaHandle = NULL;
    memset(_dmaDataBuf, 0, _adcBufSzBytes);

#if defined CONFIG_ADC_TESTS
    _noChangeCnt = 0;
#endif
  }
  else
  {
    syslog(LOG_WARNING, "Only initialize once\n");
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

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);

  _meadowAdcInit = true;

  // We need to read the following before we can calculate any results.
  // Note this doesn't use DMA, instead it used injected scheme.
  uint16_t adcTempReading;
  uint16_t adcVrefInCal;

  ret = meadow_adc_read_internal_temp_vref(&adcTempReading, &adcVrefInCal);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Temperature/Cal Ref Conversion Error. ret:%d\n");
    return ret;
  }

  // Set the global calibration value
  _adcVrefInCal = adcVrefInCal;

  // ADC + DMA won't start until adc_start() is called

#if defined CONFIG_ADC_TESTS
  syslog(1, "--> Exiting ADC config\n"); usleep(20 * 1000);
#endif

  return OK;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// This function can handle as few as 1 GPIO with 1 buffer element and as many
// as 16 GPIOs with a buffer with space for all 16.
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
          uint16_t *userDataBuf, uint32_t adcBufSzBytes)
{
#if defined CONFIG_ADC_TESTS
  syslog(1, "meadow_adc_configure() gpioCount:%lu, adcBufSzBytes:%lu, userDataBuf:%p\n",
            gpioCount, adcBufSzBytes, userDataBuf);
#endif

  // Check for valid and reasonable input parameters
  uint32_t gpioListOff;
  uint32_t mapOff = 0;

  uint32_t userBufElements;
  if(userDataBuf == NULL)
  {
    syslog(1, "%s@%d-Error:userDataBuf is NULL\n",
              __FILE__, __LINE__, adcBufSzBytes);
    return -EINVAL;   // Invalid argument
  }
  
  // Data elements are 2 bytes, therefore, size must be even number
  if(adcBufSzBytes % 2)
  {
    syslog(1, "%s@%d-Error:ADC values are always 16-bits and adcBufSzBytes is not even (%lu)\n",
              __FILE__, __LINE__, adcBufSzBytes);
    return -EINVAL;   // Invalid argument
  }

  // Each element is 2 bytes
  userBufElements = adcBufSzBytes/2;

  // Must have at least 1 entry per gpio
  if(gpioCount < userBufElements)
  {
    syslog(1, "%s@%d-Error:gpioCount:%lu < userBufElements:%lu\n",
              __FILE__, __LINE__, gpioCount, userBufElements);
    return -EINVAL;   // Invalid argument
  }

  // Verify GPIO list is valid
  for(gpioListOff = 0; gpioListOff < gpioCount; gpioListOff++)
  {
    // Look for matching port/pin in the list
    for(mapOff = 0; mapOff < MEADOW_ADC_GPIO_CHAN_MAP_LENGTH; mapOff++)
    {
      if(gpioList[gpioListOff] == _gpioAdcChanMap[mapOff])
        break;   // Found-it's connected to the ADC
    }

    // Did we go through the entire list and not find a match?
    if(mapOff == MEADOW_ADC_GPIO_CHAN_MAP_LENGTH)
    {
      syslog(LOG_INFO, "GPIO:0x%02x (P%c%d) not connected to ADC\n", gpioList[gpioListOff],
                  (gpioList[gpioListOff] >> 4) + 'A', gpioList[gpioListOff] & 0x0f);
      return -EINVAL;   // Invalid argument
    }
  }

  // Will the number of GPIOs exactly fill the provided buffer?
  if(gpioCount % userBufElements != 0)
  {
    // Don't allow empty buffer slots as it's likely the user made an unintended error.
    syslog(1, "%s@%d-Error:buffer size and the number of GPIOs not multiple.\n",
                __FILE__, __LINE__);
    return -EINVAL;   // Invalid argument
  }

  // Populate global values
  _gpioList = gpioList;
  _gpioTransferCount = gpioCount;
  _userDataBuf = userDataBuf;
  _adcBufSzBytes = adcBufSzBytes;

  // Signaling semaphore
  sem_init(&_waitTillDoneSem, 0, 0);
  sem_setprotocol(&_waitTillDoneSem, SEM_PRIO_NONE);  

  // Do before configuration
  // DON'T FREE the buffer's address as it's part of the DMA configuration
  _dmaDataBuf = kmm_malloc(_adcBufSzBytes);
  if(_dmaDataBuf == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Buffer space not available\n",
                __FILE__, __LINE__);
    return -ENOMEM;   // Out of memory
  }

  meadow_adc_initialize_dma();

  #if defined CONFIG_ADC_TESTS
  adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
  adc_test_display_basic_dma_regs();
  #endif

  return OK;
}

// //=========================================================
// // Calling this function will free all the allocations and configuration that
// // the current active configuration has used.
// int meadow_adc_dispose_of_active_config()
// {
//   if(! _meadowAdcInit)
//   {
//     syslog(LOG_ERR, "%s@%d-Error:There is no active ADC configuration.\n",
//               __FILE__, __LINE__);
//     return -EPERM;    // Operation not permitted
//   }

//   // The active configuration must have an active DMA handle from the Nuttx
//   // DMA configuration call.
//   if(_dmaHandle != NULL)
//   {
//     stm32_dmastop(_dmaHandle);
//     stm32_dmafree(_dmaHandle); 
//   }

//   if(_dmaDataBuf != NULL)
//   {
//     free(_dmaDataBuf);
//   }
 
//   return OK;
// }

//=========================================================
// This public function will return the values for Vbat and Vtemp from the
// internal STM32F7 chip. The internal sersors have calibration values written
// into the chip at the time of manufacturing. This function will use these
// values to provide the most accurate possible information to the caller.
int meadow_adc_read_temp_vbat(uint32_t *batteryVoltage,
          uint32_t *temperatureValue)
{
  int ret;
  uint16_t adcTempReading;
  uint16_t adcVrefInCal;
  uint16_t adcBatteryReading;

  // This call must be first because it reads the internal reference value
  // too, which is needed to derived the most accurate values
  ret = meadow_adc_read_internal_temp_vref(&adcTempReading, &adcVrefInCal);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Temperature Conversion Error. ret:%d\n", ret);
    return ret;
  }

  // From data sheet, temperatures calibration values were read at 30 and 110
  // degrees celsius. 
  uint32_t calAtDegC30_t1   = 30;     // Calibration temperature 1 in DegC
  uint32_t calAtDegC110_t2  = 110;    // Calibration temperature 2 in DegC
  uint32_t intRefCalVal_CAL  = (int32_t) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);  // Get the calibration values
  uint32_t intRefCalVal_TEMP1  = (int32_t) getreg16(MEADOW_ADC_TEMPSENSOR_CAL30_ADDR);
  uint32_t calValDegC110_TEMP2 = (int32_t) getreg16(MEADOW_ADC_TEMPSENSOR_CAL110_ADDR);

  syslog(1, "Raw values-TempVal:%u adcVrefInCal:%u, intRefCalVal_CAL:%d, intRefCalVal_TEMP1:%d calValDegC110_TEMP2:%d\n",
                 adcTempReading, adcVrefInCal, intRefCalVal_CAL, intRefCalVal_TEMP1, calValDegC110_TEMP2);

  // The raw temperature measurement value (0-4095) needs to be processed.
  // The following equation came from http://efton.sk/STM32/STM32_VREF.pdf.
  // calAtDegC30_t1 = 30 (degC), calAtDegC110_t2 = 110 (degC), adcRawTemp is the
  // ADC value read, adcVrefInCal is the value read from internal memory as
  // reference calibration source, intRefCalVal_TEMP1 is the calibration value
  // (30 degC) and calValDegC110_TEMP2 (110 degC) and adcIntVal is the ADC value
  // from the internal voltage reference.
  *temperatureValue = calAtDegC30_t1 + (calAtDegC110_t2 - calAtDegC30_t1) * \
            (adcTempReading * intRefCalVal_CAL - intRefCalVal_TEMP1 * adcVrefInCal) / \
            (adcVrefInCal * (calValDegC110_TEMP2 - intRefCalVal_TEMP1));
  syslog(1, "Temperature:%d, RawADC:%u\n", *temperatureValue, adcTempReading);
  
  // Note:From Ref Man 15.10
  // "The temperature sensor output voltage changes linearly with temperature. The offset of this
  // linear function depends on each chip due to process variation (up to 45°C from one chip to
  // another).
  // The internal temperature sensor is more suited for applications that detect temperature
  // variations instead of absolute temperatures. If accurate temperature reading is required, an
  // external temperature sensor should be used"
  
  // Now do the same for the battery voltage and internal CAL reference
  ret = meadow_adc_read_internal_vbat(&adcBatteryReading);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
    return ret;
  }
  
  // Reestablish the Calibration reference since we have it. Maybe it drifted?
  _adcVrefInCal = adcVrefInCal;

  // Convert the reading into a voltage value
  ret = meadow_adc_convert_adc_to_voltage(adcBatteryReading, batteryVoltage);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Conversion Cleanup Error. ret:%d\n", ret);
    return ret;
  }

  return OK;
}

//=========================================================
// To get the most best result we'll use the following calculation taken
// from http://efton.sk/STM32/STM32_VREF.pdf.
int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, uint32_t *convertedVoltage)
{
  // double math
  double intRefCalVal_CAL  = (double) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);
  double voltage = (MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN * (double)adcValue * (double)intRefCalVal_CAL + \
      ((double)_adcVrefInCal * (double)MEADOW_ADC_MAX_ADC_COUNT / 2.0)) / \
      ((double)_adcVrefInCal * (double)MEADOW_ADC_MAX_ADC_COUNT);

  *convertedVoltage = (uint32_t) (voltage * 1000.0);

  syslog(1, "--++>> value (adc):%05u, Factory CAL (adc):%04u, _adcVrefInCal (adc):%04u, (double) voltage:%3f\n",
            adcValue, getreg16(MEADOW_ADC_VREFINT_CAL_ADDR), _adcVrefInCal, voltage);
  return OK;
}

//=========================================================
// Calling this function will initiate the ADC converstion process. It will
// run until the configured buffer is full. When the buffer is full (or error)
// the calling thread will return to the caller, signifing that the buffer
// is ready for inspection.
int meadow_adc_read_conversions(void)
{
  if(! _meadowAdcInit)
  {
    syslog(LOG_ERR, "%s@%d-Error:Meadow ADC not initialized.\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // Start the conversion
  adc_start();
  
  // Wait for conversion to finish for valid data
  meadow_adc_buffer_takesem(&_waitTillDoneSem);

  // The following is used to determine if the output truely follows the analog
  // input as it switches above and below mid-point of Vdd.
#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
  // Note to execute the following display adds about 26 ms
  // Show data before copy
  static int callCount = 0;
  static int previousNoChg = 0;
  callCount++;
  if(previousNoChg != _noChangeCnt)
  {
    syslog(LOG_ERR, "%04d-Error Count:%05d\n", callCount, _noChangeCnt - 1);
  }
  previousNoChg = _noChangeCnt;
#endif

  // ADC conversion must have completed. Now convert the collected data
  // to voltage and copy to the user's buffer.
  for(int val = 0; val < _gpioTransferCount; val++)
  {
    int ret;
    uint32_t convVoltage;

  // memcpy(_userDataBuf, _dmaDataBuf, _adcBufSzBytes);

    // _dmaDataBuf contain the 0 - 4095 digital value converted from the analog
    // input via DMA
    ret = meadow_adc_convert_adc_to_voltage(_dmaDataBuf[val], &convVoltage);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%04d-Conversion Cleanup Error. ret:%d\n", ret);
      return ret;
    }
    _userDataBuf[val] = convVoltage;
  }

  return OK;
}
