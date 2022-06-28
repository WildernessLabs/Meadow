/****************************************************************************
 * configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_switch_rtc_clock.c
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

// Some of the stm32_rtc.c code has be duplicated here.

// The purpose of this module is to all allow the caller to switch between
// using the HSE and LSI clocks for driving the RTC hardware. This is needed
// for low-power operation, because while HSE is accurate at keeping time, but
// LSI is not. However, HSE is not availalbe in any of the F7's low-power
// modes. Therefore, the clock used to drive the RTC hardware must be switched
// to the LSI clock before entering low-power mode and switched back to HSE
// afterward.

// Note: The F7 Data Sheet says the LSI oscillator can maximum current of
// 0.6 micro amps. for this reason there was no attempt of turning the LSI
// clock off when not needed.

// Note: the underlying STM32_rtc.c driver doesn't support CONFIG_RTC_HIRES

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
#define USE_MEADOW_DEBUG_HELPERS
//  #undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/
static char *thisFile = __FILE__;

// Save the HSE value either read from the correct RTC register or set here
static uint32_t _hseRtcPrer;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int pwrmgmt_switch_rtc_as_per_args(uint32_t clkSrc, uint32_t rtcPrer);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

// This function creates a thread so the rest of the initialization
// isn't stalled waiting for this code to finish. Why not do this on demand?
// Because this requires Timer 5 to be setup a special way. And at runtime
// timer 5 may be assigned other responsibilites and doing this would create
// a difficult problem to fix
int pwrmgmt_init_rtc_clk_switch(void)
{
  int ret = OK;
  _hseRtcPrer = 0;
  return ret;
}

//=============================================================
// Public Function to restore the RTC source to HSE
int meadow_pwr_mgmt_use_hse_for_rtc()
{
  int ret;

#ifndef CONFIG_STM32F7_RTC_HSECLOCK
#  error "CONFIG_STM32F7_PWR must selected to use this driver"
#endif

  // What clock source is currently in use?
  uint32_t initClkSrc = getreg32(STM32_RCC_BDCR) & RCC_BDCR_RTCSEL_MASK;

  // MEADOW_TRACE_DEBUG("--> Setting clock to HSE, from %s\n",
  //           initClkSrc == RCC_BDCR_RTCSEL_HSE ? "HSE" : "LSI");
  
  // If the current clock source is hse we'll save the RTC_PRER value
  if(initClkSrc == RCC_BDCR_RTCSEL_HSE)
  { 
    // Has the HSE pre-scaler already been saved?
    if(_hseRtcPrer == 0)
    {
      // If not previously saved, read it now
      _hseRtcPrer = getreg32(STM32_RTC_PRER);
      if(_hseRtcPrer == 0)
      {
        syslog(LOG_ERR, "%s@%d-ERROR:HSE clock source but no pre-scaler\n", thisFile, __LINE__);
        return -1;
      }
    }

    // Since this is HSE there's nothing to do
    MEADOW_TRACE_DEBUG("Call to switch to HSE but already HSE. Exiting!\n");
    return  OK;   // Nothing to do
  }
  else
  {
    // Since the clock source is not HSE we are forced to use the hardcoded
    // values found in /arch/arm/src/stm32f7/stm32_rtc.c @510.
    _hseRtcPrer = (uint32_t)PWRMGMT_CLK_HSE_DIV_S_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_S_SHIFT |
              (uint32_t)PWRMGMT_CLK_HSE_DIV_A_FACTOR_FOR_1_MHZ << RTC_PRER_PREDIV_A_SHIFT;

    // MEADOW_TRACE_DEBUG("Switching to HSE using hardcoded pre-scaler\n");
  }

  // Let a more generic function do the heavy lifting
  ret = pwrmgmt_switch_rtc_as_per_args(RCC_BDCR_RTCSEL_HSE, _hseRtcPrer);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-HSE clock pre-scaler set error\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
}

//=============================================================
// Public Function to restore the RTC source to LSI
int meadow_pwr_mgmt_use_lsi_for_rtc()
{
  int ret;
  // Read current clock source
  uint32_t initClkSrc = getreg32(STM32_RCC_BDCR) & RCC_BDCR_RTCSEL_MASK;

  // MEADOW_TRACE_DEBUG("--> Setting clock to LSI, from %s\n",
  //         initClkSrc == RCC_BDCR_RTCSEL_LSI ? "LSI" : "HSE");

  // If the current clock source is HSE we can assume the RTC_PRER value is
  // good so we can save it.
  if(initClkSrc == RCC_BDCR_RTCSEL_HSE)
  {
    if(_hseRtcPrer == 0)
    {
      // Since the current clock is HSE we can save it's pre-scaler (RTC_PRER) 
      _hseRtcPrer = getreg32(STM32_RTC_PRER);
      if(_hseRtcPrer == 0)
      {
        syslog(LOG_ERR, "%s@%d-ERROR:HSE clock source but no pre-scaler\n", thisFile, __LINE__);
        return -1;
      }
    }
  }

  // If the current value in STM32_RCC_BDCR indicates that HSE is
  // in use we'll copy it's STM32_RTC_PRER value so we can restore it later.
  if(initClkSrc == RCC_BDCR_RTCSEL_LSI)
  {
    MEADOW_TRACE_DEBUG("Call to switch to LSI but already LSI. Exiting!\n");
    return OK;
  }

  // The LSI clock's frequency has already been measured and the needed
  // calibration factors saved at startup.
  if(pwrmgmt_get_lsi_calib_rtc_clk_value() == 0)
  {
    syslog(LOG_ERR, "%s@%d-LSI BBR pre-scaler value is 0. It must be set\n", thisFile, __LINE__);
    return -1;
  }

  // Switch to LSI clock for RTC timing.
  ret = pwrmgmt_switch_rtc_as_per_args(RCC_BDCR_RTCSEL_LSI, pwrmgmt_get_lsi_calib_rtc_clk_value());
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ERROR:Switching to LSI failed\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
}

//=============================================================
// Set up RTC to use the LSI Clock
// RCC_BDCR_RTCSEL_HSE or RCC_BDCR_RTCSEL_LSI supported
int pwrmgmt_switch_rtc_as_per_args(uint32_t clkSrc, uint32_t rtcPrer)
{
  int ret;
  uint32_t regval;

  // Enable write access to backup domain
  stm32_pwr_enablebkp(true);

  // Save time and date
  uint32_t tr_bkp = getreg32(STM32_RTC_TR);
  uint32_t dr_bkp = getreg32(STM32_RTC_DR);

  // Save the Battery Backed Registers we know about
  //
  // This is the value Nuttx uses in stm32_rtc.c to determine if the clock
  // has been initialized. Therefore, we must save and restore it here.
  uint32_t saveMagicRegi = getreg32(RTC_MAGIC_REG);
  // A patch was made to stm32_rtc.c @991 so if LSI is the selected clock and
  // Meadow is rebooted, the Meadow BBR won't be lost.
  uint32_t saveMeadowReg = getreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
  // The UTC offset is not saved within stm32_rtc.c during a reset because the
  // RTC will loose all it's time values, so no reason to save the UTC offset.
  uint32_t saveUtcOffReg = getreg32(MEADOW_BATTERY_BACKED_REG_RTC_UTC_OFFSET);

  // A reset of the backup domain is required to switch clocks.
  // This action resets the following registers to these defaults.
  // RTC control register (RTC_CR)                    [0]
  // RTC prescaler register (RTC_PRER)                [0x007f00ff the LSE default]
  // RTC calibration register (RTC_CALR)              [0]
  // RTC shift register (RTC_SHIFTR)                  [0]
  // RTC timestamp register (RTC_TSSSR)               [0]
  // RTC timestamp register (RTC_TSTR)                [0]
  // RTC timestamp register (RTC_TSDR)                [0]
  // RTC tamper configuration register (RTC_TAMPCR)   [0]
  // RTC backup registers (RTC_BKPxR)                 [all 32 registers reset to 0]
  // RTC wakeup timer register (RTC_WUTR)             [0x0000FFFF]
  // RTC Alarm A registers (RTC_ALRMASSR/RTC_ALRMAR)  [both to 0]
  // RTC Alarm B registers (RTC_ALRMBSSR/RTC_ALRMBR)  [both to 0]
  // RTC Option register (RTC_OR)                     [0]
  //
  modifyreg32(STM32_RCC_BDCR, 0, RCC_BDCR_BDRST);
  modifyreg32(STM32_RCC_BDCR, RCC_BDCR_BDRST, 0);

  // Switch to the requested clock as the input to the RTC block
  modifyreg32(STM32_RCC_BDCR, RCC_BDCR_RTCSEL_MASK, clkSrc);
  modifyreg32(STM32_RCC_BDCR, 0, RCC_BDCR_RTCEN);

  // Loop, attempting to initialize/resume the RTC. This loop is necessary
  // because it seems that occasionally it takes longer to initialize the
  // RTC (the actual failure is in pwrmgmt_rtc_synchwait()).
  int maxretry = 10;
  int nretry = 0;
  do
  {
    // Wait for the RTC Time and Date registers to be synchronized with
    // RTC APB clock.
    ret = pwrmgmt_rtc_synchwait();
  }
  while (ret != OK && ++nretry < maxretry);

  // Clear the RTC alarm flags and clear pending alarm
  pwrmgmt_rtc_resume();

  // Lock backup domain
  stm32_pwr_enablebkp(false);

  if (ret != OK && nretry > 0)
  {
    syslog(LOG_ERR, "%s@%d-init/resume ran %d times and failed with %d\n",
              thisFile, __LINE__, nretry, ret);
    return -ETIMEDOUT;
  }

  // Unlock RTC registers for writing
  pwrmgmt_rtc_wprunlock();

  // Enter the RTC initialization mode. Required for changes to RTC_TR,
  // RTC_DR and RTC_PRER
  ret = pwrmgmt_rtc_enterinit();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error: pwrmgmt_rtc_enterinit() returned %d\n",
              thisFile, __LINE__, ret);
  }
  else
  {
    // Set the 24 hour format by clearing the FMT bit in the RTC control register
    regval = getreg32(STM32_RTC_CR);
    regval &= ~RTC_CR_FMT;
    putreg32(regval, STM32_RTC_CR);

    // Write the 2 pre-scaler values that calibrate the RTC for the desired
    // clock. These values (PREDIV_A 22:16 and PREDIV_S 14:0) have already been
    // pre-combined.
    putreg32(rtcPrer, STM32_RTC_PRER);

    // Restore time and date
    putreg32(tr_bkp, STM32_RTC_TR);
    putreg32(dr_bkp, STM32_RTC_DR);

    pwrmgmt_rtc_exitinit();
  }

  pwrmgmt_rtc_wprlock();

  // Restore Battery Backed Registers
  putreg32(saveMagicRegi, RTC_MAGIC_REG);
  putreg32(saveUtcOffReg, MEADOW_BATTERY_BACKED_REG_RTC_UTC_OFFSET);
  putreg32(saveMeadowReg, HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);

#if PWRMGMT_CLK_SHOW_RTC_TIME_FOR_TESTING > 0
  pwrmgmt_set_dbg_clk_switched_flag(true);
#endif

  return OK;
}

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
