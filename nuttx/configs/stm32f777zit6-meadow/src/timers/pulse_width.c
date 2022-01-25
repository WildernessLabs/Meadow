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

#warning Experimental Code

/****************************************************************************
 * Private Data
 ****************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t _endPWidthSem;

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct timerInfo_s *_timerData[MEADOW_TIMERS_NUMB_OF_TIMERS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//============================================================================
// This function is called for all interrupts configured for measureing pulse width
int meadow_timer_isr_pulse_width(int irq, void *context, void *arg)
{
  if(!mtcPulseWidth)
    return OK;

  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;

  // Why are we here? Check the timer's Status Register
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);
  
  // Check the status register and acknowledge all interrupts
  if(timStatusReg & GTIM_SR_TIF)
  {
    // In gated mode GTIM_SR_TIF occurs when counter is started or stopped
    // Clear interrupt
    timStatusReg &= ~GTIM_SR_TIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Read the GPIO's state.
    // Assumes high = start and low = stop. May want to allow config to specify
    bool inputState = stm32_gpioread(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);
    if(!inputState)
    {
      // The input point's state indicates that the counting has stopped.
      if(timerInfo->timerWidth == 16)
        timerInfo->timerCount1 = (uint32_t)getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
      else
        timerInfo->timerCount1 = getreg32(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

      if(!mtcHC_SR04Filter)
      {
        sem_post(&_endPWidthSem); // Allow the requesting thread to process data
        return OK;
      }

      // Since HC-SR04 filter is requested we do the following. Why?
      // When there is no target the HC-SR04 output goes high for about 125ms
      // then low for about 150us then high again for about 6us. At 96MHz the
      // count for 6us is between 575 and 578. The following prevents this
      // effect from interfering with normally expected behavior.
      if(timerInfo->timerCount1 > 574 && timerInfo->timerCount1 < 579)
      {
        // Throw away the count. This way a zero reading is returned
        timerInfo->timerCount1 = 0;
        timerInfo->timerCount2 = 0;     // Not used here
        timerInfo->timerExtra1 = 0;
        timerInfo->timerExtra2 = 0;    // Not used here
      }
      else
      {
        sem_post(&_endPWidthSem);
      }
      return OK;
    }
  }

  //----------------------------------------------------------
  if(timStatusReg & GTIM_SR_UIF)
  {
    // GTIM_SR_UIF indicates overflow or underflow of main counter
    timerInfo->timerExtra1++;

    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  }

  // if(timStatusReg & GTIM_SR_BIF)
  // {
  //   // syslog(1, "+++> Break interrupt flag set\n");
  //   timStatusReg &= ~GTIM_SR_BIF;
  //   // putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  
  // //--------------------------------------------
  // if(timStatusReg & GTIM_SR_CC1IF)
  // {
  //   syslog(1, "+++> Capture/compare interrupt flag 1\n");
  //   timStatusReg &= ~GTIM_SR_CC1IF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC2IF)
  // {
  //   syslog(1, "+++> Capture/compare interrupt flag 2\n");
  //   timStatusReg &= ~GTIM_SR_CC2IF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC3IF)
  // {
  //   syslog(1, "+++> Capture/compare interrupt flag 3\n");
  //   timStatusReg &= ~GTIM_SR_CC3IF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC4IF)
  // {
  //   syslog(1, "+++> Capture/compare interrupt flag 4\n");
  //   timStatusReg &= ~GTIM_SR_CC4IF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }

  // //--------------------------------------------
  // if(timStatusReg & GTIM_SR_CC1OF)
  // {
  //   syslog(1, "+++> A Capture/compare over capture interrupt 1\n");
  //   timStatusReg &= ~GTIM_SR_CC1OF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC2OF)
  // {
  //   syslog(1, "+++> A Capture/compare over capture interrupt 2\n");
  //   timStatusReg &= ~GTIM_SR_CC2OF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC3OF)
  // {
  //   syslog(1, "+++> A Capture/compare over capture interrupt 3\n");
  //   timStatusReg &= ~GTIM_SR_CC3OF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }
  // if(timStatusReg & GTIM_SR_CC4OF)
  // {
  //   syslog(1, "+++> A Capture/compare over capture interrupt 4\n");
  //   timStatusReg &= ~GTIM_SR_CC4OF;
  //   putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  // }

  // syslog(1, "+++> Interrupts exit. Status Reg:0x%04x\n", timStatusReg);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_timer_setup_pulse_width(struct timerInfo_s *timerData)
{
  _timerData[0] = timerData;

  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);

  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(struct timerInfo_s *timerInfo)
{
  int ret;
  int displayCount = 0;
  static uint32_t timeOutErrCnt = 0;

  // Attempt to simulate normal operation via this loop.
  // Note: this is built around the HC-SR04.

  // Clear all timer counter's
  if(timerInfo->timerWidth == 16)
    putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
  else
    putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
  
  timerInfo->timerExtra1 = 0;

  // Should be done by MONO code
  // Send pulse to HC-SR04, this must be at least 2us wide to signal the
  // HC-SR04 to send ultrasonic pulses and wait for the echo.
  syslog(1, "--> Sending pulse to HC-SR04\n");
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, true);

  // HC-SR04 requires a trigger pulse of at least 2 usec. However, a 1-2 ms
  // pulse works too. Because of the Nuttx usleep resolution the following
  // usleep(1) will sleep between 1-2 ms. The HC-SR04 generates it's timing
  // pulse (echo) about 500us after the trigger pulses falling edge.
  usleep(1);

  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, false);

  // Wait for ISR to indicate that timer has ended
  struct timespec abstime;
  ret = clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += 2;    // This delay could be supplied by .Net user
  abstime.tv_nsec = 0;
  ret = sem_timedwait(&_endPWidthSem, &abstime);
  if(ret < 0)
  {
    if(errno == ETIMEDOUT)
    {
      timeOutErrCnt++;
      syslog(1, "--> ERROR:Pulse width-Semaphore timeout:%u, ret:%d, errno:%d\n",
                timeOutErrCnt, ret, errno);
    }
    else
    {
      syslog(1, "--> ERROR:Pulse width-Semaphore ret:%d, errno:%d\n", ret, errno);
    }

    // An error can means that the semaphore timed out. In this case we to
    // insure that the semaphore count is correct. If not correct, it means
    // that the ISR didn't do the sem_post() call. Therefore, we need to call
    // sem_post to keep the semaphore in sync with the ISR.
    int semcount;
    sem_getvalue(&_endPWidthSem, &semcount);
    if(semcount == 0)
      sem_post(&_endPWidthSem); 
  }
  else
  {
    // Successfully read the pulse width
    //
    // Purely diagnostic display for the HC-SR04.
    if(++displayCount % 10 == 0)
    {
      uint64_t cntValue;
      if(timerInfo->timerWidth == 16)
        cntValue = timerInfo->timerCount1 + (timerInfo->timerExtra1 * 0xffff);
      else
        cntValue = timerInfo->timerCount1 + (timerInfo->timerExtra1 * 0xffffffff);

      if(cntValue > 0)
      {
        // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec at 25 = 346.13
        double totalTime = (double)cntValue / (double)timerInfo->timerMaxClk;
        double oneWayTime = totalTime/2.0;
        double distance = oneWayTime /*seconds*/ * 345; /* meters/second*/
        syslog(1, "=====> Count:%lu, Time:%4.8fms, Distance:%1.6fm [overflow:%lu, ErrCnt:%u]\n",
                  cntValue, oneWayTime * 1000, distance, timerInfo->timerExtra1, timeOutErrCnt);
      }
      else
      {
        syslog(1, "--> ERROR:Timer %u count was %lu\n", timerInfo->timerNumb, cntValue);
      }
    }
  }

  return OK;
}

//=============================================================
// Pulse Width inititalization utilizing gate-controlled measurement.
int meadow_timer_init_gated_pulse_width(struct timerInfo_s *timerInfo)
{
  int ret;
  uint32_t timerBase = timerInfo->timerBase;
  
  if(mtcActiveChannel != 1 && mtcActiveChannel != 2)
  {
    syslog(1, "%s@%d-ERROR:Illegal mtcActiveChannel %d. Only 1 or 2 allowed\n",
              __FILE__, __LINE__, mtcActiveChannel);
    return -1;
  }

  // Disable slave mode while configuring
  uint32_t smcr_val = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  smcr_val &= ~GTIM_SMCR_DISAB;
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  // DMA/Interrupt enable register (DIER)
  // Don't know so set for a bunch of interrpts and keep the one the matter
  uint16_t regval16new;

  // What source generates an interrupts?
  // GTIM_DIER_UIE overflow or underflow
  // GTIM_DIER_TIE in gated mode when counter starts or stops
  regval16new = GTIM_DIER_UIE | GTIM_DIER_TIE;
  // regval16new = GTIM_DIER_TIE;
  // regval16new = GTIM_DIER_UIE |
  //               GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | GTIM_DIER_CC4IE |
  //               GTIM_DIER_TIE;
  
  syslog(1, "--> Setting up interrupt sources\n");
  // Set the interrupt sources
  // regval16 = getreg16(timerBase + STM32_GTIM_DIER_OFFSET);
  // regval16 |= regval16new;
  // putreg16(regval16, timerBase + STM32_GTIM_DIER_OFFSET);

  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_UIE   | GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | 
          GTIM_DIER_CC4IE | GTIM_DIER_COMIE | GTIM_DIER_TIE   | GTIM_DIER_BIE   |
          GTIM_DIER_UDE   | GTIM_DIER_CC1DE | GTIM_DIER_CC2DE | GTIM_DIER_CC3DE |
          GTIM_DIER_CC4DE | GTIM_DIER_COMDE | GTIM_DIER_TDE,
          regval16new);

  syslog(1, "--> Setting up Edge Detector and Gated Mode\n");

  // Note: Gated mode requires either channel 1 or 2. Channels 3 and 4 are
  // not useable for this function.
  // Set TI1 or TI2 Edge Detector and Gated Mode
  // GTIM_SMCR_TI1FP1 / GTIM_SMCR_TI1FP2
  if(mtcActiveChannel == 1)
    smcr_val |= (GTIM_SMCR_TI1FP1 | GTIM_SMCR_GATED);
  else    // mtcActiveChannel must be 2
    smcr_val |= (GTIM_SMCR_TI2FP2 | GTIM_SMCR_GATED);

  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  // All pulse width interupts are handled by same isr
  // Set the interrupt handler
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_pulse_width, timerInfo);
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

// #endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
