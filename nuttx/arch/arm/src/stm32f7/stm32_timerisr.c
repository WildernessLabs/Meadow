/****************************************************************************
 * arch/arm/src/stm32f7/stm32_timerisr.c
 *
 *   Copyright (C) 2015, 2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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

#include <stdint.h>
#include <time.h>
#include <debug.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>

#include "nvic.h"
#include "clock/clock.h"
#include "up_internal.h"
#include "up_arch.h"

#include "chip.h"
#include "chip/stm32_dbgmcu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Select MCU-specific settings
 *
 * The RCC feeds the external clock of the Cortex System Timer (SysTick) with
 * the AHB clock (HCLK) divided by 8. The SysTick can work either with this
 * clock or with the Cortex clock (HCLK), configurable in the SysTick control
 * and status register.
 *
 * The SysTick calibration value is fixed to 18750, which gives a reference
 * time base of 1 ms with the SysTick clock set to 18.75 MHz (HCLK/8, with
 * HCLK set to 150 MHz).
 */

#define STM32_SYSTICK_CLOCK  (STM32_BOARD_HCLK / 8)

/* The desired timer interrupt frequency is provided by the definition
 * CLK_TCK (see include/time.h).  CLK_TCK defines the desired number of
 * system clock ticks per second.  That value is a user configurable setting
 * that defaults to 100 (100 ticks per second = 10 MS interval).
 *
 * For example, suppose HCLK = 216 MHz and CLK_TCK = 100, then:
 *
 *   STM32_SYSTICK_CLOCK = 216 MHz / 8 = 27 MHz
 *   SYSTICK_RELOAD      = (27,000,000 / 100) - 1 = 269,999
 */

#define SYSTICK_RELOAD ((STM32_SYSTICK_CLOCK / CLK_TCK) - 1)

/* The size of the reload field is 24 bits.  Verify that the reload value
 * will fit in the reload register.
 */

#if SYSTICK_RELOAD > 0x00ffffff
#  error SYSTICK_RELOAD exceeds the range of the RELOAD register
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  stm32_timerisr
 *
 * Description:
 *   The timer ISR will perform a variety of services for various portions
 *   of the systems.
 *
 ****************************************************************************/

static int stm32_timerisr(int irq, uint32_t *regs, void *arg)
{
#ifdef MEADOW_IWDG_BACKSTOP
  /* Meadow hardware-watchdog backstop.  The IWDG (independent 32kHz LSI
   * watchdog, immune to CPU/NVIC state) is started on the first tick and
   * kicked here roughly once a second FROM THE SYSTICK HANDLER on purpose:
   * SysTick runs in handler mode every tick regardless of CPU load, so a
   * busy-but-healthy system never misses kicks -- but every wedge class
   * that freezes the kernel clock also silences this handler and the IWDG
   * hard-resets the device ~32s later.  Motivating capture: a device found
   * dark for days with VECTACTIVE=3 (HardFault handler active) and PC in
   * unmapped memory -- the fault handler itself had jumped through a
   * corrupted pointer, SysTick pended forever, and only a power cycle
   * recovered it.  With this backstop that failure is a ~32s outage.
   */

  {
    static uint32_t kick_divider = 0;

    if (kick_divider == 0)
      {
        /* Freeze the IWDG while the core is halted by a debugger, or every
         * GDB session longer than the timeout would reset the target
         * (DBGMCU_APB1_FZ.DBG_IWDG_STOP).
         */

        putreg32(getreg32(STM32_DBGMCU_APB1_FZ) | DBGMCU_APB1_IWDGSTOP,
                 STM32_DBGMCU_APB1_FZ);

        /* Unlock, prescale LSI/256 (~125Hz), max reload 0xFFF -> ~32s */

        putreg32(0x5555, STM32_IWDG_BASE + 0x00);  /* KR: enable access */
        putreg32(6,      STM32_IWDG_BASE + 0x04);  /* PR: /256 */
        putreg32(0x0fff, STM32_IWDG_BASE + 0x08);  /* RLR: max */
        putreg32(0xcccc, STM32_IWDG_BASE + 0x00);  /* KR: start */
      }

    if ((kick_divider++ & 1023) == 0)
      {
        putreg32(0xaaaa, STM32_IWDG_BASE + 0x00);  /* KR: kick */
      }
  }
#endif

  /* Process timer interrupt */

  nxsched_process_timer();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  arm_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize the timer
 *   interrupt.
 *
 ****************************************************************************/

void arm_timer_initialize(void)
{
  uint32_t regval;

  /* Configure SysTick to interrupt at the requested rate */

  putreg32(SYSTICK_RELOAD, NVIC_SYSTICK_RELOAD);
  putreg32(0, NVIC_SYSTICK_CURRENT);

  /* Attach the timer interrupt vector */

  (void)irq_attach(STM32_IRQ_SYSTICK, (xcpt_t)stm32_timerisr, NULL);

  /* Enable SysTick interrupts:
   *
   *   NVIC_SYSTICK_CTRL_CLKSOURCE=0 : Use the implementation defined clock
   *                                   source which, for the STM32F7, will be
   *                                   HCLK/8
   *   NVIC_SYSTICK_CTRL_TICKINT=1   : Generate interrupts
   *   NVIC_SYSTICK_CTRL_ENABLE      : Enable the counter
   */

  regval = (NVIC_SYSTICK_CTRL_TICKINT | NVIC_SYSTICK_CTRL_ENABLE);
  putreg32(regval, NVIC_SYSTICK_CTRL);

  /* And enable the timer interrupt */

  up_enable_irq(STM32_IRQ_SYSTICK);
}
