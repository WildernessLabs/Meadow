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

/****************************************************************************
 * Included Files
 ****************************************************************************/

// #include <nuttx/config.h>
#include "meadow_timers.h"

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

#if MEADOW_TIMER_INCLUDE_TESTING_CODE > 0
static void *meadow_timer_thread_func(int argc, char *argv[]);
static int meadow_timer_testing_support_setup(void);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if MEADOW_TIMER_INCLUDE_TESTING_CODE > 0
static int _meadow_timer_exp_thread;
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

// This array contains timer information that is fixed by the STM32F7. It
// defines which timers can be used and invariable values. Several of these
// values have be reduced to a bit-field simple to save space on the F7.
struct timerInfo_s timerInfoArray[] = 
{
            //   |--- bit-field---|
            //   #  wid max apb fut    Base Addr       Timer Clk Enable      IRQ Vector    Ptr
  /* TIM3   */  {3 , 0,  0,  0,  0, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3 , 0},
  /* TIM4   */  {4 , 0,  0,  0,  0, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4 , 0},
  /* TIM5   */  {5 , 1,  0,  0,  0, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5 , 0},
  /* TIM9   */  {9 , 0,  1,  1,  0, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9 , 0},
  /* TIM10  */  {10, 0,  1,  1,  0, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, 0},
  /* TIM11  */  {11, 0,  1,  1,  0, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, 0},
  /* TIM12  */  {12, 0,  0,  0,  0, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, 0},
};

// This array defines the GPIO values that must be used by the various timers.
// There can be up to 4 channels per timer. Notice that this array contains
// F7v1 and F7v2 values as will as the alternate function for each timer. It
// should be obvious but, this table must line up with the previous table.
static struct timerGpio_s timerGpioArray[] =
{
  //                            F7v1                                              F7v2                       Alt Func
  /* TIM3  D02, D05, D06,  D09  */ {{0x26,0x27,0x10,0x11}, /* D05, D10, A03,  A04  */ {0x14,0x27,0x10,0x11}, GPIO_AF2},
  /* TIM4  D08, D07, D03*, D04* */ {{0x16,0x17,0x18,0x19}, /* D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19}, GPIO_AF2},
  /* TIM5  D10,                 */ {{0x7a,0xff,0xff,0xff}, /* D02                  */ {0x7a,0xff,0xff,0xff}, GPIO_AF2},
  /* TIM9  A02,                 */ {{0x03,0xff,0xff,0xff}, /* A02                  */ {0x03,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM10 D03*,                */ {{0x18,0xff,0xff,0xff}, /* D03*                 */ {0x18,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM11 D04*,                */ {{0x19,0xff,0xff,0xff}, /* D04*                 */ {0x19,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM12 D12, D13             */ {{0x1e,0x1e,0xff,0xff}, /* D12, D13             */ {0x1e,0x1f,0xff,0xff}, GPIO_AF3},
};

//=================================================================
// For testing

// This structure and the following array maintain what timers have been
// configured and what function they have been configured for.
struct timerNumberUse_s
{
  uint8_t timerNumber;
  uint8_t timerUsage;   // This is from the meadow_timer_usage_config enum
};

static struct timerNumberUse_s timerNumbUseArray[] = 
{
  //#  Use
  {3 ,  0},
  {4 ,  0},
  {5 ,  0},
  {9 ,  0},
  {10,  0},
  {11,  0},
  {12,  0},
};

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

// Mono will call this function once for each timer to configure
int meadow_timer_configuration(struct timerConfig_s timerConfig)
{
  int ret;

  // Check that this timer isn't already being used.
  for(int timerOff = 0; timerOff < MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE; timerOff++)
  {
    if(timerConfig.timerNumber == timerNumbUseArray[timerOff].timerNumber)
    {
      // Found this timer, but is it being used?
      if(timerNumbUseArray[timerOff].timerUsage == Undefined)
      {
        timerNumbUseArray[timerOff].timerUsage = timerConfig.timerUsage;        // Unused
      }
      else
      {
        syslog(LOG_WARNING, "Timer %d is already configured.\n", timerConfig.timerNumber);
        return -1;
      }
    }
  }
  
  switch (timerConfig.timerUsage)
  {
  case PulseWidth:
    ret = meadow_timer_setup_pulse_width(timerConfig);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow pulse width setup for %d failed\n",
                __FILE__, __LINE__, timerConfig.timerNumber);
      return ret;
    }
    break;

  case FreqDutyCycle:
    ret = meadow_timer_setup_freq_duty(timerConfig);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow frequency + duty cycle setup for %d failed\n",
                __FILE__, __LINE__, timerConfig.timerNumber);
      return ret;
    }
    break;

   case RcServoDecode:
    ret = meadow_timer_setup_rc_servo_decode(timerConfig);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow rc servo decode setup for %d failed\n",
                __FILE__, __LINE__, timerConfig.timerNumber);
      return ret;
    }
    break;
 
  default:
    return -1;
  }

  return OK;
}

//=====================================================================
// The following functions are used by the various timer feature
// implementations.
struct timerInfo_s * meadow_timer_get_timer_info_pointer(int timerNumb)
{
  for (int offset = 0; offset < MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE; offset++)
  {
    if(timerInfoArray[offset].timerNumb == timerNumb)
    {
      return ( &(timerInfoArray[offset]));
    }
  }

  return NULL;
}

//=====================================================================
struct timerGpio_s * meadow_timer_get_timer_gpio_pointer(int timerNumb)
{
  for (int offset = 0; offset < MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE; offset++)
  {
    if(timerInfoArray[offset].timerNumb == timerNumb)
    {
      return ( &(timerGpioArray[offset]));
    }
  }

  return NULL;
}

//=============================================================
// Find the proper timer, version and channel for the GPIO Alt
// Function, Port and Pin for this timer.
uint32_t meadow_timer_get_ver_based_gpio_chan(int timerNumb, int channelOffset)
{
  struct timerGpio_s *timerGpio = meadow_timer_get_timer_gpio_pointer(timerNumb);

  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    return timerGpio->timerF7v1Gpio[channelOffset] | timerGpio->timerAltFunc;
  }
  else if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2 ||
          meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    return timerGpio->timerF7v2Gpio[channelOffset] | timerGpio->timerAltFunc;
  }
  else
  {
    return 0xffff;   // Invalid version
  }

  return 0xffff;
}

//=====================================================================
// Some timer applications only use the channel 1 GPIO
uint32_t meadow_timer_get_ver_based_gpio_timer(int timerNumb)
{
  return meadow_timer_get_ver_based_gpio_chan(timerNumb, 0);
}

//=============================================================
// Uses the bit-field to determine the RCC clock
uint32_t meadow_timer_get_apb_clock(struct timerInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
// Uses the bit-field to determine the Timer clock
uint32_t meadow_timer_get_max_clock(struct timerInfo_s *timerInfo)
{
  if(timerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
void meadow_timer_disable(uint32_t timerBase)
{
  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
}

//=============================================================
void meadow_timer_enable(uint32_t timerBase)
{
  // Why this order? tryed to copy the NUTTX order
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);

  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
}

//=====================================================================
// This is called from hcom_nx_startup_mgr.c
int meadow_timer_support_setup()
{
  int ret;

  // Initialize idle measuring
  if(true)
  {
    ret = meadow_timer_idle_measure_setup();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Timer idle measuring failed\n",
                __FILE__, __LINE__);
      return ret;
    }
  }

#if MEADOW_TIMER_INCLUDE_TESTING_CODE > 0
  ret = meadow_timer_testing_support_setup();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Timer testing setup failed\n",
              __FILE__, __LINE__);
    return ret;
  }
#endif

  return OK;
}

#if MEADOW_TIMER_INCLUDE_TESTING_CODE > 0

//=====================================================================
// This is called from hcom_nx_startup_mgr.c but ONLY for testing
int meadow_timer_testing_support_setup()
{
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

// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New timer test kthread [PID:%d],'%s'\n", getpid(), MEADOW_TIMER_EXPERIMENT_THREAD_NAME);
// #endif

  // Initialize GPIOs used for timing and verify software
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A0);
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A1);
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A2);
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A3);
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A4);
  // stm32_configgpio(MEADOW_DEBUG_PIN_V2_A5);
  
  sleep(1);

  // Configure a few timer features for testing
#if 0
  // HC-SR04 uses this configuration
  struct timerConfig_s configPulWid1;
  configPulWid1.timerNumber = 3;        // D05 is input
  configPulWid1.timerUsage = PulseWidth;
  configPulWid1.pwTimeroutMs = 1000;    // 1 - 65535 millisec
  configPulWid1.pwHCSR04Filter = 1;     // 0 = Don't filter, 1 use filter
  configPulWid1.polarityChan1 = 0;      // Leading 0 = rising, 1 = falling
  meadow_timer_configuration(configPulWid1);
#endif

#if 0
  struct timerConfig_s configFreqDc1;
  configFreqDc1.timerNumber = 5;        // Timer 5 D02 (32-bit) Timer 4 D08
  configFreqDc1.timerUsage = FreqDutyCycle;
  configFreqDc1.polarityChan1 = 0;      // Leading 0 = rising, 1 = falling
  meadow_timer_configuration(configFreqDc1);
#endif

#if 0
  struct timerConfig_s configRcServo1;
  configRcServo1.timerNumber = 4;    // D08, D07, D03, D04
  configRcServo1.timerUsage = RcServoDecode;
  configRcServo1.polarityChan1 = 0;  // Leading 0 = rising, 1 = falling
  configRcServo1.polarityChan2 = 0;
  configRcServo1.polarityChan3 = 0;
  configRcServo1.polarityChan4 = 0;
  meadow_timer_configuration(configRcServo1);
#endif

  //-----------------------------------------------------------------------
  // Now run appropriate tests, defined above, to insure everything works
  while(true)
  {
    usleep(953 * 1000);

    // Check which timers have been configured and how they are being used
    for(int timerOff = 0; timerOff < MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE; timerOff++)
    {
      switch (timerNumbUseArray[timerOff].timerUsage)
      {
      case PulseWidth:
        ret = meadow_timer_test_gated_pulse_width(timerNumbUseArray[timerOff].timerNumber);
        if(ret < 0)
        {
          syslog(LOG_ERR, "%s@%d-Meadow pulse width test failed:%d\n", __FILE__, __LINE__, ret);
        }
        break;

      case FreqDutyCycle:
        ret = meadow_timer_test_freq_and_dutycycle(timerNumbUseArray[timerOff].timerNumber);
        if(ret < 0)
        {
          syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle test failed:%d\n", __FILE__, __LINE__, ret);
        }
        break;

      case RcServoDecode:
        ret = meadow_timer_test_rc_servo_decode(timerNumbUseArray[timerOff].timerNumber);
        if(ret < 0)
        {
          syslog(LOG_ERR, "%s@%d-Meadow rc servo decode test failed:%d\n", __FILE__, __LINE__, ret);
        }
        break;
      
      default:
        break;
      }
    }

    // The following 2 test don't require the timer number because it's fixed to timer 6
    // one of the 2 basic timers.
    // Uncomment below to run either test

    // The idle code always uses timer 6
    // ret = meadow_timer_test_idle_measure_ticks();    
    // if(ret < 0)
    // {
    //   syslog(LOG_ERR, "%s@%d-Meadow measure ticks test failed:%d\n", __FILE__, __LINE__, ret);
    // }
    
    // The idle code always uses timer 6
    // ret = meadow_timer_test_idle_cpu_load();
    // if(ret < 0)
    // {
    //   syslog(LOG_ERR, "%s@%d-Meadow measure cpu load failed:%d\n", __FILE__, __LINE__, ret);
    // }
  }

  return NULL;    // Keep compiler happy
}
#endif    // #if MEADOW_TIMER_INCLUDE_TESTING_CODE > 0

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
