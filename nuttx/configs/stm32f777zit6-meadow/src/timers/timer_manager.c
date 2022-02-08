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

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void *meadow_timer_thread_func(int argc, char *argv[]);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int _meadow_timer_exp_thread;

/****************************************************************************
 * Private Types
 ****************************************************************************/

// MTC = Meadow Timer Configuration
// Future configuration options

// These are strictly for IMPLEMENTATION AND TESTING
bool mtcIncludeIdleMeasure = false;

// Select one of the following
bool mtcPulseWidth = false;
bool mtcFreqDutyCycle = false;
bool mtcRcDecoder = true;

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
 // Initialize GPIOs used for timing and verify software
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A0);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A1);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A2);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A3);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A4);
  stm32_configgpio(MEADOW_DEBUG_PIN_V2_A5);

  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D14_OUT);
  
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

  // Setup features
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
    ret = meadow_timer_setup_pulse_width(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width setup for %d failed\n",
                __FILE__, __LINE__, MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      return NULL;
    }
  }

  if(mtcFreqDutyCycle)
  {
    ret = meadow_timer_setup_freq_duty(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow frequency + duty cycle setup for %d failed\n",
                __FILE__, __LINE__, MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      return NULL;
    }
  }

  if(mtcRcDecoder)
  {
    ret = meadow_timer_setup_rc_servo_decode(MEADOW_TIMER_NUMBER_EXPERIMENTAL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow rc servo decode setup for %d failed\n",
                __FILE__, __LINE__, MEADOW_TIMER_NUMBER_EXPERIMENTAL);
      return NULL;
    }
  }

  //------------------------------------------------------
  // Setup Timer 1 to do CPU/Idle utilization. This may be in parallel with the
  // other timer features, thus a different TimerInfo pointer.
  // if(mtcIncludeIdleMeasure)
  // {
    // ret = meadow_timer_init_idle_measure();
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
