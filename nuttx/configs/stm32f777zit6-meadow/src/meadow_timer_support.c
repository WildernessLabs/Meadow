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
#define MEADOW_TIMER_EXPERIMENT_NUMBER (4)

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

// Meadow A0-A5 configured as digital output ports for DEBUGGING
#define MEADOW_DEBUG_PIN_V2_A0   (0x00040c04)
#define MEADOW_DEBUG_PIN_V2_A1   (0x00040c05)
#define MEADOW_DEBUG_PIN_V2_A2   (0x00040c03)
#define MEADOW_DEBUG_PIN_V2_A3   (0x00040c10)
#define MEADOW_DEBUG_PIN_V2_A4   (0x00040c11)
#define MEADOW_DEBUG_PIN_V2_A5   (0x00040c20)

#define MEADOW_TIMER_DEFAULT_CLOCK (96000000)
#define MEADOW_TIMER_16_BIT_OVERFLOW (65536)

#warning Experimental Code

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int _meadow_timer_exp_thread;
static sem_t _endPWidthSem;
static sem_t _endFreqDCSem;
static uint32_t _idleBeginCount;
static uint32_t _idleEndedCount;

struct timerInfo_s
{
  // Timer base address
  uint8_t timerNumb;                  // For diagnostics
  volatile uint8_t timerWidth;        // Either 16 or 32 bit wide (replace with func bit)
  volatile uint32_t timerCount1;      // Primary value of the count
  volatile uint32_t timerCount2;      // Secondary value of the count
  volatile uint32_t timerExtra1;      // Extra information 1
  volatile uint32_t timerExtra2;      // Extra information 2
  volatile uint32_t timerClkFreq;     // Running timer clock frequency
  uint32_t timerFunc;                 // Bit fields with the functions this timer has and can perform
  uint32_t timerBase;                 // Unique for each timer
  uint32_t timerMaxClk;               // Either 192MHz or 96MHz (replace with func bit)
  uint32_t timerAPBClk;               // Proper APB clock (replace with func bit)
  uint32_t timerClkEn;                // Clock enable
  uint32_t timerIrqVec;               // Interrupt vector
};

// PeterM - The timerFunc/timerFeat could be used to include:
// timer number in 4-bits, width in 1-bit and which STM32_RCC_APB2ENR or 
// clock in 1-bit
// There are 14 timers in the stm32f777
static struct timerInfo_s timerData[] =
{
            // Num  wid CC1 CC2 OF1 OF2 Frq Fnc     Base Addr       Max Clock Frequency    Correct APB Clock     Timer Enable         IRQ Vector
  /* TIM1   */  {1 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM1_BASE,  STM32_APB2_TIM1_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
  /* TIM2   */  {2 , 32, 0,  0,  0,  0,  0,  0,  STM32_TIM2_BASE,  STM32_APB1_TIM2_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
  /* TIM3   */  {3 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM3_BASE,  STM32_APB1_TIM3_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
  /* TIM4   */  {4 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM4_BASE,  STM32_APB1_TIM4_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
  /* TIM5   */  {5 , 32, 0,  0,  0,  0,  0,  0,  STM32_TIM5_BASE,  STM32_APB1_TIM5_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
  /* TIM6   */  {6 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM6_BASE,  STM32_APB1_TIM6_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
  /* TIM7   */  {7 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM7_BASE,  STM32_APB1_TIM7_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
  /* TIM8   */  {8 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM8_BASE,  STM32_APB2_TIM8_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
  /* TIM9   */  {9 , 16, 0,  0,  0,  0,  0,  0,  STM32_TIM9_BASE,  STM32_APB2_TIM9_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
  /* TIM10  */  {10, 16, 0,  0,  0,  0,  0,  0,  STM32_TIM10_BASE, STM32_APB2_TIM10_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
  /* TIM11  */  {11, 16, 0,  0,  0,  0,  0,  0,  STM32_TIM11_BASE, STM32_APB2_TIM11_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
  /* TIM12  */  {12, 16, 0,  0,  0,  0,  0,  0,  STM32_TIM12_BASE, STM32_APB1_TIM12_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
  /* TIM13  */  {13, 16, 0,  0,  0,  0,  0,  0,  STM32_TIM13_BASE, STM32_APB1_TIM13_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
  /* TIM14  */  {14, 16, 0,  0,  0,  0,  0,  0,  STM32_TIM14_BASE, STM32_APB1_TIM14_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
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

static int meadow_timer_isr_idle_measure(int irq, void *context, void *arg);
static int meadow_timer_init_idle_measure(struct timerInfo_s *timerInfo);
static int meadow_timer_test_idle_measure(struct timerInfo_s *timerInfo);

/****************************************************************************
 * Private Data
 ****************************************************************************/
// This is the beginning of a list of Meadow Timer Configuration (mtc) settings.
// These will eventually be set, directly or indirectly. by the .Net programmer.

// Filter out the 6us glitch from HC-SR04 when it finds no target.
static bool mtcHC_SR04Filter = true;

static bool mtcIncludeIdleMeasure = false;

// For now, select max of one mode of execution
static bool mtcPulseWidth = false;
static bool mtcFreqDutyCycle = true;

// In many cases only channels 1 or 2 will work.
static int mtcActiveChannel = 1;

// The following are all diagnostic and not production stuff
static struct timespec lastFDCTime;   // Watchdog for fdc
static uint32_t _idleRatio;
static volatile bool risingEdgeInterrupt;

static int debugCount = 0;  // DEBUG

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

//============================================================================
// This function is called for all frequency with duty cycle interrupts.
// On the first rising edge, of the input, the timer clears the CNT count and
// CNT begins counting up.
// On the following falling edge, the timer copies the CNT value into CCR2.
// On the next rising edge, the timer copies the CNT value into CCR1 and CNT
// is again cleared to zero and the process repeats.
// This means that we must read CCR1 and CCR2 between the rising edge and the
// falling edge.
// This is made more complex because we must maintain a count of all CNT
// overflow interupts because a 16-bit register at 96MHz will overflow every
// 683 microseconds. And currentl there is only 1 32-bit register available
int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg)
{
  if(!mtcFreqDutyCycle)
    return OK;
  
  risingEdgeInterrupt = false;

  debugCount++;    // DEBUG CODE

  // Why are we here? Check the timer's Status Register
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;

  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  //--------------------------------------------
  // Capture/Compare 1 signifies that input rising edge encountered.
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    // Clear this interrupt bit
    timStatusReg &= ~GTIM_SR_CC1IF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // syslog(1, "%d $$$$$> CC1^ Rising edge\n", debugCount);

    // Grab the current CCR1 value and, if it's a 16-bit register, add the
    // overflow totals.
    if(timerInfo->timerWidth == 16)
    {
      timerInfo->timerCount1 = (uint32_t)getreg16(timerBase + STM32_GTIM_CCR1_OFFSET);
      timerInfo->timerCount1 += timerInfo->timerExtra1;
      timerInfo->timerExtra1 = 0;      // Reset CNT overflow counts
    }
    else
    {
      timerInfo->timerCount1 = (uint32_t)getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
    }
    
    // Indicate that this interrupt means that CNT was set to zero. And that
    // the UIF interrupt flag should be ignored, since no overflow could have
    // occurred
    risingEdgeInterrupt = true;
  }

  //---------------------------------------------------
  // Capture/Compare 2 interrupt signifies an input falling edge. This
  // signifies that the timer has copied CNT into CCR2.
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Falling edge means we're done considering CCR2's value so we don't 
    // need to worry about CNT overflow with respect to CCR2.
    if(timerInfo->timerWidth == 16)
    {
      timerInfo->timerCount2 = (uint32_t)getreg16(timerBase + STM32_GTIM_CCR2_OFFSET);
      timerInfo->timerCount2 += timerInfo->timerExtra2;
      timerInfo->timerExtra2 = 0;
    }
    else
    {
      timerInfo->timerCount2 = (uint32_t)getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);
    }
  }

  //--------------------------------------------
  // Since only counting up, UIF means the CNT register overflowed or CNT was
  // reset to zero.
  // Note: At a clock speed of 96MHz, this interrupt is called every 683
  // microsec.
  //
  // UIF bit set when the first rising edge is detected which is when the
  // CNT, CC1 and CC2 registers are reset to zero, and at every overflow.
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Ignore first capture since it occurs when CNT is reset to zero.
    if(! risingEdgeInterrupt && timerInfo->timerWidth == 16)
    {
      // CNT overflow is always considered for count1, but considered for
      // count2 until first falling edge is detected.
      timerInfo->timerExtra1 += MEADOW_TIMER_16_BIT_OVERFLOW;
      timerInfo->timerExtra2 += MEADOW_TIMER_16_BIT_OVERFLOW;
    }
  }
  return OK;
}

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
int meadow_timer_support_setup()
{
  syslog(1, "--> %s@%d-timer support setup\n", __FILE__, __LINE__);

  lastFDCTime.tv_sec = 0;
  lastFDCTime.tv_nsec = 0;

  // Clear table values as needed
  for (int i = 0; i < MeadowTimerNumberOfTimers; i++)
  {
    timerData[i].timerCount1 = 0;
    timerData[i].timerCount2 = 0;
    timerData[i].timerExtra1 = 0;
    timerData[i].timerExtra2 = 0;
  }

  // DIAG Used to inspect software
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A0);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A1);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A2);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A3);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A4);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A5);

  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);

  // DIAG output for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);
  
  // TEMPORARY - this is defined just below MEADOW_TIMER_EXPERIMENT_NUMBER 
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
// Note: the test code is only able to execute one timer at a time, except for
// the idle measurement test which always uses Timer 1
void *meadow_timer_thread_func(int argc, char *argv[])
{
  int ret;
  struct timerInfo_s *idleTimerInfo;

// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
  usleep(10 * 1000);  
// #endif

  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);
  sem_init(&_endFreqDCSem, 0, 0);
  sem_setprotocol(&_endFreqDCSem, SEM_PRIO_NONE);

  // General timer initialization
  struct timerInfo_s *timerInfo = meadow_timer_init_general(MEADOW_TIMER_EXPERIMENT_NUMBER);
  if(timerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Meadow timer init failed\n", __FILE__, __LINE__);
    return NULL;
  }

  //------------------------------------------------------
  // Setup Timer 1 to do CPU/Idle utilization
  if(mtcIncludeIdleMeasure)
  {
    // Must do general initialization for Idle meassure separately
    idleTimerInfo = meadow_timer_init_general(1);
    if(timerInfo == NULL)
    {
      syslog(LOG_ERR, "%s@%d-Meadow idle measure init general failed\n", __FILE__, __LINE__);
      return NULL;
    }

    ret = meadow_timer_init_idle_measure(idleTimerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow idle measure init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }

    // Final initialization
    meadow_timer_enable(idleTimerInfo);
  }

  //------------------------------------------------------
  if(mtcPulseWidth)
  {
    ret = meadow_timer_init_gated_pulse_width(timerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }

  //------------------------------------------------------
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
  // Now run a test to insure everything works
  while(true)
  {
    usleep(997 * 1000);

    if(mtcIncludeIdleMeasure)
    {
      ret = meadow_timer_test_idle_measure(idleTimerInfo);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-Meadow Idle Measurement init failed:%d\n", __FILE__, __LINE__, ret);
        return NULL;
      }
    }

    if(mtcPulseWidth)
    {
      // For normal testing use the following sleep. Remove to do more of a
      // torture test
      // usleep(1000 * 1000);

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
  }

   return NULL;    // Keep compiler happy
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

//================================================================
// Test code for gated pulse width
int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo)
{
  // Just feed square wave into appropriate GPIO

  // Get the values in capture/compare register 1 and 2
  if(timerInfo->timerCount1 > 1 && timerInfo->timerCount2 > 1)
  {
    double dutyCycle = (double)(timerInfo->timerCount2 * 100.0)/(double)timerInfo->timerCount1;
    double freq = (double)(MEADOW_TIMER_DEFAULT_CLOCK)/(double)timerInfo->timerCount1;

    syslog(1, "===> Freq:%06.4fHz, DC:%02.2f%%, Cnt1:%lu, Cnt2:%lu\n",
              freq, dutyCycle,
              timerInfo->timerCount1,
              timerInfo->timerCount2);              
  }
  else
  {
    syslog(1, "+++> Invalid data CCR1:%lu, CCR2:%lu. Any zeros are bad.\n",
              timerInfo->timerExtra1, timerInfo->timerExtra2);
  }

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
  
  syslog(1, "--> FreqDC-Setting up interrupt sources\n");
  // Clear all interrupt sources and set the ones we want we need
  // DMA/Interrupt enable register (DIER)
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          // Advanced timers 1 & 8 ATIM_DIER_COMIE | ATIM_DIER_BIE | ATIM_DIER_COMDE
          GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_UIE);

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

  // Calculate by prescaler = timerInfo->timerMaxClk / desired frequency
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

//====================================================================
// Called from idle loop when idle has begun
void meadow_idle_has_begun(void)
{
  putreg32(0x00001000, STM32_GPIOB_BSRR); // Bit 12 sets PB12

  uint32_t currentCount = timerData[0].timerExtra1 + getreg16(STM32_TIM1_CNT);

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
  _idleEndedCount = timerData[0].timerExtra1 + getreg16(STM32_TIM1_CNT);
  putreg32(0x10000000, STM32_GPIOB_BSRR); // Bit 28 resets PB12
}
