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
#include <arch/board/board.h>

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include <nuttx/semaphore.h>
#include <nuttx/arch.h>

#include "stm32f777zit6-meadow.h"

#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"

// PeterM - still needed?
#include <nuttx/kthread.h>
#include <meadow/meadow_hw_version.h>

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
#if defined(true)
//===================================================================

#define MEADOW_TIMER_EXPERIMENT_THREAD_NAME "TimerExp"
#define MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY 120
#define MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE 2048

// ONLY F7v2 for testing
#define MEADOW_TIMER_TEST_GPIO_D14_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTB | GPIO_PIN12)
#define MEADOW_TIMER_TEST_GPIO_D15_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTG | GPIO_PIN12)

// Input points to TIMx_CHx
// Note the alternate function entries are non-optional and vary with different timer/channels
// #define MEADOW_F7V1_TIM5_CH1_PH10_D10  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTH | GPIO_PIN10)
#define MEADOW_F7VX_TIM4_CH1_PB6_D08  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTB | GPIO_PIN6)
#define MEADOW_F7V2_TIM5_CH1_PH10_D02  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTH | GPIO_PIN10)

// #define MEADOW_F7V1_TIM8_CH1_PC6_D02   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTC | GPIO_PIN6)
#define MEADOW_F7V2_TIM8_CH1_PC6_D09   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTC | GPIO_PIN6)

#define MEADOW_F7VX_TIM10_CH1_PB8_D03   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTB | GPIO_PIN8)
#define MEADOW_F7VX_TIM11_CH1_PB9_D04   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTB | GPIO_PIN9)

// DURING DEVELOPMENT ONLY F7v2 is supported
// INSURE THAT THE mtcActiveChannel VALUE LINES UP WITH THE GPIO SELECTION.
// The following will eventually be in the timer table or switch statment or ???
#define MEADOW_TIMER_EXPERIMENT_NUMBER (5)

#if MEADOW_TIMER_EXPERIMENT_NUMBER == 4
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM4_CH1_PB6_D08)
#elif MEADOW_TIMER_EXPERIMENT_NUMBER == 5
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM5_CH1_PH10_D02)
#elif MEADOW_TIMER_EXPERIMENT_NUMBER == 8
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7V2_TIM8_CH1_PC6_D09)
#elif MEADOW_TIMER_EXPERIMENT_NUMBER == 10
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM10_CH1_PB8_D03)
#elif MEADOW_TIMER_EXPERIMENT_NUMBER == 11
#define MEADOW_TIMER_APPROPRIATE_TIM_INPUT (MEADOW_F7VX_TIM11_CH1_PB9_D04)
#else
#error Unsupported Timer Number
#endif

#warning Experimental Code

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int _meadow_timer_exp_thread;
static sem_t _endCountSem;

struct timerInfo_s
{
  // Timer base address
  uint8_t timerNumb;        // For diagnostics
  uint32_t timerFunc;       // Bit fields with the functions this timer has and can perform
  uint8_t timerWidth;       // Either 16 or 32 bit wide
  uint32_t timerCountA;     // Primary value of the count
  uint32_t timerCountB;     // Secondary value of the count
  uint32_t timerOverflow;    // The number of times overflowed
  uint32_t timerBase;       // Unique for each timer
  uint32_t timerClkFreq;    // Either 192MHz or 96MHz
  uint32_t timerAPBClk;     // Proper APB clock
  uint32_t timerClkEn;      // Clock enable
  uint32_t timerIrqVec;     // Interrupt vector
};

// PeterM - The timerFunc could be used to include:
// timer number in 4-bits, width in 1-bit and which STM32_RCC_APB2ENR or 
// clock in 1-bit
// There are 14 timers in the stm32f777
static struct timerInfo_s timerData[] =
{
            // Num fnc wid CA CB  OF   Base Addr      Clock Frequency         Correct APB Clock    Timer Enable         IRQ Vector
  /* TIM1   */  {1 , 0, 16, 0, 0, 0, STM32_TIM1_BASE,  STM32_APB2_TIM1_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
  /* TIM2   */  {2 , 0, 32, 0, 0, 0, STM32_TIM2_BASE,  STM32_APB1_TIM2_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
  /* TIM3   */  {3 , 0, 16, 0, 0, 0, STM32_TIM3_BASE,  STM32_APB1_TIM3_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
  /* TIM4   */  {4 , 0, 16, 0, 0, 0, STM32_TIM4_BASE,  STM32_APB1_TIM4_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
  /* TIM5   */  {5 , 0, 32, 0, 0, 0, STM32_TIM5_BASE,  STM32_APB1_TIM5_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
  /* TIM6   */  {6 , 0, 16, 0, 0, 0, STM32_TIM6_BASE,  STM32_APB1_TIM6_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
  /* TIM7   */  {7 , 0, 16, 0, 0, 0, STM32_TIM7_BASE,  STM32_APB1_TIM7_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
  /* TIM8   */  {8 , 0, 16, 0, 0, 0, STM32_TIM8_BASE,  STM32_APB2_TIM8_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
  /* TIM9   */  {9 , 0, 16, 0, 0, 0, STM32_TIM9_BASE,  STM32_APB2_TIM9_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
  /* TIM10  */  {10, 0, 16, 0, 0, 0, STM32_TIM10_BASE, STM32_APB2_TIM10_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
  /* TIM11  */  {11, 0, 16, 0, 0, 0, STM32_TIM11_BASE, STM32_APB2_TIM11_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
  /* TIM12  */  {12, 0, 16, 0, 0, 0, STM32_TIM12_BASE, STM32_APB1_TIM12_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
  /* TIM13  */  {13, 0, 16, 0, 0, 0, STM32_TIM13_BASE, STM32_APB1_TIM13_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
  /* TIM14  */  {14, 0, 16, 0, 0, 0, STM32_TIM14_BASE, STM32_APB1_TIM14_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
};

#define MeadowTimerNumberOfTimers (14)

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void *meadow_timer_thread_func(int argc, char *argv[]);
static struct timerInfo_s * meadow_timer_init_general(int timerNumber);
static void meadow_timer_enable(struct timerInfo_s *timerInfo);
static void meadow_timer_disable(struct timerInfo_s *timerInfo);

static int meadow_timer_isr_pulse_width(int irq, void *context, void *arg);
static int meadow_timer_init_gated_pulse_width(struct timerInfo_s *timerInfo);
static int meadow_timer_test_gated_pulse_width(struct timerInfo_s *timerInfo);

static int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg);
static int meadow_timer_init_freq_and_dutycycle(struct timerInfo_s *timerInfo);
static int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo);

/****************************************************************************
 * Private Data
 ****************************************************************************/
// This is the beginning of a list of Meadow Timer Configuration (mtc) settings.
// These will eventually be set, directly or indirectly. by the .Net programmer.

// Filter out the 6us glitch from HC-SR04 when it finds no target.
static bool mtcHC_SR04Filter = true;

static bool mtcIncludeIdleTimer = true;

// Select only one
static bool mtcPulseWidth = false;
static bool mtcFreqDutyCycle = true;

// In many cases only channels 1 or 2 will work.
static int mtcActiveChannel = 1;

static struct timespec lastAbsTime;

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

// uint16_t getreg16(unsigned int addr);
// void modifyreg16(unsigned int addr, uint16_t clearbits, uint16_t setbits);
// void putreg16(regval, unsigned int addr);
// stm32_gpiowrite(pin_set, t/f);
// t/f = stm32_gpioread(pin_set);

//============================================================================
// This function is called for all interrupts configured for measureing frequency and
// duty cycle
int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg)
{
  static struct timespec abstime;
  
  // Why are we here? Check the timer's Status Register
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;

  // Why are we here? Check the timer's Status Register
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  //--------------------------------------------
  // Channel one interrupt
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    timStatusReg &= ~GTIM_SR_CC1IF;

    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    (void)clock_gettime(CLOCK_REALTIME, &abstime);
    if(lastAbsTime.tv_sec <= abstime.tv_sec)
    {
      uint32_t chan1Val = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
      if(chan1Val > 1)
      {
        timerInfo->timerCountA = chan1Val;
        timerInfo->timerCountB = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);
        
        sem_post(&_endCountSem); // Allow the requesting thread to process data

        // If we don't enter this 'if' statement we will on next interrupt
        lastAbsTime.tv_sec = abstime.tv_sec + 2;
      }
    }
  }

  // Since only counting up, UIF means we overflowed
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // GTIM_SR_UIF indicates overflow or underflow of main counter
    timerInfo->timerOverflow++;
  }

  return OK;
}

//============================================================================
// This function is called for all interrupts configured for measureing pulse width
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
        timerInfo->timerCountA = (uint32_t)getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
      else
        timerInfo->timerCountA = getreg32(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

      if(!mtcHC_SR04Filter)
      {
        sem_post(&_endCountSem); // Allow the requesting thread to process data
        return OK;
      }

      // Since HC-SR04 filter is requested we do the following. Why?
      // When there is no target the HC-SR04 output goes high for about 125ms
      // then low for about 150us then high again for about 6us. At 96MHz the
      // count for 6us is between 575 and 578. The following prevents this
      // effect from interfering with normally expected behavior.
      if(timerInfo->timerCountA > 574 && timerInfo->timerCountA < 579)
      {
        // Throw away the count. This way a zero reading is returned
        timerInfo->timerCountA = 0;
        timerInfo->timerOverflow = 0;
      }
      else
      {
        sem_post(&_endCountSem);
      }
      return OK;
    }
  }

  //----------------------------------------------------------
  if(timStatusReg & GTIM_SR_UIF)
  {
    // GTIM_SR_UIF indicates overflow or underflow of main counter
    timerInfo->timerOverflow++;

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
int meadow_timer_support_setup()
{
  syslog(1, "--> %s@%d-timer support setup\n", __FILE__, __LINE__);

  lastAbsTime.tv_sec = 0;
  lastAbsTime.tv_nsec = 0;

  // Clear table values as needed
  for (int i = 0; i < MeadowTimerNumberOfTimers; i++)
  {
    timerData[i].timerCountA = 0;
    timerData[i].timerCountB = 0;
    timerData[i].timerOverflow = 0;
  }

  // DIAG Used to inspect software
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);

  // DIAG Used for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);
  
  // For the time being, this is defined just below MEADOW_TIMER_EXPERIMENT_NUMBER 
  stm32_configgpio(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);

  syslog(1, "==> %s@%d-Creating timer experiment thread\n", __FILE__, __LINE__);

  // Create a thread to use for experimenting
  _meadow_timer_exp_thread = kthread_create(MEADOW_TIMER_EXPERIMENT_THREAD_NAME,
                                  MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY,
                                  MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE,
                                  (main_t) meadow_timer_thread_func,
                                  (char *const *) NULL);
  if (_meadow_timer_exp_thread <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              __FILE__, __LINE__, MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
    return -ENOEXEC;
  }

  return OK;
}

//========================================================
// New thread for running tests
void *meadow_timer_thread_func(int argc, char *argv[])
{
  int ret;
  
// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
  usleep(10 * 1000);  
// #endif

  sem_init(&_endCountSem, 0, 0);
  sem_setprotocol(&_endCountSem, SEM_PRIO_NONE);

  // General timer initialization
  struct timerInfo_s *timerInfo = meadow_timer_init_general(MEADOW_TIMER_EXPERIMENT_NUMBER);
  if(timerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Meadow timer init failed\n", __FILE__, __LINE__);
    return NULL;
  }

  // Setup Timer 1 to do CPU/Idle utilization
  // if(mtcIncludeIdleTimer)
  // {
  //   struct timerInfo_s *idleTimerInfo = meadow_timer_init_general(1);
  //   ret = meadow_timer_init_gated_pulse_width(timerInfo);
  //   if(ret < 0)
  //   {
  //     syslog(LOG_ERR, "%s@%d-Meadow pulsh width init failed:%d\n", __FILE__, __LINE__, ret);
  //     return NULL;
  //   }
    
  //   // Final initialization
  //   meadow_timer_enable(idleTimerInfo);
  // }

  // Purpose specific initializations
  if(mtcPulseWidth)
  {
    ret = meadow_timer_init_gated_pulse_width(timerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  } 
  else if(mtcFreqDutyCycle)
  {
    ret = meadow_timer_init_freq_and_dutycycle(timerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }
  
  // Final initialization
  meadow_timer_enable(timerInfo);

  //-----------------------------------------------------------------------
  // Now run some tests
  if(mtcPulseWidth)
   {
    ret = meadow_timer_test_gated_pulse_width(timerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  } 
  else if(mtcFreqDutyCycle)
  {
    ret = meadow_timer_test_freq_and_dutycycle(timerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }
   return NULL;    // Keep compiler happy
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo)
{
  // Just feed square wave into appropriate GPIO
  
  // Use this thread to capture the data and display periodically. We'll
  // let the interrupt handler tell us when this is.

  while(true)
  {
    sem_wait(&_endCountSem);

    // Get the values in the capture/compare register
    double chan1Val = (double)timerInfo->timerCountA;
    double chan2Val = (double)timerInfo->timerCountB;

    if(chan1Val > 1.0 && chan2Val > 1.0)
    {
      double dutyCycle = ((chan2Val + 1.0) * 100.0)/(chan1Val);
      double freq = 96000000.0/(chan1Val + 1.0);

      syslog(1, "===> Freq:%5.2fHz, DutyCycle:%3.2f%%, A:%lu, B:%lu\n",
                freq, dutyCycle, timerInfo->timerCountA, timerInfo->timerCountB);
    
      // timerInfo->timerCountA = 0;
      // timerInfo->timerCountB = 0;
    }
    else
    {
      syslog(1, "+++> Invalid data received A:%lu, B:%lu\n", timerInfo->timerCountA, timerInfo->timerCountB);
    }
  }
  
  return OK;
}

//=============================================================
// Frequency and duty cycle measurement.
int meadow_timer_init_freq_and_dutycycle(struct timerInfo_s *timerInfo)
{
  // See RM0410 Reference manual for STM32F76xxx and STM32F77xxx section 26.3.6
  // for original concept.

  // A single input (T1) is used. It is configured as input to Compare/Capture
  // registers 1 and 2. For 1 compare/capture register it is configured to 
  // for rising edge interrupt and falling edge for ccr2. By using the
  // count between interrupts for one CCR's (i.e. rising to rising edges)
  // the frequency can be found by counting between interrupts from rising to
  // falling the duty cycle can be found.

  int ret;
  uint16_t regVal16;
  uint32_t regVal32;
  uint32_t timerBase = timerInfo->timerBase;

  // Before starting disable capture/control for input 1 and 2 by setting CC1E
  // and CC2E to 0. Ref Man (26.4.7 at end) "Note: CC1S bits are writable only
  // when the channel is OFF (CC1E = 0 in TIMx_CCER)."
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xffcc;   // c = 1110, clear CC1E and CC2E bits 0 & 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // 1. Select the active input for TIMx_CCR1: write the CC1S bits to 01 in
  // the TIMx_CCMR1 register (TI1 selected).
  // Ref Man "01: CC1 channel is configured as input, IC1 is mapped on TI1."
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffffc;   // c = 1100, clear CC1S bits 1:0
  regVal32 |= 0x00000001;   // 1 = 0001, set '01'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // 2. Select the active polarity for TI1FP1 (used both for capture in
  // TIMx_CCR1 and counter clear): write the CC1P to ‘0’ and the CC1NP bit to
  // ‘0’ (active on rising edge).
  //
  // Note: the CCR1 register is readonly. Capture/Compare Enable Register (CCER)
  // is were the polarity is set by CC1P & CC1NP. In Ref Man the CC1P for input
  // discribes both the CC1P and CC1NP bit as if a 2 bit field. But they are
  // actually bit 1 and bit 3. Ref Man "00: noninverted/rising edge
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xfff5;   // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // Map appropriate input to the channel 2 capture/compare register
  // 3. Select the active input for TIMx_CCR2: write the CC2S bits to 10 in the TIMx_CCMR1
  // register (TI1 selected).
  // Ref Man "10: CC2 channel is configured as input, IC2 is mapped on TI1"
  regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
  regVal32 &= 0xfffffcff;   // c = 1100, clear CC2S bits 9:8
  regVal32 |= 0x00000200;   // 2 = 0010, set to '10'
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // 4. Select the active polarity for TI1FP2 (used for capture in TIMx_CCR2): write the CC2P
  // bit to ‘1’ and the CC2NP bit to ’0’ (active on falling edge).
  // Ref Man "01: inverted/falling edge
  //  Circuit is sensitive to TIxFP1 falling edge (capture, trigger in reset,
  //  external clock or trigger mode), TIxFP1 is inverted (trigger in gated
  //  mode, encoder mode)."
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xff5f;   // 5 = 0101, clear GTIM_CCER_CC2NP (bit 7) & GTIM_CCER_CC2P (bit 5).
  regVal16 |= 0x0020;   // 2 = 0010 set bit 5 and leave bit 7 clear
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // 5. Select the valid trigger input: write the TS bits to 101 in the TIMx_SMCR
  // register (TI1FP1 selected). Ref Man "101: Filtered Timer Input 1 (TI1FP1)"
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= 0xffffff8f;   // 8 = 1000, clear TS bits 6:4
  regVal32 |= 0x00000050;   // 5 = 0101 sets '101'
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??

  // 6. Configure the slave mode controller in reset mode: write the SMS bits
  // to 100 in the TIMx_SMCR register. Reg Man "0100: Reset Mode - Rising edge
  // of the selected trigger input (TRGI) reinitializes the counter and
  // generates an update of the registers."
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);   // ?? NEEDED??
  regVal32 &= 0xfffefff8;    // e = 1110, Clear SMS bit 16, 8 = 1000, clear 2:0
  regVal32 |= 0x00000004;    // 4 = 0100 sets 2:0 = '100', leave bit 16 = 0
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // 7. Enable the captures: write the CC1E (bit 0) and CC2E (bit 4) bits to
  // ‘1' in the TIMx_CCER register.
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 |= 0x0011;   // set bit 0 and bit 4
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Which interrupts?
  uint16_t regval16new;
  // regval16new = GTIM_DIER_UIE | GTIM_DIER_TIE | GTIM_DIER_CC1IE | GTIM_DIER_CC2IE;
  // regval16new = GTIM_DIER_UIE | GTIM_DIER_TIE;
  // regval16new = GTIM_DIER_CC1IE | GTIM_DIER_CC2IE;
  regval16new = GTIM_DIER_CC1IE;
  // regval16new = GTIM_DIER_TIE;
  // regval16new = GTIM_DIER_UIE |
  //               GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | GTIM_DIER_CC4IE |
  //               GTIM_DIER_TIE;
  
  syslog(1, "--> Setting up interrupt sources\n");
  // Clear all interrupt sources and set the ones we want
  // we want
  // DMA/Interrupt enable register (DIER)
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_UIE   | GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | 
          GTIM_DIER_CC4IE | GTIM_DIER_COMIE | GTIM_DIER_TIE   | GTIM_DIER_BIE   |
          GTIM_DIER_UDE   | GTIM_DIER_CC1DE | GTIM_DIER_CC2DE | GTIM_DIER_CC3DE |
          GTIM_DIER_CC4DE | GTIM_DIER_COMDE | GTIM_DIER_TDE,
          regval16new);

  // All Frequency/Duty Cycle interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_freq_dutycycle, timerInfo);
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

//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(struct timerInfo_s *timerInfo)
{
  int ret;
  int displayCount = 0;

  // Attempt to simulate normal operation via this loop.
  // Note: this is built around the HC-SR04.
  uint32_t timeOutErrCnt = 0;
  while(true)
  {
    // For normal testing use the following sleep. Remove it to do more of a
    // torture test
    // usleep(1000 * 1000);

    // Clear all timer counter's
    if(timerInfo->timerWidth == 16)
      putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
    else
      putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
    
    timerInfo->timerOverflow = 0;

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
    ret = sem_timedwait(&_endCountSem, &abstime);
    if(ret < 0)
    {
      if(errno == ETIMEDOUT)
      {
        timeOutErrCnt++;
        syslog(1, "--> ERROR:Pulse width-Semaphore timeout:%u, ret:%d, errno:%d\n", timeOutErrCnt, ret, errno);

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
      sem_getvalue(&_endCountSem, &semcount);
      if(semcount == 0)
        sem_post(&_endCountSem); 
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
          cntValue = timerInfo->timerCountA + (timerInfo->timerOverflow * 0xffff);
        else
          cntValue = timerInfo->timerCountA + (timerInfo->timerOverflow * 0xffffffff);

        if(cntValue > 0)
        {
          // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec at 25 = 346.13
          double totalTime = (double)cntValue / (double)timerInfo->timerClkFreq;
          double oneWayTime = totalTime/2.0;
          double distance = oneWayTime /*seconds*/ * 345; /* meters/second*/
          syslog(1, "=====> Count:%lu, Time:%4.8fms, Distance:%1.6fm [overflow:%lu, ErrCnt:%u]\n",
                    cntValue, oneWayTime * 1000, distance, timerInfo->timerOverflow, timeOutErrCnt);
        }
        else
        {
          syslog(1, "--> ERROR:Timer %u count was %lu\n", timerInfo->timerNumb, cntValue);
        }
      }
    }
  }

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)

//=============================================================
// Scheme from stm32_tim.c stm32_tim_disable()
void meadow_timer_disable(struct timerInfo_s *timerInfo)
{
  uint32_t timerBase = timerInfo->timerBase;

  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
}

//=============================================================
// Scheme from stm32_tim.c stm32_tim_enable()
void meadow_timer_enable(struct timerInfo_s *timerInfo)
{
  uint32_t timerBase = timerInfo->timerBase;

  // Why this order? tryed to copy the NUTTX order
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;
  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);
  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
}

//=============================================================
// The provide timerNumber (1-14) to configure. Returns the correct table entry.
struct timerInfo_s * meadow_timer_init_general(int timerNumber)
{
  uint16_t regval;
  uint16_t prescaler;
  struct timerInfo_s *timerInfo = &(timerData[timerNumber - 1]);

  uint32_t timerBase = timerInfo->timerBase;

  // Setup the clock to enable
  modifyreg32(timerInfo->timerAPBClk, 0, timerInfo->timerClkEn);

  meadow_timer_disable(timerInfo);

  // Calculate by prescaler = timerInfo->timerClkFreq / desired frequency
  // Must be between 0 and 0xffff.
  prescaler = 0;  // Set the prescaler value of 0 to allow highest speed
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == 16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // Not sure if this is needed
  //  regval = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  //   // From Reference 25.3.11
  //   // As the preload registers are transferred to the shadow registers only when an update event
  //   // occurs, before starting the counter, you have to initialize all the registers by setting the UG
  //   // bit in the TIMx_EGR register.
  //   regval |= GTIM_EGR_UG;   /* Bit 0: Update generation */
  //   putreg16(regval, timerBase + STM32_GTIM_EGR_OFFSET);

  // // If timer channel input, tell Nuttx what we care about
  // ret = stm32_gpiosetevent(
  //           cfgset,               // special gpio for call
  //           1,                    // risingEdge,
  //           1,                    // fallingEdge,
  //           0,                    // event
  //           upd_gpio_interrupt,   // function to call
  //           gpioMapTblPtr);       // table entry pointer
  
  return timerInfo;
}
