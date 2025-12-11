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
// preallocated buffer. This code primarly uses STM32_ADC1_BASE throughout.
// This will make it easy if ADC1 needs to switched to ADC2 or switch
// STM32_ADC1_BASE to baseADCAddr and support multiple ADCs.

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
#include "chip/stm32f74xx77xx_adc.h"
#include "chip/stm32f76xx77xx_rcc.h"
#include "chip/stm32f76xx77xx_memorymap.h"
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_syscall_support.h>
#include "chip/stm32f76xx77xx_dma.h"
#include "../include/board.h"

#ifndef CONFIG_STM32F7_DMA2
#error "Meadow ADC requires CONFIG_STM32F7_DMA2 to be configured"
#endif

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
// #pragma GCC optimize "Og"

#if defined CONFIG_ADC_TESTS
#pragma message "(--) meadow_adc.c includes CONFIG_ADC_TESTS"
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
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

// All 3 ADCs have a max of 16 analog GPIOs that can be digitized. They all
// also contain 3 other inputs. However, only ADC1 connects the 17th AND 18th
// to things like Vbat, Vref and temperature, etc. (see User Manual for
// details).
#define MEADOW_ADC_MAX_NUMBER_OF_ADC_INPUTS (16 + 3)

// Each analog input is digitized to a 12-bit number
#define MEADOW_ADC_MAX_DMA_BUFFER_SIZE (MEADOW_ADC_MAX_NUMBER_OF_ADC_INPUTS * sizeof(uint16_t))

// From Datasheet-Internal reference addresses of parameter TS_CAL1, TS_CAL2
// (Table 79) and VREFINT_CAL (Table 81).
#define MEADOW_ADC_TEMPSENSOR_CAL30_ADDR ((const uint16_t*) 0x1FF0F44C)
#define MEADOW_ADC_TEMPSENSOR_CAL110_ADDR ((const uint16_t*) 0x1FF0F44E)
#define MEADOW_ADC_VREFINT_CAL_ADDR ((const uint16_t*) 0x1FF0F44A)

// From the Datasheet Table 82 the calibration value was read at 3.3v.
#define MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN (3.30)

// With a 12-bit ADC this is the maximum count that can be read is 4095
#define MEADOW_ADC_MAX_ADC_COUNT_DOUBLE (4095.0)

#if defined CONFIG_ADC_TESTS
// This define is only used for specific testing
#define MEADOW_ADC_TEST_TOGGLE_ADC_INPUT (0)
#else
#define MEADOW_ADC_TEST_TOGGLE_ADC_INPUT (0)
#endif 

#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
#define ADC_TEST_PIN_CCM_D03_PB8   (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN8)
#define ADC_TEST_PIN_CCM_D04_PB9   (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)
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

DMA_HANDLE _dmaHandle;
uint16_t _dmaAdcBuf[MEADOW_ADC_MAX_DMA_BUFFER_SIZE];     // This will contain ADC values (0-4095)

// These are user provided configuration values that must be persisted.
double *_userVoltageResultBuf;
static uint32_t _userGpioXferCount;

// Calibration reference value
static double _doubVrefInCal;

// Semaphores for releasing caller's thread when gpio or bat/temp done
static sem_t _gpioDoneSem;
static sem_t _injectionDoneSem;

static bool _hardwareConfigDone;
static bool _isFirstTimeInitDone = false;
static bool _isMeadowAdcInitialized;
static bool _isReinitialization;    // Was configure called again?

#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
static int _noChangeCnt;
#endif

// /************************************************************************************
//  * Private Function Prototypes
//  ************************************************************************************/

static int meadow_adc_hardware_initialize(void);
static void meadow_adc_turn_on(void);
static void meadow_adc_restart(void);
static int meadow_adc_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan);
static int meadow_adc_fill_seq_chan(uint32_t *regval, uint32_t seqRegMaxGpios,
          uint32_t initRegShift, uint8_t *userGpioList);
static int meadow_adc_read_injected_temp_vref(uint16_t *adcTempReading, uint16_t *adcVrefInCal);
static int meadow_adc_read_injected_vbat(uint16_t *adcBatteryReading);
static int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, double *convertedVoltage);
static int meadow_adc_check_first_func_call(void);
static int meadow_adc_config_sequence_regs(uint32_t userGpioXferCount, uint8_t *userGpioList);
static void meadow_adc_initialize(uint16_t *dmaAdcBuf, uint32_t userGpioXferCount);
static int meadow_adc_read_injected_common(uint16_t *analogIn18, uint16_t *analogIn17);
static int meadow_adc_free_configuration_resources(void);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// DMA ISR
static void meadow_adc_dma_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;

  // Without this call the buffer's data is usually not correct
  up_invalidate_dcache((uintptr_t)_dmaAdcBuf,
                       (uintptr_t)_dmaAdcBuf + MEADOW_ADC_MAX_DMA_BUFFER_SIZE);

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
    syslog(LOG_MTEST, "DMA ISR No Interrupts\n");
    return;
  }
  // Bit 0: Stream FIFO error interrupt flag
  if((status & DMA_STREAM_FEIF_BIT) != 0)
  {
    syslog(LOG_MTEST, "DMA ISR reason:FIFO error\n");
  }

  // Bit 2: Stream direct mode error interrupt flag
  if((status & DMA_STREAM_DMEIF_BIT) != 0)
  {
    syslog(LOG_MTEST, "DMA ISR reason:direct mode error\n");
  }

  // Bit 3: Stream Transfer Error flag
  if((status & DMA_STREAM_TEIF_BIT) != 0)
  {
    // CTEIF
    syslog(LOG_MTEST, "DMA ISR reason:Transfer Error\n");
  }
  
  //------------------------------------------------------
  // Stream Half Transfer flag
  if((status & DMA_STREAM_HTIF_BIT) != 0)
  {
    // syslog(LOG_MTEST, "DMA ISR reason:Half Transfer\n");
  }
#endif

  //------------------------------------------------------
  // Stream Transfer Complete flag
  if((status & DMA_STREAM_TCIF_BIT) != 0)
  {
#if defined CONFIG_ADC_TESTS
    syslog(LOG_MTEST, "DMA ISR reason:Transfer Complete\n");
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
    if(_dmaAdcBuf[0] > 2048)
    {
      // Voltage now above the mid point "high"
      if(_previousA00High)
        _noChangeCnt++;             // Still high, error
      else
        _previousA00High = true;    // Was Low, now high

      // syslog(LOG_MTEST, "DMA:Transfer complete - setting GPIO LOW\n");
      stm32_gpiowrite(ADC_TEST_PIN_CCM_D03_PB8, false);
    }
    else
    {
      // Voltage now below the mid-point "low"
      if(! _previousA00High)
        _noChangeCnt++;             // Still low, error
      else
         _previousA00High = false;   // Was high, now low

      // syslog(LOG_MTEST, "DMA:Transfer complete - setting GPIO HIGH\n");
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

    // Wakeup caller's thread
    sem_post(&_gpioDoneSem);
  }
}

//==========================================================================
// ADC conversion ISR
static int meadow_adc_conversion_isr(int irq, FAR void *context,
            FAR void *arg)
{
  // The interrupt conversion can be programmed to be called for the following
  // Discription                          Event Flag    Enable control bit
  // -----------                          ----------    ------------------
  // End of conversion of a regular group     EOC             EOCIE
  // End of conversion of an injected group   JEOC            JEOCIE
  // Analog watchdog status bit is set        AWD             AWDIE
  // Overrun                                  OVR             OVRIE
  uint32_t adcStatusReg;
  uint32_t baseADCAddr = (uint32_t)arg;

  adcStatusReg = getreg32(baseADCAddr + STM32_ADC_SR_OFFSET);

#if defined CONFIG_ADC_TESTS
  if(adcStatusReg == 0)
  {
    syslog(LOG_MTEST, "-- ADC ISR-NO Interrupts --\n");
    return OK;
  }

  if ((adcStatusReg & ADC_SR_AWD) != 0)
  {
    syslog(LOG_MTEST, "-- ADC ISR-WatchDog --\n");
  }

  if ((adcStatusReg & ADC_SR_OVR) != 0)
  {
    // Meadow_adc.c ignores data Overruns
    // From Ref Man 15.8.1 & 15.8.2
    // To recover the ADC from OVR when the DMA is used, follow the steps
    //   below:
    // 1. Reinitialize the DMA (adjust destination address and NDTR counter).
    //    This is DMA
    // 2. Clear the ADC OVR bit in ADC_SR register (below)
    //    This is ADC
    // 3. Trigger the ADC to start the conversion (below)
    //    This is ADC
    syslog(LOG_MTEST, "-- ADC ISR-Over Run --\n");
  }

  // End of conversion
  // With DMA there is no End Of Conversion
  if ((adcStatusReg & ADC_SR_EOC) != 0)
  {
    syslog(LOG_MTEST, "-- ADC ISR-End of Conversion --\n");
  }
#endif

  // End of injection conversion?
  if ((adcStatusReg & ADC_SR_JEOC) != 0)
  {
    // Wakeup caller's thread, injected data is ready
    sem_post(&_injectionDoneSem);
  }

  // Clear any interrupts
  adcStatusReg &= ~ADC_ALL_POSSIBLE_ADC_INTERRUPTS;
  putreg32(adcStatusReg, baseADCAddr + STM32_ADC_SR_OFFSET);

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
#if defined CONFIG_ADC_TESTS
// Only for TESTING
static void adc_test_display_basic_adc_regs(uint32_t baseADCAddr)
{
  syslog(LOG_MTEST, "SR:  0x%08x CR1:  0x%08x CR2:  0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SR_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_CR2_OFFSET));

  syslog(LOG_MTEST, "SQR1: 0x%08x SQR2: 0x%08x SQR3: 0x%08x\n",
        getreg32(baseADCAddr + STM32_ADC_SQR1_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR2_OFFSET),
        getreg32(baseADCAddr + STM32_ADC_SQR3_OFFSET));

  syslog(LOG_MTEST, "CCR:  0x%08x\n", getreg32(STM32_ADC_CCR));
}

static void adc_test_display_basic_dma_regs(void)
{
  syslog(LOG_MTEST, "S0CR:  0x%08x  S0NDTR: 0x%08x\n",
        getreg32(STM32_DMA2_S0CR),
        getreg32(STM32_DMA2_S0NDTR));

  syslog(LOG_MTEST, "S0PAR: 0x%08x  S0M0AR: 0x%08x S0M1AR: 0x%08x\n",
        getreg32(STM32_DMA2_S0PAR),
        getreg32(STM32_DMA2_S0M0AR),
        getreg32(STM32_DMA2_S0M1AR));
}
#endif

//==========================================================================
// Find ADC input channel (0-16) from the provided GPIO input port/pin
static int meadow_adc_chan_from_pinid(uint32_t pinId, uint32_t *adcInputChan)
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
// Helper to fill the sequence registers
static int meadow_adc_fill_seq_chan(uint32_t *regval, uint32_t seqRegGpios,
          uint32_t initRegShift, uint8_t *userGpioList)
{
  int ret;
  uint32_t regCnt;
  uint32_t adcInputChan;

  // Cycle through all the slots in this register
  for(regCnt = 0; regCnt < seqRegGpios; regCnt++)
  {
    // For entry Pin/Port what 'ADC input' channel
    ret = meadow_adc_chan_from_pinid(userGpioList[regCnt], &adcInputChan);
    if(ret < 0)
      return ret;    // Error

    // Each entry takes 5 bits of register value
    *regval |= (adcInputChan << (initRegShift + (regCnt * 5)));
  }
  return OK;
}

//======================================================================
// This function will place the GPIO information into the sequence registers.
// This informs the ADC which GPIOs to convert and what order to do the
// conversion.
int meadow_adc_config_sequence_regs(uint32_t userGpioXferCount, uint8_t *userGpioList)
{
  uint32_t regval;
  
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
  uint32_t remainingCnt = userGpioXferCount;

  // Sequence Register 3 - ADC channels 1-6
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);
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

    ret = meadow_adc_fill_seq_chan(&regval, regCount, ADC_SQR3_SQ1_SHIFT, userGpioList);
    if(ret < 0)
      return ret;    // Error
  }
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR3_OFFSET);

  // Sequence Register 2 - ADC channels 7-12
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SQR2_OFFSET);
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

    ret = meadow_adc_fill_seq_chan(&regval, regCount, ADC_SQR3_SQ1_SHIFT, userGpioList);
    if(ret < 0)
      return ret;    // Error
  }
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SQR2_OFFSET);
 
  // Sequence Register 1 - ADC channels 13-16
  regval = getreg32(STM32_ADC1_BASE+STM32_ADC_SQR1_OFFSET);
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

    ret = meadow_adc_fill_seq_chan(&regval, regCount, ADC_SQR3_SQ1_SHIFT, userGpioList);
    if(ret < 0)
      return ret;    // Error
  }

  // Last set the total number of transfers, also in TM32_ADC1_SQR1
  regval |= ((userGpioXferCount - 1) << ADC_SQR1_L_SHIFT);
  putreg32(regval, STM32_ADC1_BASE+STM32_ADC_SQR1_OFFSET);

  return OK;
}

//======================================================================
// Common code for reading Analog In 18 and Analog In 17 using injected mode.
// This code always obtains the 2 adc values. Even if only one is needed.
int meadow_adc_read_injected_common(uint16_t *analogIn18, uint16_t *analogIn17)
{
  uint32_t regval;

  // Injection Sequence Register - set ADC input at 18 since ADC_IN18 is used
  // by temperature and vbat depending on eht value of TSVREFE
  // Note: for the injected conversions the first is from JSQ3 and the second
  // from JSQ4 (see Ref Man 15.13.12). Why not 1 & 2, don't know.
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= ~0x003fffff;   // Clear all JSQR Bits
  // Note: 4 and 3? These may look wrong but are correct.
  regval |= (18 << ADC_JSQR_JSQ4_SHIFT);    // ADC_IN18 - Vsense (temp or vbat)
  regval |= (17 << ADC_JSQR_JSQ3_SHIFT);    // ADC_IN17 - Vrefint
  regval |= (1 << ADC_JSQR_JL_SHIFT);       // Set to 1 indicates 2 inputs
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

  // Start ADC via Control Register 2 (CR2) - Start the ADC
  // JSWSTART cannot be set if ADON is not set
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_JSWSTART;       // Start injection ADC
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // Wait for injected conversion to complete
  meadow_adc_buffer_takesem(&_injectionDoneSem);

  *analogIn18 = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR2_OFFSET);
  *analogIn17 = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR1_OFFSET);
  return OK;
}

//======================================================================
// Using injection get the adc value (0-4095) for temp and vbat
int meadow_adc_read_injected_temp_vref(uint16_t *adcTempReading, uint16_t *adcVrefInCal)
{
  int ret;
  uint32_t regval;
  uint16_t tempAnaIn18;
  uint16_t vrefAnaIn17;
  
  // CCR setup to read Temperature & Vref
  regval = getreg32(STM32_ADC_CCR);
  regval |= ADC_CCR_TSVREFE;        // 1=enable temperature sensor and reference
  regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  // Per Data Sheet 5.3.25 startup time maximum is 10us tSTART
  usleep(10);

  ret = meadow_adc_read_injected_common(&tempAnaIn18, &vrefAnaIn17);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Injected common, ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  if(adcTempReading != NULL)
    *adcTempReading = tempAnaIn18;

  if(adcVrefInCal != NULL)
    *adcVrefInCal = vrefAnaIn17;

  // Set or refresh the calibration reference
  _doubVrefInCal = (double)vrefAnaIn17;

  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature/vref on this channel
  putreg32(regval, STM32_ADC_CCR);

  return ret;
}

//======================================================================
// This will read the raw ADC value for Vbat.
int meadow_adc_read_injected_vbat(uint16_t *adcBatteryReading)
{
  int ret;
  uint32_t regval;
  uint16_t dummyAnaIn17;
  uint16_t battAnaIn18;

  if(adcBatteryReading == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Read battery 'adcBatteryReading' can't be NULL\n",
              __FILE__, __LINE__);
    return -EINVAL;     // Invalid argument
  }

  // Setup to read Vbat
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor and ref
  regval |= ADC_CCR_VBATE;        // 1=enable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  // In this case we'll ignore the dummyAnaIn17 return value
  ret = meadow_adc_read_injected_common(&battAnaIn18, &dummyAnaIn17);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Read battery, ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  *adcBatteryReading = battAnaIn18;

  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_VBATE;        // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  return ret;
}

//======================================================================
// This function will do all the register configuration needed for regular ADC
// and injected ADC. It excludes the DMA configuration.
static int meadow_adc_hardware_reg_init (void)
{
  uint32_t regval;

#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
  // This is used in testing it toggled high and low to switch the ADC input
  // voltage from high to low.
  stm32_configgpio(ADC_TEST_PIN_CCM_D03_PB8);
  stm32_gpiowrite(ADC_TEST_PIN_CCM_D03_PB8, false);
#endif

  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
  // If ADC2 or ADC3 are needed add RCC_APB2ENR_ADC2EN and RCC_APB2ENR_ADC3EN.
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
  
  // Set sample time for channels 10-18
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET);
  regval &= 0xf8000000;      // Clear Sample Time fields 10-18
  // Set all sample times to the same value.
  regval |= ADC_SMPR1_DEFAULT;

  // For Vbat and internal temperature, overwrite with the longest sample time
  // available. This is because the internal impedance of these inputs, likely
  // need more time to charge the Sample and Hold capactors.
  regval &= ~ADC_SMPR1_SMP17_MASK;
  regval |= (ADC_SMPR_480 << ADC_SMPR1_SMP17_SHIFT);
  regval &= ~ADC_SMPR1_SMP18_MASK;
  regval |= (ADC_SMPR_480 << ADC_SMPR1_SMP18_SHIFT);
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET);

  // Set sample time for channels 0-9
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SMPR2_OFFSET);
  regval &= 0xc0000000;      // Clear sample time fields 0-9
  regval |= ADC_SMPR2_DEFAULT;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SMPR2_OFFSET);

  //---------------------------------------------------
  // ADC Control Register 1 (CR1)
  // This register controls a lot of options. I put the following in the same
  // order as the Ref Man 15.13.2.
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);
  regval &= ~ADC_CR1_OVRIE;         // 0=Disable Overrun interrupt
  regval &= ~ADC_CR1_RES_MASK;      // Insure all resolution bit are clear
  regval |= ADC_CR1_RES_12BIT;      // Set resolution 00=12, 01=10, 10=8 or 11=6 bits
  regval &= ~ADC_CR1_AWDEN;         // 0=Disable Analog watchdog on regular channels
  regval &= ~ADC_CR1_JAWDEN;        // 0=Disable Analog watchdog on injected
  regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  regval &= ~ADC_CR1_JDISCEN;       // 0=Disable discontinuous mode on injected channels
  regval &= ~ADC_CR1_DISCEN;        // 0=Disable discontinuous mode on regular channels
  regval &= ~ADC_CR1_JAUTO;         // 0=Automatic Injected Group conversion
  regval &= ~ADC_CR1_AWDSGL;        // 0=Disable watchdog on single channel in scan mode
  // In Scan mode, the inputs selected through the ADC_SQRx
  regval |= ADC_CR1_SCAN;           // 1=Scan mode (Scans channels in ADC_SQRx registers)
  regval |= ADC_CR1_JEOCIE;         // 1=Enable interrupt for injected channels

  regval &= ~ADC_CR1_AWDIE;         // 0=Disable Analog Watchdog interrupt
  regval &= ~ADC_CR1_EOCIE;         // 0=Disable ADC interrupt for EOC
  regval &= ~ADC_CR1_AWDCH_MASK;    // Clear the watchdog channel to 00000=Chan 0
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  //---------------------------------------------------
  // ADC Control Register 2 (CR2)
  // Note:fields not defined in Nuttx header file have been ignored. Some
  // members of CR2 are set/cleared other places.
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_EXTEN_MASK;  // Clear bits
  regval |= ADC_CR2_EXTEN_NONE;   // No trigger from external sources
  regval &= ~ADC_CR2_ALIGN;       // 0=Right alignment (1=left alignment)
  regval &= ~ADC_CR2_EOCS;        // 1=End of each conversion, 0=End of sequence

  // Note: the field names 'DDS' and 'DMA' exist in 2 ADC registers, CR2 and CCR.
  // DDS=0 No new DMA request is issued after the last transfer
  // DDS=1 DMA requests are issued as long as data are converted and DMA=1
  regval &= ~ADC_CR2_DDS;
  regval |= ADC_CR2_DMA;          // 1=Enable DMA
  // Not doing continous conversion
  regval &= ~ADC_CR2_CONT;         // 0=Disable continuous conversion
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  //------------------------------------------------------------
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
// Start ADC1
void meadow_adc_turn_on(void)
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
// Run ADC once. This must be done before every GPIO conversion series.
void meadow_adc_restart(void)
{
  uint32_t regval;

  // Enable DMA
  regval  = getreg32(STM32_DMA2_S0CR);
  regval |= DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);

  // Start the ADC conversion
  regval  = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_SWSTART;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
}

//================================================================
// Initialize DMA. This module leverages the NuttX DMA implementation.
void meadow_adc_initialize(uint16_t *dmaAdcBuf, uint32_t userGpioXferCount)
{
  uint32_t regval;

  // The active configuration will have an active DMA handle from Nuttx
  if(_dmaHandle != NULL)
  {
    stm32_dmastop(_dmaHandle);
    stm32_dmafree(_dmaHandle); 
  }

  // Using Nuttx DMA to handle ADC DMA. Because our SDCard implementation
  // uses SDMMC2 which uses the other ADC DMA2 channel.
  _dmaHandle = stm32_dmachannel(ADC1_DMA_CHAN);

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
                 (uint32_t) dmaAdcBuf,     // Destination buffer
                 userGpioXferCount,        // number to transfers
                 regval);

  // Provides DMA callback information
  // void *arg will be returned via callback to ISR
  // true/false for half buffer callback as well as full buffer.
  // Seems to not honor 'false' 1/2 callbacks are still made
  stm32_dmastart(_dmaHandle, meadow_adc_dma_isr,
            (void *)STM32_ADC1_BASE, false);
}

//================================================================
// Call all the sub-initialization functions. This function prepares the ADC
// to do analog conversion.
// This only needs to be called once
int meadow_adc_hardware_initialize(void)
{
  int ret;

#if defined CONFIG_ADC_TESTS
  syslog(LOG_MTEST, "Entered meadow_adc_hardware_initialize()\n");
#endif

  if(_hardwareConfigDone)
  {
    syslog(LOG_WARNING, "Can only initialize once\n");
    return -EALREADY;
  }

#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
  _noChangeCnt = 0;
#endif

  // Setup the ADC Interrupt handler
  ret = irq_attach(STM32_IRQ_ADC, meadow_adc_conversion_isr,
            (void *)STM32_ADC1_BASE);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error calling irq_attach, ret:%d\n",
                  __FILE__, __LINE__, ret);
    return ret;
  }

  ret = meadow_adc_hardware_reg_init();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:hardware init\n, ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  meadow_adc_turn_on();

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);

  // We need to do the following before we can calculate any of the results.
  // Note: this doesn't use DMA, instead it uses the ADC's injected mode.
  ret = meadow_adc_read_injected_temp_vref(NULL, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Initial Temp/Cal Ref Conversion Error. ret:%d\n",
                  __FILE__, __LINE__, ret);
    return ret;
  }

#if defined CONFIG_ADC_TESTS
  // syslog(LOG_MTEST, "Post hardware configuration register values:\n");
  // adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
  // adc_test_display_basic_dma_regs();
#endif

  _hardwareConfigDone = true;

  return OK;
}

//=========================================================
// To get the best result we'll use the following calculation taken
// from http://efton.sk/STM32/STM32_VREF.pdf.
// Per Data Sheet section 5.3.27. This reference voltage is used in
// this calculation as _doubVrefInCal.
int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, double *convertedVoltage)
{
  double calRefVal_CAL  = (double) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);
  double voltage = (MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN * (double)adcValue * calRefVal_CAL + \
      (_doubVrefInCal * MEADOW_ADC_MAX_ADC_COUNT_DOUBLE / 2.0)) / \
      (_doubVrefInCal * MEADOW_ADC_MAX_ADC_COUNT_DOUBLE);

  // Don't know the cause but, the voltage returned is always exactly 0.50
  // volts too high. Tried this on different platforms (F7FeatherV2,
  // CCM versions), it's always the same. Must be something in the math.
  *convertedVoltage = voltage - 0.5;
  return OK;
}

//=========================================================
// One time initialization. This is called on each configuration. And by both
// Battery/Vbat and normal ADC initialization.
int meadow_adc_check_first_func_call()
{
  if(! _isFirstTimeInitDone)
  {
    // Initialize and allocate things needed exactly once
    _isFirstTimeInitDone = true;

    _isReinitialization = false;
    _isMeadowAdcInitialized = false;

    // Global initiation
    _dmaHandle = NULL;
    _doubVrefInCal = 0.0;
    _hardwareConfigDone = false;
    
    // Signaling semaphore for GPIO conversion
    sem_init(&_gpioDoneSem, 0, 0);
    sem_setprotocol(&_gpioDoneSem, SEM_PRIO_NONE);

    // Signaling semaphore for Vbat and Temperature conversion
    sem_init(&_injectionDoneSem, 0, 0);
    sem_setprotocol(&_injectionDoneSem, SEM_PRIO_NONE);
  }
  else
  {
    _isReinitialization = true;
  }

  return OK;
}

//================================================================
// This function will free all the resources used by the application
int meadow_adc_free_configuration_resources()
{
  // Are there any resources to free?
  if(!_isFirstTimeInitDone)
  {
    return OK;
  }

  // Semaphores for GPIO conversion and for Vbat and Temperature
  sem_destroy(&_gpioDoneSem);
  sem_destroy(&_injectionDoneSem);

  // An active configuration will have an active DMA handle from Nuttx
  if(_dmaHandle != NULL)
  {
    stm32_dmastop(_dmaHandle);
    stm32_dmafree(_dmaHandle);
    _dmaHandle = NULL;
  }

  _isFirstTimeInitDone = false;

  return OK;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// This function can handle as few as 1 GPIO and as many as 16 GPIOs. It is
// called for GPIO initialization. And if the gpioCount -s 0 it will release
// any resources that have been claimed by an earlier configuration.
// For VBat and Temperature values. A part of the configuration is needed for
// both. This is not duplicated here.
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
          double *resultBuffer)
{
  int ret;
  uint32_t gpioListOff;
  uint32_t mapOff = 0;

#if defined CONFIG_ADC_TESTS
  syslog(LOG_MTEST, "Entry meadow_adc_configure() gpioCount:%lu, resultBuffer:%p\n",
            gpioCount, resultBuffer);
  adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
  adc_test_display_basic_dma_regs();
#endif

  if(gpioCount == 0)
  {
    ret = meadow_adc_free_configuration_resources();
    return ret;
  }
  
  if(resultBuffer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-resultBuffer is NULL\n",
              __FILE__, __LINE__);
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
      syslog(LOG_ERR, "%s@%d-(P%c%d) not valid pin\n",
              __FILE__, __LINE__,
              (gpioList[gpioListOff] >> 4) + 'A',
              gpioList[gpioListOff] & 0x0f);
      return -EINVAL;   // Invalid argument
    }
  }

  // Check if this is the very first time this has been called.
  ret = meadow_adc_check_first_func_call();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow adc config, ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // Is this a re-initialization or the first time?
  if(_isReinitialization)
  {
    _hardwareConfigDone = false;
  }

  //----------------------------------------------
  // Passed the tests so save need user parameters and initialize. Note:
  // initialization can be repeated.
  _userGpioXferCount = gpioCount;
  _userVoltageResultBuf = resultBuffer;

  // There are 3 initialization steps to fully initialize the ADC for analog
  // GPIO and Battery/Temperature conversion. Hardware initialization is needed
  // for reading the internal MCU temperature and battery voltage.
  if(!_hardwareConfigDone)
  {
    ret = meadow_adc_hardware_initialize();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Hardware initialize failed. ret:%d\n",
              __FILE__, __LINE__, ret);
      return ret;
    }
  }

  // Configure GPIO sequence registers. This call will populate the sequence
  // registers.
  ret = meadow_adc_config_sequence_regs(gpioCount, gpioList);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Initialization failed. ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // Initialize ADC
  meadow_adc_initialize(_dmaAdcBuf, _userGpioXferCount);

  _isMeadowAdcInitialized = true;

#if defined CONFIG_ADC_TESTS
  syslog(LOG_MTEST, "Exit meadow_adc_configure()\n");
  adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
  adc_test_display_basic_dma_regs();
#endif

  return OK;
}

//=========================================================
// Calling this function will initiate the ADC converstion process. The
// conversion process will run until the configured buffer is full (up to 16
// entries). When the buffer is full (or error) the calling thread is released
// (via semaphore) to calculate the GPIO's voltage and return to the caller.
int meadow_adc_read_values(void)
{
  int ret = OK;

  if(! _isMeadowAdcInitialized)
  {
    syslog(LOG_ERR, "%s@%d-Error:Meadow ADC not configured\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // Since not doing continous conversion we need to re-start ADC each time
  meadow_adc_restart();

  // Wait for conversion to finish for valid data
  meadow_adc_buffer_takesem(&_gpioDoneSem);

  // The following is diagnostic used to determine if the output truly
  // follows the analog input as it checks that the analog input
  // switched between above and below mid-point of Vdd  on each call.
#if MEADOW_ADC_TEST_TOGGLE_ADC_INPUT > 0
  // Note to execute the following display adds about 26 ms
  // Show data before copy
  static int callCount = 0;
  static int previousNoChg = 0;
  callCount++;
  if(previousNoChg != _noChangeCnt)
  {
    syslog(LOG_ERR, "%s@%d-%04d-Error Count:%05d\n",
                  __FILE__, __LINE__, callCount, _noChangeCnt - 1);
  }
  previousNoChg = _noChangeCnt;
#endif

  // ADC conversion must have completed. Now convert the collected data
  // to voltage and copy to the user's buffer.
  for(int valOff = 0; valOff < _userGpioXferCount; valOff++)
  {
    double convVoltage;

    // _dmaAdcBuf contain the adc output values (0-4095) placed there by DMA.
    //  Convert these values to a voltage and for the user.
    ret = meadow_adc_convert_adc_to_voltage(_dmaAdcBuf[valOff], &convVoltage);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Error:ADC to Voltage, ret:%d\n",
              __FILE__, __LINE__, ret);
      return ret;
    }

    // Populate caller's buffer
    _userVoltageResultBuf[valOff] = convVoltage;
  }

  return ret;
}

//=========================================================
// This public function will return the values for Vbat and Vtemp from the
// internal STM32F7 chip. The internal sersors have calibration values written
// into the chip at the time of manufacture. This function will use these
// values to provide the best possible information to the caller.
//
// Quote from Ref Man 15.10
// "The temperature sensor output voltage changes linearly with temperature.
// The offset of this linear function depends on each chip due to process
// variation (up to 45°C from one chip to another).
// The internal temperature sensor is more suited for applications that detect
// temperature variations instead of absolute temperatures. If accurate
// temperature reading is required, an external temperature sensor should be
// used"
int meadow_adc_read_temp_vbat(double *batteryVoltage, double *tempValue)
{
  int ret;
  uint16_t adcTempReading;
  uint16_t adcVrefInCal;

  // Check if this is the very first time any function has been called
  ret = meadow_adc_check_first_func_call();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Reading Temp/Vbat failed. ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // Has the hardware configuration been done already?
  if(!_hardwareConfigDone)
  {
    // Hardware not yet configured. This is needed by both temp/vbat and GPIO
    // ADC but GPIO requires more init. So, we'll only do what's needed and
    // any GPIO config will be done when needed. Why? because the rest of
    // the init requires information only available from the caller.
    ret = meadow_adc_hardware_initialize();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Reading Temp/Vbat failed. ret:%d\n",
              __FILE__, __LINE__, ret);
      return ret;
    }
  }

  // This must be done before reading Vbat because it reads and sets
  // _doubVrefInCal which is used for both temperature and voltage calculations
  // FWIW - meadow_adc_hardware_initialize() sets up the hardware but also
  // calls meadow_adc_read_injected_temp_vref(), internally, so the
  // following call is a duplicate, which happens once.
  ret = meadow_adc_read_injected_temp_vref(&adcTempReading, &adcVrefInCal);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Temperature/Vref read error, ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // From Data Sheet , temperatures calibration values have been read at 30
  // and 110 degrees celsius. 
  double calAtDegC30_t1  = 30.0;     // Calibration temperature 1 in DegC
  double calAtDegC110_t2 = 110.0;    // Calibration temperature 2 in DegC
  // Get the STM factory calibration values stored in these registers.
  double calRefVal_CAL  = (double) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);
  double tempCal1_TEMP1 = (double) getreg16(MEADOW_ADC_TEMPSENSOR_CAL30_ADDR);
  double tempCal2_TEMP2 = (double) getreg16(MEADOW_ADC_TEMPSENSOR_CAL110_ADDR);

  // The adc temperature measurement value (0-4095) needs to be converted to
  // a temperature value. To do the best conversion the information defined
  // in the Data Sheet needs to be incorporated.
  // The following equation came from http://efton.sk/STM32/STM32_VREF.pdf.
  *tempValue = calAtDegC30_t1 + (calAtDegC110_t2 - calAtDegC30_t1) * \
            ((double)adcTempReading * calRefVal_CAL - \
            tempCal1_TEMP1 * _doubVrefInCal) / \
            (_doubVrefInCal * (tempCal2_TEMP2 - tempCal1_TEMP1));
  
  // Now do the same for the battery voltage
  uint16_t adcBatteryReading;
  ret = meadow_adc_read_injected_vbat(&adcBatteryReading);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Reading Vbat Error. ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // Convert the Vbat reading into a voltage value
  // Note:Per Ref Man section 15.11 the Battery voltage read is VBAT/4
  // We must multiple before conversion to recover the correct value.
  double vbat;
  ret = meadow_adc_convert_adc_to_voltage(adcBatteryReading * 4, &vbat);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-VBat Conversion Error. ret:%d\n",
              __FILE__, __LINE__, ret);
    return ret;
  }

  // Per Data Sheet 2.17, any voltage below 1.65 is not allowed
  if(vbat < 1.0)
    *batteryVoltage = 0.0;
  else
    *batteryVoltage = vbat;

  return OK;
}

