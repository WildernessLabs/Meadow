/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/pulse_width.c
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

#include "meadow_timers.h"

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

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

// CONFIGURATION
// Filter out the 6us glitch from HC-SR04 when it finds no target.
static bool mtcHC_SR04Filter = true; // Used with pulse width only

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This function is called only twice. Once for counter start and again for
// counter stop.
int meadow_timer_isr_pulse_width(int irq, void *context, void *arg)
{
  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;

  // Why are we here? Check the timer's Status Register
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);
  
  // Check the status register and acknowledge all interrupts
  if(timStatusReg & GTIM_SR_TIF)
  {
    // Clear interrupt
    timStatusReg &= ~GTIM_SR_TIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // In gated mode GTIM_SR_TIF occurs when counter is started or stopped.
    // The CNT register must have already been set to 0. On the trailing
    // edge the CNT value stops counting
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
        // These values indicate a very fast pulse
        // PeterM - THESE HARD CODED VALUES ARE BAD!!! THEY ARE RELATIVE TO THE
        // TIMER'S CLOCK FREQ AND BASED ON A 96MHz CLOCK.
        if(timerInfo->timerCount1 > 574 && timerInfo->timerCount1 < 579)
        {
          // Throw away the count. This way a zero reading is returned
          timerInfo->timerCount1 = 0;
          timerInfo->timerExtra1 = 0;
        }
      }
      
      // If 16-bit add the CNT overflows
      if(timerInfo->timerWidth == 16)
        timerInfo->timerCount1 += timerInfo->timerExtra1 * MEADOW_TIMER_16_BIT_OVERFLOW;

      sem_post(&_endPWidthSem); // Allow the requesting thread to process data
    }
    else
    {
      // Leading edge indicates start so clear the previous values
      timerInfo->timerCount1 = 0;
      timerInfo->timerExtra1 = 0;
    }
  }

  //----------------------------------------------------------
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // GTIM_SR_UIF indicates overflow or underflow of CNT, since we only count
    // up it must mean overflow.
    timerInfo->timerExtra1++;
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_timer_setup_pulse_width()
{
  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);

  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(int timerNumber)
{
  int ret;
  struct timerInfo_s *timerInfo = &(timerInfoArray[timerNumber - 1]);
  
  // Need to "ARM" the system by clearing the count which is currently stopped.
  if(timerInfo->timerWidth == 16)
    putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
  else
    putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

  // Should be done by MONO code
  // Send pulse to HC-SR04, this must be at least 2us wide to signal the
  // HC-SR04 to send ultrasonic pulses and wait for the echo.
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, true);

  // HC-SR04 requires a trigger pulse of at least 2 usec. However, a 1-2 ms
  // pulse works too. Because of the Nuttx usleep resolution the following
  // usleep(1) will sleep between 1-2 ms. The HC-SR04 generates it's timing
  // pulse (echo) about 500us after the trigger pulses falling edge.
  usleep(1);

  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, false);

  // Wait for ISR to indicate timer has finished
  struct timespec abstime;
  ret = clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += 1;    // This delay should be supplied by .Net user?
  abstime.tv_nsec = 0;
  ret = sem_timedwait(&_endPWidthSem, &abstime);
  if(ret < 0)
  {
    // An error can means that the semaphore timed out. In this case we to
    // insure that the semaphore count is correct. If not correct, it means
    // that the ISR didn't do the sem_post() call. Therefore, we need to call
    // sem_post to keep the semaphore in sync with the ISR.
    syslog(LOG_ERR, "ERROR:Pulse width-Semaphore ret:%d, errno:%d\n", ret, errno);

    int semcount;
    sem_getvalue(&_endPWidthSem, &semcount);

    if(semcount == 0)
      sem_post(&_endPWidthSem); 
  }
  else
  {
    // Successfully read the pulse width. Display for the HC-SR04.
    uint32_t cntValue = timerInfo->timerCount1;
    if(cntValue > 0)
    {
      // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec,
      // at 25C = 346.13
      double totalTimeMs = (double)cntValue / (double)timerInfo->timerFreq;
      double oneWayTimeMs = totalTimeMs/2.0;
      double distance = oneWayTimeMs /*seconds*/ * 345; /* meters/second*/
      syslog(1, "=====> Count:%lu, Time:%4.8fms, Distance:%1.6fm\n",
                cntValue, oneWayTimeMs * 1000, distance);
    }
    else
    {
      syslog(1, "--> ERROR:Timer%u count was %lu\n", timerInfo->timerNumb, cntValue);
    }
  }

  return OK;
}

//=============================================================
// Pulse Width inititalization utilizing gate-controlled measurement.
// This is only useable with channels 1 & 2. Channel 1 
int meadow_timer_init_gated_pulse_width(int timerNumber)
{
  int ret;

  struct timerInfo_s *timerInfo = &(timerInfoArray[timerNumber - 1]);
  uint32_t timerBase = timerInfo->timerBase;

  if(MEADOW_TIMER_CHANNEL_BEING_USED != 1 && MEADOW_TIMER_CHANNEL_BEING_USED != 2)
  {
    syslog(1, "%s@%d-ERROR:Illegal MEADOW_TIMER_CHANNEL_BEING_USED %d. Only 1 or 2 allowed\n",
              __FILE__, __LINE__, MEADOW_TIMER_CHANNEL_BEING_USED);
    return -1;
  }

  // Output for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);

  // Disable slave mode while configuring
  uint32_t smcr_val = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  smcr_val &= ~GTIM_SMCR_DISAB;
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  //------------------------------------------
  // Setup the clock enable
  modifyreg32(timerInfo->timerAPBClk, 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff.
  // Set the prescaler value of 0 to allow highest speed. A prescaler value of
  // 1 will divide the clock by 2.
  uint16_t prescaler = 0;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);
  timerInfo->timerFreq = timerInfo->timerMaxClk;

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == 16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  //------------------------------------------
  // DMA/Interrupt enable register (DIER)
  // What generates an interrupts?
  // GTIM_DIER_UIE overflow or underflow
  // GTIM_DIER_TIE in gated mode when counter starts or stops
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_UIE   | GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | 
          GTIM_DIER_CC4IE | GTIM_DIER_COMIE | GTIM_DIER_TIE   | GTIM_DIER_BIE   |
          GTIM_DIER_UDE   | GTIM_DIER_CC1DE | GTIM_DIER_CC2DE | GTIM_DIER_CC3DE |
          GTIM_DIER_CC4DE | GTIM_DIER_COMDE | GTIM_DIER_TDE,
          GTIM_DIER_UIE | GTIM_DIER_TIE);

  syslog(1, "--> Setting up Edge Detector and Gated Mode\n");

  // Note: Gated mode requires either channel 1 or 2. Channels 3 and 4 are
  // not useable for this function. And cannot use both channel 1 and 2.
  // Set TI1 or TI2 Edge Detector and Gated Mode
  // GTIM_SMCR_TI1FP1 / GTIM_SMCR_TI1FP2
  if(MEADOW_TIMER_CHANNEL_BEING_USED == 1)
    smcr_val |= (GTIM_SMCR_TI1FP1 | GTIM_SMCR_GATED);
  else if(MEADOW_TIMER_CHANNEL_BEING_USED == 2)
    smcr_val |= (GTIM_SMCR_TI2FP2 | GTIM_SMCR_GATED);
  else
    return -ENODEV;   // No device because must be 1 or 2

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

  meadow_timer_enable(timerInfo);

  return OK;
}

// #endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
