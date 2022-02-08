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

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

// PeterM - This can probably be trimmed to a lower frequency and still work
// just as good as 96MHz
#define MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ (96000000) // 96MHz target frequency

#define MEADOW_TIMER_PULSE_WID_BAD_GPIO (0xffffffff)

/****************************************************************************
 * Private Data
 ****************************************************************************/
struct pulseWidData_s
{
  volatile uint32_t timerCount;       // Primary value of the count
  volatile uint32_t timerOvrFlo;      // Count of any overflow
  uint32_t timerGpioCfg;              // GPIO definition
};

struct pwidthInfo_s
{
  uint8_t timerNumb   : 4;            // 0 - 15 timer number as diagnostic
  uint8_t timerWidth  : 1;            // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;            // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;            // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerPolarity : 1;          // Used during config, 0 = Leading is Rising, 1 = Leading is Falling
  uint32_t timerBase;                 // Unique for each timer
  uint32_t timerClkEn;                // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;               // Interrupt vector
  struct pulseWidData_s *dataPtr;     // Points to the variable data array
};

static struct pwidthInfo_s pwidthInfoArray[] = 
{
            //   |--- bit-field---|
            //   #  wid max apb pol    Base Addr       Timer Clk Enable      IRQ Vector    Ptr
  /* TIM3   */  {3 , 0,  0,  0,  0, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3 , 0},
  /* TIM4   */  {4 , 0,  0,  0,  0, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4 , 0},
  /* TIM5   */  {5 , 1,  0,  0,  0, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5 , 0},
  /* TIM9   */  {9 , 0,  1,  1,  0, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9 , 0},
  /* TIM10  */  {10, 0,  1,  1,  0, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, 0},
  /* TIM11  */  {11, 0,  1,  1,  0, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, 0},
  /* TIM12  */  {12, 0,  0,  0,  0, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, 0},
};

#define MEADOW_TIMER_PULSE_WID_TOTAL_NUMB (sizeof(pwidthInfoArray) / sizeof(struct pwidthInfo_s))

// GPIOs are in there own table due to the need to change GPIO definitions 
// based on the F7 version number. Hopefully, if there's additional versions
// this will simplify the effort
struct pWidthGpio_s
{
  // In Nuttx pin is bits 3:0, port bits 7:4 and Alt Func 15:12
  uint8_t timerF7v1Gpio;    // GPIO for each timer channel
  uint8_t timerF7v2Gpio;    // GPIO for each timer channel
  uint16_t timerAltFunc;    // GPIO Alternate Function for each timer
};

// Same timers as above
// Note: Since this is implemented using Gate Mode the CNT is started and
// stopped with each rising/falling edge. There for only one input per
// timer can be used.
static struct pWidthGpio_s pWidthGpioArray[] =
{
  //          F7v1            F7v2       Alt Func
  /* TIM3  D02 */ {0x26, /* D05 */ 0x14, GPIO_AF2},
  /* TIM4  D08 */ {0x16, /* D08 */ 0x16, GPIO_AF2},
  /* TIM5  D10 */ {0x7a, /* D02 */ 0x7a, GPIO_AF2},
  /* TIM9  A02 */ {0x03, /* A02 */ 0x03, GPIO_AF3},
  /* TIM10 D03 */ {0x18, /* D03 */ 0x18, GPIO_AF3},
  /* TIM11 D04 */ {0x19, /* D04 */ 0x19, GPIO_AF3},
  /* TIM12 D12 */ {0x1e, /* D12 */ 0x1e, GPIO_AF3},
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

// This ISR is called only twice per pulse. Once for counter start and again for
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
    bool inputState = stm32_gpioread(timerInfo->dataPtr->timerGpioCfg);

// timerInfo->timerPolarity

    if(!inputState)
    {
      // The input point's state indicates that the counting has stopped.
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
        timerInfo->dataPtr->timerCount = (uint32_t)getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
      else
        timerInfo->dataPtr->timerCount = getreg32(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);

      if(!mtcHC_SR04Filter)
      {
        // These values indicate a very fast pulse
        // PeterM - THESE HARD CODED VALUES ARE BAD!!! THEY ARE RELATIVE TO THE
        // TIMER'S CLOCK FREQ AND BASED ON A 96MHz CLOCK.
        if(timerInfo->dataPtr->timerCount > 574 && timerInfo->dataPtr->timerCount < 579)
        {
          // Throw away the count. This way a zero reading is returned
          timerInfo->dataPtr->timerCount = 0;
          timerInfo->dataPtr->timerOvrFlo = 0;
        }
      }
      
      // If 16-bit add the CNT overflows
      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
        timerInfo->dataPtr->timerCount += timerInfo->dataPtr->timerOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW;

      sem_post(&_endPWidthSem); // Allow the requesting thread to process data
    }
    else
    {
      // Leading edge indicates start so clear the previous values
      timerInfo->dataPtr->timerCount = 0;
      timerInfo->dataPtr->timerOvrFlo = 0;
    }
  }

  //----------------------------------------------------------
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // GTIM_SR_UIF indicates overflow or underflow of CNT, since we only count
    // up it must mean overflow.
    timerInfo->dataPtr->timerOvrFlo++;
  }

  return OK;
}

//=============================================================
// Find the proper timer, version and channel for the GPIO Alt
// Function, Port and Pin for this timer.
static uint16_t meadow_timer_get_gpio_for_timer(int timerNumb)
{
  for (int i = 0; i < MEADOW_TIMER_PULSE_WID_TOTAL_NUMB; i++)
  {
    if(pwidthInfoArray[i].timerNumb == timerNumb)
    {
      if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
      {
        return pWidthGpioArray[i].timerF7v1Gpio |
                  pWidthGpioArray[i].timerAltFunc;
      }
      else if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2 ||
              meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2)
      {
        return pWidthGpioArray[i].timerF7v2Gpio |
                  pWidthGpioArray[i].timerAltFunc;
      }
      else
      {
        return 0xffff;   // Invalid version
      }
    }
  }

  return 0xffff;
}

//=============================================================
static struct pwidthInfo_s * meadow_timer_get_timer_pointer(int timerNumb)
{
  for (int i = 0; i < MEADOW_TIMER_PULSE_WID_TOTAL_NUMB; i++)
  {
    if(pwidthInfoArray[i].timerNumb == timerNumb)
    {
      return ( &(pwidthInfoArray[i]));
    }
  }

  return NULL;
}

//=============================================================
static uint32_t meadow_timer_get_apb_clock(struct pwidthInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
static uint32_t meadow_timer_get_max_clock(struct pwidthInfo_s *timerInfo)
{
  if(timerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_timer_setup_pulse_width(int timerNumber)
{
  struct pwidthInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);

  timerInfo->dataPtr = malloc(sizeof(struct pulseWidData_s));
  memset(timerInfo->dataPtr, 0, sizeof(struct pulseWidData_s));

  sem_init(&_endPWidthSem, 0, 0);
  sem_setprotocol(&_endPWidthSem, SEM_PRIO_NONE);

  // Get and test the GPIO for this Timer
  uint32_t afPortPin = meadow_timer_get_gpio_for_timer(timerNumber);

  if(afPortPin != 0xffff)
  {
    // Even if not directly read or written it must be configured
    timerInfo->dataPtr->timerGpioCfg = MEADOW_TIMER_GPIO_CONST | afPortPin;
    stm32_configgpio(timerInfo->dataPtr->timerGpioCfg);
  }
  else
  {
    syslog(1, "meadow_timer_setup_freq_dc_decode() no gpio defined\n");
    timerInfo->dataPtr->timerGpioCfg = MEADOW_TIMER_PULSE_WID_BAD_GPIO;
  }

  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_gated_pulse_width(int timerNumber)
{
  int ret;
  
  struct pwidthInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);
  
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
    uint32_t cntValue = timerInfo->dataPtr->timerCount;
    if(cntValue > 0)
    {
      // Temperature effects speed of sound. At 20 degrees C = 343.21 M/Sec,
      // at 25C = 346.13
      double totalTimeMs = (double)cntValue / (double)MEADOW_TIMER_PULSE_WIDTH_CLK_FREQ;
      double oneWayTimeMs = totalTimeMs/2.0;
      double distance = oneWayTimeMs /*seconds*/ * 345; /* meters/second*/
      syslog(1, "=====> Count:%lu, Time:%4.8fms, Distance:%1.6fm\n",
                cntValue, oneWayTimeMs * 1000, distance);
    }
    else
    {
      syslog(1, "--> ERROR:Timer%u count was %lu\n",
              timerInfo->timerNumb, cntValue);
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

  struct pwidthInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);
  uint32_t timerBase = timerInfo->timerBase;

  // Output for triggering HC-SR04 to begin a distance measurement
  stm32_configgpio(MEADOW_TIMER_TEST_GPIO_D15_OUT);

  // Disable slave mode while configuring
  uint32_t smcr_val = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  smcr_val &= ~GTIM_SMCR_DISAB;
  putreg32(smcr_val, timerBase + STM32_GTIM_SMCR_OFFSET);

  //------------------------------------------
  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);

  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.

  // Find proper pre-scaler value so all rc servo timers run at the same speed
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

// #endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
