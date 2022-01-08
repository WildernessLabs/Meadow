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

// #include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <arch/board/board.h>
// #include <nuttx/mqueue.h>
// #include <nuttx/signal.h>
// #include <nuttx/drivers/pwm.h>
// #include <nuttx/spi/spi.h>

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32_pwm.h"
#include "stm32f777zit6-meadow.h"

#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"

// PeterM - still needed?
#include <nuttx/kthread.h>

#include <meadow/meadow_hw_version.h>

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
#if defined(true)

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
#define MEADOW_F7V2_TIM5_CH1_PH10_D02  (GPIO_ALT | GPIO_AF2 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTH | GPIO_PIN10)

// #define MEADOW_F7V1_TIM8_CH1_PC6_D02   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTC | GPIO_PIN6)
#define MEADOW_F7V2_TIM8_CH1_PC6_D09   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTC | GPIO_PIN6)

#define MEADOW_F7VX_TIM10_CH1_PB8_D03   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTB | GPIO_PIN8)
#define MEADOW_F7VX_TIM11_CH1_PB9_D04   (GPIO_ALT | GPIO_AF3 | GPIO_INPUT | GPIO_FLOAT | GPIO_PORTB | GPIO_PIN9)

// The following will eventually be in the timer table or a large switch statment TBD.
#define MEADOW_TIMER_EXPERIMENT_NUMBER (5)
#if MEADOW_TIMER_EXPERIMENT_NUMBER == 5
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

struct timerInfo_s
{
  // Timer base address
  uint8_t timerNumb;        // For diagnostics
  uint32_t timerFunc;       // Bit map with the functions this timer has and can perform
  uint32_t timerBase;       // Unique for each timer
  uint8_t timerWidth;       // Either 16 or 32 bit wide
  uint32_t timerCount;      // The value of the count
  uint32_t timerClkFreq;    // Either 192MHz or 96MHz
  uint32_t timerAPBClk;     // Proper APB clock
  uint32_t timerClkEn;      // Clock enable
  uint32_t timerIrqVec;     // Interrupt vector
};

// There are 14 timers in the stm32f777
static struct timerInfo_s timerData[] =
{
            // Numb func  Base Addr    width Cnt     Clock Frequency      Correct APB Clock     Timer Enable         IRQ Vector
  /* TIM1   */  {1 , 0, STM32_TIM1_BASE,  16, 0, STM32_APB2_TIM1_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
  /* TIM2   */  {2 , 0, STM32_TIM2_BASE,  32, 0, STM32_APB1_TIM2_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
  /* TIM3   */  {3 , 0, STM32_TIM3_BASE,  16, 0, STM32_APB1_TIM3_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
  /* TIM4   */  {4 , 0, STM32_TIM4_BASE,  16, 0, STM32_APB1_TIM4_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
  /* TIM5   */  {5 , 0, STM32_TIM5_BASE,  32, 0, STM32_APB1_TIM5_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
  /* TIM6   */  {6 , 0, STM32_TIM6_BASE,  16, 0, STM32_APB1_TIM6_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
  /* TIM7   */  {7 , 0, STM32_TIM7_BASE,  16, 0, STM32_APB1_TIM7_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
  /* TIM8   */  {8 , 0, STM32_TIM8_BASE,  16, 0, STM32_APB2_TIM8_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
  /* TIM9   */  {9 , 0, STM32_TIM9_BASE,  16, 0, STM32_APB2_TIM9_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
  /* TIM10  */  {10, 0, STM32_TIM10_BASE, 16, 0, STM32_APB2_TIM10_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
  /* TIM11  */  {11, 0, STM32_TIM11_BASE, 16, 0, STM32_APB2_TIM11_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
  /* TIM12  */  {12, 0, STM32_TIM12_BASE, 16, 0, STM32_APB1_TIM12_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
  /* TIM13  */  {13, 0, STM32_TIM13_BASE, 16, 0, STM32_APB1_TIM13_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
  /* TIM14  */  {14, 0, STM32_TIM14_BASE, 16, 0, STM32_APB1_TIM14_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
};

#define MeadowTimerNumberOfTimers (14)

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_isr(int irq, void *context, void *arg);
static void *_meadow_timer_thread_func(int argc, char *argv[]);
// static int send_echo_sequence(struct timerInfo_s *timerInfo);
static int meadow_timer_init_gated_pulse_width(struct timerInfo_s *timerInfo);
static struct timerInfo_s * meadow_timer_init_general(int timerNumber);
static void meadow_timer_enable(struct timerInfo_s *timerInfo);
static void meadow_timer_disable(struct timerInfo_s *timerInfo);

/****************************************************************************
 * Private Data
 ****************************************************************************/

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
// This function is called for all interrupts configured in timers
// Note with the HC-SR04 if there is no target it's output will go high for
// about 125ms then low, but will then after about 150us output a 6 us pulse.
int meadow_timer_isr(int irq, void *context, void *arg)
{
  static bool gpioToggle = true;
  static uint32_t startCount;
  
  gpioToggle = !gpioToggle;
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D14_OUT, gpioToggle);

  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;

  // Why are we here? Check the timer's Status Register
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);
  // syslog(1, "+++++> Timer %u interrupt caught, Status Reg:0x%04x\n",
  //           timerInfo->timerNumb, timStatusReg);
  
  // Check the status register and acknowledge all interrupts
  if(timStatusReg & GTIM_SR_TIF)
  {
    // GTIM_SR_TIF in gated mode occurs when counter starts or stops

    // Clear interrupt
    timStatusReg &= ~GTIM_SR_TIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Read the GPIO's state.
    // Assumes high = start and low = stop. May want to allow config to specify
    bool inputState = stm32_gpioread(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);
    if(inputState)
    {
      // Interrupt arrived just after counting started
      syslog(1, "+++> TIF ^\n");
    }
    else
    {
      // Counting has stopped so save the value
      syslog(1, "+++> TIF v\n");
      if(timerInfo->timerWidth == 16)
        timerInfo->timerCount = (uint32_t)getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
      else
        timerInfo->timerCount = getreg32(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
    }
  }

  if(timStatusReg & GTIM_SR_UIF)
  {
    // GTIM_SR_UIF overflow or underflow
    // timerInfo->timerCount++;
    // if(timerInfo->timerCount < 4 || timerInfo->timerCount % 4096 == 0)
    //   syslog(1, "+++> UIF:%u\n", timerInfo->timerCount);

    // syslog(1, "+++> UIF (Update interrupt flag) Timer %u\n",
    //           timerInfo->timerNumb);

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
  syslog(1, "==> %s@%d-timer support setup\n", __FILE__, __LINE__);

  // Clear table values as needed
  for (int i = 0; i < MeadowTimerNumberOfTimers; i++)
  {
    timerData[i].timerCount = 0;
  }

  // Used to inspect software
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);

  // Used for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);
  
  // For the time being, this is defined just below MEADOW_TIMER_EXPERIMENT_NUMBER 
  stm32_configgpio(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);

  syslog(1, "==> %s@%d-Creating timer experiment thread\n", __FILE__, __LINE__);

  // Create a thread to use for experimenting
  _meadow_timer_exp_thread = kthread_create(MEADOW_TIMER_EXPERIMENT_THREAD_NAME,
                                  MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY,
                                  MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE,
                                  (main_t) _meadow_timer_thread_func,
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
void *_meadow_timer_thread_func(int argc, char *argv[])
{
  int ret;
  
// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
  usleep(10 * 1000);  
// #endif

  // General timer initialization
  syslog(1, "--> Setting up general timer init\n");
  struct timerInfo_s *timerInfo = meadow_timer_init_general(MEADOW_TIMER_EXPERIMENT_NUMBER);
  if(timerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Meadow timer init failed\n", __FILE__, __LINE__);
    return NULL;
  }

  // Test pulse width
  syslog(1, "--> Pulse width initialization\n");
  ret = meadow_timer_init_gated_pulse_width(timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow pulsh width init failed:%d\n", __FILE__, __LINE__, ret);
    return NULL;
  }
  
  // Final initialization
  syslog(1, "--> Enabling timer\n");
  meadow_timer_enable(timerInfo);

  syslog(1, "--> Configuration complete, entering main loop\n");
  usleep(200 * 1000);

  uint32_t cntValue;
  uint32_t failSafeCount = 0;
  #define MeadowTimerFailSafeDelay (100)      // Wait 2 seconds max

  while(true)
  {
    // PeterM - HOPING THIS CAN BE REMOVED ONCE CODE IS WORKING WELL
    // Wait for state to be ready,(i.e. input point to be low)
    while(stm32_gpioread(MEADOW_TIMER_APPROPRIATE_TIM_INPUT))
    {
      syslog(1, "Waiting for input to go low. Count now:%u\n", timerInfo->timerCount);
      failSafeCount++;
      if(failSafeCount == MeadowTimerFailSafeDelay)
        break;
      usleep(20 * 1000);
    }

    // Pulse has ended or fail safe escape was reached
    if(failSafeCount == MeadowTimerFailSafeDelay)
    {
      syslog(1, "??? Fail Safe check timed out after 2 seconds\n");
    }
    else
    {
      syslog(1, "--> HC-SR04 Input point has gone low\n"); 
    }

    // Get the counter value
    cntValue = timerInfo->timerCount;

    if(cntValue > 0)
    {
      double totalTime = (double)cntValue / (double)timerInfo->timerClkFreq;
      double oneWayTime = totalTime/2.0;

      // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec at 25 = 346.13
      double distance = oneWayTime /*seconds*/ * 345; /* meters/second*/
      syslog(1, "=====> Count:%lu, Time:%10.8fms, Distance:%1.6fm\n",
                cntValue, oneWayTime * 1000, distance);
    }
    else
    {
      syslog(1, "--> Timer %u count was %lu\n", timerInfo->timerNumb, cntValue);
    }

    // In the FUTURE - wait for trigger request here, not usleep.
    syslog(1, "--> Timer %d waiting 1 second to simulate trigger request\n", timerInfo->timerNumb);
    usleep(1000 * 1000);   // Temporary

    // Set the Timer's internal Counter to = 0
    if(timerInfo->timerWidth == 16)
      putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
    else
      putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

    syslog(1, "--> Timer %u sending pulse to HC-SR04\n", timerInfo->timerNumb);
    // Send pulse to HC-SR04, this must be at least 2us wide to signal the
    // HC-SR04 to send ultrasonic pulses and wait for the echo.
    stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, true);

    // Insure pulse is at least 2 usec
    for(int i = 0; i < 1800; i++);
    // Note a 1-2 ms pulse works too
    // usleep(1000);

    stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, false);
    syslog(1, "--> Pulse sent to HC-SR04\n");
  }

  return NULL;    // Keep compiler happy
}

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
  int ret;
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

  // All interupts are handled by same isr, at least for now
  // Set the interrupt handler
  xcpt_t isrHandler = meadow_timer_isr;
  ret = irq_attach(timerInfo->timerIrqVec, isrHandler, timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return NULL;
  }

  syslog(1, "---> Enabling IRQ up_enable_irq\n");
  // Nuttx handles the interrupts
  up_enable_irq(timerInfo->timerIrqVec);

  // Final setup.
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

//=============================================================
// Pulse Width inititalization
int meadow_timer_init_gated_pulse_width(struct timerInfo_s *timerInfo)
{
  // Setup for timer gate-controlled measurement. This will be our base line
  // for determining the best possible performance. When other techniques are
  // tried we can compare to this gate-controlled method.

  uint32_t timerBase = timerInfo->timerBase;
  
  // Disable slave mode while configuring
  uint32_t smcr_val = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  smcr_val &= ~GTIM_SMCR_DISAB;
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  // DMA/Interrupt enable register (DIER)
  // Don't know so set for a bunch of interrpts and keep the one the matter
  uint16_t regval16new;
  uint16_t regval16;

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
  regval16 = getreg16(timerBase + STM32_GTIM_DIER_OFFSET);
  regval16 |= regval16new;
  putreg16(regval16, timerBase + STM32_GTIM_DIER_OFFSET);

  syslog(1, "--> Setting up Edge Detector and Gated Mode\n");
  // Set TI1 or TI2 Edge Detector and Gated Mode
  // GTIM_SMCR_TI1FP1 GTIM_SMCR_TI1FP2
  smcr_val |= (GTIM_SMCR_TI1FP1 | GTIM_SMCR_GATED);
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  // syslog(1, "--> Just set input and Gated mode:0x%08x\n", smcr_val);
  // usleep(20 * 1000);
  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)