/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_calib_lsi_clock.c
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
// adjusting the source clock.
//
// This module first calibrates the LSI RC clock so it reasonably generates
// the 1 Hz clock needed by the F7's RTC hardware.

// Testing showed that these clocks
// are very inaccurate. This list shows 7 random F7Feature boards.
// 1-100 LSI Average:31017.77Hz (-5.34%,  -3:12/hr), Hi:31118.31, Lo:30878.10
// 2-100 LSI Average:29721.36Hz (-9.30%,  -5:34/hr), Hi:29813.66, Lo:29593.09
// 3-100 LSI Average:29906.54Hz (-8.73%,  -5:14/hr), Hi:30009.38, Lo:29776.67
// 4-100 LSI Average:33934.25Hz (+3.56%,  +2:08/hr), Hi:34030.49, Lo:33814.72
// 5-100 LSI Average:29702.97Hz (-9.35%,  -5:36/hr), Hi:29804.41, Lo:29583.98
// 6-100 LSI Average:29384.76Hz (-10.32%, -6:11/hr), Hi:29493.09, Lo:29259.37
// 7-100 LSI Average:33970.28Hz (+3.67%,  +2:12/hr), Hi:34090.91, Lo:33862.43
// Using a function generator the test results were
//   100 LSI Average:32775.69Hz (+0.02%, +0:00/hr), Hi:32775.69, Lo:32764.51
// Proving that the frequency measurement code was working properly.

// The goal of this module is to adjust the RTC so that the variation in
// the LSI clock speed can be compensated for.

// Note: the underlying STM32_rtc.c driver doesn't support CONFIG_RTC_HIRES

// Note: There's a lot of useful code in /arch/arm/src/stm32f7/stm32_rtc.c.
// However, the public functions are limited to common things like setting
// time and setting alarms. The type of low-level functionality needed here
// is not available. Some of the stm32_rtc.c code has be duplicated here.
//

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
#include "stm32_pwr.h"
#include "stm32_rtc.h"
#include "stm32_exti.h"

#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_common.h"

#include "pwrmgmt_local.h"
#include <meadow/hcom_bbreg_defn.h>

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// The timer 5, channel 4 input is the only input that can be connected to LSI
// clock.
// STM32_APB1_TIM5_CLKIN is 96MHz assuming 192MHz MPU clock speed
#define PWRMGMT_CLK_TIMER_5_BASE_CLOCK_FREQ (STM32_APB1_TIM5_CLKIN)
#define PWRMGMT_CLK_CAL_TARGET_FREQUENCY (32768.00)     // Ideal clock source freq
#define PWRMGMT_CLK_CAL_MEASURE_CLK_COUNT (100)         // Test LSI freq x times
#define PWRMGMT_CLK_CAL_MEASURE_CLK_DELAY (5000)        // Wait x usec between tests

/************************************************************************************
 * Private Data
 ************************************************************************************/
static char *thisFile = __FILE__;

volatile uint32_t _prevISRCount;
volatile uint32_t _elapsedCount;
static int _pwrmgmt_lsi_calc_thread_id;

// These variables need to be shared 
static uint32_t _lsiRtcPrer;    // Set here, read on to switch

#if PWRMGMT_RTC_SOURCE_CLK_CHANGED_TESTING > 0
static bool _dbgClkSwitched;    // Set on both sides, read here
#endif

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int pwrmgmt_lsi_init_timer_5_for_measuring(void);
static int pwrmgmt_create_lsi_calc_thread(void);
static void *pwrmgmt_lsi_calc_prep_thread_func(int argc, char *argv[]);
static int pwrmgmt_lsi_calculate_lsi_clock_freq(double *lsiMeasuredFreq);
static int pwrmgmt_lsi_calc_rtc_prescaler_values(double lsiMeasuredFreq, uint8_t *PreDivA, uint16_t *PreDivS);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//=============================================================
// This function returns the LSI pre-scaler values set here
uint32_t pwrmgmt_get_lsi_calib_rtc_clk_value()
{
  return _lsiRtcPrer;
}

//=============================================================
#if PWRMGMT_RTC_SOURCE_CLK_CHANGED_TESTING > 0
// This allows the test code to be notified when the RTC source clock has
// been changed.
void pwrmgmt_rtc_source_clk_changed_flag(bool dbgClkSwitched)
{
  _dbgClkSwitched = dbgClkSwitched;
}
#endif

//=============================================================
// This ISR is only used to measure the LSI frequency for a moment
static int pwrmgmt_lsi_use_isr_lsi_clock(int irq, void *context, void *arg)
{  
  uint16_t timStatusReg = getreg16(STM32_TIM5_SR);

  // Get interrupt on rising edge
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    uint32_t currentCount = getreg32(STM32_TIM5_CCR4);

    // Roll over? If it did ignore data
    if(currentCount > _prevISRCount)
    {
      // No, didn't roll over save count for non-isr processing
      _elapsedCount = currentCount - _prevISRCount;
    }

    _prevISRCount = currentCount;
  }

  putreg16(timStatusReg, STM32_TIM5_SR);
  return OK;
}

//=============================================================
// Enable timer 5 for calibration
static void pwrmgmt_lsi_enable_timer_5(void)
{
  uint16_t cr1Val = getreg16(STM32_TIM5_CR1);
  cr1Val |= GTIM_CR1_CEN;
  
  uint16_t egrVal = getreg16(STM32_TIM5_EGR);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, STM32_TIM5_EGR);
  putreg16(cr1Val, STM32_TIM5_CR1);
}

//=============================================================
// Disable timer 5 for calibration
static void pwrmgmt_lsi_disable_timer_5(void)
{
  uint16_t regval = getreg16(STM32_TIM5_CR1);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, STM32_TIM5_CR1);
}

//====================================================================
// This function is called during startup. It is responsible for finding the
// LSI clock frequency and the needed factors for calibrating the RTC hardware.
// This function creates a thread so the rest of the initialization
// isn't stalled waiting for this to finish. Why not do this on demand? Because
// this requires Timer 5 to be setup a special way. And at runtime timer 5 has
// other responsibilites.
int pwrmgmt_init_lsi_calib(void)
{
  int ret;

  _lsiRtcPrer = 0;

#if PWRMGMT_RTC_SOURCE_CLK_CHANGED_TESTING > 0
  _dbgClkSwitched = false;
#endif

  // This call will initialize and enable timer 5 which is the only one that
  // can be used to measure the LSI's frequency. It will also turn-on the LSI
  // clock.
  ret = pwrmgmt_lsi_init_timer_5_for_measuring();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-LSI init TIM5 failed:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Create a thread to do the calibration work. The results will be stored in
  // a battery backed register.
  ret = pwrmgmt_create_lsi_calc_thread();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-LSI calc freq failed:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
  return ret;
}

//=====================================================================
// Create a thread to do the early initialization
int pwrmgmt_create_lsi_calc_thread()
{
  // Create a thread to use for experimenting
  _pwrmgmt_lsi_calc_thread_id = kthread_create(PWRMGMT_CAL_LSI_THREAD_NAME,
                                  PWRMGMT_CAL_LSI_THREAD_PRIORITY,
                                  PWRMGMT_CAL_LSI_THREAD_STACKSIZE,
                                  (main_t) pwrmgmt_lsi_calc_prep_thread_func,
                                  (char *const *) NULL);
  if (_pwrmgmt_lsi_calc_thread_id <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              thisFile, __LINE__, PWRMGMT_CAL_LSI_THREAD_NAME);
    return -ENOEXEC;
  }

  return OK;
}

//=============================================================
// This thread function will calculate the calibration needed for the LSI
// clocks timing error. It is called before entering the low-power mode.
void *pwrmgmt_lsi_calc_prep_thread_func(int argc, char *argv[])
{
  int ret;
  double lsiMeasuredFreq;
  uint8_t PreDivA = 0;
  uint16_t PreDivS = 0;
  uint32_t rtcPrer;

  // Under some undetermined conditions RTC pre-scaler could become 0x007f00ff
  // while Nuttx is using the HSE clock. A 0x007f00ff is the correct
  // pre-scaler for LSE clock, which we cannot use. Meaning that this
  // pre-scaler value is way wrong! This patch corrects this error.
  rtcPrer = getreg32(STM32_RTC_PRER);
  if(rtcPrer == 0x007f00ff)
  {
    MEADOW_TRACE_DEBUG("At startup detected wrong pre-scaler:0x%08x\n", rtcPrer);

    // Unlock RTC registers
    pwrmgmt_rtc_wprunlock();
    ret = pwrmgmt_rtc_enterinit();
    if(ret == -ETIMEDOUT)
    {
      syslog(LOG_ERR, "%s@%d-STARTUP->RTC INIT state timed out\n", thisFile, __LINE__);
      pwrmgmt_rtc_wprlock();
      return NULL;
    }

    // Hardcoded HSE pre-scaler values.
    rtcPrer = (uint32_t)PWRMGMT_CLK_HSE_DIV_S_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_S_SHIFT |
               (uint32_t)PWRMGMT_CLK_HSE_DIV_A_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_A_SHIFT;
    putreg32(rtcPrer, STM32_RTC_PRER);

    pwrmgmt_rtc_exitinit();
    pwrmgmt_rtc_wprlock();
  }

#if defined(USE_MEADOW_DEBUG_HELPERS)
  uint32_t clkSrc = getreg32(STM32_RCC_BDCR) & RCC_BDCR_RTCSEL_MASK;
  uint32_t rtcPRER = getreg32(STM32_RTC_PRER);
  uint32_t rtcPrerDBG = (uint32_t)PWRMGMT_CLK_HSE_DIV_S_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_S_SHIFT |
                (uint32_t)PWRMGMT_CLK_HSE_DIV_A_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_A_SHIFT;
  MEADOW_TRACE_DEBUG("At startup, clkSrc:0x%08x, rtc pre-scaler:0x%08x and HSE default:0x%08x\n",
            clkSrc, rtcPRER, rtcPrerDBG);
#endif

  ret = pwrmgmt_lsi_calculate_lsi_clock_freq(&lsiMeasuredFreq);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-LSI clock average error\n", thisFile, __LINE__);
    return NULL;
  }

  // Finshed with Timer 5
  up_disable_irq(STM32_IRQ_TIM5);

  pwrmgmt_lsi_disable_timer_5();

  // We have the LSI frequency. Next find the pre-scaler factors that yield
  // the desired divisor needed for this frequency.
  ret = pwrmgmt_lsi_calc_rtc_prescaler_values(lsiMeasuredFreq,
            &PreDivA, &PreDivS);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-LSI clock pre-scaler calc error\n", thisFile, __LINE__);
    return NULL;
  }

  MEADOW_TRACE_DEBUG("FREQ Calc->Correct pre-scaler values are PreDivA:%u(0x%08x), PreDivS:%u(0x%08x), product:%u\n",
            PreDivA, PreDivA, PreDivS, PreDivS, PreDivA * PreDivS);

  // We have the pre-scaler factors needed to do the calibration
  // Convert PreDivA and PreDivS to the 32-bit value that will be written
  // to the RTC_PRER register.
  _lsiRtcPrer = (uint32_t)PreDivS << RTC_PRER_PREDIV_S_SHIFT |
          (uint32_t)PreDivA << RTC_PRER_PREDIV_A_SHIFT;
  
#if PWRMGMT_RTC_SOURCE_CLK_CHANGED_TESTING > 0
  //---------------------------------------------------------------------
  // x999 millisec sleep is closer to a x+1 seconds period
  #define PWRMGMT_CAL_SHOW_STATS_EVERY_mSEC (999)  // msec
  #define PWRMGMT_CAL_SHOW_STATS_EVERY_LOOP (6)
  #define PWRMGMT_CAL_SHOW_NEXT_SECONDS ((PWRMGMT_CAL_SHOW_STATS_EVERY_mSEC + 1) / 1000)

  // If tests are enabled, this thread won't exit when calibration is completed
  struct tm tmNowRtc;
  int nextSec = 0;
  int loopCount = 0;
  int errorCount = 0;
  char *clkName = "";
  uint32_t clkSrc;

  // First switch to the LSI or HSE clock. If neither of these functions are
  // called  then the Nuttx default clock will be used, probably HSE.
  //
  // ret = meadow_pwr_mgmt_use_lsi_for_rtc();
  ret = meadow_pwr_mgmt_use_hse_for_rtc();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Switching to HSE/LSI failed\n", thisFile, __LINE__);
    return NULL;
  }

  // Do first time config
  _dbgClkSwitched = true;

  // Allow the clock to fully start. The LSI clock isn't very reliable and this
  // 1 second delay may help it be a bit more consistant.
  sleep(1);

  // We want to check the seconds to determine if any are skipped or reported
  // twice.
  while(true)
  {
    // The following gets the time (ultimately from the nuttx system timer)
    struct timespec abstime;
    struct tm tmNowOs;
    clock_gettime(CLOCK_REALTIME, &abstime);
    gmtime_r(&abstime.tv_sec, &tmNowOs);

    // Get time from the hardware RTC. Because the /arch/arm/src/stm32f7/stm32_rtc.c
    // driver doesn't allow CONFIG_RTC_HIRES to be configured, clock_gettime()
    // returns the nuttx system timer based time. This is a Nuttx short coming!
    up_rtc_getdatetime(&tmNowRtc);

    if(_dbgClkSwitched)
    {
      clkSrc = getreg32(STM32_RCC_BDCR) & RCC_BDCR_RTCSEL_MASK;
      if(clkSrc == RCC_BDCR_RTCSEL_LSI)
        clkName = "LSI\0";
      else
        clkName = "HSE\0";

      errorCount = 0;
      loopCount = 0;
      nextSec = tmNowRtc.tm_sec;

      _dbgClkSwitched = false;
    }

    loopCount++;

    if(nextSec != tmNowRtc.tm_sec)
    {
      MEADOW_TRACE_DEBUG("Next sec:%03d != tm_sec:%03d\n", nextSec,  tmNowRtc.tm_sec);
      // Ignore first few errors
      if(loopCount > 2)
      {
        errorCount++;
        if(errorCount > 0)
        {
          MEADOW_TRACE_DEBUG("Clock time error #%03d in %03d seconds:%02d Seconds/Error\n",
                  errorCount, loopCount, loopCount/errorCount);
        }
      }
    }

    // Periodically show what's going on
    if((loopCount % PWRMGMT_CAL_SHOW_STATS_EVERY_LOOP) == 0)
    {
      MEADOW_TRACE_DEBUG("(%s) After %03d seconds, Error count:%03d (Sec/Err:%02d) [rtcPrer:0x%08x]\n",
                clkName, loopCount,
                errorCount, loopCount/errorCount,
                getreg32(STM32_RTC_PRER));
    }

    // Show the date & time on every loop
    MEADOW_TRACE_DEBUG("Time check #%03u - %4d-%02d-%02dT%02d:%02d:%02d RTC - %4d-%02d-%02dT%02d:%02d:%02d OS\n",
              loopCount,
              tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
              tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
              tmNowOs.tm_year + 1900, tmNowOs.tm_mon + 1, tmNowOs.tm_mday,
              tmNowOs.tm_hour, tmNowOs.tm_min, tmNowOs.tm_sec);

    // Seconds run from 0 - 59
    nextSec = tmNowRtc.tm_sec + PWRMGMT_CAL_SHOW_NEXT_SECONDS;
    if(nextSec > 59)
      nextSec -= 60;

    usleep(PWRMGMT_CAL_SHOW_STATS_EVERY_mSEC * 1000);
  }
#endif

  return NULL;
}

//=============================================================
// Low-level register setup for Timer 5
int pwrmgmt_lsi_init_timer_5_for_measuring(void)
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

  // Interupts are handled by isr
  ret = irq_attach(STM32_IRQ_TIM5, pwrmgmt_lsi_use_isr_lsi_clock, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          thisFile, __LINE__, ret, errno);
    return ret;
  }

  up_enable_irq(STM32_IRQ_TIM5);

  pwrmgmt_lsi_enable_timer_5();

  // To test enable the Low-Speed Internal (LSI) RC Oscillator by setting
  // the LSION bit the RCC CSR register.
  // Note: this clock runs even when it is not needed. Why? It only draws
  // 0.6 micro amps max. while running.
  modifyreg32(STM32_RCC_CSR, 0, RCC_CSR_LSION);

  // Wait for the internal RC oscillator to become stable
  while ((getreg32(STM32_RCC_CSR) & RCC_CSR_LSIRDY) == 0);

  return OK;
}

//================================================================
// This function will measure the frequency of the LSI clock using the
//  data via Timer5's ISR
int pwrmgmt_lsi_calculate_lsi_clock_freq(double *lsiMeasuredFreq)
{
  static uint32_t freqCount;
  static uint32_t isrCount = 0;
  static uint64_t totCount = 0;

  // Measure the clock frequency x times, waiting y usec
  for (freqCount = 0; freqCount < PWRMGMT_CLK_CAL_MEASURE_CLK_COUNT; freqCount++)
  {
    // Pause a moment
    usleep(PWRMGMT_CLK_CAL_MEASURE_CLK_DELAY);

    // Ignore possibly bad values. Warning: this loop will never exit if there
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

  // Since we know the clock frequency of Timer 5 and the average number of
  // counts between the LSI clock's rising edges we have everything we need.
  *lsiMeasuredFreq = (double)(PWRMGMT_CLK_TIMER_5_BASE_CLOCK_FREQ) / (double) (totCount/freqCount);
  
  MEADOW_TRACE_DEBUG("LSI Average Frequency:%06.03f\n", *lsiMeasuredFreq);

  return OK;
}

//================================================================
// In STMicro's AN4759 Rev 7 section 2.1.4 there is a brief section on
// adjusting the RTC clock to match the input frequency of the reference
// clock, LSI in our case. There are 2 factors needed and the product
// of these 2 factors equal the frequency being measured. Doing this will
// result in a RTC clock frequency of 1 Hz.
// The following code finds these 2 factors, however, the factors needed by
// the MCU's hardware needs both factors to be 1 less than calculated.
int pwrmgmt_lsi_calc_rtc_prescaler_values(double lsiMeasuredFreq,
          uint8_t *PreDivA, uint16_t *PreDivS)
{
  // int ret;
  uint8_t preDivA;
  uint16_t initialPreDivS;
  uint16_t chkOffset;
  uint16_t chkDiff = 0xffff;
  uint16_t smallestDiff = 0xffff;
  uint8_t smallestDivA = 127;
  uint16_t smallestDivS = 255;
  uint16_t lsiTargetFreq = (uint32_t)round(lsiMeasuredFreq);

   MEADOW_TRACE_DEBUG("Entered Find Pre-scaler value() lsiMeasuredFreq:%.3f as uint32:%lu\n",
          lsiMeasuredFreq, lsiTargetFreq);

  // LSI frequency already perfect?
  if(lsiMeasuredFreq == (double)PWRMGMT_CLK_CAL_TARGET_FREQUENCY)
  {
    // Use the default values
     MEADOW_TRACE_DEBUG("**Perfect:%u**\n", lsiMeasuredFreq);
    *PreDivA = 127;
    *PreDivS = 255;
    return OK;
  }

  // We'll solve this with 2 loops. The outer loop counts from 127 (PREDIVA)
  // to 1 and the inner loop for the PREDIVS, which is typically around 512. The
  // goal is to find the largest PREDIVA, that when multiplied by PREDIVS
  // results in a value as close as possible to the lsiMeasuredFreq. This will
  // result in the RTC receiving, as close as possible, the needed 1Hz clock.
  for (preDivA = 127; preDivA >= 2; preDivA--)
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
         MEADOW_TRACE_DEBUG("*** New smallest - chkOffset:%u, A(%03u) * S(%04u) = %05lu (dif:%03u)\n",
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

  MEADOW_TRACE_DEBUG("Final PreDivA:%u, PreDivS:%u, (product:%u), target:%.3f\n",
            smallestDivA,
            smallestDivS,
            smallestDivA * smallestDivS,
            lsiMeasuredFreq);

  // Per STM documents a DivA of 0x7f and a DevS of 0xff is perfect for 32,768
  // HZ. And the product of 128 * 256 = 32768. Therefore, 1 must be subtracted
  // from both of the below factors to compensate for the hardware adding 1.
  *PreDivA = (uint8_t)smallestDivA - 1;
  *PreDivS = (uint16_t)smallestDivS - 1;
  return OK;
}

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
