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

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

// This frequency is low enough to prevent more than one 16-bit overflow when
// the input frequence is 50Hz which is the frequency used by RC Servo's.
#define MEADOW_TIMER_RC_SERVO_CLK_FREQ (3000000) // 3MHz target frequency

#define MEADOW_TIMER_GPIO_INPUT_TRIGGER (2)

#define MEADOW_TIMER_RC_SERVO_BAD_GPIO (0xffffffff)

/****************************************************************************
 * Private Data
 ****************************************************************************/

// This struct is pointed to by the dataPtr element of the timerItem table
struct rcServoData_s
{
  // These change during execution or are set during configuration. So, they
  // can be in allocated memory.
  uint8_t timerPolarity1 : 1;      // 0 = Leading is Rising, 1 = Leading is Falling
  uint8_t timerPolarity2 : 1;      // 0 = Leading is Rising, 1 = Leading is Falling
  uint8_t timerPolarity3 : 1;      // 0 = Leading is Rising, 1 = Leading is Falling
  uint8_t timerPolarity4 : 1;      // 0 = Leading is Rising, 1 = Leading is Falling
  uint32_t timerGpioCfg[4];        // GPIO for channel 1 - 4
  volatile uint16_t timerPulseW1;   // Pulse width channel 1 (0-65535 microsec)
  volatile uint16_t timerPulseW2;   // Pulse width channel 2
  volatile uint16_t timerPulseW3;   // Pulse width channel 3
  volatile uint16_t timerPulseW4;   // Pulse width channel 4
  volatile uint32_t timerLeadCnt1;  // Leading edge Count channel 1
  volatile uint32_t timerLeadCnt2;  // Leading edge Count channel 2
  volatile uint32_t timerLeadCnt3;  // Leading edge Count channel 3
  volatile uint32_t timerLeadCnt4;  // Leading edge Count channel 4
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_isr_rc_servo_decode(int irq, void *context, void *arg);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
int meadow_timer_isr_rc_servo_decode(int irq, void *context, void *arg)
{
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  struct rcServoData_s *rcServoData = (struct rcServoData_s *)timerInfo->dataPtr;

  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  // DIAGNOSTICS
  if(timStatusReg & GTIM_SR_CC1IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A0, true);
  if(timStatusReg & GTIM_SR_CC2IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A1, true);
  if(timStatusReg & GTIM_SR_CC3IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A2, true);
  if(timStatusReg & GTIM_SR_CC4IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A3, true);
  if(timStatusReg & GTIM_SR_UIF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A4, true);

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    timStatusReg &= ~GTIM_SR_CC1IF;


    // Rising or falling is leading edge?
    bool inputState = stm32_gpioread(rcServoData->timerGpioCfg[0]);
    if(rcServoData->timerPolarity1)
      inputState = !inputState;

    if(inputState)
    {
      rcServoData->timerLeadCnt1 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR1_OFFSET);
    }
    else
    {
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

        // Pulse width in micro seconds
        rcServoData->timerPulseW1 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulseW1 = 0;
      }

      // Set to 0 for leading edge detection
      rcServoData->timerLeadCnt1 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    bool inputState = stm32_gpioread(rcServoData->timerGpioCfg[1]);
    
    if(rcServoData->timerPolarity2)
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
        rcServoData->timerPulseW2 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulseW2 = 0;
      }

      rcServoData->timerLeadCnt2 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    timStatusReg &= ~GTIM_SR_CC3IF;

    bool inputState = stm32_gpioread(rcServoData->timerGpioCfg[2]);
    
    if(rcServoData->timerPolarity3)
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
        rcServoData->timerPulseW3 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulseW3 = 0;
      }

      rcServoData->timerLeadCnt3 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    bool inputState = stm32_gpioread(rcServoData->timerGpioCfg[3]);

    if(rcServoData->timerPolarity4)
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
        rcServoData->timerPulseW4 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        rcServoData->timerPulseW4 = 0;
      }

      rcServoData->timerLeadCnt4 = 0;
    }  
  }

  //--------------------------------------------
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  
  // DIAGNOSTICS
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A0, false);
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A1, false);
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A2, false);
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A3, false);
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A4, false);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_rc_servo_decode(int timerNumber)
{
  struct timerInfo_s *timerInfo = meadow_timer_get_timer_info_pointer(timerNumber);
  struct rcServoData_s *rcServoData;

  if(timerInfo == NULL)
  {
    syslog(1, "meadow_timer_get_timer_info_pointer() returned NULL\n");
    return -ENXIO;
  }

  rcServoData = malloc(sizeof(struct rcServoData_s));
  memset(rcServoData, 0, sizeof(struct rcServoData_s));
  timerInfo->dataPtr = (void *) rcServoData;

  // Initialize GPIOs for this timer
  for(int i = 0; i < 4; i++)
  {
    uint32_t afPortPin = meadow_timer_get_ver_based_gpio_chan(timerNumber, i);
    if(afPortPin != 0xffff)
    {
      rcServoData->timerGpioCfg[i] = MEADOW_TIMER_GPIO_CONST | afPortPin;
      stm32_configgpio(rcServoData->timerGpioCfg[i]);
    }
    else
    {
      syslog(1, "meadow_timer_setup_rc_servo_decode() no gpio at offset:%d\n", i);
      rcServoData->timerGpioCfg[i] = MEADOW_TIMER_RC_SERVO_BAD_GPIO;
    }
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

  bool chan1 = rcServoData->timerGpioCfg[0] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan2 = rcServoData->timerGpioCfg[1] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan3 = rcServoData->timerGpioCfg[2] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan4 = rcServoData->timerGpioCfg[3] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  
  // Before starting disable capture/control for all channels. Ref Man (26.4.7 at
  // end) "Note: CC1S bits are writable only when the channel is OFF (i.e.
  // CC1E = 0 in TIMx_CCER)." 
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xeeee;   // e = 1110, clear CCxE bits 0, 4, 8 & 12
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);

  // Configure TI1 to IC1 etc.
  // CCMR1 handles channels 1 & 2
  regVal32 = 0;   // For input 1 & 2, Input Capture Filters [15:12] & [7:4]
  // and Input Capture Prescaler [9:8] & [1:0] are disabled by setting to 0.
  if(chan1)
  {
    // 01: IC1 is mapped on TI1
    regVal32 |= 0x00000001;   // 1 = 01, set bits 1:0
    dierBits |= GTIM_DIER_CC1IE;
  }
  if(chan2)
  {
    // 01: IC2 is mapped on TI2
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    dierBits |= GTIM_DIER_CC2IE;
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
  }
  if(chan4)
  {
    // 01: IC4 is mapped on TI4
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    dierBits |= GTIM_DIER_CC4IE;
  }
  putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

  //-------------------------------------------------------------------------
  // Set the input polarity for each channel by setting the CCxP and CCXNP bits
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  if(chan1)
  {
    // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)
    switch (MEADOW_TIMER_GPIO_INPUT_TRIGGER)
    {
    case 0: // Rising (0b00)
      regVal16 &= ~(GTIM_CCER_CC1P | GTIM_CCER_CC1NP);
      break;
    case 1: // Falling (0b01)
      regVal16 &= ~GTIM_CCER_CC1NP;
      regVal16 |= GTIM_CCER_CC1P;
      break;
    case 2: // Both rising and falling (0b11)
      regVal16 |= (GTIM_CCER_CC1P | GTIM_CCER_CC1NP);
      break;
    default:
      break;
    }
  }
  if(chan2)
  {
    // GTIM_CCER_CC2NP (bit 7) & GTIM_CCER_CC2P (bit 5)
    switch (MEADOW_TIMER_GPIO_INPUT_TRIGGER)
    {
    case 0: // Rising
      regVal16 &= ~(GTIM_CCER_CC2P | GTIM_CCER_CC2NP);
      break;
    case 1: // Falling
      regVal16 &= ~GTIM_CCER_CC2NP;
      regVal16 |= GTIM_CCER_CC2P;
      break;
    case 2: // Both rising and falling
      regVal16 |= (GTIM_CCER_CC2P | GTIM_CCER_CC2NP);
      break;
    default:
      break;
    }
  }
  if(chan3)
  {
    // GTIM_CCER_CC4NP (bit 11) & GTIM_CCER_CC4P (bit 9)
    switch (MEADOW_TIMER_GPIO_INPUT_TRIGGER)
    {
    case 0: // Rising
      regVal16 &= ~(GTIM_CCER_CC3P | GTIM_CCER_CC3NP);
      break;
    case 1: // Falling
      regVal16 &= ~GTIM_CCER_CC3NP;
      regVal16 |= GTIM_CCER_CC3P;
      break;
    case 2: // Both rising and falling
      regVal16 |= (GTIM_CCER_CC3P | GTIM_CCER_CC3NP);
      break;
    default:
      break;
    }
  }
  if(chan4)
  {
    // GTIM_CCER_CC4NP (bit 15) & GTIM_CCER_CC4P (bit 13)
    switch (MEADOW_TIMER_GPIO_INPUT_TRIGGER)
    {
    case 0: // Rising
      regVal16 &= ~(GTIM_CCER_CC4P | GTIM_CCER_CC4NP);
      break;
    case 1: // Falling
      regVal16 &= ~GTIM_CCER_CC4NP;
      regVal16 |= GTIM_CCER_CC4P;
      break;
    case 2: // Both rising and falling
      regVal16 |= (GTIM_CCER_CC4P | GTIM_CCER_CC4NP);
      break;
    default:
      break;
    }
  }
  putreg16(regVal16, timerBase + STM32_GTIM_CCER_OFFSET);
  
  //------------------------------------------
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

  //--------------------------------------------------------
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
          rcServoData->timerPulseW1,
          rcServoData->timerPulseW2,
          rcServoData->timerPulseW3,
          rcServoData->timerPulseW4);
  return OK;
}
