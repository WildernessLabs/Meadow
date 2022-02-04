/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/timer_manager.c
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

#warning Experimental Code

/****************************************************************************
 * Included Files
 ****************************************************************************/

// #include <nuttx/config.h>
#include "meadow_timers.h"

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)

// #define STM32_GTIM_CCR1_OFFSET     0x0034  /* Capture/compare register 1 (16-bit on all TIMx and 32-bit on TIM2,5 only) */
// #define STM32_GTIM_CCR2_OFFSET     0x0038  /* Capture/compare register 2 (16-bit TIM 3-4, 9, 12 and 32-bit on TIM2,5 only) */
// #define STM32_GTIM_CCR3_OFFSET     0x003c  /* Capture/compare register 3 (16-bit TIM 3-4 and 32-bit on TIM2,5 only) */
// #define STM32_GTIM_CCR4_OFFSET     0x0040  /* Capture/compare register 4 (16-bit TIM 3-4 and 32-bit on TIM2,5 only) */

// This determines the timer's clock speed for RC Servo usage
#define MEADOW_TIMER_RC_SERVO_PRESCALER (32) // To overflow (65536) just below 50 Hz

//===================================================================

  // uint8_t timerNumb;                  // For diagnostics
  // volatile uint8_t timerWidth;        // Either 16 or 32 bit wide (replace with func bit)
  // volatile uint8_t timerDectSync;     // FDc - CCR1 interrupt missing
  // volatile uint32_t timerCount1;      // Primary value of the count
  // volatile uint32_t timerCount2;      // Secondary value of the count
  // volatile uint32_t timerExtra1;      // Extra information 1
  // volatile uint32_t timerExtra2;      // Extra information 2
  // volatile uint32_t timerFreq;        // Running timer clock frequency (could be prescaler value)
  // uint32_t timerFunc;                 // Bit fields with the functions this timer has and can perform
  // uint32_t timerChan[4];              // Channels for each timer
  // uint32_t timerBase;                 // Unique for each timer
  // uint32_t timerMaxClk;               // Either 192MHz or 96MHz (replace with func bit)
  // uint32_t timerAPBClk;               // Proper APB clock register for timer enable bit field (replace with func bit)
  // uint32_t timerClkEn;                // Bit of timer enable bit for APB1 or APB2
  // uint32_t timerIrqVec;               // Interrupt vector

// static struct timerInfo_s timerInfoFixed[] =
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


struct timerInfo_s timerInfoArray[] =
{
  // NOTE: CHANNELS REFLECT F7V2, ONLY TIM3 DIFFERENT IN F7V1
            // Num  wid  Syn CC1 CC2 Ex1 Ex2 Frq Fnc     Chan1-4                  Base Addr      Max Clock Frequency    Correct APB Clock     Timer Enable         IRQ Vector
  /* TIM1   */  {1 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM1_BASE,  STM32_APB2_TIM1_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM1EN,  STM32_IRQ_TIM1UP},
  /* TIM2   */  {2 , 32,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM2_BASE,  STM32_APB1_TIM2_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2},
  /* TIM3   */  {3 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0},        STM32_TIM3_BASE,  STM32_APB1_TIM3_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3},
  /* TIM4   */  {4 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0x3c,0x40},  STM32_TIM4_BASE,  STM32_APB1_TIM4_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4},
  /* TIM5   */  {5 , 32,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM5_BASE,  STM32_APB1_TIM5_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5},
  /* TIM6   */  {6 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM6_BASE,  STM32_APB1_TIM6_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM6EN,  STM32_IRQ_TIM6},
  /* TIM7   */  {7 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM7_BASE,  STM32_APB1_TIM7_CLKIN,  STM32_RCC_APB1ENR, RCC_APB1ENR_TIM7EN,  STM32_IRQ_TIM7},
  /* TIM8   */  {8 , 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0x40},     STM32_TIM8_BASE,  STM32_APB2_TIM8_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM8EN,  STM32_IRQ_TIM8UP},
  /* TIM9   */  {9 , 16,  0,  0,  0,  0,  0,  0,  0,  {0,0x38,0,0},           STM32_TIM9_BASE,  STM32_APB2_TIM9_CLKIN,  STM32_RCC_APB2ENR, RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9},
  /* TIM10  */  {10, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM10_BASE, STM32_APB2_TIM10_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10},
  /* TIM11  */  {11, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0,0,0},           STM32_TIM11_BASE, STM32_APB2_TIM11_CLKIN, STM32_RCC_APB2ENR, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11},
  /* TIM12  */  {12, 16,  0,  0,  0,  0,  0,  0,  0,  {0x34,0x38,0,0},        STM32_TIM12_BASE, STM32_APB1_TIM12_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12},
  /* TIM13  */  {13, 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM13_BASE, STM32_APB1_TIM13_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13},
  /* TIM14  */  {14, 16,  0,  0,  0,  0,  0,  0,  0,  {0,0,0,0},              STM32_TIM14_BASE, STM32_APB1_TIM14_CLKIN, STM32_RCC_APB1ENR, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14},
};

// static uint32_t timerInfoArrayCnt = (sizeof(timerInfoArray) / sizeof(struct timerInfo_s));

static int _meadow_timer_exp_thread;

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void *meadow_timer_thread_func(int argc, char *argv[]);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

// MTC = Meadow Timer Configuration
// Future configuration options
bool mtcIncludeIdleMeasure = false;

// For now, select max of one of the following
bool mtcPulseWidth = true;

bool mtcFreqDutyCycle = false;

bool mtcRcDecoder = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

// uint16_t getreg16(unsigned int addr);
// void modifyreg16(unsigned int addr, uint16_t clearbits, uint16_t setbits);
// void putreg16(regval, unsigned int addr);
// stm32_gpiowrite(pin_set, t/f);
// t/f = stm32_gpioread(pin_set);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This is called from hcom_nx_startup_mgr.c
int meadow_timer_support_setup()
{
  // Clear table values as needed
  for (int i = 0; i < MEADOW_TIMERS_NUMB_OF_TIMERS; i++)
  {
    timerInfoArray[i].timerCount1 = 0;
    timerInfoArray[i].timerCount2 = 0;
    timerInfoArray[i].timerExtra1 = 0;
    timerInfoArray[i].timerExtra2 = 0;
  }

 // Initialize GPIOs used for timing and verify software
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A0);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A1);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A2);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A3);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A4);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A5);

  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);
  
  // TEMPORARY - During development configure a GPIO input for measurement 
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
  // struct timerInfo_s *timerInfo = NULL;
  // struct timerInfo_s *idleTimerInfo = NULL;

// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
// #endif

  // Setup all the sub-features
  // if(mtcIncludeIdleMeasure)
  // {
  //   ret = meadow_timer_setup_idle_detect();
  //   if(ret < 0)
  //   {
  //     syslog(LOG_ERR, "%s@%d-Meadow timer setup failed\n", __FILE__, __LINE__);
  //     return NULL;
  //   }
  // }

  if(mtcPulseWidth)
  {
    ret = meadow_timer_setup_pulse_width();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow timer setup failed\n", __FILE__, __LINE__);
      return NULL;
    }
  }

  if(mtcFreqDutyCycle)
  {
    ret = meadow_timer_setup_freq_duty();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow timer setup failed\n", __FILE__, __LINE__);
      return NULL;
    }
  }

  if(mtcRcDecoder)
  {
    ret = meadow_timer_setup_rc_servo_decode();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow timer rc servo decode failed\n", __FILE__, __LINE__);
      return NULL;
    }
  }

  //------------------------------------------------------
  // Setup Timer 1 to do CPU/Idle utilization. This may be in parallel with the
  // other timer features, thus a different TimerInfo pointer.
  // if(mtcIncludeIdleMeasure)
  // {
    // ret = meadow_timer_init_idle_measure(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    // if(ret < 0)
    // {
    //   syslog(LOG_ERR, "%s@%d-Meadow idle measure init failed:%d\n", __FILE__, __LINE__, ret);
    //   return NULL;
    // }
  // }

  if(mtcPulseWidth)
  {
    ret = meadow_timer_init_gated_pulse_width(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }

  if(mtcFreqDutyCycle)
  {
    ret = meadow_timer_init_freq_and_dutycycle(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }
  
  if(mtcRcDecoder)
  {
    ret = meadow_timer_init_rc_servo_decode(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow rc servo decode init failed:%d\n", __FILE__, __LINE__, ret);
      return NULL;
    }
  }

  //-----------------------------------------------------------------------
  // Now run a tests to insure everything works
  while(true)
  {
    usleep(997 * 1000);

    // if(mtcIncludeIdleMeasure)
    // {
    //   ret = meadow_timer_test_idle_measure(idleTimerInfo);
    //   if(ret < 0)
    //   {
    //     syslog(LOG_ERR, "%s@%d-Meadow Idle Measurement init failed:%d\n", __FILE__, __LINE__, ret);
    //     return NULL;
    //   }
    // }

    if(mtcPulseWidth)
    {
      // For normal testing use the following sleep. Remove to do torture test
      // usleep(1000 * 1000);
      ret = meadow_timer_test_gated_pulse_width(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-Meadow pulse width test failed:%d\n", __FILE__, __LINE__, ret);
        return NULL;
      }
    }

    if(mtcFreqDutyCycle)
    {
      ret = meadow_timer_test_freq_and_dutycycle(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle test failed:%d\n", __FILE__, __LINE__, ret);
        return NULL;
      }
    }

    if(mtcRcDecoder)
    {
      ret = meadow_timer_test_rc_servo_decode(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-Meadow rc servo decode test failed:%d\n", __FILE__, __LINE__, ret);
        return NULL;
      }
    }
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
