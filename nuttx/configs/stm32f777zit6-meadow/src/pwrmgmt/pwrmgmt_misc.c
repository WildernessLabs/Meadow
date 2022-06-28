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

// The functions in this module were copied from stm32_rtc.c

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

// #include <stdlib.h>
// #include <math.h>
#include <syslog.h>

#include <arch/board/board.h>
// #include <nuttx/arch.h>
// #include <nuttx/kthread.h>
// #include "stm32_tim.h"
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

#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
void pwrmgmt_rtc_dumpregs(FAR const char *msg)
{
  int rtc_state;

  // After backup domain reset these are the hardware defined default values
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

  syslog(1, "%s:\n", msg);
  syslog(1, "Registers set to default by Backup Domain Reset\n");
  syslog(1, "  RTC_CR: %08x\n", getreg32(STM32_RTC_CR));
  syslog(1, "    PRER: %08x\n", getreg32(STM32_RTC_PRER));
  syslog(1, "    CALR: %08x\n", getreg32(STM32_RTC_CALR));
  syslog(1, "  SHIFTR: %08x\n", getreg32(STM32_RTC_SHIFTR));
  syslog(1, "   TSSSR: %08x\n", getreg32(STM32_RTC_TSSSR));
  syslog(1, "    TSTR: %08x\n", getreg32(STM32_RTC_TSTR));
  syslog(1, "    TSDR: %08x\n", getreg32(STM32_RTC_TSDR));
  syslog(1, "  TAMPCR: %08x\n", getreg32(STM32_RTC_TAMPCR));
  syslog(1, "    WUTR: %08x\n", getreg32(STM32_RTC_WUTR));
  syslog(1, "ALRMASSR: %08x\n", getreg32(STM32_RTC_ALRMASSR));
  syslog(1, "  ALRMBR: %08x\n", getreg32(STM32_RTC_ALRMBR));
  syslog(1, "ALRMBSSR: %08x\n", getreg32(STM32_RTC_ALRMBSSR));
  syslog(1, "  ALRMAR: %08x\n", getreg32(STM32_RTC_ALRMAR));
  syslog(1, "Other RTC Registers\n");
  syslog(1, "TR(time): %08x\n", getreg32(STM32_RTC_TR));
  syslog(1, "DR(date): %08x\n", getreg32(STM32_RTC_DR));
  syslog(1, "     ISR: %08x\n", getreg32(STM32_RTC_ISR));
  syslog(1, "MAGICREG: %08x\n", getreg32(RTC_MAGIC_REG));

  rtc_state =
    ((getreg32(STM32_EXTI_RTSR) & EXTI_RTC_ALARM) ? 0x1000 : 0) |
    ((getreg32(STM32_EXTI_FTSR) & EXTI_RTC_ALARM) ? 0x0100 : 0) |
    ((getreg32(STM32_EXTI_IMR)  & EXTI_RTC_ALARM) ? 0x0010 : 0) |
    ((getreg32(STM32_EXTI_EMR)  & EXTI_RTC_ALARM) ? 0x0001 : 0);
  syslog(1, "EXTI (RTSR FTSR ISR EVT): %01x\n",rtc_state);
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
// Required for to change RTC_TR, RTC_DR and RTC_PRER
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
void pwrmgmt_rtc_resume(void)
{
#ifdef CONFIG_RTC_ALARM
  uint32_t regval;

  // Clear the RTC alarm flags
  regval  = getreg32(STM32_RTC_ISR);
  regval &= ~(RTC_ISR_ALRAF | RTC_ISR_ALRBF);
  putreg32(regval, STM32_RTC_ISR);

  // Clear the RTC Alarm Pending bit
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);
#endif
}

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
