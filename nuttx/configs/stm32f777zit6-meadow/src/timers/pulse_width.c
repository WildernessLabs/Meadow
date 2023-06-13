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

#include <stdlib.h>

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)
//===================================================================

#define MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ (96000000) // 96MHz target frequency

// These values were picked so that the 6 usec glitch that occurs when the
// HC_SR04 sends a pulse but gets no response. This 6 usec pulse follows a
// long pulse of about 17 milliseconds and this 6 usec pulse follows the
// long pulse's falling edge by 146 usec.
#define MEADOW_TIMER_PULSE_WIDTH_HCSR04_BOTTOM (574) // only valid at 96MHz
#define MEADOW_TIMER_PULSE_WIDTH_HCSR04_TOP    (579) // only valid at 96MHz

// This is the default pulse width time out value
#define MEADOW_TIMER_PULSE_WIDTH_TIME_OUT (1000)

// USED FOR TRIGGERING HC-SR04. THIS MUST BE IN THE .NET CODE FOR A REAL APPLICATION
#define MEADOW_TIMER_TEST_GPIO_D15_OUT  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | \
          GPIO_PORTG | GPIO_PIN12)

/****************************************************************************
 * Private Data
 ****************************************************************************/
struct pulseWidData_s
{
  volatile uint32_t timerCount;   // Primary value of the count
  volatile uint32_t timerOvrFlo;  // Count of any overflow
  uint16_t pulseTimeoutMs;        // How long to wait for pulse to end? Default is 1 second
  uint8_t pwHCSR04Filter;         // 0 = don't removes 6 usec glitch when no target
  uint32_t gpioInputConfig;       // Nuttx style GPIO configuration
  uint8_t inputPolarity;          // 0 = leading is rising, 1 = leading is falling
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_init_gated_pulse_width(int timerNumber);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t _endPWidthSem;

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

// This ISR is called only twice per pulse. Once for counter start and again for
// counter stop.
static int meadow_timer_isr_pulse_width(int irq, void *context, void *arg)
{
  // The timer structure is returned because we told Nuttx this would be 'arg'
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  struct pulseWidData_s *pulseWidData = (struct pulseWidData_s *)timerInfo->dataPtr;
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
    bool inputState = stm32_gpioread(pulseWidData->gpioInputConfig);
    if(!inputState)
    {
      // The input point's state indicates that the counting has stopped.
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
        pulseWidData->timerCount = (uint32_t)getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
      else
        pulseWidData->timerCount = getreg32(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

      if(pulseWidData->pwHCSR04Filter)
      {
        // These values indicate a very fast pulse
        if(pulseWidData->timerCount > MEADOW_TIMER_PULSE_WIDTH_HCSR04_BOTTOM &&
           pulseWidData->timerCount < MEADOW_TIMER_PULSE_WIDTH_HCSR04_TOP)
        {
          // Throw away the count. This way a zero reading is returned
          pulseWidData->timerCount = 0;
          pulseWidData->timerOvrFlo = 0;
        }
      }
      
      // If 16-bit add the CNT overflows
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
        pulseWidData->timerCount += pulseWidData->timerOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW;

      sem_post(&_endPWidthSem); // Allow the requesting thread to process data
    }
    else
    {
      // Leading edge indicates start so clear the previous values
      pulseWidData->timerCount = 0;
      pulseWidData->timerOvrFlo = 0;
    }
  }

  //----------------------------------------------------------
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // GTIM_SR_UIF indicates overflow or underflow of CNT, since we only count
    // up it must mean overflow.
    pulseWidData->timerOvrFlo++;
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_timer_setup_pulse_width(struct timerConfig_s timerConfig)
{
  int ret;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerConfig.timerNumber);
  struct pulseWidData_s *pulseWidData;

  pulseWidData = malloc(sizeof(struct pulseWidData_s));
  memset(pulseWidData, 0, sizeof(struct pulseWidData_s));
  timerInfo->dataPtr = (void *)pulseWidData;

  // Set the time to wait for the trailing edge of the pulse to arrive.
  if(timerConfig.pwTimeroutMs > 0)
    pulseWidData->pulseTimeoutMs = timerConfig.pwTimeroutMs;
  else
    pulseWidData->pulseTimeoutMs = (uint16_t)MEADOW_TIMER_PULSE_WIDTH_TIME_OUT;

  pulseWidData->pwHCSR04Filter = timerConfig.pwHCSR04Filter;

  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);

  // Get and test the GPIO for this Timer
  uint32_t afPortPin = meadow_timer_get_ver_based_gpio_timer(timerConfig.timerNumber);
  if(afPortPin != 0xffff)
  {
    // Even if not directly read or written it must be configured
    pulseWidData->gpioInputConfig = MEADOW_TIMER_GPIO_CONST | afPortPin;
    stm32_configgpio(pulseWidData->gpioInputConfig);
  }
  else
  {
    // syslog(2, "meadow_timer_setup_freq_dc_decode() no GPIO defined\n");
    pulseWidData->gpioInputConfig = MEADOW_TIMER_BAD_GPIO_VALUE;
    return -1;
  }

  // Save the polarity
  pulseWidData->inputPolarity = timerConfig.polarityChan1;

  // Initialized the timer itself
  ret = meadow_timer_init_gated_pulse_width(timerConfig.timerNumber);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow pulse width init failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//=============================================================
// Pulse Width inititalization utilizing gate-controlled measurement.
// This is only useable with channels 1 & 2. Channel 1 
int meadow_timer_init_gated_pulse_width(int timerNumber)
{
  int ret;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  struct pulseWidData_s *pulseWidData = (struct pulseWidData_s *)timerInfo->dataPtr;
  uint32_t timerBase = timerInfo->timerBase;

  // Disable slave mode while configuring
  uint32_t smcr_val = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  smcr_val &= ~GTIM_SMCR_DISAB;
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  // Capture/Compare Enable Register (CCER) is were the polarity is set by
  // CC1P & CC1NP bits. In Ref Man the CC1P for input discribes both the CC1P
  // and CC1NP bit as if a 2 bit field. But they are actually bit 1 and bit 3.
  // Ref Man "00: noninverted/rising edge, 01: inverted/falling edge
  uint16_t regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xfff5;   // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)
  if(pulseWidData->inputPolarity) // 0 = leading is rising, 1 = leading is falling
    regVal16 |= 0x0002;         // Set bit 1 to change 00 to 01 (inverted/falling)
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);

  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.

  // Find proper pre-scaler value so all timers run at the same speed
  uint16_t prescaler = (meadow_timer_get_max_clock(timerInfo)/ \
            MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = meadow_timer_get_max_clock(timerInfo) == \
                        MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

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

  // Note: Gated mode requires either channel 1 or 2. Channels 3 and 4 are
  // not useable for this function. And cannot use both channel 1 and 2.
  // Set TI1 or TI2 Edge Detector and Gated Mode
  // GTIM_SMCR_TI1FP1 / GTIM_SMCR_TI1FP2
  // Only allowed to use channel 1
  smcr_val |= (GTIM_SMCR_TI1FP1 | GTIM_SMCR_GATED);
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

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(timerInfo->timerIrqVec);

  meadow_timer_enable(timerBase);

  return OK;
}

#if MEADOW_INCLUDE_TIMER_HARDWARE_TESTS_IN_BUILD > 0
//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(int timerNumber)
{
  int ret;
  
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  struct pulseWidData_s *pulseWidData = (struct pulseWidData_s *)timerInfo->dataPtr;

//----------------------------------------------------------------------------------
// This block of code is only for testing. This functionality must come from
// the mono app.
  // THIS GPIO CONFIG ONLY NEEDS TO BE DONE ONCE! BUT, I WANTED ALL THE CODE
  // THAT MUST ULTIMATELY BE PART OF THE .NET CODE TO NOT BE SPREAD ALL OVER.
  // Configure output GPIO for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);

  // Send pulse to HC-SR04, this must be at least 2us wide to signal the
  // HC-SR04 to send ultrasonic pulses and wait for the echo.
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, true);

  // HC-SR04 requires a trigger pulse of at least 2 usec. However, a 1-2 ms
  // pulse works too. Because of the Nuttx usleep resolution the following
  // usleep(1) will sleep between 1-2 ms. The HC-SR04 generates it's timing
  // pulse (echo) about 500us after the trigger pulses falling edge.
  usleep(1);
  stm32_gpiowrite(MEADOW_TIMER_TEST_GPIO_D15_OUT, false);
//----------------------------------------------------------------------------------

  // Wait for ISR to indicate timer has finished
  struct timespec abstime;
  ret = clock_gettime(CLOCK_REALTIME, &abstime);
  
  // Add the timeout value
  abstime.tv_nsec += pulseWidData->pulseTimeoutMs * 1000 * 1000;
  if (abstime.tv_nsec >= 1000 * 1000 * 1000)
  {
    abstime.tv_sec++;
    abstime.tv_nsec -= 1000 * 1000 * 1000;
  }

  ret = sem_timedwait(&_endPWidthSem, &abstime);
  if(ret < 0)
  {
    // Error. Could mean that the semaphore timed out. In this case we insure
    // that the semaphore count is correct. If not correct, it means that the
    // ISR didn't do the sem_post() call. Therefore, we need to call sem_post
    // to keep the semaphore in sync with the ISR.
    syslog(LOG_ERR, "ERROR:Pulse width-Semaphore ret:%d, errno:%d\n", ret, errno);

    int semcount;
    sem_getvalue(&_endPWidthSem, &semcount);

    if(semcount == 0)
      sem_post(&_endPWidthSem);

    ret = -1;
  }
  else
  {
    // Successfully read the pulse width.
    uint32_t cntValue = pulseWidData->timerCount;
    if(cntValue > 0)
    {
      double totalTimeSec = (double)cntValue / (double)MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ;

      // This will be the only value to return to managed code. On the managed
      // side, depending on the application it can be processed as needed.
      // Of course we don't have nano second resolution but this will allow the
      // managed code to get a pretty accurate double value.
      // However, this also means that the largest pulse we can measure is one
      // that is 4.294967295 seconds long.
      // uint32_t iPeriodNanoSec = (uint32_t)(totalTimeSec * 1000000000.0);

      // The following is specific to the HC-SR04
      // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec,
      // at 25C = 346.13
      double oneWayTimeSec = totalTimeSec/2.0;
      double distance = oneWayTimeSec /*seconds*/ * 345; /* meters/second*/

      // syslog(2, "Count:%lu, Time:%4.8f, Distance:%1.6fm\n",
                cntValue, oneWayTimeSec, distance);
      ret = OK;
    }
    else
    {
      // syslog(2, "ERROR:Timer%u count was %lu\n", timerInfo->timerNumb, cntValue);
      ret = -1;
    }
  }

  // Need to "ARM" the system by clearing the previous counts
  pulseWidData->timerCount = 0;   // Primary value of the count
  pulseWidData->timerOvrFlo = 0;  // Count of any overflow

  if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
    putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
  else
    putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

  return OK;
}

#endif

//================================================================
// Return Pulse Width infomation to mono
int meadow_timer_mono_pulse_width(struct timerReturnData_s *returnData)
{
  int ret;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(returnData->timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  struct pulseWidData_s *pulseWidData = (struct pulseWidData_s *)timerInfo->dataPtr;

  if(returnData->timerUsage != PulseWidth)
  {
    syslog(LOG_ERR, "Pulse Width called but received:%u, expected:%u\n",
              returnData->timerUsage, PulseWidth);
    return -1;
  }

  // Wait for ISR to indicate timer has finished
  struct timespec abstime;
  ret = clock_gettime(CLOCK_REALTIME, &abstime);
  
  abstime.tv_nsec += pulseWidData->pulseTimeoutMs * 1000 * 1000;
  if (abstime.tv_nsec >= 1000 * 1000 * 1000)
  {
    abstime.tv_sec++;
    abstime.tv_nsec -= 1000 * 1000 * 1000;
  }

  ret = sem_timedwait(&_endPWidthSem, &abstime);
  if(ret < 0)
  {
    // Error. Could mean that the semaphore timed out. In this case we insure
    // that the semaphore count is correct. If not correct, it means that the
    // ISR didn't do the sem_post() call. Therefore, we need to call sem_post
    // to keep the semaphore in sync with the ISR.
    syslog(LOG_ERR, "ERROR:Pulse width-Semaphore ret:%d, errno:%d\n", ret, errno);

    int semcount;
    sem_getvalue(&_endPWidthSem, &semcount);

    if(semcount == 0)
      sem_post(&_endPWidthSem); 
    ret = -1;
  }
  else
  {
    // Read the pulse width.
    uint32_t cntValue = pulseWidData->timerCount;
    if(cntValue > 0)
    {
      double totalTimeSec = (double)cntValue / (double)MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ;

      // This is the only value returned to managed code. On the managed side,
      // depending on the application it can be processed as needed.
      // Of course we don't have nano second resolution but this will allow
      // the managed code to create a pretty accurate double value.
      // However, this also means that the longest pulse we can measure is one
      // that is 4.294967295 seconds long.
      uint32_t iPeriodNanoSec = (uint32_t)(totalTimeSec * 1000000000.0);
      returnData->dataField1 = iPeriodNanoSec;
      ret = OK;
    }
    else
    {
      syslog(LOG_ERR, "--> ERROR:Timer%u pulse width count was 0\n",
                timerInfo->timerNumb);
      ret = -1;
    }
  }
  
  // Need to "ARM" the system by clearing the CNT register and the other
  // items for the next cycle.
  pulseWidData->timerCount = 0;   // Primary value of the count
  pulseWidData->timerOvrFlo = 0;  // Count of any overflow

  if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
    putreg16(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
  else
    putreg32(0, timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

  return ret;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
