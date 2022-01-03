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

// #include <nuttx/clock.h>    // for testing

#include <meadow/meadow_hw_version.h>

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
#if defined(true)

// F7v2
#define MEADOW_TIMER_TEST_GPIO_D14_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTB | GPIO_PIN12)

#define MEADOW_TIMER_TEST_GPIO_D15_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTG | GPIO_PIN12)

#define MEADOW_TIMER_TEST_GPIO_A02_IN  (GPIO_INPUT | GPIO_FLOAT | GPIO_PORTA | GPIO_PIN3)

// F7v1 pins
// #define DEBUG_PIN_V1_D14  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTG | GPIO_PIN3)
// #define DEBUG_PIN_V1_D15  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTE | GPIO_PIN3)

#define MEADOW_TIMER_EXPERIMENT_THREAD_NAME "TimerExp"
#define MEADOW_TIMER_EXPERIMENT_THREAD_PRIORITY 120
#define MEADOW_TIMER_EXPERIMENT_THREAD_STACKSIZE 2048 // 1024 was small

#define MEADOW_TIMER_EXPERIMENT_NUMBER (2)

#warning Experimental Code

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int _meadow_timer_exp_thread;
static struct stm32_tim_dev_s *_timerHandle;  // Remove this
static uint32_t _activeTimBase;

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_isr(int irq, void *context, void *arg);
static void *_meadow_timer_thread_func(int argc, char *argv[]);
static int send_echo_sequence(void);
static int meadow_timer_support_init_timer(int timerNumber);
static int meadow_timer_process_tif_interrupt(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct timerInfo_s
{
  // Timer base address
  uint32_t timerBase;
  uint8_t size;
  uint32_t maxFreq;         // NUTTX term
  uint32_t maxPeriod;       // NUTTX term
};

// There are 14 available timers
static struct timerInfo_s timerData[] =
{
                // Base Addr     size  Maximum Clock Freq
  /* TIM1   */  {STM32_TIM1_BASE,  16, STM32_APB2_TIM1_CLKIN},
  /* TIM2   */  {STM32_TIM2_BASE,  32, STM32_APB1_TIM2_CLKIN},
  /* TIM3   */  {STM32_TIM3_BASE,  16, STM32_APB1_TIM3_CLKIN},
  /* TIM4   */  {STM32_TIM4_BASE,  16, STM32_APB1_TIM4_CLKIN},
  /* TIM5   */  {STM32_TIM5_BASE,  32, STM32_APB1_TIM5_CLKIN},
  /* TIM6   */  {STM32_TIM6_BASE,  16, STM32_APB1_TIM6_CLKIN},
  /* TIM7   */  {STM32_TIM7_BASE,  16, STM32_APB1_TIM7_CLKIN},
  /* TIM8   */  {STM32_TIM8_BASE,  16, STM32_APB2_TIM8_CLKIN},
  /* TIM9   */  {STM32_TIM9_BASE,  16, STM32_APB2_TIM9_CLKIN},
  /* TIM10  */  {STM32_TIM10_BASE, 16, STM32_APB2_TIM10_CLKIN},
  /* TIM11  */  {STM32_TIM11_BASE, 16, STM32_APB2_TIM11_CLKIN},
  /* TIM12  */  {STM32_TIM12_BASE, 16, STM32_APB1_TIM12_CLKIN},
  /* TIM13  */  {STM32_TIM13_BASE, 16, STM32_APB1_TIM13_CLKIN},
  /* TIM14  */  {STM32_TIM14_BASE, 16, STM32_APB1_TIM14_CLKIN},
};

/****************************************************************************
 * Private Types
 ****************************************************************************/
struct stm32_tim_priv_s
{
  struct stm32_tim_ops_s *ops;
  stm32_tim_mode_t        mode;
  uint32_t                base;   /* TIMn base address */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static inline uint16_t stm32_getreg16(FAR struct stm32_tim_dev_s *dev,
                                      uint8_t offset)
{
  return getreg16(((struct stm32_tim_priv_s *)dev)->base + offset);
}

// uint16_t getreg16(unsigned int addr);
// void modifyreg16(unsigned int addr, uint16_t clearbits, uint16_t setbits);
// void putreg16(regval, unsigned int addr);

//============================================================================
// This function is called every 100 microseconds when the timer is running
int meadow_timer_isr(int irq, void *context, void *arg)
{
  static bool gpioToggle = true;

  gpioToggle = !gpioToggle;
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D14_OUT, gpioToggle);

  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct stm32_tim_dev_s *activeTimer = (struct stm32_tim_dev_s *)arg;

  // Why are we here? Check the timer's Status Register
  uint16_t timStatusReg = stm32_getreg16(activeTimer, STM32_GTIM_SR_OFFSET);

  syslog(1, "--> Interrupt caught. Status Reg:0x%04x\n", timStatusReg);

  // Check the status register and acknowledge all interrupts
  if(timStatusReg & GTIM_SR_TIF)
  {
    syslog(1, "--> TIF (Trigger Interrupt Flag)\n");
    timStatusReg &= ~GTIM_SR_TIF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
    meadow_timer_process_tif_interrupt();
  }

  if(timStatusReg & GTIM_SR_UIF)
  {
    syslog(1, "--> UIF (Update interrupt flag)\n");
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  
  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    syslog(1, "--> A Capture/compare interrupt flag 1\n");
    timStatusReg &= ~GTIM_SR_CC1IF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    syslog(1, "--> A Capture/compare interrupt flag 2\n");
    timStatusReg &= ~GTIM_SR_CC2IF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    syslog(1, "--> A Capture/compare interrupt flag 3\n");
    timStatusReg &= ~GTIM_SR_CC3IF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    syslog(1, "--> A Capture/compare interrupt flag 4\n");
    timStatusReg &= ~GTIM_SR_CC4IF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC1OF)
  {
    syslog(1, "--> A Capture/compare over capture interrupt 1\n");
    timStatusReg &= ~GTIM_SR_CC1OF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC2OF)
  {
    syslog(1, "--> A Capture/compare over capture interrupt 2\n");
    timStatusReg &= ~GTIM_SR_CC2OF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC3OF)
  {
    syslog(1, "--> A Capture/compare over capture interrupt 3\n");
    timStatusReg &= ~GTIM_SR_CC3OF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  if(timStatusReg & GTIM_SR_CC4OF)
  {
    syslog(1, "--> A Capture/compare over capture interrupt 4\n");
    timStatusReg &= ~GTIM_SR_CC4OF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }
  
  if(timStatusReg & GTIM_SR_BIF)
  {
    syslog(1, "--> Break interrupt flag set\n");
    timStatusReg &= ~GTIM_SR_BIF;
    putreg16(timStatusReg, _activeTimBase + STM32_GTIM_SR_OFFSET);
  }


  syslog(1, "--> Interrupts cleared. Status Reg:0x%04x\n", timStatusReg);

  // Toggle GPIO for o'scope
  // if(gpioToggle)
  // {
  //   stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D14_OUT, true);
  //   gpioToggle = false;
  // }
  // else
  // {
  //   stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D14_OUT, false);
  //   gpioToggle = true;
  // }
// usleep(20 * 1000);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_support_setup()
{
  syslog(1, "==> %s@%d-timer support setup\n", __FILE__, __LINE__);

  // Used to inspect software
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);

  // Used for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);
  
  // A02 is used for gating counter on Timer 2 channel 4
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_A02_IN);

  syslog(1, "==> %s@%d-Creating timer experiment thread\n", __FILE__, __LINE__);usleep(10 * 1000);

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
void *_meadow_timer_thread_func(int argc, char *argv[])
{
// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
  usleep(10 * 1000);  
// #endif

  // Generic initialization
  meadow_timer_support_init_timer(MEADOW_TIMER_EXPERIMENT_NUMBER);

  // ---- Function Specific Initialization ----
  // Setup for timer gate-controlled measurement. This will be our base line
  // for determining the best possible performance. When other techniques are
  // tried we can compare to this gate-controlled method.
  // uint32_t regval = getreg32(_activeTimBase + STM32_GTIM_SMCR_OFFSET);
  // putreg32(regval, _activeTimBase + STM32_GTIM_SMCR_OFFSET);

  // Slave mode selection (SMS) 
  // Setup first for external trigger from ETR line and for gated mode.
  // 1. Disable the timer
  // 1. Establish gated source and polarity
  // 2. Configure as Gated Mode
  // 3. Enable the timer

  uint32_t regval32 = getreg32(_activeTimBase + STM32_GTIM_SMCR_OFFSET);
  syslog(1, "--> pre-disabled SMCR:0x%08x\n", regval32);
  regval32 &= ~GTIM_SMCR_DISAB;
  putreg32(regval32, _activeTimBase + STM32_GTIM_SMCR_OFFSET);
  syslog(1, "--> post-disabled SMCR:0x%08x\n", regval32);

  // DMA/Interrupt enable register (DIER)
  // Don't know so set for a bunch of interrpts and keep the one the matter
  uint16_t regval16new;
  uint16_t regval16;

  // regval16new = GTIM_DIER_UIE + GTIM_DIER_TIE;
  regval16new = GTIM_DIER_UIE + GTIM_DIER_TIE + GTIM_DIER_CC1IE;

  // regval16 = GTIM_DIER_UIE |
  //            GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_CC3IE | GTIM_DIER_CC4IE |
  //            GTIM_DIER_TIE;

  syslog(1, "--> DIER value will be:0x%04x (expect 0x005F)\n", regval16new);
  usleep(10 * 1000);
  
  // Set the interrupt sources
  regval16 = getreg16(_activeTimBase + STM32_GTIM_DIER_OFFSET);
  syslog(1, "--> pre-interrupt cfg:0x%04x\n", regval16);
  regval16 |= regval16new;
  putreg16(regval16, _activeTimBase + STM32_GTIM_DIER_OFFSET);
  syslog(1, "--> post-interrupt cfg:0x%04x\n", regval16);

  // Set the trigger source
  regval32 |= GTIM_SMCR_ETRF;
  putreg32(regval32, _activeTimBase + STM32_GTIM_SMCR_OFFSET);
  syslog(1, "--> post-ETR settup:0x%08x\n", regval32);
  
  // Add the slave mode, we may get interrupts after this step
  regval32 |= GTIM_SMCR_GATED;
  putreg32(regval32, _activeTimBase + STM32_GTIM_SMCR_OFFSET);
  syslog(1, "--> post-GATED settup:0x%08x\n", regval32);

  syslog(1, "--> Configuration complete, entering main loop\n");
  usleep(10 * 1000);

  while(true)
  {
    usleep(2000 * 1000);
    send_echo_sequence();
  }

  return NULL;    // Keep compiler happy
}

//=============================================================
// Keeps sending a start pulse to the HC-SD04 ultrasonic range finder
int send_echo_sequence()
{
  int i;
  while(true)
  {
    syslog(1, "--> sending pulse to HC-SR04\n");
    // 2 times a second
    usleep(1000 * 1000);

    // Clear CNT
    // _timerHandle->ops->setcounter(_timerHandle, 0);

//  // Try this
//   bool etrIsHigh = stm32_gpioread(MEADOW_TIMER_TEST_GPIO_A02_IN);
//   uint32_t cntValue = _timerHandle->ops->getcounter(_timerHandle);

//   if(etrIsHigh)
//   {    
//     syslog(1, "==> %s@%d-Timer2 ETR is HIGH and CNT:%d\n", __FILE__, __LINE__, cntValue);
//   }
//   else
//   {
//     syslog(1, "==> %s@%d-Timer2 ETR is LOW and CNT:%d\n", __FILE__, __LINE__, cntValue);
//   }
// End try this
    // Send pulse to HC-SR04, this must be 2us wide to signal the HC-SR04 to
    // send ultrasonic pulses and wait for the echo.
    stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, true);

    // Insure pulse is at least 2 usec
    for(i = 0; i < 1800; i++);

    stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, false);
  }

  return OK;
}

//=============================================================
// This indicates that the counter CNT has started or stopped.
// We should be able to look at the GPIO and determine which it is.
int meadow_timer_process_tif_interrupt(void)
{
  bool etrIsHigh = stm32_gpioread(MEADOW_TIMER_TEST_GPIO_A02_IN);
  uint32_t cntValue = _timerHandle->ops->getcounter(_timerHandle);

  if(etrIsHigh)
  {    
    syslog(1, "==> %s@%d-Timer2 ETR is HIGH and CNT:%d\n", __FILE__, __LINE__, cntValue);
  }
  else
  {
    syslog(1, "==> %s@%d-Timer2 ETR is LOW and CNT:%d\n", __FILE__, __LINE__, cntValue);
  }
  return OK;
}

//=============================================================
// Returns OK or -error
int meadow_timer_support_init_timer(int timerNumber)
{
  int ret;
  struct stm32_tim_dev_s *tempTimer;
  uint32_t maxFreq;
  uint32_t maxPeriod;
  uint32_t timerBase;
  uint8_t bitSize;

  tempTimer = stm32_tim_init(timerNumber);    // stm32_timer_numb
  if(tempTimer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-stm32_tim_init() returned NULL for timer:%d\n",
          __FILE__, __LINE__, timerNumber);
    return -1;
  }

  // THE FOLLOWING ARE WHAT stm32_tim_init() DOES
  // Depends on timer number which RTC to enable BUT ONLY ONE OF THE
  // FOLLOWING 2
  // modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIMxEN);
  // modifyreg32(STM32_RCC_APB2ENR, 0, RCC_APB2ENR_TIMxEN);
  // NEXT DISABLES TIMER BASED ON dev THE INTERNAL NUTTX STRUCT.
  // Note this clears the CEN bit of the CR1 register
  // uint16_t val = stm32_getreg16(dev, STM32_BTIM_CR1_OFFSET);
  // val &= ~ATIM_CR1_CEN;
  // stm32_putreg16(dev, STM32_BTIM_CR1_OFFSET, val);

  // Find the clock speed
  // We assume fastest possible clock and full counting period
  struct timerInfo_s timerInfo = timerData[timerNumber - 1];
  timerBase = timerInfo.timerBase;
  maxFreq = timerInfo.maxFreq;
  maxPeriod = timerInfo.maxPeriod;
  bitSize = timerInfo.maxBits;

  switch (timerNumber)
  {
#ifdef CONFIG_STM32F7_TIM1
    case 1:
      timerBase = STM32_TIM1_BASE;
      maxFreq = STM32_APB2_TIM1_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM2
    case 2:
      timerBase = STM32_TIM2_BASE;
      maxFreq = STM32_APB1_TIM2_CLKIN;
      maxPeriod = 0xffffffff;   // 32-bit counter
      break;
#endif
#ifdef CONFIG_STM32F7_TIM3
    case 3:
      timerBase = STM32_TIM3_BASE;
      maxFreq = STM32_APB1_TIM3_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM4
    case 4:
      timerBase = STM32_TIM4_BASE;
      maxFreq = STM32_APB1_TIM4_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM5
    case 5:
      timerBase = STM32_TIM5_BASE;
      maxFreq = STM32_APB1_TIM5_CLKIN;
      maxPeriod = 0xffffffff;   // 32-bit counter
      break;
#endif
#ifdef CONFIG_STM32F7_TIM6
    case 6:
      timerBase = STM32_TIM6_BASE;
      maxFreq = STM32_APB1_TIM6_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM7
    case 7:
      timerBase = STM32_TIM7_BASE;
      maxFreq = STM32_APB1_TIM7_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM8
    case 8:
      timerBase = STM32_TIM8_BASE;
      maxFreq = STM32_APB2_TIM8_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM9
    case 9:
      timerBase = STM32_TIM9_BASE;
      maxFreq = STM32_APB2_TIM9_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM10
    case 10:
      timerBase = STM32_TIM10_BASE;
      maxFreq = STM32_APB2_TIM10_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM11
    case 11:
      timerBase = STM32_TIM11_BASE;
      maxFreq = STM32_APB2_TIM11_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM12
    case 12:
      timerBase = STM32_TIM12_BASE;
      maxFreq = STM32_APB1_TIM12_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM13
    case 13:
      timerBase = STM32_TIM13_BASE;
      maxFreq = STM32_APB1_TIM13_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
#ifdef CONFIG_STM32F7_TIM14
    case 14:
      timerBase = STM32_TIM14_BASE;
      maxFreq = STM32_APB1_TIM14_CLKIN;
      maxPeriod = 0xffff;
      break;
#endif
    default:
    syslog(1, "TIMER Number not found:%d\n", timerNumber);
      return -EINVAL;
  }
  
  _activeTimBase = timerBase;

  // Timer clocks run at 192MHz or 96Mhz and size is 16 or 32-bits
  STM32_TIM_SETCLOCK(tempTimer, maxFreq);

  // The period is the value put into the ARR 
  STM32_TIM_SETPERIOD(tempTimer, maxPeriod);

  // All interupts are handled by same isr
  xcpt_t isrHandler = meadow_timer_isr;

  // arg (third parameter) is a pointer that's returned in the isr handler
  // Currently this is the NUTTX structure CHANGE THIS!!!
  ret = STM32_TIM_SETISR(tempTimer, isrHandler, tempTimer, 0);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-STM32_TIM_SETISR failed:%d\n",
          __FILE__, __LINE__, ret);
    return ret;
  }

  // // If timer channel input, tell Nuttx what we care about
  // ret = stm32_gpiosetevent(
  //           cfgset,               // special gpio for call
  //           1,                    // risingEdge,
  //           1,                    // fallingEdge,
  //           0,                    // event
  //           upd_gpio_interrupt,   // function to call
  //           gpioMapTblPtr);       // table entry pointer

  // start
  STM32_TIM_SETMODE(tempTimer, STM32_TIM_MODE_UP);

  // Finish setup. NOW DONE ELSE WHERE
  // STM32_TIM_ACKINT(tempTimer, GTIM_SR_UIF);
  // STM32_TIM_ENABLEINT(tempTimer, GTIM_DIER_UIE);

  _timerHandle = tempTimer;
  
  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)