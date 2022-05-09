/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_calibrate_lsi.c
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

// In STMicro's AN4759 Rev 7 section 2.1.4 there is a brief section on
// adjusting the LSI clock.

// This module calibrates the LSI RC clock. Testing showed that these clocks
// are very inaccurate. This list shows 7 random F7Feature boards.
// 1-100. LSI Average:31017.77Hz (-5.34%,  -3:12/hr), Hi:31118.31, Lo:30878.10
// 2-100. LSI Average:29721.36Hz (-9.30%,  -5:34/hr), Hi:29813.66, Lo:29593.09
// 3-100. LSI Average:29906.54Hz (-8.73%,  -5:14/hr), Hi:30009.38, Lo:29776.67
// 4-100. LSI Average:33934.25Hz (+3.56%,  +2:08/hr), Hi:34030.49, Lo:33814.72
// 5-100. LSI Average:29702.97Hz (-9.35%,  -5:36/hr), Hi:29804.41, Lo:29583.98
// 6-100. LSI Average:29384.76Hz (-10.32%, -6:11/hr), Hi:29493.09, Lo:29259.37
// 7-100. LSI Average:33970.28Hz (+3.67%,  +2:12/hr), Hi:34090.91, Lo:33862.43
// Using a function generator the test results were
//   100. LSI Freq:32775.69, Ave:32775.69 (+0.02%, +0:00/hr), Hi:32775.69, Lo:32764.51

// The goal of this code module is to adjust the RTC so that the variation in
// the LSI clock speed can be compensated for.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdlib.h>
#include <math.h>
#include <syslog.h>

#include <arch/board/board.h>
#include <nuttx/arch.h>
#include <nuttx/kthread.h>
#include "stm32_tim.h"
#include <meadow/hcom_shared_common.h>

#include "pwrmgmt_local.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
// Diagnostic only
#define USE_MEADOW_DEBUG_HELPERS
// #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
#include "stm32_gpio.h"
#endif  // #if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// This implementation only supports using Timer 5 to measure LSI frequency
// because the LSI clock can only be connected to timer 5's channel 4 input.

// 96MHz is top speed for Timer 5 assuming an 192MHz MPU clock speed
#define PWRMGMT_LSI_TIMER_5_BASE_CLOCK_FREQ (96000000)  // 96MHz
#define PWRMGMT_LSI_CAL_TARGET_FREQUENCY (32768.00)     // Perfect clock input freq

/************************************************************************************
 * Private Data
 ************************************************************************************/

volatile uint32_t _prevCount;
volatile uint32_t _elapsedCount;
static int _pwrmgmt_lsi_cal_thread_id;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int pwrmgmt_lsi_cal_init_timer_5(void);
static int pwrmgmt_lsi_cal_create_calc_thread(void);
static void *pwrmgmt_lsi_calc_thread_func(int argc, char *argv[]);
static int pwrmgmt_lsi_cal_find_lsi_clock_freq(double *lsiAvgFreq);
static int pwrmgmt_lsi_cal_find_pre_scaler_values(double lsiAvgFreq, uint8_t *PreDivA, uint16_t *PreDivS);

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int pwrmgmt_lsi_cal_isr_lsi_clock(int irq, void *context, void *arg)
{  
  uint16_t timStatusReg = getreg16(STM32_TIM5_SR);

  // Get interrupt on  rising edge
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    uint32_t currentCount = getreg32(STM32_TIM5_CCR4);

    // Roll over? If it did ignore data
    if(currentCount > _prevCount)
    {
      // No, didn't roll over
      _elapsedCount = currentCount - _prevCount;
    }

    _prevCount = currentCount;
  }

  putreg16(timStatusReg, STM32_TIM5_SR);
  return OK;
}

//=============================================================
static void pwrmgmt_lsi_cal_timer_5_enable(void)
{
  uint16_t cr1Val = getreg16(STM32_TIM5_CR1);
  cr1Val |= GTIM_CR1_CEN;
  
  uint16_t egrVal = getreg16(STM32_TIM5_EGR);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, STM32_TIM5_EGR);
  putreg16(cr1Val, STM32_TIM5_CR1);
}

//=============================================================
static void pwrmgmt_lsi_cal_timer_5_disable(void)
{
  uint16_t regval = getreg16(STM32_TIM5_CR1);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, STM32_TIM5_CR1);
}

//====================================================================
// This function is called to use the LSI clock for keeping the RTC hardware
// running. This includes calibrating the RTC to match the LSIs frequency
// and configuring the STM32F777 to use this clock for time keeping while
// power management has reduced the power usage
int pwrmgmt_lsi_cal_use_lsi_for_clock(void)
{
  int ret;

  // Initialized the STM32F7 timer itself. This call will initialize and enable
  // the timer which will immediately start doing the LSI measurements.
  ret = pwrmgmt_lsi_cal_init_timer_5();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow lsi calib setup failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  // THIS IS TEMPORARY UNTIL THIS CODE IS PUT INTO USE
  // Create a thread to do the work
  ret = pwrmgmt_lsi_cal_create_calc_thread();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow lsi calib run failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }
  
  return ret;
}

//=====================================================================
// THIS IS TEMPORARY UNTIL THIS CODE IS PUT INTO USE
int pwrmgmt_lsi_cal_create_calc_thread()
{
  // Create a thread to use for experimenting
  _pwrmgmt_lsi_cal_thread_id = kthread_create(PWRMGMT_CAL_LSI_THREAD_NAME,
                                  PWRMGMT_CAL_LSI_THREAD_PRIORITY,
                                  PWRMGMT_CAL_LSI_THREAD_STACKSIZE,
                                  (main_t) pwrmgmt_lsi_calc_thread_func,
                                  (char *const *) NULL);
  if (_pwrmgmt_lsi_cal_thread_id <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              __FILE__, __LINE__, PWRMGMT_CAL_LSI_THREAD_NAME);
    return -ENOEXEC;
  }

  return OK;
}

//========================================================
// THIS IS TEMPORARY UNTIL THIS CODE IS PUT INTO USE
// New thread for running the LSI setup.
void *pwrmgmt_lsi_calc_thread_func(int argc, char *argv[])
{
  int ret;
  double lsiAvgFreq;
  uint8_t PreDivA;
  uint16_t PreDivS;

  ret = pwrmgmt_lsi_cal_find_lsi_clock_freq(&lsiAvgFreq);
  if(ret < 0)
  {
    syslog(LOG_ERR, "LSI clock average error\n");
    return NULL;
  }

  // Stop using Timer 5
  up_disable_irq(STM32_IRQ_TIM5);

  pwrmgmt_lsi_cal_timer_5_disable();

  // We have the LSI frequency needed to proceed with the calibration
  ret = pwrmgmt_lsi_cal_find_pre_scaler_values(lsiAvgFreq, &PreDivA, &PreDivS);
  if(ret < 0)
  {
    syslog(LOG_ERR, "LSI clock pre-scaler calc error\n");
    sleep(1);
    return NULL;
  }
  
  // We have the prescaler values needed to do the calibration
  syslog(1, "Pre-scaler values are PreDivA:%u, PreDivS:%u, product:%u\n",
            PreDivA, PreDivS, PreDivA * PreDivS);

  // The register where these values ar used is the RTC_PRER register.
  

  return NULL;
}

//=============================================================
// Low-level register setup for Timer 5
int pwrmgmt_lsi_cal_init_timer_5(void)
{
  int ret;
  uint16_t regVal16;
  uint32_t regVal32;

  // Before starting disable capture/control for all channels. Ref Man (26.4.7 at
  // end) "Note: CC1S bits are writable only when the channel is OFF (i.e.
  // CC1E = 0 in TIMx_CCER)." 
  regVal16 = getreg16(STM32_TIM5_CCER);
  regVal16 &= 0xeeee;   // e = 1110, clear CCxE bits 0, 4, 8 & 12
  putreg16(regVal16, STM32_TIM5_CCER);
  
  // Timer5 Option Register is used to connect GPIO (default), LSI, LSE or 
  // RTC wakeup interrupt to Timer 5 Channel 4
  regVal16 = getreg16(STM32_TIM5_OR);
  regVal16 &= ~TIM5_OR_TI4_RMP_MASK;  // Clear the bits
  regVal16 |= TIM5_OR_TI4_LSI;        // Use LSI to measure its frequency
  putreg16(regVal16, STM32_TIM5_OR);
  
  // 01: CC4 channel mapped on TI4
  regVal32 = getreg32(STM32_TIM5_CCMR2);
  regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
  putreg32(regVal32, STM32_TIM5_CCMR2);
  
  // GTIM_CCER_CC4NP (bit 15) & GTIM_CCER_CC4P (bit 13)
  // Set 00 rising only, 01 falling, 11 both rising and falling
  // Clear both bits for rising only.
  regVal16 = getreg16(STM32_TIM5_CCER);
  regVal16 &= ~(GTIM_CCER_CC4P | GTIM_CCER_CC4NP);
  putreg16(regVal16, STM32_TIM5_CCER);
  
  // Setup the clock enable
  modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIM5EN);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // Timer 5 (STM32_APB1_TIM5_CLKIN).
  uint16_t prescaler = 0;
  putreg16(prescaler, STM32_TIM5_PSC);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = 0xffffffff;
  putreg32(maxARRValue, STM32_TIM5_ARR);

  uint16_t regval = getreg16(STM32_TIM5_CR1);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, STM32_TIM5_CR1);

  // External Clock Enable (ECE bit 14) needs to be diabled.
  // as does Slave Mode (SMS bit 16, DISAB 3:0)
  regVal32 = getreg32(STM32_TIM5_SMCR);
  regVal32 &= ~(GTIM_SMCR_ECE | GTIM_SMCR_DISAB | GTIM_SMCR_SMS);
  putreg32(regVal32, STM32_TIM5_SMCR);

  // Enable the timer input capture, which was disabled earlier
  // but only for channel 4
  regVal16 = getreg16(STM32_TIM5_CCER);
  regVal16 |= ( GTIM_CCER_CC4E); // bit 12
  putreg16(regVal16, STM32_TIM5_CCER);

  // Clear all interrupt sources and set the ones we need in the DMA/Interrupt
  // enable register (DIER). Note: Advanced timers 1 & 8 add ATIM_DIER_COMIE,
  // ATIM_DIER_BIE and ATIM_DIER_COMDE
  modifyreg16(STM32_TIM5_BASE + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          GTIM_DIER_CC4IE);

  // All interupts are handled by same isr
  ret = irq_attach(STM32_IRQ_TIM5, pwrmgmt_lsi_cal_isr_lsi_clock, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(STM32_IRQ_TIM5);

  pwrmgmt_lsi_cal_timer_5_enable();

  // Turn on LSI clock
  // Enable the Internal Low-Speed (LSI) RC Oscillator by setting the LSION
  // bit the RCC CSR register.
  modifyreg32(STM32_RCC_CSR, 0, RCC_CSR_LSION);

  // Wait for the internal RC oscillator to be stable.
  while ((getreg32(STM32_RCC_CSR) & RCC_CSR_LSIRDY) == 0);

  return OK;
}

//================================================================
// Test code for measuring the frequency of the LSI clock
int pwrmgmt_lsi_cal_find_lsi_clock_freq(double *lsiAvgFreq)
{
  static uint32_t freqCount;
  static uint32_t isrCount = 0;
  static uint64_t totCount = 0;

  for (freqCount = 0; freqCount < 100; freqCount++)
  {
    // Pause a moment
    usleep(1000);

    // Filter out obvious bad values. Note this loop will not exit if there
    // are no ISR interrupts.
    for ( ; ; )
    {
      // We should not have the same count as last time
      if(isrCount != _elapsedCount)
      {
        // Get a valid ISR count value
        isrCount = _elapsedCount;
        break;
      }
    }

    totCount += isrCount;
  }

  // Since we know the clock frequency of Timer 5 and the number of counts
  // between the LSI clock's rising edges we have everything we need.
  *lsiAvgFreq = (double)(PWRMGMT_LSI_TIMER_5_BASE_CLOCK_FREQ) / (double) (totCount/freqCount);
  
  // syslog(1, "LSI Average Frequency:%06.03f\n", *lsiAvgFreq);

  return OK;
}

//================================================================
// In STMicro's AN4759 Rev 7 section 2.1.4 there is a brief section on
// adjusting the RTC clock to match the input frequency of the reference
// clock. There are 2 clock dividers and the goal is to have the product
// of these 2 dividers equal the LSI frequency. Doing this will result in
// a RTC clock frequency of 1 Hz.
int pwrmgmt_lsi_cal_find_pre_scaler_values(double lsiAvgFreq, uint8_t *PreDivA, uint16_t *PreDivS)
{
  // int ret;
  uint8_t preDivA;
  uint16_t initialPreDivS;
  uint16_t chkOffset;
  uint16_t chkDiff = 0xffff;
  uint16_t smallestDiff = 0xffff;
  uint16_t smallestDivA = 127;
  uint16_t smallestDivS = 255;
  uint16_t lsiTargetFreq = (uint32_t)round(lsiAvgFreq);

  syslog(1, "Entered Find Pre-scaler value() lsiAvgFreq:%.3f as uint32:%lu\n",
          lsiAvgFreq, lsiTargetFreq);

  // LSI frequency just right?
  if(lsiAvgFreq == (double)PWRMGMT_LSI_CAL_TARGET_FREQUENCY)
  {
    // Use the default values
    syslog(1, "**Perfect:%u**\n", lsiAvgFreq);
    *PreDivA = 127;
    *PreDivS = 255;
    return OK;
  }

  // We'll solve this with 2 loops. The outer loop counts 127 (PREDIVA) and
  // the inner loop for the PREDIVS, which is typically around 512. The goal
  // is to find the largest PREDIVA, that when multiplied by the PREDIVS
  // value produces a value as close as possible to the lsiAvgFreq. This will
  // result in the RTC receiving, as close as possible, the 1Hz clock needed.
  for (preDivA = 127; preDivA > 1; preDivA--)
  {    
    // To be thorough check above and below our initial guess too
    initialPreDivS = (lsiTargetFreq / preDivA) + 2;   // Start above

    // Is this closest to our average frequency target (i.e. smallest
    // difference)?
    for (chkOffset = 0; chkOffset <  4; chkOffset++)
    {  
      chkDiff = (preDivA * initialPreDivS) - lsiTargetFreq;
      if(chkDiff < smallestDiff)
      {
        syslog(1, "*** New smallest - chkOffset:%u, A(%03u) * S(%04u) = %05lu (dif:%03u)\n",
                chkOffset, preDivA, initialPreDivS, preDivA * initialPreDivS, chkDiff);

        // Save values of the new smallest error
        smallestDiff = chkDiff;
        smallestDivA = preDivA;
        smallestDivS = initialPreDivS;

        // Did we find a perfect combination (i.e. prime factors)?
        if(smallestDiff == 0)
          break;
      }

      initialPreDivS--;   // Move down for this PREDIVA
    }

    if(smallestDiff == 0)
      break;
  }

  syslog(1, "Done-smallestDivA:%lu, smallestDivS:%lu, product:%lu, target:%.3f\n",
          smallestDivA,
          smallestDivS,
          smallestDivA * smallestDivS,
          lsiAvgFreq);
  
  *PreDivA = (uint8_t)smallestDivA;
  *PreDivS = (uint16_t)(smallestDivS);
  return OK;

}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
