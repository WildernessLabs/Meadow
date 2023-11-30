/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/misc/meadow_idle_monitor.c
 * 
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

// The module uses the idle information from stm32_idle.c to
// determine the current MPU idle percentage.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <arch/board/board.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"

#include "../hcom_nx/hcom_nx_common.h"
#include <meadow/hcom_misc_diag.h>

#if MEADOW_INCLUDE_IDLE_MONITOR_TESTS_IN_BUILD > 0
#include <nuttx/kthread.h>
#endif

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//===================================================================
// Timer 6 is one of the 2 basic timers. It has no connected GPIO I/O
#define MEADOW_IDLE_MON_USE_TIMER_NUMBER (6)
#define MEADOW_IDLE_MON_TIMER_BASE (STM32_TIM6_BASE)

// 65536 Will overflow 1 time each second
#define MEADOW_IDLE_MON_TARGET_FREQUENCY (65536)

// Since Meadow has a systick every millisecond, we'll need a value that
// is a bit over the systick rate. This value was tuned since 1000
// resulted in less that a 0-100 result. Being called 1000 times is related to
// this configuration entry (CONFIG_USEC_PER_TICK=1000)
// This is a reasonable maximum for the no load idle count.
#define MEADOW_IDLE_MON_MAX_100_PER_CENT_COUNT (1060)

// For diagnostics
// #define DEBUG_PIN_V2_D14  (0x00040c1c)

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

int meadow_idle_mon_timer_init(uint32_t timerBase);
static int meadow_idle_mon_isr(int irq, void *context, void *arg);

#if MEADOW_INCLUDE_IDLE_MONITOR_TESTS_IN_BUILD > 0
int meadow_idle_mon_create_test_thread(void);
void *meadow_idle_mon_test_thread_proc(int argc, char *argv[]);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if defined (CONFIG_ARCH_IDLE_CUSTOM)
static volatile uint32_t _idleActiveCount;
static volatile uint32_t _idleCountSnapShot;
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This callback is called for interrupts configured for measuring idle time.
// This is called designed to be called about once/second.
int meadow_idle_mon_isr(int irq, void *context, void *arg)
{
  // Check the timer's Status Register
  uint16_t timStatusReg = getreg16(MEADOW_IDLE_MON_TIMER_BASE + STM32_BTIM_SR_OFFSET);

  if(timStatusReg & BTIM_SR_UIF)
  {
    timStatusReg &= ~BTIM_SR_UIF;

#if defined (CONFIG_ARCH_IDLE_CUSTOM)
    // Once per second, take a snapshot of idle count.
    // The higher the count, the more often the OS dropped into idle mode. This
    // is an indirect indication of how busy the system is. When Nuttx is
    // really busy, the idle thread might not run for for several seconds.
    if(_idleActiveCount < MEADOW_IDLE_MON_MAX_100_PER_CENT_COUNT)
    {
      _idleCountSnapShot = _idleActiveCount;
    }
    else
    {
      _idleCountSnapShot = MEADOW_IDLE_MON_MAX_100_PER_CENT_COUNT;
    }

    _idleActiveCount = 0;    // And restart counting

#endif

    putreg16(timStatusReg, MEADOW_IDLE_MON_TIMER_BASE + STM32_BTIM_SR_OFFSET);
  }

  return OK;
}

//================================================================
// Called during startup
int meadow_idle_monitor_setup()
{
  int ret;

  MEADOW_TRACE_INFORMATION("Entered meadow_idle_monitor_setup at startup\n");

#if defined (CONFIG_ARCH_IDLE_CUSTOM)
  _idleActiveCount = 0;
  _idleCountSnapShot = 0;
#endif

  // Initialized the timer itself
  ret = meadow_idle_mon_timer_init(MEADOW_IDLE_MON_TIMER_BASE);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Idle monitor init failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//================================================================
// This function uses Timer 6 to create a timer that will call the above ISR
// once / second. It assumes timer #6 will be used
int meadow_idle_mon_timer_init(uint32_t timerBase)
{
  int ret;

  #if defined (CONFIG_ARCH_IDLE_CUSTOM)
  _idleActiveCount = 0;
#endif

  // Setup the clock enable for timer 6
  modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIM6EN);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.
  // Find proper pre-scaler value so all timers run at the same speed
  uint16_t prescaler = (STM32_APB1_TIM6_CLKIN/MEADOW_IDLE_MON_TARGET_FREQUENCY) - 1;
  putreg16(prescaler, timerBase + STM32_BTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = 0xffff;
  putreg32(maxARRValue, timerBase + STM32_BTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval |= BTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);

  // Timer 6 & 7 only support UIE interrupt
  putreg16(BTIM_DIER_UIE, timerBase + STM32_BTIM_DIER_OFFSET);

  // The interupts are handled this ISR
  ret = irq_attach(STM32_IRQ_TIM6, meadow_idle_mon_isr, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  up_enable_irq(STM32_IRQ_TIM6);

  // Enable the timer
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);
  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);

#if MEADOW_INCLUDE_IDLE_MONITOR_TESTS_IN_BUILD > 0
  ret = meadow_idle_mon_create_test_thread();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_idle_mon_create_test_thread failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }
#endif
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Return the current MCU idle percentage 0 - 100%
int meadow_idle_monitor_get_value()
{
  static int IdlePercent;

#if defined (CONFIG_ARCH_IDLE_CUSTOM)

  // Calculate the percentage 
  if(_idleCountSnapShot > 0)
  {
    IdlePercent = 
        (_idleCountSnapShot * 100) / MEADOW_IDLE_MON_MAX_100_PER_CENT_COUNT;
  }
  else
  {
    IdlePercent = 0;
  }
#endif    // #if defined (CONFIG_ARCH_IDLE_CUSTOM)

  return IdlePercent;
}

//================================================================
// Called by idle thread in stm32_idle. This happens whenever the STM32 is
// entering idle mode. This function is only called by the idle thread.
#if defined (CONFIG_ARCH_IDLE_CUSTOM)
void meadow_idle_mon_entering_idle_mode(void)
{
  _idleActiveCount++;
}
#endif

// Test code follows
#if MEADOW_INCLUDE_IDLE_MONITOR_TESTS_IN_BUILD > 0
//=========================================================
int meadow_idle_mon_create_test_thread()
{
  int test_kthrd;

  // Create a thread to run the tests. Note: the priority is really high
  // because this is test code. and if priority is lower will not see the
  // output when doing things like downloading a file.
  test_kthrd = kthread_create("IdleMonTest",
                              248,      // Pri really high for testing
                              2048,     // Stack
                              (main_t) meadow_idle_mon_test_thread_proc,
                              (char *const *) NULL);
  if (test_kthrd <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
                __FILE__, __LINE__, "IdleMonTest");
    return -ENOEXEC;
  }
  return OK;
}

//=========================================================
// Test code for mcu idle measurement. It only reports via syslog the current
// idle percentage calculated.
void *meadow_idle_mon_test_thread_proc(int argc, char *argv[])
{
  int idleValue;
  
#if defined (CONFIG_ARCH_IDLE_CUSTOM)
  while(true)
  {
    idleValue = meadow_idle_monitor_get_value();
    MEADOW_TRACE_INFORMATION"MCU Idle percent:%d, snapshot:%d\n", idleValue, _idleCountSnapShot);
    sleep(1);
  }
#endif

  return NULL;
}
#endif
