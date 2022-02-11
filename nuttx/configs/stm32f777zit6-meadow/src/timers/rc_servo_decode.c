/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/rc_servo_decode.c
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

// This implementation only support Timers 3 and 4 because they are connected
// to 4 GPIOs for F7v1 and F7v2. While Timer 8 has 3 GPIOs these are the same
// as those on Timer 3, so might as well use timer 3.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "meadow_timers.h"

#include <stdlib.h>

#if defined(CONFIG_MEADOW_TIMER_SUPPORT)
//===================================================================

// This frequency is low enough to prevent more than one 16-bit overflow when
// the input frequence is 50Hz which is the frequency used by RC Servo's.
#define MEADOW_TIMER_RC_SERVO_CLK_FREQ (3000000) // 3MHz target frequency

/****************************************************************************
 * Private Data
 ****************************************************************************/

// This struct is pointed to by the dataPtr element of the timerItem table
struct rcServoData_s
{
  // These change during execution or are set during configuration. So, they
  // can be in allocated memory.
  uint8_t inputPolarity1 : 1;       // 0 = leading is rising, 1 = leading is falling
  uint8_t inputPolarity2 : 1;
  uint8_t inputPolarity3 : 1;
  uint8_t inputPolarity4 : 1;
  uint32_t gpioInputConfig[4];      // Nuttx style GPIO configurations
  volatile uint16_t timerPulWid1;   // Pulse width channel 1 (0-65535 microsec)
  volatile uint16_t timerPulWid2;   // Pulse width channel 2
  volatile uint16_t timerPulWid3;   // Pulse width channel 3
  volatile uint16_t timerPulWid4;   // Pulse width channel 4
  volatile uint32_t timerLeadCnt1;  // Leading edge Count channel 1
  volatile uint32_t timerLeadCnt2;  // Leading edge Count channel 2
  volatile uint32_t timerLeadCnt3;  // Leading edge Count channel 3
  volatile uint32_t timerLeadCnt4;  // Leading edge Count channel 4
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_init_rc_servo_decode(int timerNumber);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int meadow_timer_isr_rc_servo_decode(int irq, void *context, void *arg)
{
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  struct rcServoData_s *rcServoData = (struct rcServoData_s *)timerInfo->dataPtr;

  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    timStatusReg &= ~GTIM_SR_CC1IF;

    // Rising or falling is leading edge?
    bool inputState = stm32_gpioread(rcServoData->gpioInputConfig[0]);
    if(rcServoData->inputPolarity1)
      inputState = !inputState;

    if(inputState)
    {
      // Leading edge
      rcServoData->timerLeadCnt1 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR1_OFFSET);
    }
    else
    {
      // Trailing edge
      uint32_t prevLeadingCount = rcServoData->timerLeadCnt1;

      // Valid leading edge?
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR1_OFFSET);

        // Correct for overflow
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        // Pulse width in counts
        uint32_t pulseCount = currentCount - prevLeadingCount;

        // Pulse width in micro seconds (1 - 65535). Clock frequency chosen
        // for 50Hz input frequency.
        rcServoData->timerPulWid1 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        // Throw away as previous leading edge count was zero
        rcServoData->timerPulWid1 = 0;
      }

      // Set to 0 for leading edge detection
      rcServoData->timerLeadCnt1 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    bool inputState = stm32_gpioread(rcServoData->gpioInputConfig[1]);
    
    if(rcServoData->inputPolarity2)
      inputState = !inputState;

    if(inputState)
    {
      rcServoData->timerLeadCnt2 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR2_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = rcServoData->timerLeadCnt2;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR2_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        rcServoData->timerPulWid2 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulWid2 = 0;
      }

      rcServoData->timerLeadCnt2 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    timStatusReg &= ~GTIM_SR_CC3IF;

    bool inputState = stm32_gpioread(rcServoData->gpioInputConfig[2]);
    
    if(rcServoData->inputPolarity3)
      inputState = !inputState;

    if(inputState)
    {
      rcServoData->timerLeadCnt3 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR3_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = rcServoData->timerLeadCnt3;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR3_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        rcServoData->timerPulWid3 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulWid3 = 0;
      }

      rcServoData->timerLeadCnt3 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    bool inputState = stm32_gpioread(rcServoData->gpioInputConfig[3]);

    if(rcServoData->inputPolarity4)
      inputState = !inputState;

    if(inputState)
    {
      rcServoData->timerLeadCnt4 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR4_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = rcServoData->timerLeadCnt4;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR4_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        rcServoData->timerPulWid4 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulWid4 = 0;
      }

      rcServoData->timerLeadCnt4 = 0;
    }  
  }

  //--------------------------------------------
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_rc_servo_decode(struct timerConfig_s timerConfig)
{
  int ret;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerConfig.timerNumber);
  struct rcServoData_s *rcServoData;

  if(timerInfo == NULL)
  {
    syslog(1, "meadow_timer_get_timer_info_pointer() returned NULL\n");
    return -ENXIO;
  }

  rcServoData = malloc(sizeof(struct rcServoData_s));
  memset(rcServoData, 0, sizeof(struct rcServoData_s));
  timerInfo->dataPtr = (void *) rcServoData;

  // Initialize GPIOs for this timer based on version information
  for(int i = 0; i < 4; i++)
  {
    uint32_t afPortPin = meadow_timer_get_ver_based_gpio_chan(timerConfig.timerNumber, i);
    if(afPortPin != 0xffff)
    {
      rcServoData->gpioInputConfig[i] = MEADOW_TIMER_GPIO_CONST | afPortPin;
      stm32_configgpio(rcServoData->gpioInputConfig[i]);
    }
    else
    {
      syslog(1, "meadow_timer_setup_rc_servo_decode() no gpio at offset:%d\n", i);
      rcServoData->gpioInputConfig[i] = MEADOW_TIMER_BAD_GPIO_VALUE;
      return -1;
    }
  }

  // Save the polarity of each input
  rcServoData->inputPolarity1 = timerConfig.polarityChan1;
  rcServoData->inputPolarity2 = timerConfig.polarityChan2;
  rcServoData->inputPolarity3 = timerConfig.polarityChan3;
  rcServoData->inputPolarity4 = timerConfig.polarityChan4;

  // Initialized the timer itself
  ret = meadow_timer_init_rc_servo_decode(timerConfig.timerNumber);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Meadow rc servo decode init failed:%d\n", __FILE__, __LINE__, ret);
    return ret;
  }

  return OK;
}

//=============================================================
// RC Servo Decode. The signal to be decoded is a pulse between 1 ms and 2 ms.
// These pulses are sent at a 50/per second rate (50 Hz).
int meadow_timer_init_rc_servo_decode(int timerNumber)
{
  int ret;
  uint16_t regVal16;
  uint32_t regVal32;
  uint32_t dierBits = 0;

  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  struct rcServoData_s *rcServoData = (struct rcServoData_s *)timerInfo->dataPtr;

  uint32_t timerBase = timerInfo->timerBase;

  // Before starting disable capture/control for all channels. Ref Man (26.4.7 at
  // end) "Note: CC1S bits are writable only when the channel is OFF (i.e.
  // CC1E = 0 in TIMx_CCER)." 
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xeeee;   // e = 1110, clear CCxE bits 0, 4, 8 & 12
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Find the channels with valid F7v1 or F7v2 inputs
  bool chan1 = rcServoData->gpioInputConfig[0] == MEADOW_TIMER_BAD_GPIO_VALUE ? false : true;
  bool chan2 = rcServoData->gpioInputConfig[1] == MEADOW_TIMER_BAD_GPIO_VALUE ? false : true;
  bool chan3 = rcServoData->gpioInputConfig[2] == MEADOW_TIMER_BAD_GPIO_VALUE ? false : true;
  bool chan4 = rcServoData->gpioInputConfig[3] == MEADOW_TIMER_BAD_GPIO_VALUE ? false : true;
  
  // Set the input triggers, rising, falling or both for each channel by
  // setting the CCxP and CCXNP bits in the CCER
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);

  // CCMR1 handles channels 1 & 2
  // Configure TI1 to IC1 etc.
  regVal32 = 0;   // For input 1 & 2, Input Capture Filters [15:12] & [7:4]
  // and Input Capture Prescaler [9:8] & [1:0] are disabled by setting to 0.

  if(chan1)
  {
    // 01: IC1 is mapped on TI1
    regVal32 |= 0x00000001;   // 1 = 01, set bits 1:0
    dierBits |= GTIM_DIER_CC1IE;

    // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)
    regVal16 |= (GTIM_CCER_CC1P | GTIM_CCER_CC1NP); // Both rising and falling (0b11)
  }
  if(chan2)
  {
    // 01: IC2 is mapped on TI2
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    dierBits |= GTIM_DIER_CC2IE;

    // GTIM_CCER_CC2NP (bit 7) & GTIM_CCER_CC2P (bit 5)
    regVal16 |= (GTIM_CCER_CC2P | GTIM_CCER_CC2NP); // Both rising and falling
  }
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

  // CCMR2 handles channels 3 & 4
  regVal32 = 0;   // For input 3 & 4, Input Capture Filters [15:12] & [7:4]
  // and Input Capture Prescaler [9:8] & [1:0] are disabled by setting to 0.
  if(chan3)
  {
    // 01: IC3 is mapped on TI3
    regVal32 |= 0x00000001;   // 1 = 01, set bits 1:0
    dierBits |= GTIM_DIER_CC3IE;

    // GTIM_CCER_CC4NP (bit 11) & GTIM_CCER_CC4P (bit 9)
    regVal16 |= (GTIM_CCER_CC3P | GTIM_CCER_CC3NP); // Both rising and falling
  }
  if(chan4)
  {
    // 01: IC4 is mapped on TI4
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    dierBits |= GTIM_DIER_CC4IE;

    // GTIM_CCER_CC4NP (bit 15) & GTIM_CCER_CC4P (bit 13)
    regVal16 |= (GTIM_CCER_CC4P | GTIM_CCER_CC4NP); // Both rising and falling
  }
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

  // Save rising, falling or both
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.

  // Find proper pre-scaler value so all rc servo timers run at the same speed
  uint16_t prescaler = (meadow_timer_get_max_clock(timerInfo)/ \
            MEADOW_TIMER_RC_SERVO_CLK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == \
            MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // External Clock Enable (ECE bit 14) needs to be diabled.
  // as does Slave Mode (SMS bit 16, DISAB 3:0) 
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= ~(GTIM_SMCR_ECE | GTIM_SMCR_DISAB | GTIM_SMCR_SMS);
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // Enable the timer input capture, which was disabled earlier
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 |= ( GTIM_CCER_CC4E | \
                GTIM_CCER_CC3E | \
                GTIM_CCER_CC2E | \
                GTIM_CCER_CC1E); // set bits 0, 4, 8 & 12
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  // Clear all interrupt sources and set the ones we need in the DMA/Interrupt
  // enable register (DIER). Note: Advanced timers 1 & 8 add ATIM_DIER_COMIE,
  // ATIM_DIER_BIE and ATIM_DIER_COMDE
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          dierBits);

  // All Frequency/Duty Cycle interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_rc_servo_decode, timerInfo);
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

//================================================================
// Test code for gated frequency and pulse width
int meadow_timer_test_rc_servo_decode(int timerNumber)
{
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  struct rcServoData_s *rcServoData = (struct rcServoData_s *)timerInfo->dataPtr;

  syslog(1, "+++> Pulse Width - Channel 1:%04lu, Channel 2:%04lu, Channel 3:%04lu, Channel 4:%04lu\n",
          rcServoData->timerPulWid1,
          rcServoData->timerPulWid2,
          rcServoData->timerPulWid3,
          rcServoData->timerPulWid4);

  // Insure that when input stops the output will be 0.
  rcServoData->timerPulWid1 = 0;
  rcServoData->timerPulWid2 = 0;
  rcServoData->timerPulWid3 = 0;
  rcServoData->timerPulWid4 = 0;
          
  return OK;
}

//================================================================
// Return RC Servo infomation to mono
int meadow_timer_mono_rc_servo_decode(struct timerReturnData_s *returnData)
{
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(returnData->timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  struct rcServoData_s *rcServoData = (struct rcServoData_s *)timerInfo->dataPtr;

  if(returnData->timerUsage != RcServoDecode)
  {
    syslog(LOG_ERR, "RC Servo Decode called but usage:%u, expected:%u\n",
              returnData->timerUsage, RcServoDecode);
    return -1;
  }
  
  returnData->dataField1 = rcServoData->timerPulWid1;
  returnData->dataField2 = rcServoData->timerPulWid1;
  returnData->dataField3 = rcServoData->timerPulWid1;
  returnData->dataField4 = rcServoData->timerPulWid1;

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
