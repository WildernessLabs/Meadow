/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/lsi/clk_freq_measure.c
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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

// This implementation only support Timer 5 because the LSI clock can be
// conneccted to timer 5's channel 4 input via register bits.
// This code is not flexable it ONLY does one job, measure LSI frequency.
// This code began as lsi_clock.c and has been modified.
//
// 1. Enable the TIM5 timer and configure channel4 in Input capture mode.
// 2. This bit is set the TI4_RMP bits.
// 2a in the TIM5_OR register to 0x01 to connect the LSI clock internally
// to TIM5 channel4 input capture.
// 3. Measure the LSI clock frequency using the TIM5 capture/compare 4 event or interrupt.
// 4. Use the measured LSI frequency to update the prescaler of the RTC depending on the
// desired time base and/or to compute the IWDG timeout.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "meadow_timers.h"

#include <stdlib.h>
#include <math.h>

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)

#if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0

//===================================================================
// 96MHz is top speed. Since the iterrupts are based on leading and falling
// edges the only reason for slowing the clock would be to slow down the
// number of overflows for a 16-bit timer.
#define MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ (96000000) // 96MHz target frequency

// The Nuttx configuration can be left using the HSE clock while doing
// this testing. This code will turn-on the LSI clock and set everything
// as needed. System Type -> RTC Configuration -> (*) HSE clock

/****************************************************************************
 * Private Data
 ****************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_init_lsi_clock(int timerNumber);
volatile uint32_t _prevCount;
volatile uint32_t _elapsedCount;

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR is used to count the number of HSE clock between each LSI clock
static int meadow_timer_isr_lsi_clock(int irq, void *context, void *arg)
{
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  
  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  // Get interrupt on rising edge
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    uint32_t currentCount = getreg32(timerBase + STM32_GTIM_CCR4_OFFSET);

    // Roll over? If it did ignore data
    if(currentCount > _prevCount)
    {
      // No. Didn't roll over
      _elapsedCount = currentCount - _prevCount;
    }

    _prevCount = currentCount;
  }

  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_lsi_clock(struct timerConfig_s timerConfig)
{
  int ret;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerConfig.timerNumber);

  if(timerInfo == NULL)
  {
    // syslog(LOG_ERR, "meadow_timer_get_timer_info_pointer() returned NULL\n");
    return -ENXIO;
  }

#if MEADOW_MEASURE_LSI_CLOCK_NOT_THE_GPIO_PA3_INPUT == 0
  // Initialize GPIO for this test using TIM5 channel GPIO
  uint32_t configInfo;
  int i = 3;
  {
    uint32_t afPortPin = meadow_timer_get_ver_based_gpio_chan(timerConfig.timerNumber, i);
    if(afPortPin != 0x00ff)
    {
      // syslog(LOG_INFO, "Initializing GPIO for TIM5 CH4\n");
      configInfo = MEADOW_TIMER_GPIO_CONST | afPortPin;
      stm32_configgpio(configInfo);
    }
    else
    {
      // syslog(LOG_WARNING, "meadow_timer_setup_rc_servo_decode() no gpio at offset:%d\n", i);
      configInfo = MEADOW_TIMER_BAD_GPIO_VALUE;
      return -1;
    }
  }
#endif

  // Initialized the timer itself
  ret = meadow_timer_init_lsi_clock(timerConfig.timerNumber);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow lsi clock failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//=============================================================
// RC Servo Decode. The signal to be decoded is a pulse between 1 ms and 2 ms.
// These pulses are sent at a 50/per second rate (50 Hz).
int meadow_timer_init_lsi_clock(int timerNumber)
{
  int ret;
  uint16_t regVal16;
  uint32_t regVal32;

#if MEADOW_MEASURE_LSI_CLOCK_NOT_THE_GPIO_PA3_INPUT == 0
#warning Measuring the frequency input for A02 (PA3)
#endif

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  uint32_t timerBase = timerInfo->timerBase;

  // Before starting disable capture/control for all channels. Ref Man (26.4.7 at
  // end) "Note: CC1S bits are writable only when the channel is OFF (i.e.
  // CC1E = 0 in TIMx_CCER)." 
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xeeee;   // e = 1110, clear CCxE bits 0, 4, 8 & 12
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // --------------------------------------------------------------------
  // Timer5 Option Register is used to connect GPIO (default), LSI, LSE or 
  // RTC wakeup interrupt to Timer 5 Channel 4
  regVal16 = getreg16(timerBase + STM32_GTIM_OR_OFFSET);
  regVal16 &= ~TIM5_OR_TI4_RMP_MASK;  // Clear the bits

#if MEADOW_MEASURE_LSI_CLOCK_NOT_THE_GPIO_PA3_INPUT > 0
  regVal16 |= TIM5_OR_TI4_LSI;        // Use LSI to measure its frequency
#else
  regVal16 |= TIM5_OR_TI4_GPIO;       // Use GPIO to measure its frequency
#endif

  putreg16(regVal16, timerBase + STM32_GTIM_OR_OFFSET);
  // --------------------------------------------------------------------
  
  // 01: CC4 channel mapped on TI4
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
  regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);
  
  // GTIM_CCER_CC4NP (bit 15) & GTIM_CCER_CC4P (bit 13)
  // Set 00 rising only, 01 falling, 11 both rising and falling
  // Clear both bits for rising only.
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= ~(GTIM_CCER_CC4P | GTIM_CCER_CC4NP);
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // Find proper pre-scaler value so all lsi clock timers run at the same speed
  uint16_t prescaler = (meadow_timer_get_max_clock(timerInfo)/ \
            MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == \
            MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // External Clock Enable (ECE bit 14) needs to be diabled.
  // as does Slave Mode (SMS bit 16, DISAB 3:0)
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= ~(GTIM_SMCR_ECE | GTIM_SMCR_DISAB | GTIM_SMCR_SMS);
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // Enable the timer input capture, which was disabled earlier
  // but only for channel 4
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 |= ( GTIM_CCER_CC4E); // bit 12
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Clear all interrupt sources and set the ones we need in the DMA/Interrupt
  // enable register (DIER). Note: Advanced timers 1 & 8 add ATIM_DIER_COMIE,
  // ATIM_DIER_BIE and ATIM_DIER_COMDE
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          GTIM_DIER_CC4IE);

  // All interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_lsi_clock, timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(timerInfo->timerIrqVec);

  meadow_timer_enable(timerBase);

#if MEADOW_MEASURE_LSI_CLOCK_NOT_THE_GPIO_PA3_INPUT > 0
  // Turn on LSI clock
  
  /* Enable the Internal Low-Speed (LSI) RC Oscillator by setting the LSION
   * bit the RCC CSR register.
   */

  modifyreg32(STM32_RCC_CSR, 0, RCC_CSR_LSION);

  /* Wait for the internal RC 40 kHz oscillator to be stable. */

  while ((getreg32(STM32_RCC_CSR) & RCC_CSR_LSIRDY) == 0);
#endif

  return OK;
}

//================================================================
// Test code for measuring the frequency of the LSI clock
// This is where the frequency is calculated and displayed
int meadow_timer_test_lsi_clock(int timerNumber)
{
  static uint32_t freqCount = 0;
  static uint32_t lsiCountHigh = 1;
  static uint32_t lsiCountLow = 1000000;
  static uint32_t prevElapsed = 0;
  static uint64_t totCount = 0;
  double targetFreq = 32768.0;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  // Filter out obvious values.
  // Note this code will cause the output to stop when there's no input
  for ( ; ; )
  {
    if(prevElapsed != _elapsedCount)
    {
      prevElapsed = _elapsedCount;
      break;
    }
  }

  // Find lowest and highest frequency values
  if(prevElapsed < lsiCountLow)
    lsiCountLow = prevElapsed;

  if(prevElapsed > lsiCountHigh)
    lsiCountHigh = prevElapsed;

  freqCount++;
  totCount += prevElapsed;
  
  // Since we know the clock frequency of Timer 5 and the number of counts
  // between the LSI clock's rising and falling edges we can determine
  // everything we need.
  double lsiFreq = (double)(MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ) / (double)_elapsedCount;

  // Lowest count becomes highest frequency
  double lsiHigh = (double)(MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ) / (double)lsiCountLow;
  double lsiLow = (double)(MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ) / (double)lsiCountHigh;
  double lsiAvg = (double)(MEADOW_TIMER_LSI_FREQ_MEASURE_CLK_FREQ) / (double) (totCount/freqCount);
  double lsiPercentErr = (lsiAvg - targetFreq)/targetFreq;

  double lsiErrTotSec = 3600 * lsiPercentErr;
  int lsiErrMin;
  if(lsiPercentErr < 0)
    lsiErrMin = ceil(lsiErrTotSec / 60.0);
  else
    lsiErrMin = floor(lsiErrTotSec / 60.0);
  
  int lsiErrSec = round(abs(lsiErrTotSec - (lsiErrMin * 60)));

  // syslog(LOG_INFO,
  //  "%03u. LSI Freq:%06.2f, Ave:%06.02f (%+03.2f%%, %+d:%02d/hr), Hi:%06.2f, Lo:%06.2f\n",
  // freqCount, lsiFreq, lsiAvg, lsiPercentErr * 100,
  // lsiErrMin, lsiErrSec, lsiHigh, lsiLow);
  
  return OK;
}

#endif    // #if MEADOW_MEASURE_LSI_CLOCK_INCLUDE_IN_BUILD > 0

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
