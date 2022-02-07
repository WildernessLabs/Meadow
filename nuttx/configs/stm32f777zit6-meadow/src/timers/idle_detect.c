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

// static int meadow_timer_isr_idle_measure(int irq, void *context, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

// static uint32_t _idleRatio;
// static uint32_t _idleBeginCount;
// static uint32_t _idleEndedCount;

struct timerInfo_s
{
  // Timer base address
  uint8_t timerNumb;                  // For diagnostics
  volatile uint8_t timerWidth;        // Either 16 or 32 bit wide (replace with func bit)
  volatile uint8_t timerDectSync;     // FDc - CCR1 interrupt missing
  volatile uint32_t timerCount1;      // Primary value of the count
  volatile uint32_t timerCount2;      // Secondary value of the count
  volatile uint32_t timerExtra1;      // Extra information 1
  volatile uint32_t timerExtra2;      // Extra information 2
  volatile uint32_t timerFreq;        // Running timer clock frequency (could be prescaler value)
  uint32_t timerFunc;                 // Bit fields with the functions this timer has and can perform
  uint32_t timerChan[4];              // Channels for each timer
  uint32_t timerBase;                 // Unique for each timer
  uint32_t timerMaxClk;               // Either 192MHz or 96MHz (replace with func bit)
  uint32_t timerAPBClk;               // Proper APB clock register for timer enable bit field (replace with func bit)
  uint32_t timerClkEn;                // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;               // Interrupt vector
};


// static struct timerInfo_s timerInfoArray[] =
// {
//   // NOTE: CHANNELS REFLECT F7V2, ONLY TIM3 DIFFERENT IN F7V1
//             // Num  wid  Syn CC1 CC2 Ex1 Ex2 Frq Fnc     Chan1-4                  Base Addr      Max Clock Frequency    Correct APB Clock     Timer Enable         IRQ Vector
//   /* TIM1   */  {1 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM1_BASE,  STM32_APB2_TIM1_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
//   /* TIM2   */  {2 , 32,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM2_BASE,  STM32_APB1_TIM2_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
//   /* TIM3   */  {3 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0},        STM32_TIM3_BASE,  STM32_APB1_TIM3_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
//   /* TIM4   */  {4 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0x3c,0x40},  STM32_TIM4_BASE,  STM32_APB1_TIM4_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
//   /* TIM5   */  {5 , 32,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM5_BASE,  STM32_APB1_TIM5_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
//   /* TIM6   */  {6 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM6_BASE,  STM32_APB1_TIM6_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
//   /* TIM7   */  {7 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM7_BASE,  STM32_APB1_TIM7_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
//   /* TIM8   */  {8 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0x40},     STM32_TIM8_BASE,  STM32_APB2_TIM8_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
//   /* TIM9   */  {9 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0x38,0,0},           STM32_TIM9_BASE,  STM32_APB2_TIM9_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
//   /* TIM10  */  {10, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM10_BASE, STM32_APB2_TIM10_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
//   /* TIM11  */  {11, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM11_BASE, STM32_APB2_TIM11_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
//   /* TIM12  */  {12, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0},        STM32_TIM12_BASE, STM32_APB1_TIM12_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
//   /* TIM13  */  {13, 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM13_BASE, STM32_APB1_TIM13_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
//   /* TIM14  */  {14, 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM14_BASE, STM32_APB1_TIM14_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
// };

/****************************************************************************
 * Private Types
 ****************************************************************************/

// struct timerInfo_s *_idleTimerData[MEADOW_TIMERS_NUMB_OF_TIMERS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// static void meadow_timer_disable(uint32_t timerBase)
// {
//   uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
//   regval &= ~ATIM_CR1_CEN;
//   putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
// }

//=============================================================
// static void meadow_timer_enable(uint32_t timerBase)
// {
//   // Why this order? tryed to copy the NUTTX order
//   uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
//   cr1Val |= GTIM_CR1_CEN;
  
//   uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
//   egrVal |= GTIM_EGR_UG;

//   putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);

//   putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
// }

//============================================================================
// This function is called for interrupts configured for measuring idle time
// int meadow_timer_isr_idle_measure(int irq, void *context, void *arg)
// {
  // // Check the timer's Status Register
  // struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  // uint32_t timerBase = timerInfo->timerBase;
  // uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  // // Since only counting up, UIF means overflow
  // if(timStatusReg & GTIM_SR_UIF)
  // {
  //   timStatusReg &= ~GTIM_SR_UIF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

  //   // For the idle counter we keep the overflow in the upper 16-bits. This
  //   // makes adding the current count fast.
  //   timerInfo->timerExtra1 += MEADOW_TIMER_16_BIT_OVERFLOW;
  // }

//   return OK;
// }

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_idle_detect()
{
  // Clear table values as needed
  // for (int i = 0; i < MEADOW_TIMERS_NUMB_OF_TIMERS; i++)
  // {
  //   timerInfoArray[i].timerCount1 = 0;
  //   timerInfoArray[i].timerCount2 = 0;
  //   timerInfoArray[i].timerExtra1 = 0;
  //   timerInfoArray[i].timerExtra2 = 0;
  // }

  return OK;
}

//================================================================
// Test code for idle measurement
int meadow_timer_test_idle_measure()
{
  // struct timerInfo_s *timerInfo = &(timerInfoArray[timerNumber - 1]);

  // Display the info
  // COMMIT OUT UNTIL READY TO USE
  // syslog(1, "--==--> Current Idle Ratio::%lu\n", _idleRatio);
  // syslog(1, "--==--> Current Idle Ratio::%lu, (0x%08lx), begin:0x%08lx, ended:0x%08lx, %d\n",
  //           _idleRatio, _idleRatio, _idleBeginCount, _idleEndedCount);
  return OK;
}

//=============================================================
// Measuring idle time
int meadow_timer_init_idle_measure()
{
  // int ret;

  // struct timerInfo_s *timerInfo = &(timerInfoArray[timerNumber - 1]);
  // uint32_t timerBase = timerInfo->timerBase;
  
  // //------------------------------------------
  // // Setup the clock enable
  // modifyreg32(timerInfo->timerAPBClk, 0, timerInfo->timerClkEn);
  
  // // Must be between 0 and 0xffff.
  // // Set the prescaler value of 0 to allow highest speed. A prescaler value of
  // // 1 will divide the clock by 2.
  // prescaler = 0;
  // putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);
  // timerInfo->timerFreq = timerInfo->timerMaxClk;

  // // The value put into the ARR is maximum
  // uint32_t maxARRValue = timerInfo->timerWidth == 16 ? 0xffff : 0xffffffff;
  // putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  // regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  // regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  // putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);
  // //------------------------------------------

  // // Which interrupts?  
  // syslog(1, "--> IDLE-Setting up interrupt sources\n");
  // // Clear all interrupt sources and set the ones we want we need
  // // DMA/Interrupt enable register (DIER)
  // modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
  //         GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
  //         GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
  //         GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
  //         GTIM_DIER_UIE);   //  Only care about overflow

  // // All idle measurement interupts are handled by same isr
  // ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_idle_measure, timerInfo);
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
  //         __FILE__, __LINE__, ret, errno);
  //   return ret;
  // }

  // syslog(1, "---> Enabling IRQ up_enable_irq\n");

  // // Nuttx handles the interrupts at the lowest level
  // up_enable_irq(timerInfo->timerIrqVec);

  return OK;
}

//====================================================================
// Called from idle loop when idle has begun
void meadow_idle_has_begun(void)
{
  // COMMIT OUT UNTIL READY TO USE
  // putreg32(0x00001000, STM32_GPIOB_BSRR); // Bit 12 sets PB12

  // uint32_t currentCount = _idleTimerData[0]->timerExtra1 + getreg16(STM32_TIM1_CNT);

  // // Ignore the case of overflow
  // if(_idleBeginCount < currentCount)
  // {
  //   uint32_t totalCount = _idleBeginCount + currentCount;
  //   uint32_t idleCount = _idleEndedCount - _idleBeginCount;

  //   // Scale up to improve resolution
  //   _idleRatio = (totalCount * 1024)/(idleCount * 1024);
  // }
  
  // // Save current count
  // _idleBeginCount = currentCount;
}

//====================================================================
// Called from idle loop when idle has ended
void meadow_idle_has_ended(void)
{
  // // Save current count
  // _idleEndedCount = _idleTimerData[0]->timerExtra1 + getreg16(STM32_TIM1_CNT);
  // putreg32(0x10000000, STM32_GPIOB_BSRR); // Bit 28 resets PB12
}
