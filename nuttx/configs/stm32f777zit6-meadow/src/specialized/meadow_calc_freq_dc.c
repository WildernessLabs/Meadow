/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/misc/meadow_calc_freq_d_c.c
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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

// This module, with the help of a timer calculates the frequency and duty
// cycle of the timer input provided.

/****************************************************************************
 * Included Files
 ****************************************************************************/

// Consider removing this and always build
#define MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD (1)

#if MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD > 0

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

#include <nuttx/kthread.h>
#include <meadow/meadow_hw_version.h>
#include <meadow/hcom_shared_common.h>
#include <stdlib.h>

#include "specialized/meadow_calc_freq_dc.h"

#pragma message "(--) meadow_calc_freq_dutycycle.c"

//=====================================================
// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//=====================================================
// NEEDED below
//=====================================================

// TEMPORARY from meadow_timers.h
struct timerReturnData_s
{
  uint32_t timerNumber; // 1 - 14 timer number to use
  uint32_t dataField1;  // Frequency
  uint32_t dataField2;  // Duty Cycle
};

#define MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE   (7)
#define MEADOW_TIMER_MAX_TIMER_CHANNELS       (4)
#define MEADOW_TIMER_READ_GOOD_DATA_ATTEMPTS  (5)
#define MEADOW_TIMER_BAD_GPIO_VALUE           (0xffffffff)
#define MEADOW_TIMER_WIDTH_16 (0)
#define MEADOW_TIMER_WIDTH_32 (1)
#define MEADOW_TIMER_16_BIT_OVERFLOW (65536)

#define MEADOW_TIMER_FREQ_DC_SYNC_ERROR (100)
#define MEADOW_TIMER_FREQ_DC_SYNC_LEADING (101)
#define MEADOW_TIMER_FREQ_DC_SYNC_TRAILING (102)

// Count below this value are not valid because they would represent pulses
// to short to measure.
#define MEADOW_TIMER_MINIMUM_USABLE_CNT (180)

// 96 MHz is top speed. Since the interrupts are based on leading and falling
// edges the only reason for slowing the clock would be to slow down the
// number of overflows for a 16-bit timer (see comments below in ISR).
#define MEADOW_TIMER_FREQ_DC_CLK_FREQ (96000000) // 96 MHz target frequency

/****************************************************************************
 * Private Data
 ****************************************************************************/
// This internal structure contains the data that all timer applications
// require to operate.
// The timerInfo_s contains information that is defined by the F7's internal
// hardware structure. Each field is populated at build time from the
// struct timerInfo_s array.
struct timerInfo_s
{
  uint8_t timerNumb   : 4;    // 0 - 15 timer number
  uint8_t timerWidth  : 1;    // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;    // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;    // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerFuture : 1;    // Future
  uint32_t timerBase;         // Unique for each timer
  uint32_t timerClkEn;        // Bit of timer enable bit for APB1 or APB2
  uint32_t timerIrqVec;       // Interrupt vector
  void *dataPtr;              // Points to usage individual running timer
};

// This array contains timer information that is fixed by the STM32F7. It
// contains the timers that are currently available and useable. It also
// defines which timers can be used and invariant characteristics. Several
// of these values have be reduced to a bit-field simple to save space.
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

//----------------------------------------------------------------------------
// GPIOs are in their own table due to the need to change GPIO definitions 
// based on the F7 version number.
struct timerGpio_s
{
  // In Nuttx pin is bits 3:0, port bits 7:4 (one byte) and Alt Func 15:12
  uint8_t timerF7v1Gpio[4];  // GPIO for each timer channel
  uint8_t timerF7v2Gpio[4];  // GPIO for each timer channel
  uint16_t timerAltFunc;     // GPIO Alternate Function for each timer
};

// This array defines the GPIO values that must be used by the various timers.
// There can be up to 4 channels per timer. Notice that this array contains
// F7v1 and F7v2 values as well as the alternate function for each timer. It
// should be obvious but, this table must line up with the previous
// timerInfoArray table.
static struct timerGpio_s timerGpioArray[] =
{
  //                            F7v1                                              F7v2                       Alt Func
  /* TIM3  D02, D05, D06,  D09  */ {{0x26,0x27,0x10,0x11}, /* D05, D10, A03,  A04  */ {0x14,0x27,0x10,0x11}, GPIO_AF2},
  /* TIM4  D08, D07, D03*, D04* */ {{0x16,0x17,0x18,0x19}, /* D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19}, GPIO_AF2},
  /* TIM5  D10,                 */ {{0x7a,0xff,0xff,0xff}, /* D02,            A02  */ {0x7a,0xff,0xff,0x03}, GPIO_AF2},
  /* TIM9  A02,                 */ {{0x03,0xff,0xff,0xff}, /* A02                  */ {0x03,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM10 D03*,                */ {{0x18,0xff,0xff,0xff}, /* D03*                 */ {0x18,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM11 D04*,                */ {{0x19,0xff,0xff,0xff}, /* D04*                 */ {0x19,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM12 D12, D13             */ {{0x1e,0x1e,0xff,0xff}, /* D12, D13             */ {0x1e,0x1f,0xff,0xff}, GPIO_AF3},
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_init_freq_and_dutycycle(int timerNumber);

/****************************************************************************
 * Private Types
 ****************************************************************************/
// This structure contains runtime data
struct freqDcData_s
{
  volatile uint8_t timerDectSync;     // Missing interrupt detection
  volatile uint32_t timerFullPeriod;  // Full period count
  volatile uint32_t timerPartPeriod;  // Part period count
  volatile uint32_t timerFullOvrFlo;  // Full overflow count
  volatile uint32_t timerPartOvrFlo;  // Part overflow count
  uint32_t gpioInputConfig;           // Nuttx style GPIO configuration
  uint8_t inputPolarity;              // 0 = leading is rising, 1 = leading is falling
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
uint32_t meadow_timer_get_apb_clock(struct timerInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
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

//==================================================================
// This ISR is called for all frequency with duty cycle interrupts.
// On the first rising edge of the input, the timer clears the CNT count and
// CNT begins counting up.
// On the following falling edge, the timer copies the CNT value into CCR2.
// On the next rising edge, the timer copies the CNT value into CCR1 and CNT
// is again cleared to zero and the process repeats.
// This means that we must save the CCR1 and CCR2 timer values between the
// rising edge and the falling edge.
// For 16-bit timers, this is more complex because we must maintain a count
// for each CNT overflow interrupt. This allows us to maintain a 32-bit value.
// For CCR1 overflow for the entire period but for CCR2 only between the
// rising edge and the falling edge.
// Note: a 16-bit register at 96 MHz will overflow every 683 microseconds.
static int meadow_timer_freq_dutycycle_isr(int irq, void *context, void *arg)
{
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  struct freqDcData_s *freqDcData = (struct freqDcData_s *)timerInfo->dataPtr;

  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  // The Problem with 16-bit timers.
  // At certain input frequencies the CNT being cleared and the CNT overflow
  // are reported in the same interrupt. However, there is only 1 bit available
  // to indicate both conditions.
  // The highest frequency this occurs at is TimerClock/65536, which is
  // 1,464.844 Hz with a timer clock of 96 MHz. This reoccurs at the intervals
  // (TimerClock/65536)/2, (TimerClock/65536)/3 etc.
  // More details.
  // For most input frequencies, on a leading (e.g. rising) or trailing (e.g.
  // falling) edge an interrupt is generated containing the CC1IF (leading) or
  // CC2IF (trailing) flags set. The UIF flag is always set with the CC1IF
  // flag, to indicate that the CNT register has been cleared. However, at
  // some frequencies, the overflow and the CNT reset occur at the same moment.
  // The UIF is set but it cannot be determined if it indicates CNT overflow or
  // CNT reset.
  switch(timStatusReg & 0x0007)
  {
    case 0x00:    // Nothing happened, just ignore
      break;

    //------------------------------------------------------------
    case 0x01:    // Lone UIF flag. CNT register changed, either reset or overflow)
      timStatusReg &= ~GTIM_SR_UIF;

      freqDcData->timerFullOvrFlo++;
      break;

    //------------------------------------------------------------
    // Leading Edge
    case 0x02:    // Lone leading edge, never expected
      timStatusReg &= ~GTIM_SR_CC1IF;
      freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;

    case 0x03:     // Leading edge + UIF (Normal for End/Start of capture)
      timStatusReg &= ~GTIM_SR_UIF;   // Could be overflow or CNT reset
      timStatusReg &= ~GTIM_SR_CC1IF; // Leading edge should be CNT reset

      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_32)
      {
        // At this point we expect to have seen a trailing edge and no errors
        if(freqDcData->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_TRAILING)
        {
          // Save values
          freqDcData->timerFullPeriod = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
          freqDcData->timerPartPeriod = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);
        }
        else
        {
          freqDcData->timerFullPeriod = 0;
          freqDcData->timerPartPeriod = 0;
        }

        freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_LEADING;
        break;
      }

      // Must be 16-bit timer
      uint32_t count1;
      uint32_t count2;

      if(freqDcData->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_TRAILING)
      {
        // Save values
        count1 = getreg16(timerBase + STM32_GTIM_CCR1_OFFSET);
        count2 = getreg16(timerBase + STM32_GTIM_CCR2_OFFSET);

        // Add each 16-bit CNT overflow to counts
        count1 += (freqDcData->timerFullOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW);
        count2 += (freqDcData->timerPartOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW);

        // Check for various detectable errors. There are some that cannot
        // be detected.
        // Since there's a limit to the highest frequency we can detect then
        // count1 has a minimum value it can be.
        if(count1 < MEADOW_TIMER_MINIMUM_USABLE_CNT)
        {
          count1 = 0;
          count2 = 0;
        }
        else if(count1 < count2)
        {
          // This "fix" works in some cases, one is the initial frequency
          // that causes trouble (i.e. TimerClock/65536). It may be that
          // multiple overflow interrupts are being missed.
          count1 += MEADOW_TIMER_16_BIT_OVERFLOW;
        }
        else
        {
          // Reasonable Duty Cycle test, must be > 1.0% and < 99.0%
          uint32_t dutyCycle = (count2 * 1000)/count1;
          if(dutyCycle < 10 || dutyCycle > 990)
          {
            count1 = 0;
            count2 = 0;
          }
        }
      }
      else
      {
        count1 = 0;
        count2 = 0;
      }

      // Provide consumer with values
      freqDcData->timerFullPeriod = count1;
      freqDcData->timerPartPeriod = count2;
      
      // Clear previous overflow
      freqDcData->timerFullOvrFlo = 0;
      freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_LEADING;
      break;

    //------------------------------------------------------------
    // Trailing/Falling Edge
    case 0x05:    // Falling edge with UIF (i.e. assume Falling Edge + Overflow)
      timStatusReg &= ~GTIM_SR_UIF;

    case 0x04:    // Falling edge alone. End of CCR2 capture.
      timStatusReg &= ~GTIM_SR_CC2IF;

      // Falling edge check if there has been a valid leading edge
      if(freqDcData->timerDectSync != MEADOW_TIMER_FREQ_DC_SYNC_LEADING)
      {
        freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
        break;  // And quit
      }
      
      // Falling edge means we're done with CCR2's value. We don't need
      // to worry about CNT overflow with respect to CCR2 either.
      if(timStatusReg & GTIM_SR_UIF)
        freqDcData->timerFullOvrFlo++;    // Adjust overflow count

      // Time to capture the first half of the overflow
      freqDcData->timerPartOvrFlo = freqDcData->timerFullOvrFlo;
    
      // This value will be tested when the leading edge arrives
      freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_TRAILING;
      break;

    //------------------------------------------------------------
    case 0x06:    // (illegal) Rising and Falling together, no
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;

    case 0x07:    // (illegal) Rising and Falling plus UIF
      // This case exists when the duty cycle is very small (< 0.5%) or very
      // large (> 99.5%)
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timStatusReg &= ~GTIM_SR_UIF;
      freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;

    // There are only 3 bits to check, so this is a not needed.
    default:
      break;
  }

  // Clear status register as needed
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  return OK;
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

//=====================================================================
// From the timer number find the correct GPIO table entry
static struct timerGpio_s * meadow_timer_get_timer_gpio_pointer(int timerNumb)
{
  // Look through all the times and find the matching one 
  for (int offset = 0; offset < MEADOW_TIMER_TOTAL_NUMBER_AVAILABLE; offset++)
  {
    if(timerInfoArray[offset].timerNumb == timerNumb)
    {
      return ( &(timerGpioArray[offset]));
    }
  }

  return NULL;
}

//=====================================================================
// The following function finds the specified timer if one exists and
// returns a point information structure.
static struct timerInfo_s * meadow_timer_get_timer_info_pointer(int timerNumb)
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

//=============================================================
// Find the proper timer, version and channel for the GPIO Alt
// Function, Port and Pin for this timer.
uint32_t meadow_timer_get_ver_based_gpio_chan(int timerNumb,
          uint32_t pinDesignation)
{
  // Get the pointer to the master structure that contains all the GPIO
  // information.
  struct timerGpio_s *timerGpio = meadow_timer_get_timer_gpio_pointer(timerNumb);

  // Depending on the version look for the specific GPIO supplied to function
  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    for(uint32_t chan = 0; chan < MEADOW_TIMER_MAX_TIMER_CHANNELS; chan++)
    {
      if(timerGpio->timerF7v1Gpio[chan] == pinDesignation)
        return chan;
    }
    return MEADOW_TIMER_BAD_GPIO_VALUE;    // Not found
  }
  else if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2 ||
          meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    for(uint32_t chan = 0; chan < MEADOW_TIMER_MAX_TIMER_CHANNELS; chan++)
    {
      if(timerGpio->timerF7v2Gpio[chan] == pinDesignation)
        return chan;
    }
    return MEADOW_TIMER_BAD_GPIO_VALUE;    // Not found
  }
  else
  {
    return MEADOW_TIMER_BAD_GPIO_VALUE;   // Invalid version
  }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called by Meadow.Core to configure
// Timer numbers range from 1 - 14. However, some are not defined because they
// aren't available to Meadow.
int meadow_timer_freq_duty_config(uint32_t timerNumber, uint32_t gpioPort,
          uint32_t gpioPin, uint32_t gpioPolarity)
{
  int ret;
  struct freqDcData_s *freqDcData;
  uint8_t pinDesignation = gpioPort << 4 | gpioPin;

  // Check if there's already an object in this slot
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  if(timerInfo != NULL)
  {
    syslog(LOG_ERR, "%s@%d-There is already a timer defined in slot.\n", __FILE__, __LINE__);
  }
  
  // Allocate a new struct for each timer desired
  freqDcData = zalloc(sizeof(struct freqDcData_s));
  if(freqDcData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Memory allocation returned NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // Populate timer information structure
  timerInfo->dataPtr = (void *) freqDcData;

  freqDcData->gpioInputConfig = pinDesignation;

  // Timers can have 1-4 channels connected to 1-4 GPIOs. For the specified
  // timer we need to verify a proper GPIO has been selected.
  uint32_t channelFound = meadow_timer_get_ver_based_gpio_chan(timerNumber, pinDesignation);
  if(channelFound == MEADOW_TIMER_BAD_GPIO_VALUE)
  {
    syslog(2, "The requested GPIO and Timer combination are not supported\n");
    freqDcData->gpioInputConfig = MEADOW_TIMER_BAD_GPIO_VALUE;
    return -ENOTSUP;
  }

  // Valid GPIO so configure it
  stm32_configgpio(freqDcData->gpioInputConfig);

  // Save the polarity
  freqDcData->inputPolarity = gpioPolarity;
    
  // Initialized the timer itself
  ret = meadow_timer_init_freq_and_dutycycle(timerNumber);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow freq + duty cycle init failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//=============================================================
// Frequency and duty cycle measurement.
int meadow_timer_init_freq_and_dutycycle(int timerNumber)
{
  // See RM0410 Reference manual for STM32F76xxx and STM32F77xxx section 26.3.6
  // for original concept.

  // A single input (T1) is used. It is configured as input to Compare/Capture
  // registers 1 and 2. For CCR1 it is configured to for rising edge interrupt
  // and falling edge for ccr2. By using the count between interrupts for one
  // CCR's (i.e. rising to rising edges) the frequency can be found by counting
  // between interrupts from rising to falling the duty cycle can be found.

  int ret;
  uint16_t regVal16;
  uint32_t regVal32;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);

  // Has this slot already been taken?
  if(timerInfo->dataPtr != NULL)
  {
    syslog(LOG_ERR, "%s@%d-There is already a timer defined in slot.\n", __FILE__, __LINE__);
    return -ENOTEMPTY;
  }

  struct freqDcData_s *freqDcData = (struct freqDcData_s *)timerInfo->dataPtr;
  uint32_t timerBase = timerInfo->timerBase;

  freqDcData->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;

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
  // Note: the CCR1 register is readonly so the above configuration
  // instructions are wrong.
  // Capture/Compare Enable Register (CCER) is were the polarity is set by
  // CC1P & CC1NP. In Ref Man the CC1P for input describes both the CC1P and
  // CC1NP bit as if a 2 bit field. But they are actually bit 1 and bit 3.
  // Ref Man "00: non-inverted/rising edge, 01: inverted/falling edge
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xfff5;   // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)

  if(freqDcData->inputPolarity) // 0 = leading is rising, 1 = leading is falling
    regVal16 |= 0x0002;         // Set bit 1 to change 00 to 01 (inverted/falling)

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

  // CC2P & CC2NP must be opposite of CC1P & CC1NP
  if(!freqDcData->inputPolarity) // 0 = leading is rising, 1 = leading is falling
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
  
  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.

  // Find proper pre-scaler value so all timers run at the same speed, no
  // matter which clock line they are connected to.
  uint16_t prescaler = (meadow_timer_get_max_clock(timerInfo)/ \
            MEADOW_TIMER_FREQ_DC_CLK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum allowed for the timer. Either
  // 32-bit or 16-bit ARR register.
  uint32_t maxARRValue = timerInfo->timerWidth ==
            MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // Clear all interrupt sources and set the ones we need. CC1IE is the rising
  // edge, CC2IE is the falling edge and UIE is whenever the CNT register is
  // cleared or overflows, DMA/Interrupt enable register (DIER)
  // Advanced timers 1 & 8 add ATIM_DIER_COMIE | ATIM_DIER_BIE | ATIM_DIER_COMDE
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
        GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
        GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
        GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
        GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_UIE);

  // All Frequency/Duty Cycle interrupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_freq_dutycycle_isr, timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(timerInfo->timerIrqVec);

  meadow_timer_enable(timerBase);

  return OK;
}

#if MEADOW_INCLUDE_FREQ_DUTY_CYCLE_TESTS_IN_BUILD > 0
//================================================================
// Test code for gated frequency and pulse width
int meadow_timer_test_freq_and_dutycycle(int timerNumber)
{
  // Just feed pulse train into appropriate GPIO  
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  struct freqDcData_s *freqDcData = (struct freqDcData_s *)timerInfo->dataPtr;

  uint32_t validCheckCount = 0;

  // These are so once a valid value is found, a change in the timer's data
  // structure won't affect the output.
  uint32_t fullPeriod;
  uint32_t partPeriod;

  // Find valid data. This is only an issue at higher frequencies.
  for(validCheckCount = 0; validCheckCount < 5; validCheckCount++)
  {
    fullPeriod = freqDcData->timerFullPeriod;
    partPeriod = freqDcData->timerPartPeriod;
    if(fullPeriod > 0 && partPeriod > 0)
      break;

    usleep(1 * 1000);   // delay 1 - 2 ms waiting for better data
  }

  if(fullPeriod > 0 && partPeriod > 0)
  {
    // Do floating point math then convert to integer times 1000
    double dutyCycle = (double)(partPeriod * 100.0) / (double)fullPeriod;
    double freq = (double)(MEADOW_TIMER_FREQ_DC_CLK_FREQ)/ \
              (double)fullPeriod;

    uint32_t iDutyCycle = (dutyCycle * 1000.0);
    uint32_t iFrequency = (freq * 1000.0);

    syslog(2, "Freq:%06.4fHz [%lu], DC:%02.2f%% [%lu], CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
             freq, iFrequency, dutyCycle, iDutyCycle, fullPeriod, partPeriod, validCheckCount);
  }
  else
  {
    syslog(2, "Invalid data CCR1:%06lu, CCR2:%06lu, retries:%lu\n",
              fullPeriod, partPeriod, validCheckCount);
  }

  // Prevent this count from being used when there's no input.
  freqDcData->timerFullPeriod = 0;
  freqDcData->timerPartPeriod = 0;
  return OK;
}
#endif

//================================================================
// Return Frequency and Duty Cycle infomation to mono
int meadow_timer_mono_freq_duty_cycle(struct timerReturnData_s *returnData)
{
  // These insure that once a valid value is found, a change in the timer's
  // data structure won't affect the output.
  uint32_t fullPeriod;
  uint32_t validCheckCount = 0;
  uint32_t partPeriod;
  struct freqDcData_s *freqDcData;

  // Use timer number to find the information to return
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(returnData->timerNumber);
  if(timerInfo == NULL)
  {
    return -ENXIO;      // Unsupported timer for this feature
  }

  // Is there an object here?
  if(timerInfo->dataPtr == NULL)
  {
    return -ENODATA;      // No timer initialized
  }

  freqDcData = (struct freqDcData_s *)timerInfo->dataPtr;

  // Find valid data. This is only an issue at higher frequencies.
  for(validCheckCount = 0;\
      validCheckCount < MEADOW_TIMER_READ_GOOD_DATA_ATTEMPTS;
      validCheckCount++)
  {
    fullPeriod = freqDcData->timerFullPeriod;
    partPeriod = freqDcData->timerPartPeriod;

    if(fullPeriod > 0 && partPeriod > 0)
      break;      // Located valid data

    usleep(2 * 1000);   // delay 1-2 ms waiting for better data
  }

  if(fullPeriod > 0 && partPeriod > 0)
  {
    // Do floating point math for initial calculations to get good resolution.
    double dutyCycle = (double)(partPeriod * 100.0) / (double)fullPeriod;
    double freq = (double)(MEADOW_TIMER_FREQ_DC_CLK_FREQ)/ \
              (double)fullPeriod;

    // Multiply by 1000 and convert to uint32_t for return.
    returnData->dataField1 = (uint32_t)(freq * 1000.0);
    returnData->dataField2 = (uint32_t)(dutyCycle * 1000.0);
  }
  else
  {
    // Return 0s as no data available
    returnData->dataField1 = 0;
    returnData->dataField2 = 0;
  }

  // Prevent this count from being used when there's no input.
  freqDcData->timerFullPeriod = 0;
  freqDcData->timerPartPeriod = 0;

  return OK;
}

#endif    // #if defined(MEADOW_INCLUDE_CALC_FREQ_DC_IN_BUILD)
