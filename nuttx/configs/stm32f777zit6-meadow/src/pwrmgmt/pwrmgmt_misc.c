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

// The functions in this module mimic those in stm32_rtc.c. This
// allows a build without CONFIG_RTC_ALARM being configured.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <syslog.h>

#include <arch/board/board.h>
#include "stm32_pwr.h"
#include "stm32_rtc.h"
#include "stm32_exti.h"

#include <meadow/hcom_shared_common.h>
#include "pwrmgmt_local.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#if defined(CONFIG_POWER_MANAGEMENT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
void pwrmgmt_rtc_dumpregs(FAR const char *msg)
{
  int rtc_state;

  // After backup domain reset these are the default values
  // RTC control register (RTC_CR)                    [0]
  // RTC prescaler register (RTC_PRER)                [0x007f00ff the LSE default]
  // RTC calibration register (RTC_CALR)              [0]
  // RTC shift register (RTC_SHIFTR)                  [0]
  // RTC timestamp register (RTC_TSSSR)               [0]
  // RTC timestamp register (RTC_TSTR)                [0]
  // RTC timestamp register (RTC_TSDR)                [0]
  // RTC tamper configuration register (RTC_TAMPCR)   [0]
  // RTC backup registers (RTC_BKPxR)                 [all 32 registers to 0]
  // RTC wakeup timer register (RTC_WUTR)             [0]
  // RTC Alarm A registers (RTC_ALRMASSR/RTC_ALRMAR)  [both 0]
  // RTC Alarm B registers (RTC_ALRMBSSR/RTC_ALRMBR)  [both 0]
  // RTC Option register (RTC_OR)                     [0]

  syslog(LOG_MTEST, "%s:\n", msg);
  syslog(LOG_MTEST, "Registers set to default by Backup Domain Reset\n");
  syslog(LOG_MTEST, "  RTC_CR: %08x\n", getreg32(STM32_RTC_CR));
  syslog(LOG_MTEST, "    PRER: %08x\n", getreg32(STM32_RTC_PRER));
  syslog(LOG_MTEST, "    CALR: %08x\n", getreg32(STM32_RTC_CALR));
  syslog(LOG_MTEST, "  SHIFTR: %08x\n", getreg32(STM32_RTC_SHIFTR));
  syslog(LOG_MTEST, "   TSSSR: %08x\n", getreg32(STM32_RTC_TSSSR));
  syslog(LOG_MTEST, "    TSTR: %08x\n", getreg32(STM32_RTC_TSTR));
  syslog(LOG_MTEST, "    TSDR: %08x\n", getreg32(STM32_RTC_TSDR));
  syslog(LOG_MTEST, "  TAMPCR: %08x\n", getreg32(STM32_RTC_TAMPCR));
  syslog(LOG_MTEST, "    WUTR: %08x\n", getreg32(STM32_RTC_WUTR));
  syslog(LOG_MTEST, "ALRMASSR: %08x\n", getreg32(STM32_RTC_ALRMASSR));
  syslog(LOG_MTEST, "  ALRMBR: %08x\n", getreg32(STM32_RTC_ALRMBR));
  syslog(LOG_MTEST, "ALRMBSSR: %08x\n", getreg32(STM32_RTC_ALRMBSSR));
  syslog(LOG_MTEST, "  ALRMAR: %08x\n", getreg32(STM32_RTC_ALRMAR));
  syslog(LOG_MTEST, "Other RTC Registers\n");
  syslog(LOG_MTEST, "TR(time): %08x\n", getreg32(STM32_RTC_TR));
  syslog(LOG_MTEST, "DR(date): %08x\n", getreg32(STM32_RTC_DR));
  syslog(LOG_MTEST, "     ISR: %08x\n", getreg32(STM32_RTC_ISR));
  syslog(LOG_MTEST, "MAGICREG: %08x\n", getreg32(RTC_MAGIC_REG));

  rtc_state =
    ((getreg32(STM32_EXTI_IMR)  & EXTI_RTC_ALARM) ? 0x0010 : 0) |
    ((getreg32(STM32_EXTI_EMR)  & EXTI_RTC_ALARM) ? 0x0001 : 0) |
    ((getreg32(STM32_EXTI_RTSR) & EXTI_RTC_ALARM) ? 0x1000 : 0) |
    ((getreg32(STM32_EXTI_FTSR) & EXTI_RTC_ALARM) ? 0x0100 : 0);
  syslog(LOG_MTEST, "EXTI (IMR EMR RTSR FTSR): %01x\n",rtc_state);
}
#endif

//=============================================================
// Required for making changes to RTC registers
void pwrmgmt_rtc_wprunlock(void)
{
  // Sets the PWR_CR1_DBP bit in the STM32_PWR_CR1_OFFSET register
  // Ref Man 4.4.1 PWR power control register (PWR_CR1)
  // True enables ability to write to backup domain registers
  stm32_pwr_enablebkp(true);

  // Enable write access to RTC Registers
  putreg32(0xca, STM32_RTC_WPR);
  putreg32(0x53, STM32_RTC_WPR);
}

//=============================================================
void pwrmgmt_rtc_wprlock(void)
{
  // Disable write access to RTC Registers
  putreg32(0xff, STM32_RTC_WPR);

  // Clears the PWR_CR1_DBP bit in the STM32_PWR_CR1_OFFSET register
  // Ref Man 4.4.1 PWR power control register (PWR_CR1)
  // False disables ability to write to backup domain registers
  stm32_pwr_enablebkp(false);
}

//=============================================================
// Set RTC_ISR_INIT bit in STM32_RTC_ISR and wait for RTC_ISR_INITF bit
// Required for to change RTC_TR (time), RTC_DR (date) and RTC_PRER (prescaler)
int pwrmgmt_rtc_enterinit(void)
{
  volatile uint32_t timeout;
  uint32_t regval;
  int ret;

  // Check if the Initialization mode is already set
  regval = getreg32(STM32_RTC_ISR);

  ret = OK;
  // RTC_ISR_INITF bit = 1 means calendar register update allowed
  if ((regval & RTC_ISR_INITF) == 0)
  {
    // Set the Initialization mode bit
    putreg32(RTC_ISR_INIT, STM32_RTC_ISR);

    // Wait until the RTC is in the INIT state (or a timeout occurs)
    ret = -ETIMEDOUT;
    for (timeout = 0; timeout < 10000; timeout++)
    {
      regval = getreg32(STM32_RTC_ISR);

      // Loop till calendar register update allowed (i.e. not 0)
      if ((regval & RTC_ISR_INITF) != 0)
      {
        ret = OK;
        break;
      }
    }
  }
  else
  {
    MEADOW_TRACE_DEBUG("===> pwrmgmt_rtc_enterinit() on Entry found RTC_ISR_INITF == 0\n");
  }

  return ret;
}

//=============================================================
void pwrmgmt_rtc_exitinit(void)
{
  uint32_t regval;

  regval = getreg32(STM32_RTC_ISR);
  regval &= ~(RTC_ISR_INIT);
  putreg32(regval, STM32_RTC_ISR);

  // The STM32F77X Errata warns in ES0334-Rev 9 section 2.12.3 of a potential
  // problem if the above RTC_ISR_INIT bit is cleared and reset before the
  // proper wait time has occurred. This is the workaround given in the Errata.
  // "After existing the initialization mode, clear the BYPSHAD bit (if set)
  // then wait for RSF to rise, before entering the initialization mode again."
  //
  // Do this on exit to be sure the workaround is honored.
  regval = getreg32(STM32_RTC_CR);
  regval &= ~RTC_CR_BYPSHAD;    // Clear BYPSHAD
  putreg32(regval, STM32_RTC_CR);

  // Wait for RFS bit to indicate that shadow registers are synchronized.
  while ((getreg32(STM32_RTC_CR) & RTC_ISR_RSF) != 0);
  return;
}

//=============================================================
int pwrmgmt_rtc_synchwait(void)
{
  volatile uint32_t timeout;
  uint32_t regval;
  int ret;

  // Disable the write protection for RTC registers
  pwrmgmt_rtc_wprunlock();

  // Clear Registers synchronization flag (RSF)
  regval  = getreg32(STM32_RTC_ISR);
  regval &= ~RTC_ISR_RSF;
  putreg32(regval, STM32_RTC_ISR);

  // Now wait the registers to become synchronised
  ret = -ETIMEDOUT;
  for (timeout = 0; timeout < 20000; timeout++)
    {
      regval = getreg32(STM32_RTC_ISR);
      if ((regval & RTC_ISR_RSF) != 0)
        {
          // Synchronized
          ret = OK;
          break;
        }
    }

  // Re-enable the write protection for RTC registers
  pwrmgmt_rtc_wprlock();
  return ret;
}

//=============================================================
// Convert a 2 byte value into it's BCD representation
uint32_t pwrmgmt_rtc_bin2bcd(int value)
{
  uint32_t msbcd = 0;

  while (value >= 10)
  {
    msbcd++;
    value -= 10;
  }

  return (msbcd << 4) | value;
}

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
