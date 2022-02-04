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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "meadow_timers.h"

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================
#define MEADOW_TIMER_RC_SERVO_PRESCALER (32) // To overflow (65536) just below 50 Hz

/****************************************************************************
 * Private Data
 ****************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_isr_rc_servo_decode(int irq, void *context, void *arg)
{
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

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

    uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR1_OFFSET);
    bool inputState = stm32_gpioread(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);
    uint32_t prevLeadingCount = timerInfo->timerExtra1;

    // inputState is true = Rising edge, inputState false = falling edge
    if(inputState)
    {
      // Leading edge
      timerInfo->timerExtra1 = currentCount;   // Save for next leading edge

      // Test and fix current count if overflow
      if(currentCount < prevLeadingCount)
        currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

      // Save total count of entire cycle (leading edge to leading edge)
      timerInfo->timerExtra2 = currentCount - prevLeadingCount;
    } 
    else
    {
      // Trailing edge
      // Test and fix current count overflow
      if(currentCount < prevLeadingCount)
        currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

      // Width of pulse in counts
      uint32_t pulseCount = currentCount - prevLeadingCount;

      // Pulse width in micro seconds
      uint32_t pulseWidth = ((pulseCount * 1000)/(timerInfo->timerFreq/1000));

      // Find the duty cycle
      uint32_t dutyCycle = (pulseCount * 10000)/timerInfo->timerExtra2;
      uint32_t freq = (timerInfo->timerFreq * 1000)/timerInfo->timerExtra2;

      syslog(1, "+++> Capture/compare 1 Falling - freq:%lu mHz, pulseWidth:%lu usec, DC:%lu %%*100\n",
                freq, pulseWidth, dutyCycle);
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    uint16_t CCR2 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR2_OFFSET);
    uint16_t CNT = getreg16(timerInfo->timerBase + STM32_GTIM_CNT_OFFSET);
    syslog(1, "+++> Capture/compare 2 CCR2:%u, CNT:%u.\n",
              CCR2, CNT);
    // putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  }
  
  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    syslog(1, "+++> Capture/compare 3\n");
    timStatusReg &= ~GTIM_SR_CC3IF;
    // putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    syslog(1, "+++> Capture/compare 4\n");
    timStatusReg &= ~GTIM_SR_CC4IF;
    // putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  }

  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  
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
int meadow_timer_setup_rc_servo_decode()
{
  return OK;
}

//================================================================
// Test code for gated frequency and pulse width
int meadow_timer_test_rc_servo_decode(int timerNumber)
{
  // NO TEST WRITTEN YET

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

  struct timerInfo_s *timerInfo = &(timerInfoArray[timerNumber - 1]);
  uint32_t timerBase = timerInfo->timerBase;

  bool chan1 = timerInfo->timerChan[0] = 0 ? false : true;
  bool chan2 = timerInfo->timerChan[1] = 0 ? false : true;
  bool chan3 = timerInfo->timerChan[2] = 0 ? false : true;
  bool chan4 = timerInfo->timerChan[3] = 0 ? false : true;

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
    switch (MEADOW_TIMER_CHAN1_INPUT_POLARITY)
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
    switch (MEADOW_TIMER_CHAN2_INPUT_POLARITY)
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
    switch (MEADOW_TIMER_CHAN2_INPUT_POLARITY)
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
    switch (MEADOW_TIMER_CHAN2_INPUT_POLARITY)
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
  modifyreg32(timerInfo->timerAPBClk, 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff.
  // Set the prescaler value of 0 to allow highest speed. A prescaler value of
  // 1 will divide the clock by 2.
  uint16_t prescaler = MEADOW_TIMER_RC_SERVO_PRESCALER - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // Set timer frequency
  timerInfo->timerFreq = timerInfo->timerMaxClk/MEADOW_TIMER_RC_SERVO_PRESCALER;

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == 16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);
  //------------------------------------------

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

  meadow_timer_enable(timerInfo);

  return OK;
}
