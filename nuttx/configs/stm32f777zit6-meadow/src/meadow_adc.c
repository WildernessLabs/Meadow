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
// 12Oct23 - Known issues when returning to adc coding
// X1. Added but not fully tested. _doubVrefInCal is populated before any other
//    calls. Made double throughout.
// X2. Voltage reading are 0.5 volts too high. Use meadow set developer -d 14 -v 1
//    and meadow set developer -d 14 -v 2 to test.
// X3. All calculations should use double e.g. meadow_adc_read_temp_vbat() and
//     meadow_adc_configure()
// X4. Need to add code to unconfigure ADC.
// X6. Re-test all Analog inputs, Battery voltage, Internal MCU temperature etc.
// X7. Need to use Bat/Temp without normal configuration.
// 8. Test disable code by re-configuring and testing again.
// X9. How to use MCU temperature and Vbat without configuring entire ADC?
// 10. Refactor STM32_ADC1_BASE into a variable throughout?
// 11. Look for "syslog(1," and insure syslog etc. have correct number of
//    arguments for the string. Also check  (--) in code and do general cleanup.
//    And look/adjust all usleep entries
// 12. All errors should include file and line number.
// 13. Add syscall for interacting with Meadow.Core

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

// All 3 ADCs have a max of 16 analog GPIOs that can be digitized
#define MEADOW_ADC_MAX_NUMBER_OF_GPIOS (16)

// Each analog GPIO is digitized to a 12-bit number
#define MEADOW_ADC_MAX_DMA_BUFFER_SIZE (MEADOW_ADC_MAX_NUMBER_OF_GPIOS * sizeof(uint16_t))

// From Data Sheet-Internal reference addresses of parameter TS_CAL1, TS_CAL2
// (Table 79) and VREFINT_CAL (Table 81).
#define MEADOW_ADC_TEMPSENSOR_CAL30_ADDR ((const uint16_t*) 0x1FF0F44C)
#define MEADOW_ADC_TEMPSENSOR_CAL110_ADDR ((const uint16_t*) 0x1FF0F44E)
#define MEADOW_ADC_VREFINT_CAL_ADDR ((const uint16_t*) 0x1FF0F44A)

// From the data sheet table 82 the calibration value was read at 3.3v. We'll
// multiply by 10 to keep everything in integers
#define MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN (3.30)

// With a 12-bit ADC this is the maximum count that can be read
#define MEADOW_ADC_MAX_ADC_COUNT_DOUBLE (4095.0)

#if defined CONFIG_ADC_TESTS
#define MEADOW_ADC_TEST_TOGGLE_ADC_INPUT (0)
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
uint16_t *_dmaAdcBuf;     // This will contain ADC values (0-4095)

// These are user provided values that must be persisted.
double *_userVoltageResultBuf;
static uint32_t _userGpioXferCount;

static double _doubVrefInCal;
static bool _hardwareConfigDone;
static bool _meadowAdcUserInit;
static bool _meadowAdcRegInit;
static sem_t _gpioDoneSem;
static sem_t _injectionDoneSem;

#if defined CONFIG_ADC_TESTS
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
static int meadow_adc_injected_adc_cleanup(void);
static int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, double *convertedVoltage);
static int meadow_adc_check_first_func_call(void);
static int meadow_adc_config_sequence_regs(uint32_t userGpioXferCount, uint8_t *userGpioList);
static void meadow_adc_dma_initialize(uint16_t *dmaAdcBuf, uint32_t userGpioXferCount);
static int meadow_adc_read_injected_common(uint16_t *analogIn18, uint16_t *analogIn17);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
// DMA ISR
static void meadow_adc_dma_isr(DMA_HANDLE handle, uint8_t status,
            FAR void *arg)
{
  uint32_t regval;
  // uint32_t baseADCAddr = (uint32_t)arg;
  // static int execCnt = 0;

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
    if(_dmaAdcBuf[0] > 2048)
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

  // End of injection conversion?
  if ((pendingInterrupts & ADC_SR_JEOC) != 0)
  {
    // Wakeup caller's thread, data is ready
    sem_post(&_injectionDoneSem);
  }

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
// Helps fill the sequence registers
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

    syslog(1, "SEQ_3-initial transfer count:%lu\n", regCount);

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

    syslog(1, "SEQ_2-initial transfer count:%lu\n", regCount);

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

    syslog(1, "SEQ_1-initial transfer count:%lu\n", regCount);

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
// Note this code always obtains 2 adc values. Even if only one is needed
int meadow_adc_read_injected_common(uint16_t *analogIn18, uint16_t *analogIn17)
{
  uint32_t regval;

  // // THIS SEEMS TO DO NOTHING
  // regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);
  // regval |= ADC_CR1_JAUTO;         // 1=Automatic Injected Group conversion
  // putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  // Injection Sequence Register - set ADC input at 18 since ADC_IN18 is used
  // by temperature and vbat depending on eht value of TSVREFE
  // Note: for the injected conversions the first is from JSQ3 and the second
  // from JSQ4 (see Ref Man 15.13.12). Why not 1 & 2, don't know.
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= ~0x003fffff;   // Clear all JSQR Bits
  // Note: These may look wrong but are correct.
  regval |= (18 << ADC_JSQR_JSQ4_SHIFT);    // ADC_IN18 - Vsense (temp or vbat)
  regval |= (17 << ADC_JSQR_JSQ3_SHIFT);    // ADC_IN17 - Vrefint
  regval |= (1 << ADC_JSQR_JL_SHIFT);       // Set to 1 indicates 2 inputs
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

// (--) TESTING
  // syslog(1, "==>> Before conversion JDR1 (Ain17):%u, JDR2 (Ain18):%u\n",
  //           getreg16(STM32_ADC1_BASE + STM32_ADC_JDR1_OFFSET),
  //           getreg16(STM32_ADC1_BASE + STM32_ADC_JDR2_OFFSET));

  // Start ADC via Control Register 2 (CR2) - Start the ADC
  // JSWSTART cannot be set if ADON is not set
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval |= ADC_CR2_JSWSTART;       // Start injection ADC
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // (*) USING THIS INSTEAD OF INTERRUPT Wait till done
  while((getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET) & ADC_SR_JEOC) == 0);

  // Wait for injected conversion to complete
  // (*) NOT USING THIS INSTEAD OF INTERRUPT Wait till done
  // meadow_adc_buffer_takesem(&_injectionDoneSem);

  // Clear end of injected conversion bit
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);
  regval &= ~ADC_SR_JEOC;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);


  *analogIn18 = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR2_OFFSET);
  *analogIn17 = getreg16(STM32_ADC1_BASE + STM32_ADC_JDR1_OFFSET);

  // Clear End Of Conversion bit
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);
  regval &= ~ADC_SR_JEOC;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);

  // (--) Remove configuration 
  // int ret = meadow_adc_injected_adc_cleanup();
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
  //   return ret;
  // }

  return OK;
}

//======================================================================
// (--) is this needed?
// Undoes the above injected configuration
int meadow_adc_injected_adc_cleanup(void)
{
  uint32_t regval;

  // Clear End Of Conversion bit
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);
  regval &= ~ADC_SR_JEOC;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);

  // Restore register values to neutral
  // Disable both Temperature and Vbat
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor channel
  regval &= ~ADC_CCR_VBATE;        // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);
  regval &= ~0x003fffff;   // Clear all JSQR Bits
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_JSQR_OFFSET);

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
    syslog(LOG_ERR, "Injected common, ret:%d\n", ret);
    return ret;
  }

  if(adcTempReading != NULL)
    *adcTempReading = tempAnaIn18;

  if(adcVrefInCal != NULL)
    *adcVrefInCal = vrefAnaIn17;

  // Set or refresh the calibration reference
  _doubVrefInCal = (double)vrefAnaIn17;

  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor channel
  putreg32(regval, STM32_ADC_CCR);

  // syslog(1, "-->> ADC values (0-4095), Temp:%u, adc VrefCal:%u\n", tempAnaIn18, vrefAnaIn17);
  return ret;
}

//======================================================================
// This will read the raw ADC value for Vbat.
int meadow_adc_read_injected_vbat(uint16_t *adcBatteryReading)
{
  int ret;
  uint32_t regval;
  uint16_t analogIn17;
  uint16_t battAnaIn18;

  if(adcBatteryReading == NULL)
  {
    syslog(LOG_ERR, "adcBatteryReading cannot be NULL\n");
    return -EINVAL;     // Invalid argument
  }

  // Setup to read Vbat
  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;     // 0=disable temperature sensor and ref
  regval |= ADC_CCR_VBATE;        // 1=enable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  // In this case we ignore the analogIn17 return value
  ret = meadow_adc_read_injected_common(&battAnaIn18, &analogIn17);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Injected common, ret:%d\n", ret);
    return ret;
  }

  *adcBatteryReading = battAnaIn18;

  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_VBATE;        // 0=disable vbat channel
  putreg32(regval, STM32_ADC_CCR);

  return ret;
}

//======================================================================
// This function will do all the register configuration needed for ADC,
// excluding the DMA configuration. Some of these settings are needed for both
// regular ADC and injected ADC.
static int meadow_adc_hardware_reg_init (void)
{
  uint32_t regval;

#if defined CONFIG_ADC_TESTS
  // This is used in testing it toggled high and low to switch the ADC input
  // voltage from high to low.
  syslog(1, "Warning: ADC Tests Active\n");
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
  // TESTING-Set all channels to the same default ADC_SMPR_DEFAULT
  // Set sample time for channels 10-18
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET);
  // This #define will set all sample times to the same value. Here
  // ADC_SMPR_DEFAULTs are used to set ADC conversion the same.
  regval &= 0xf8000000;      // Clear Sample Time fields 10-18
  regval |= ADC_SMPR1_DEFAULT;

// (--) Injected ADC results with different sample times. This looks backward
// from what the Ref Man describes
//                ADC    Voltage      Temp
// ADC_SMPR_480 - 134     0.930       32.908
// ADC_SMPR_144 - 137     0.942       38.630
// ADC_SMPR_84  - 160     1.010       40.942 Changed boards-v
// ADC_SMPR_28  - 375     1.722       37.104
// ADC_SMPR_3   - 862     3.214       40.999

  // For Vbat and internal temperature overwrite with the longest sample time
  regval &= ~ADC_SMPR1_SMP17_MASK;
  regval |= (ADC_SMPR_480 << ADC_SMPR1_SMP17_SHIFT);
  regval &= ~ADC_SMPR1_SMP18_MASK;
  regval |= (ADC_SMPR_480 << ADC_SMPR1_SMP18_SHIFT);
  // syslog(1, "After ->SMPR1:0x%08x\n", regval);
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
  regval &= ~ADC_CR1_DISCNUM_MASK;  // Set number of discontinuous channels to 1
  regval &= ~ADC_CR1_JDISCEN;       // 0=Disable discontinuous mode on injected channels
  regval &= ~ADC_CR1_DISCEN;        // 0=Disable discontinuous mode on regular channels
  regval &= ~ADC_CR1_JAUTO;         // 0=Automatic Injected Group conversion
  regval &= ~ADC_CR1_AWDSGL;        // 0=Disable watchdog on single channel in scan mode
  // In Scan mode, the inputs selected through the ADC_SQRx
  regval |= ADC_CR1_SCAN;           // 1=Scan mode (Scans channels in ADC_SQRx registers)

  // (*) REMOVED INJECTED ISR CALL FOR TESTING?
  // regval |= ADC_CR1_JEOCIE;         // 1=Enable interrupt for injected channels
  regval &= ~ADC_CR1_JEOCIE;         // 0=Disable interrupt for injected channels

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
  // Warning: the field names 'DDS' and 'DMA' exist in 2 ADC registers, CR2 and CCR.
  // DDS=0 No new DMA request is issued after the last transfer
  // DDS=1 DMA requests are issued as long as data are converted and DMA=1
  regval &= ~ADC_CR2_DDS;
  regval |= ADC_CR2_DMA;          // 1=Enable DMA
  // Disable continous conversion
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
// Enable ADC. This must be done before every GPIO conversion series
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
// Start ADC
void meadow_adc_restart(void)
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
void meadow_adc_dma_initialize(uint16_t *dmaAdcBuf, uint32_t userGpioXferCount)
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
                 (uint32_t) dmaAdcBuf,     // Destination buffer
                 userGpioXferCount,        // number to transfers
                 regval);

  // Provides DMA callback information
  // void *arg will be returned via callback to ISR
  // true/false for half buffer callback as well as full buffer.
  // But doesn't seem to honor 'false' 1/2 callbacks are still made
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
  static bool adcInitFirstTime = true;

#if defined CONFIG_ADC_TESTS
  syslog(1, "Entered meadow_adc_hardware_initialize()\n"); usleep(20 * 1000);
#endif

  if(adcInitFirstTime)
  {
    adcInitFirstTime = false;

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
  ret = irq_attach(STM32_IRQ_ADC, meadow_adc_conversion_isr,
            (void *)STM32_ADC1_BASE);
  if(ret < 0)
  {
    syslog(1, "Error calling irq_attach\n");
    return ret;
  }

  ret = meadow_adc_hardware_reg_init();
  if(ret < 0)
  {
    syslog(1, "Error calling meadow_adc_hardware_reg_init\n"); usleep(20 * 1000);
    return ret;
  }

  meadow_adc_turn_on();

  // Enable ADC interrupt handler
  up_enable_irq(STM32_IRQ_ADC);

  // Note: ADC won't start until meadow_adc_restart() is called

  // We need to do the following before we can calculate any of the results.
  // Note: this doesn't use DMA, instead it uses the ADC's injected mode.
  ret = meadow_adc_read_injected_temp_vref(NULL, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Initial Temp/Cal Ref Conversion Error. ret:%d\n");
    return ret;
  }

  // Set the flag indicating that this initialzation has been done
  _hardwareConfigDone = true;

#if defined CONFIG_ADC_TESTS
  // syslog(1, "Post hardware configuration register values:\n");
  // adc_test_display_basic_adc_regs(STM32_ADC1_BASE);
  // adc_test_display_basic_dma_regs();
#endif

  return OK;
}

//=========================================================
// To get the most best result we'll use the following calculation taken
// from http://efton.sk/STM32/STM32_VREF.pdf.
// Per Data Sheet section 5.3.27. This reference voltage is considered in
// this calculation as _doubVrefInCal.
int meadow_adc_convert_adc_to_voltage(uint16_t adcValue, double *convertedVoltage)
{
  double calRefVal_CAL  = (double) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);
  double voltage = (MEADOW_ADC_VOLTAGE_ADC_CAL_TAKEN * (double)adcValue * calRefVal_CAL + \
      (_doubVrefInCal * MEADOW_ADC_MAX_ADC_COUNT_DOUBLE / 2.0)) / \
      (_doubVrefInCal * MEADOW_ADC_MAX_ADC_COUNT_DOUBLE);

  // Don't know the cause but, the voltage returned is always exactly 0.50
  // volts too high. Tried this on different platforms (F7FeatherV2,
  // CCM versions) and it the same for all. Must be something about the above
  // math.
  *convertedVoltage = voltage - 0.5;

  // syslog(1, "--++>> value (adc):%04u, voltage:%.3f\n", adcValue, *convertedVoltage);
  return OK;
}

//=========================================================
// One time initialization for GPIO or MCU Temp/Vbat
int meadow_adc_check_first_func_call()
{
  static bool firstCallFirstTime = false;

  if(! firstCallFirstTime)
  {
    firstCallFirstTime = true;
    
    // Global initiation
    _hardwareConfigDone = false;
    _meadowAdcUserInit = false;
    _doubVrefInCal = 0.0;

    // Signaling semaphore for GPIO conversion
    sem_init(&_gpioDoneSem, 0, 0);
    sem_setprotocol(&_gpioDoneSem, SEM_PRIO_NONE);

    // Signaling semaphore for Vbat and Temperature conversion
    sem_init(&_injectionDoneSem, 0, 0);
    sem_setprotocol(&_injectionDoneSem, SEM_PRIO_NONE);

    // Do before configuration
    _dmaAdcBuf = kmm_malloc(MEADOW_ADC_MAX_DMA_BUFFER_SIZE);
    if(_dmaAdcBuf == NULL)
    {
      syslog(LOG_ERR, "%s@%d-Error:Buffer space not available\n",
                  __FILE__, __LINE__);
      return -ENOMEM;   // Out of memory
    }

    memset(_dmaAdcBuf, 0, MEADOW_ADC_MAX_DMA_BUFFER_SIZE);
    _dmaHandle = NULL;
  }
  return OK;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/
// This function can handle as few as 1 GPIO and as many as 16 GPIOs with a
// buffer with space for all 16.
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount,
          double *resultBuffer)
{
  int ret;
  uint32_t gpioListOff;
  uint32_t mapOff = 0;

  ret = meadow_adc_check_first_func_call();
  if(ret < 0)
  {
    syslog(LOG_ERR, "Meadow adc config failed. ret:%d\n", ret);
    return ret;
  }

#if defined CONFIG_ADC_TESTS
  syslog(1, "meadow_adc_configure() gpioCount:%lu, resultBuffer:%p\n",
            gpioCount, resultBuffer);
#endif

  // Check for valid and reasonable input parameters
  if(_meadowAdcUserInit)
  {
    syslog(LOG_ERR, "%s@%d-Error:There is already an active configuration.\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  if(resultBuffer == NULL)
  {
    syslog(1, "%s@%d-Error:resultBuffer is NULL\n",
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
      syslog(LOG_INFO, "GPIO:0x%02x (P%c%d) not connected to ADC\n", gpioList[gpioListOff],
                  (gpioList[gpioListOff] >> 4) + 'A', gpioList[gpioListOff] & 0x0f);
      return -EINVAL;   // Invalid argument
    }
  }

  //----------------------------------------------
  // Passed the tests so save needed user parameters
  _userGpioXferCount = gpioCount;
  _userVoltageResultBuf = resultBuffer;
  _meadowAdcUserInit = true;

  // There are 3 initialization steps to fully initialize the ADC for analog
  // GPIO data conversion. The first of these can be done at an earlier time
  // as it's needed for reading the internal MCU temperature and battery
  // voltage.
  if(!_hardwareConfigDone)
  {
    ret = meadow_adc_hardware_initialize();
    if(ret < 0)
    {
      syslog(LOG_ERR, "Initialization failed. ret:%d\n", ret);
      return ret;
    }
  }

  ret = meadow_adc_config_sequence_regs(gpioCount, gpioList);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Initialization failed. ret:%d\n", ret);
    return ret;
  }

  meadow_adc_dma_initialize(_dmaAdcBuf, _userGpioXferCount);

  return OK;
}

//=========================================================
// This public function will return the values for Vbat and Vtemp from the
// internal STM32F7 chip. The internal sersors have calibration values written
// into the chip at the time of manufacturing. This function will use these
// values to provide the most accurate possible information to the caller.
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

  ret = meadow_adc_check_first_func_call();
  if(ret < 0)
  {
    syslog(LOG_ERR, "Reading Temp/Vbat failed. ret:%d\n", ret);
    return ret;
  }

  // Has the hardware configuration been done already?
  if(!_hardwareConfigDone)
  {
    // FWIW - meadow_adc_hardware_initialize() sets up the hardware but also
    // calls meadow_adc_read_injected_temp_vref(), internally, so the
    // following call is a duplicate, but only once.
    ret = meadow_adc_hardware_initialize();
    if(ret < 0)
    {
      syslog(LOG_ERR, "Reading Temp/Vbat failed. ret:%d\n", ret);
      return ret;
    }
  }

  // This must be before reading Vbat because it reads and sets _doubVrefInCal
  // which is used for both temperature and voltage calculations
  ret = meadow_adc_read_injected_temp_vref(&adcTempReading, &adcVrefInCal);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Temperature/Vref read error, ret:%d\n", ret);
    return ret;
  }

  // From Data Sheet (not Ref Man), temperatures calibration values have been
  // read at 30 and 110 degrees celsius. 
  double calAtDegC30_t1  = 30.0;     // Calibration temperature 1 in DegC
  double calAtDegC110_t2 = 110.0;    // Calibration temperature 2 in DegC
  // Get the STM factory calibration values stored in these registers.
  double calRefVal_CAL  = (double) getreg16(MEADOW_ADC_VREFINT_CAL_ADDR);
  double tempCal1_TEMP1 = (double) getreg16(MEADOW_ADC_TEMPSENSOR_CAL30_ADDR);
  double tempCal2_TEMP2 = (double) getreg16(MEADOW_ADC_TEMPSENSOR_CAL110_ADDR);

  // The adc temperature measurement value (0-4095) needs to be converted to
  // a temperature value. To do the best conversion the information in the
  // Data Sheet needs to be incorporated in the math.
  // The following equation came from http://efton.sk/STM32/STM32_VREF.pdf.
  *tempValue = calAtDegC30_t1 + (calAtDegC110_t2 - calAtDegC30_t1) * \
            ((double)adcTempReading * calRefVal_CAL - \
            tempCal1_TEMP1 * _doubVrefInCal) / \
            (_doubVrefInCal * (tempCal2_TEMP2 - tempCal1_TEMP1));

// // Peter - HERE (--) This is for TESTING ONLY
//   double voltageBefore;
//   double voltageAfter;
//   uint16_t adcBattery;
//   ret = meadow_adc_read_injected_vbat(&adcBattery);
//   if(ret < 0)
//   {
//     syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
//     return ret;
//   }

//   meadow_adc_convert_adc_to_voltage(adcBattery, &voltageAfter);
//   voltageAfter *= voltageAfter;
//   meadow_adc_convert_adc_to_voltage(adcBattery * 4, &voltageBefore);

//   syslog(1, "-->> adc:%u, voltageBefore:%.3f, voltageAfter:%.3f\n", adcBattery, voltageBefore, voltageAfter);
// // Peter - HERE (--)
  
  // Now do the same for the battery voltage and internal CAL reference
  uint16_t adcBatteryReading;
  ret = meadow_adc_read_injected_vbat(&adcBatteryReading);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Vbat Conversion Error. ret:%d\n", ret);
    return ret;
  }
  syslog(1, "-->> adc BatteryReading:%u, x4:%u\n", adcBatteryReading, adcBatteryReading * 4);

  // Convert the Vbat reading into a voltage value
  // Note:Per Ref Man section 15.11 the Battery voltage read is VBAT/4
  // We must multiple before conversion to get the correct value.
  ret = meadow_adc_convert_adc_to_voltage(adcBatteryReading * 4, batteryVoltage);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%04d-Conversion Cleanup Error. ret:%d\n", ret);
    return ret;
  }

  return OK;
}

//================================================================
// Calling this function will free all the allocations and configurations
// that the current active configuration has changed.
int meadow_adc_unconfigure_active_config()
{
  uint32_t regval;
  
  // Turn off ADC
  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_ADON;
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  // Stop DMA
  regval  = getreg32(STM32_DMA2_S0CR);
  regval &= ~DMA_SCR_EN;
  putreg32(regval, STM32_DMA2_S0CR);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);
  regval &= ~ADC_CR1_SCAN;           // 1=Scan mode (Scans channels in ADC_SQRx registers)
  regval &= ~ADC_CR1_EOCIE;          // 1=Enable ADC interrupt for EOC (not with DMA)
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET);

  regval = getreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);
  regval &= ~ADC_CR2_DMA;          // 0=Disable DMA
  putreg32(regval, STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET);

  regval = getreg32(STM32_ADC1_SQR1);
  regval &= ADC_SQR1_RESERVED;   // Clear all SQR Bits
  putreg32(regval, STM32_ADC1_SQR1);

  regval = getreg32(STM32_ADC1_SQR2);
  regval &= ADC_SQR2_RESERVED;   // Clear all SQR Bits
  putreg32(regval, STM32_ADC1_SQR2);

  regval = getreg32(STM32_ADC1_SQR3);
  regval &= ADC_SQR3_RESERVED;   // Clear all SQR Bits
  putreg32(regval, STM32_ADC1_SQR3);

  regval = getreg32(STM32_ADC_CCR);
  regval &= ~ADC_CCR_TSVREFE;       // 0=disable temperature sensor channel
  regval &= ~ADC_CCR_VBATE;         // 0=disable vbat channel
  regval &= ~ADC_CCR_ADCPRE_MASK;   // Clear any bits in ADC prescaler
  regval &= ~ADC_CCR_DMA_MASK;      // Clear any bits in DMA mode (multi-ADC mode only) 
  regval &= ~ADC_CCR_DELAY_MASK;    // 0000=5*Tadcclk (only used for dual/triple)
  regval &= ~ADC_CCR_MULTI_MASK;    // Clear any bits
  regval |= ADC_CCR_MULTI_NONE;     // 00000=Independent mode
  putreg32(regval, STM32_ADC_CCR);

  // The active configuration must have an active DMA handle from the Nuttx
  // DMA configuration call.
  if(_dmaHandle != NULL)
  {
    stm32_dmastop(_dmaHandle);
    stm32_dmafree(_dmaHandle); 
  }
  _dmaHandle = NULL;

  if(_dmaAdcBuf != NULL)
  {
    free(_dmaAdcBuf);
  }

  _meadowAdcRegInit = false;
  return OK;
}

//=========================================================
// Calling this function will initiate the ADC converstion process. It will
// run until the configured buffer is full. When the buffer is full (or error)
// the calling thread will return to the caller, signifing that the buffer
// is ready for inspection.
int meadow_adc_read_conversions(void)
{
  if(! _meadowAdcUserInit)
  {
    syslog(LOG_ERR, "%s@%d-Error:Meadow ADC not configured.\n",
              __FILE__, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // Since not doing continous conversion we need to start ADC each time
  meadow_adc_restart();

  // Wait for conversion to finish for valid data
  meadow_adc_buffer_takesem(&_gpioDoneSem);

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
  for(int valOff = 0; valOff < _userGpioXferCount; valOff++)
  {
    int ret;
    double convVoltage;

    // _dmaAdcBuf contain the adc output values (0-4095) placed there by DMA.
    //  Convert these values to a voltage and return it to the user.
    ret = meadow_adc_convert_adc_to_voltage(_dmaAdcBuf[valOff], &convVoltage);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%04d-Conversion Cleanup Error. ret:%d\n", ret);
      return ret;
    }

    syslog(1, "%d...Regular ADC:%u, Voltage%.3f\n", valOff + 1, _dmaAdcBuf[valOff], convVoltage);

    // Don't know the cause but, the voltage returned for GPIOs is exactly
    // 0.50 volts too high, from 0 - 3.3.
    _userVoltageResultBuf[valOff] = convVoltage - 0.5;
  }

  return OK;
}
