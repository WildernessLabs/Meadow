/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/timers/freq_duty.c
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

#include <stdlib.h>

#define MEADOW_TIMER_FREQ_DC_SYNC_ERROR (100)
#define MEADOW_TIMER_FREQ_DC_SYNC_LEADING (101)
#define MEADOW_TIMER_FREQ_DC_SYNC_TRAILING (102)

// Count below this value are not valid because they would represent pulses
// to short to measure.
#define MEADOW_TIMER_MINIMUM_USABLE_CNT (180)

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

// PeterM - This can probably be trimmed to a lower frequency and still work
// just as good as 96MHz
#define MEADOW_TIMER_FREQ_DC_CLK_FREQ (96000000) // 96MHz target frequency

/****************************************************************************
 * Private Data
 ****************************************************************************/
struct freqDcData_s
{
  volatile uint8_t timerDectSync;     // Missing interrupt detection
  volatile uint32_t timerFullPeriod;  // Full period count
  volatile uint32_t timerPartPeriod;  // Part period count
  volatile uint32_t timerFullOvrFlo;  // Full overflow count
  volatile uint32_t timerPartOvrFlo;  // Part overflow count
};

struct freqDcInfo_s
{
  uint8_t timerNumb   : 4;          // 0 - 15 timer number as diagnostic
  uint8_t timerWidth  : 1;          // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  uint8_t timerMaxClk : 1;          // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  uint8_t timerAPBClk : 1;          // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  uint8_t timerPolarity : 1;        // Used during config, 0 = Leading is Rising, 1 = Leading is Falling
  uint32_t timerBase;               // Unique for each timer
  uint32_t timerClkEn;              // Enable bit for timer for APB1 or APB2
  uint32_t timerIrqVec;             // Interrupt vector
  struct freqDcData_s *dataPtr;     // Points to the variable data array
};

static struct freqDcInfo_s freqDcInfoArray[] =
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

#define MEADOW_TIMER_FREQ_DC_TOTAL_NUMB (sizeof(freqDcInfoArray) / sizeof(struct freqDcInfo_s))

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//===========================================================================
// This function is called for all frequency with duty cycle interrupts.
// On the first rising edge of the input, the timer clears the CNT count and
// CNT begins counting up.
// On the following falling edge, the timer copies the CNT value into CCR2.
// On the next rising edge, the timer copies the CNT value into CCR1 and CNT
// is again cleared to zero and the process repeats.
// This means that we must read CCR1 and CCR2 between the rising edge and the
// falling edge.
// This is made more complex because for 16-bit timers, we must maintain a
// count of all CNT overflow interupts. For CCR1 overflow for the entire period
// but for CCR2 only between the rising edge and the falling edge.
// Note: a 16-bit register at 96MHz will overflow every 683 microseconds.
int meadow_timer_isr_freq_dutycycle(int irq, void *context, void *arg)
{
  struct freqDcInfo_s *timerInfo = (struct freqDcInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  if(timStatusReg & GTIM_SR_CC1IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A0, true);
  if(timStatusReg & GTIM_SR_CC2IF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A1, true);
  if(timStatusReg & GTIM_SR_UIF)
    stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A2, true);

  // This is a bit more complex that may seem is necessary. This is because
  // at certain input frequencies the CNT being cleared and the CNT overflow
  // are reported in the same interrupt. But, there is only 1 bit available
  // to indicate these 2 things.
  // The highest frequency this occurs at is TimerClock/65536, which is
  // 1,464.844 Hz with a timer clock of 96 MHz. This reoccurs at at intervals
  // (TimerClock/65536)/2, (TimerClock/65536)/3 etc.
  // For most input frequencies, on a leading (e.g. rising) or trailing (e.g.
  // falling) edge an interrupt is generated containing the CC1IF (leading) or
  // CC2IF (trailing) flags set. The UIF flag is always set with the CC1IF
  // flag, to indicate that the CNT register has been cleared. However, at
  // some frequencies, the overflow and the CNT reset occur at the same moment.
  // The UIF is set but it cannot be determined if it indicates CNT overflow or
  // CNT reset.

  switch(timStatusReg & 0x0007)
  {
    case 0x00:    // Do nothing, just ignore
      break;

    //------------------------------------------------------------
    case 0x01:    // Lone UIF flag. CNT register changed, either reset or overflow)
      timStatusReg &= ~GTIM_SR_UIF;
      timerInfo->dataPtr->timerFullOvrFlo++;
      break;

    //------------------------------------------------------------
    // Leading Edge
    case 0x02:    // Lone leading edge, never expected
      timStatusReg &= ~GTIM_SR_CC1IF;
      timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;

    case 0x03:     // Leading edge + UIF (Normal for End/Start of capture)
      timStatusReg &= ~GTIM_SR_UIF;   // Could be overflow or CNT reset
      timStatusReg &= ~GTIM_SR_CC1IF; // Leading edge should be CNT reset

      if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_32)
      {
        // At this point we expect to have seen a trailing edge and no errors
        if(timerInfo->dataPtr->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_TRAILING)
        {
          // Provide consumer with values
          timerInfo->dataPtr->timerFullPeriod = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);
          timerInfo->dataPtr->timerPartPeriod = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);
        }
        else
        {
          timerInfo->dataPtr->timerFullPeriod = 0;
          timerInfo->dataPtr->timerPartPeriod = 0;
        }

        timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_LEADING;
        break;
      }

      // Must be 16-bit timer
      uint32_t count1;
      uint32_t count2;

      if(timerInfo->dataPtr->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_TRAILING)
      {
        count1 = getreg16(timerBase + STM32_GTIM_CCR1_OFFSET);
        count2 = getreg16(timerBase + STM32_GTIM_CCR2_OFFSET);

        // Add any 16-bit CNT overflow
        count1 += (timerInfo->dataPtr->timerFullOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW);
        count2 += (timerInfo->dataPtr->timerPartOvrFlo * MEADOW_TIMER_16_BIT_OVERFLOW);

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
      timerInfo->dataPtr->timerFullPeriod = count1;
      timerInfo->dataPtr->timerPartPeriod = count2;
      timerInfo->dataPtr->timerFullOvrFlo = 0;   // Clear previous overflow
      timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_LEADING;
      break;

    //------------------------------------------------------------
    // Trailing/Falling Edge
    case 0x05:    // Falling edge with UIF (i.e. assume Falling Edge + Overflow)
      timStatusReg &= ~GTIM_SR_UIF;

    case 0x04:    // Falling edge alone. End of CCR2 capture.
      timStatusReg &= ~GTIM_SR_CC2IF;

      // Falling edge check if there has been a valid leading edge
      if(timerInfo->dataPtr->timerDectSync != MEADOW_TIMER_FREQ_DC_SYNC_LEADING)
      {
        timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
        break;  // And quit
      }
      
      // Falling edge means we're done with CCR2's value. We don't need
      // to worry about CNT overflow with respect to CCR2 either.
      if(timStatusReg & GTIM_SR_UIF)
        timerInfo->dataPtr->timerFullOvrFlo++;    // Adjust overflow count

      // Time to capture the first half of the signal
      timerInfo->dataPtr->timerPartOvrFlo = timerInfo->dataPtr->timerFullOvrFlo;    // Save for duty cycle
    
      // This value will be tested when the leading edge arrives
      timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_TRAILING;
      break;

    //------------------------------------------------------------
    case 0x06:    // (illegal) Rising and Falling together, no
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;

    case 0x07:    // (illegal) Rising and Falling plus UIF
      // This case exists when the duty cycle is very small (< 0.5%) or very
      // large (> 99.5%)
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timStatusReg &= ~GTIM_SR_UIF;
      timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      break;
  }

  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A0, false);    
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A1, false);    
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A2, false);    

  return OK;
}

//=============================================================
static struct freqDcInfo_s * meadow_timer_get_timer_pointer(int timerNumb)
{
  for (int i = 0; i < MEADOW_TIMER_FREQ_DC_TOTAL_NUMB; i++)
  {
    if(freqDcInfoArray[i].timerNumb == timerNumb)
    {
      return ( &(freqDcInfoArray[i]));
    }
  }

  return NULL;
}

//=============================================================
static uint32_t meadow_timer_get_apb_clock(struct freqDcInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
static uint32_t meadow_timer_get_max_clock(struct freqDcInfo_s *timerInfo)
{
  if(timerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
// Not currently used
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
int meadow_timer_setup_freq_duty(int timerNumber)
{
  struct freqDcInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);

  timerInfo->dataPtr = malloc(sizeof(struct freqDcData_s));
  memset(timerInfo->dataPtr, 0, sizeof(struct freqDcData_s));

  // TEMPORARY - During development configure a GPIO input for measurement 
  stm32_configgpio(MEADOW_TIMER_APPROPRIATE_TIM_INPUT);

  return OK;
}

//================================================================
// Test code for gated frequency and pulse width
int meadow_timer_test_freq_and_dutycycle(int timerNumber)
{
  // Just feed pulse train into appropriate GPIO  
  struct freqDcInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);

  int validCheckCount = 0;

  if(timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16)
  {
    // Scan looking for valid data
    for(validCheckCount = 0; validCheckCount < 500; validCheckCount++)
    {
      if(timerInfo->dataPtr->timerFullPeriod > 0 && timerInfo->dataPtr->timerPartPeriod > 0)
        break;

      usleep(1 * 1000);
    }
  }

  if(timerInfo->dataPtr->timerFullPeriod > 0 && timerInfo->dataPtr->timerPartPeriod > 0)
  {
    double dutyCycle = (double)(timerInfo->dataPtr->timerPartPeriod * 100.0)/ \
              (double)timerInfo->dataPtr->timerFullPeriod;

    double freq = (double)(MEADOW_TIMER_FREQ_DC_CLK_FREQ)/ \
              (double)timerInfo->dataPtr->timerFullPeriod;

    syslog(1, "===> Freq:%06.4fHz, DC:%02.2f%%, CCR1:%06lu, CCR2:%06lu\n",
              freq, dutyCycle,
              timerInfo->dataPtr->timerFullPeriod,
              timerInfo->dataPtr->timerPartPeriod);
  }
  else
  {
    syslog(1, "+++> Invalid data                CCR1:%06lu, CCR2:%06lu\n",
              timerInfo->dataPtr->timerFullPeriod,
              timerInfo->dataPtr->timerPartPeriod);
  }

  // Prevent this count from being used when there's no input.
  timerInfo->dataPtr->timerFullPeriod = 0;
  timerInfo->dataPtr->timerPartPeriod = 0;
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

  struct freqDcInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);
  uint32_t timerBase = timerInfo->timerBase;

  timerInfo->dataPtr->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;

syslog(1, "$$$$> Entered meadow_timer_init_freq_and_dutycycle, timerNumb:%d timerInfo:%p, timerBase:%p\n",
          timerInfo->timerNumb, timerInfo, timerBase);

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
  //
  // Note: the CCR1 register is readonly. Capture/Compare Enable Register (CCER)
  // is were the polarity is set by CC1P & CC1NP. In Ref Man the CC1P for input
  // discribes both the CC1P and CC1NP bit as if a 2 bit field. But they are
  // actually bit 1 and bit 3. Ref Man "00: noninverted/rising edge
  regVal16 = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  regVal16 &= 0xfff5;   // 5 = 0101, clear GTIM_CCER_CC1NP (bit 3) & GTIM_CCER_CC1P (bit 1)
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
  
  //------------------------------------------
  // Setup the clock enable
  modifyreg32(meadow_timer_get_apb_clock(timerInfo), 0, timerInfo->timerClkEn);
  
  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed. A prescaler value of 1 will divide the clock by 2.

  // Find proper pre-scaler value so all rc servo timers run at the same speed
  uint16_t prescaler = (meadow_timer_get_max_clock(timerInfo)/ \
            MEADOW_TIMER_FREQ_DC_CLK_FREQ) - 1;
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum
  uint32_t maxARRValue = timerInfo->timerWidth == MEADOW_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);
  //------------------------------------------

  // Clear all interrupt sources and set the ones we need. CC1IE is the rising
  // edge, CC2IE is the falling edge and UIE is whenever the CNT register is
  // cleared or overflows.
  // DMA/Interrupt enable register (DIER)
  // Advanced timers 1 & 8 add ATIM_DIER_COMIE | ATIM_DIER_BIE | ATIM_DIER_COMDE
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_UIE);

  // All Frequency/Duty Cycle interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_freq_dutycycle, timerInfo);
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
