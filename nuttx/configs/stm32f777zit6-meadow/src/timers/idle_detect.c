/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_timer_support.c
 * 
 *   Copyright (C) 2020, 2021 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
// #include <arch/board/board.h>

// #include <string.h>
// #include <stdbool.h>
// #include <assert.h>
// #include <debug.h>
// #include <errno.h>

// #include "chip.h"
// #include "fcntl.h"
// #include <nuttx/semaphore.h>
// #include <nuttx/arch.h>

// #include "stm32f777zit6-meadow.h"

// #include <sys/ioctl.h>
// #include <nuttx/timers/timer.h>
// #include "stm32_tim.h"

// // PeterM - still needed?
// #include <nuttx/kthread.h>
// #include <meadow/meadow_hw_version.h>

#include "meadow_timers.h"

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _idleRatio;
static uint32_t _idleBeginCount;
static uint32_t _idleEndedCount;

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct timerInfo_s *_timerData[MEADOW_TIMERS_NUMB_OF_TIMERS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//============================================================================
// This function is called for interrupts configured for measuring idle time
int meadow_timer_isr_idle_measure(int irq, void *context, void *arg)
{
  if(! mtcIncludeIdleMeasure)
    return OK;

  // Check the timer's Status Register
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  // Since only counting up, UIF means overflow
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // For the dile counter we keep the overflow in the upper 16-bits. This
    // makes adding the current count very fast.
    timerInfo->timerExtra1 += MEADOW_TIMER_16_BIT_OVERFLOW;
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_idle_detect(struct timerInfo_s *timerData)
{
  _timerData[0] = timerData;
  return OK;
}

//================================================================
// Test code for idle measurement
int meadow_timer_test_idle_measure(struct timerInfo_s *idleTimerInfo)
{
  // Display the info
  syslog(1, "--==--> Current Idle Ratio::%lu\n", _idleRatio);
  // syslog(1, "--==--> Current Idle Ratio::%lu, (0x%08lx), begin:0x%08lx, ended:0x%08lx, %d\n",
  //           _idleRatio, _idleRatio, _idleBeginCount, _idleEndedCount);
  return OK;
}


//=============================================================
// Measuring idle time
int meadow_timer_init_idle_measure(struct timerInfo_s *timerInfo)
{
  int ret;
  uint32_t timerBase = timerInfo->timerBase;

  // Which interrupts?  
  syslog(1, "--> IDLE-Setting up interrupt sources\n");
  // Clear all interrupt sources and set the ones we want we need
  // DMA/Interrupt enable register (DIER)
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          GTIM_DIER_UIE);   //  Only care about overflow

  // All idle measurement interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_idle_measure, timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  syslog(1, "---> Enabling IRQ up_enable_irq\n");

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(timerInfo->timerIrqVec);

  return OK;
}

//====================================================================
// Called from idle loop when idle has begun
void meadow_idle_has_begun(void)
{
  putreg32(0x00001000, STM32_GPIOB_BSRR); // Bit 12 sets PB12

  uint32_t currentCount = _timerData[0]->timerExtra1 + getreg16(STM32_TIM1_CNT);

  // Ignore the case of overflow
  if(_idleBeginCount < currentCount)
  {
    uint32_t totalCount = _idleBeginCount + currentCount;
    uint32_t idleCount = _idleEndedCount - _idleBeginCount;

    // Scale up to improve resolution
    _idleRatio = (totalCount * 1024)/(idleCount * 1024);
  }
  
  // Save current count
  _idleBeginCount = currentCount;
}

//====================================================================
// Called from idle loop when idle has ended
void meadow_idle_has_ended(void)
{
  // Save current count
  _idleEndedCount = _timerData[0]->timerExtra1 + getreg16(STM32_TIM1_CNT);
  putreg32(0x10000000, STM32_GPIOB_BSRR); // Bit 28 resets PB12
}
