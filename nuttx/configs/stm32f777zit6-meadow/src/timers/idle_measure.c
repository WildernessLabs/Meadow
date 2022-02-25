/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/idle_detect.c
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

// The module was initially indended to server 2 purposes. The first was to
// maintain a micorseconds resolution count for Meadow.Core. And the second
// was to use the idle information from stm32_idle.c to determine the current
// CPU load. This second feature turned out to be more difficult that orginally
// thought and had to be dropped.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <inttypes.h>
#include "meadow_timers.h"

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)
//===================================================================

// Timer 6 has no I/O
#define MEADOW_TIMER_IDLE_TIMER_NUMBER (6)

// 1 MHz
#define MEADOW_TIMER_IDLE_MEASURE_CLK_FREQ (1000000)

#define DEBUG_PIN_V2_D14  (0x00040c1c)

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_idle_measure_init(int timerNumber);
static int meadow_timer_idle_isr_measure(int irq, void *context, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static volatile uint32_t _timerOvrFlo;
static volatile uint32_t _idleRatio;

static volatile uint64_t _idleEndCount;
static volatile uint64_t _idleBeginCount;

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This function is called for interrupts configured for measuring idle time
int meadow_timer_idle_isr_measure(int irq, void *context, void *arg)
{
  // Check the timer's Status Register
  uint16_t timStatusReg = getreg16(STM32_TIM6_BASE + STM32_BTIM_SR_OFFSET);

  // Since only counting up, UIF means overflow
  if(timStatusReg & BTIM_SR_UIF)
  {
    timStatusReg &= ~BTIM_SR_UIF;

    // Count the overflows. At 1MHz this occurs every 0.065536 seconds and will
    // itself overflow in 8.9 years using a 32-bit register
    _timerOvrFlo++;
    putreg16(timStatusReg, STM32_TIM6_BASE + STM32_BTIM_SR_OFFSET);
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_idle_measure_setup()
{
  int ret;

  stm32_configgpio(DEBUG_PIN_V2_D14);

  _timerOvrFlo = 0;

  // Initialized the timer itself
  ret = meadow_timer_idle_measure_init(MEADOW_TIMER_IDLE_TIMER_NUMBER);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow idle measure init failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//=============================================================
// Setup timer #6 to run at 1 MHz
// Note: Timers 6 & 7 are Basic Timers
int meadow_timer_idle_measure_init(int timerNumber)
{
  int ret;
  
  // Setup the clock enable
  modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIM6EN);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // Find proper pre-scaler value so all timers run at the same speed
  uint16_t prescaler = (STM32_APB1_TIM6_CLKIN/MEADOW_TIMER_IDLE_MEASURE_CLK_FREQ) - 1;
  putreg16(prescaler, STM32_TIM6_BASE + STM32_BTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = 0xffff;
  putreg32(maxARRValue, STM32_TIM6_BASE + STM32_BTIM_ARR_OFFSET);

  uint16_t regval = getreg16(STM32_TIM6_BASE + STM32_BTIM_CR1_OFFSET);
  regval |= BTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, STM32_TIM6_BASE + STM32_BTIM_CR1_OFFSET);

  // Timer 6 & 7 only support UIE interrupt
  putreg16(BTIM_DIER_UIE, STM32_TIM6_BASE + STM32_BTIM_DIER_OFFSET);

  // The idle measurement interupts are handled by one isr
  ret = irq_attach(STM32_IRQ_TIM6, meadow_timer_idle_isr_measure, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  up_enable_irq(STM32_IRQ_TIM6);

  meadow_timer_enable(STM32_TIM6_BASE);

  return OK;
}

//================================================================
// Test code for idle measurement
uint64_t meadow_timer_idle_measure_total_ticks(void)
{
  uint16_t cntValue1;
  uint16_t cntValue2;
  uint64_t overFlow;

  // The reason for this loop is because we cannot atomically read these 2
  // values. So there's a reasonable possibility that after reading the
  // timer's count but before reading the overflow value, the timer's count
  // could roll-over and add 1 to the overflow, which would give us an bad
  // result. Also, this function may be called from the idle loop so any
  // type of wait is not allowed.
  // So, we check the count before and after reading the overFlow value and
  // if the first count is still smaller than the second, we know an overflow
  // has not occurred.
  do
  {
    cntValue1 = getreg16(STM32_TIM6_BASE + STM32_BTIM_CNT_OFFSET);
    overFlow = (uint64_t)_timerOvrFlo;
    cntValue2 = getreg16(STM32_TIM6_BASE + STM32_BTIM_CNT_OFFSET);
  } while (cntValue1 > cntValue2);
  
  // Now put the pieces together and form the 64-bit return value. Note that
  // the upper 16-bits will always be 0.
  // Since an overflow of _timerOvrFlo happens only once every 8.9 years the
  // problem has not been addressed. Adding another 16-bits would mean the
  // overflow would happen 8.9 * 65536 = 583,270 years.
  // If this is important then each time this function is entered save the
  // _timerOvrFlo value and compare this with current _timerOvrFlo value on
  // each entry. If the saved previous _timerOvrFlo value is ever greater
  // than the current _timerOvrFlo value then add one to a "overOverFlow"
  // uint16_t. This "overOverFlow" must be used as the upper 16-bits of the
  // tick result.

  return (overFlow << 16) | cntValue1;
}

//================================================================
// Test code for idle measurement tick counter
int meadow_timer_test_idle_measure_ticks()
{
  uint64_t currentTicks = meadow_timer_idle_measure_total_ticks();

  // Only need to show lower 48-bits
  syslog(1, "Current Tick count:%012x (%llu)\n", currentTicks, currentTicks);
  
  return OK;
}

//================================================================
// Returns the number of microseconds that Nuttx has been running to
// Meadow.Core.
int meadow_timer_mono_ticks_from_start(struct timerReturnData_s *returnData)
{
  uint64_t currentTicks = meadow_timer_idle_measure_total_ticks();

  returnData->dataField1 = (uint32_t)currentTicks & 0x00000000ffffffff;
  returnData->dataField2 = (uint32_t)(currentTicks >> 32);

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
