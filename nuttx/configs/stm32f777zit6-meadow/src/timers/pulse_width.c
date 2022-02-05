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

struct pwidthInfo_s
{
  uint8_t timerNumb   : 4;            // 0 - 15 timer number as diagnostic
  uint8_t timerWidth  : 1;            // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;            // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;            // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerPolarity : 1;          // 0 = Leading is Rising, 1 = Leading is Falling
  volatile uint32_t timerCount1;      // Primary value of the count
  volatile uint32_t timerExtra1;      // Extra information 1
  uint32_t timerFreq;                 // Running timer clock frequency (could be prescaler value)
  uint32_t timerBase;                 // Unique for each timer
  uint32_t timerClkEn;                // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;               // Interrupt vector
};

static struct pwidthInfo_s pwidthInfoArray[] = 
{
            //   |--- bit-field---|
            //   #  wid max apb pol CC1 Ex1 Frq     Base Addr       Timer Clk Enable      IRQ Vector
  /* TIM1   */  {1 , 0,  1,  1,  0,  0,  0,  0,  STM32_TIM1_BASE,  RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
  /* TIM2   */  {2 , 1,  0,  0,  0,  0,  0,  0,  STM32_TIM2_BASE,  RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
  /* TIM3   */  {3 , 0,  0,  0,  0,  0,  0,  0,  STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
  /* TIM4   */  {4 , 0,  0,  0,  0,  0,  0,  0,  STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
  /* TIM5   */  {5 , 1,  0,  0,  0,  0,  0,  0,  STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
  /* TIM6   */  {6 , 0,  0,  0,  0,  0,  0,  0,  STM32_TIM6_BASE,  RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
  /* TIM7   */  {7 , 0,  0,  0,  0,  0,  0,  0,  STM32_TIM7_BASE,  RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
  /* TIM8   */  {8 , 0,  1,  1,  0,  0,  0,  0,  STM32_TIM8_BASE,  RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
  /* TIM9   */  {9 , 0,  1,  1,  0,  0,  0,  0,  STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
  /* TIM10  */  {10, 0,  1,  1,  0,  0,  0,  0,  STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
  /* TIM11  */  {11, 0,  1,  1,  0,  0,  0,  0,  STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
  /* TIM12  */  {12, 0,  0,  0,  0,  0,  0,  0,  STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
  /* TIM13  */  {13, 0,  0,  0,  0,  0,  0,  0,  STM32_TIM13_BASE, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
  /* TIM14  */  {14, 0,  0,  0,  0,  0,  0,  0,  STM32_TIM14_BASE, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_isr_pulse_width(int irq, void *context, void *arg);

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
static uint8_t meadow_timer_get_timer_numb(struct pwidthInfo_s *timerInfo)
{
  return timerInfo->timerNumb;
}

static uint32_t meadow_timer_get_apb_clock(struct pwidthInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

static uint32_t meadow_timer_get_max_clock(struct pwidthInfo_s *timerInfo)
{
  if(timerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

// static void meadow_timer_disable(uint32_t timerBase)
// {
//   uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
//   regval &= ~ATIM_CR1_CEN;
//   putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
// }

//=============================================================
static void meadow_timer_enable(uint32_t timerBase)
{
  // Why this order? tryed to copy the NUTTX order
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);

  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
}

//===================================================================
// This function is called only twice. Once for counter start and again for
// counter stop.
int meadow_timer_isr_pulse_width(int irq, void *context, void *arg)
{
  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct pwidthInfo_s *timerInfo = (struct pwidthInfo_s *)arg;
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
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
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
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
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
  // Clear table values as needed
  for (int i = 0; i < MEADOW_TIMERS_NUMB_OF_TIMERS; i++)
  {
    pwidthInfoArray[i].timerCount1 = 0;
    pwidthInfoArray[i].timerExtra1 = 0;
  }

  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);

  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(int timerNumber)
{
  int ret;
  struct pwidthInfo_s *timerInfo = &(pwidthInfoArray[timerNumber - 1]);
  
  // Need to "ARM" the system by clearing the previous count which is currently stopped.
  if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
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
      syslog(1, "--> ERROR:Timer%u count was %lu\n",
              meadow_timer_get_timer_numb(timerInfo), cntValue);
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

  struct pwidthInfo_s *timerInfo = &(pwidthInfoArray[timerNumber - 1]);
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
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff.
  // Set the prescaler value of 0 to allow highest speed. A prescaler value of
  // 1 will divide the clock by 2.
  uint16_t prescaler = 0;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);
  timerInfo->timerFreq = meadow_timer_get_max_clock(timerInfo);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = meadow_timer_get_max_clock(timerInfo) == \
                        MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
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

  meadow_timer_enable(timerBase);

  return OK;
}

// #endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
