/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_timer_support.c
 * 
 *   Copyright (C) 2020, 2021 Wilderness Labs. All rights reserved.
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

#define MEADOW_TIMER_FREQ_DC_SYNC_ERROR (100)
#define MEADOW_TIMER_FREQ_DC_SYNC_LEADING (101)
#define MEADOW_TIMER_FREQ_DC_SYNC_TRAILING (102)

// #if defined(CONFIG_MEADOW_TIMER_SUPPORT)
// #if defined(true)
//===================================================================

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

//============================================================================
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
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
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
  // to indicate these 2 events. A simpler solution cannot cope with this.
  // Because the last 3 bits of the SR register is all we care about and the
  // pattern of these bits changes the need action we'll use a switch.

  // The root cause of this problem is that the timer or the interrupt
  // system cannot cope with the situation when a CNT overflow occurs
  // at the moment the external signals leading or falling edge
  // arrives. This highest frequency this occurs at is TimerClock/65536.
  // It reoccurs at (TimerClock/65536)/2, (TimerClock/65536)/3 etc.
  // 
  // What has been observed.
  // An interrupt is generated containing the CC1IF or CC2IF flags
  // set and the UIF flag set. The software must assume that the UIF
  // bit is indicating that the CNT register has been reset. The
  // software has no way to determine that a CNT overflow also
  // occured.

  switch(timStatusReg & 0x0007)
  {
    case 0x00:    // Do nothing, just ignore
    break;

    //------------------------------------------------------------
    case 0x01:    // Lone CNT (i.e. reset/overflow)
      timStatusReg &= ~GTIM_SR_UIF;

      // This check is not needed, and since errors are rare it's better t o
      // not waste the time testing.
      // if(timerInfo->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_ERROR)
      //   break;

      if(timerInfo->timerWidth == 16)
      {
        timerInfo->timerExtra1 += MEADOW_TIMER_16_BIT_OVERFLOW;

        // Handle all CCR2 16-bit CNT overflows, but only until falling edge for CCR2.
        if(timerInfo->timerCapCCR2OvFl)
          timerInfo->timerExtra2 += MEADOW_TIMER_16_BIT_OVERFLOW;
      }
    break;

    //------------------------------------------------------------
    // Rising/Leading Edge
    case 0x02:    // Lone leading edge, never expected
      timStatusReg &= ~GTIM_SR_CC1IF;
      timerInfo->timerDEBUG = 2;    
      timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      syslog(1, "$$$$> Leading edge without UIF???\n");
    break;

    case 0x03:     // Leading edge + UIF (Normal for End/Start of capture)
      timStatusReg &= ~GTIM_SR_UIF;   // Problem is this could be overflow or CNT reset
      timStatusReg &= ~GTIM_SR_CC1IF;

      uint32_t count1;
      uint32_t count2;
      bool noErrorEncountered = true;

      if(timerInfo->timerWidth == 32)
      {
        // No overflow with 32-bit timers
        count1 = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET) + \
                  MEADOW_TIMER_CORRECTION_COUNT;

        count2 = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET) + \
                  MEADOW_TIMER_CORRECTION_COUNT;
      }
      else
      {
        // A trailing edge sets timerDectSync to
        // MEADOW_TIMER_FREQ_DC_SYNC_TRAILING, but only if it was proceeded by
        // a valid leading edge
        // (timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_LEADING).
        if(timerInfo->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_TRAILING)
        {
          // 16-bit timers
          count1 = ((uint32_t)getreg16(timerBase + STM32_GTIM_CCR1_OFFSET)) + \
                    timerInfo->timerExtra1 + \
                    MEADOW_TIMER_CORRECTION_COUNT;
          count2 = ((uint32_t)getreg16(timerBase + STM32_GTIM_CCR2_OFFSET)) + \
                    timerInfo->timerExtra2 + \
                    MEADOW_TIMER_CORRECTION_COUNT;

          // Check for various detectable errors. There are other that cannot
          // be distingushed 
          if(timerInfo->timerDectSync == MEADOW_TIMER_FREQ_DC_SYNC_ERROR)
          {
            noErrorEncountered = false;
            timerInfo->timerDEBUG = 10;
            count1 = 0;
            count2 = 0;  
          }
          else if(count1 < MEADOW_TIMERS_MINIMUM_USABLE_CNT)
          {
            noErrorEncountered = false;
            timerInfo->timerDEBUG = 20;
            count1 = 0;
            count2 = 0;  
          }
          else if(count1 < count2)
          {
            // This fix only works for the initial frequency that causes
            // trouble (i.e. TimerClock/65536). At frequencies lower than
            // this, it doesn't work. It may be that multiple overflow
            // interrupts are being missed.
            noErrorEncountered = false;
            timerInfo->timerDEBUG = 30;
            count1 += MEADOW_TIMER_16_BIT_OVERFLOW;
          }
          else
          {
            // Reasonable Duty Cycle test, must be > 0.1% and < 99.9%
            uint32_t dutyCycle = (count2 * 1000)/count1;
            if(dutyCycle < 1 || dutyCycle > 999)
            {
              noErrorEncountered = false;
              timerInfo->timerDEBUG = 40;
              count1 = 0;
              count2 = 0;
            }
          }
        }
        else
        {
          // timerDectSync unexpected value
          noErrorEncountered = false;
          timerInfo->timerDEBUG = timerInfo->timerDectSync;
          count1 = 0;
          count2 = 0;
        }
      }

      timerInfo->timerCount1 = count1;
      timerInfo->timerCount2 = count2;

      timerInfo->timerCapCCR2OvFl = true;   // Collect CCR2 overflows
      timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_LEADING;
      timerInfo->timerExtra1 = 0;           // Clear previous overflow
      timerInfo->timerExtra2 = 0;           // Clear previous overflow
      if(noErrorEncountered)
        timerInfo->timerDEBUG = 0;
        
    break;

    //------------------------------------------------------------
    // Trailing/Falling Edge
    case 0x05:    // Falling edge with UIF (i.e. assume Falling Edge + Overflow)
      timStatusReg &= ~GTIM_SR_UIF;

    case 0x04:    // Falling edge alone. End of CCR2 capture.
      timStatusReg &= ~GTIM_SR_CC2IF;

      // Falling edge check if there was a valid leading edge
      if(timerInfo->timerDectSync != MEADOW_TIMER_FREQ_DC_SYNC_LEADING)
      {
        timerInfo->timerDEBUG = 99;
        // We missed the leading edge
        timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
        break;                              // And quit
      }
      
      // This value will be tested when the leading edge arrives
      timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_TRAILING;

      // Falling edge means we're done with CCR2's value. We don't need
      // to worry about CNT overflow with respect to CCR2 either.
      if(timerInfo->timerWidth == 16)
      {
        timerInfo->timerCapCCR2OvFl = false;   // Stop collecting CCR2 overflows

        // Trailing edge with UIF flag set. Assume we need to add this to CCR2's value
        if(timStatusReg & GTIM_SR_UIF)
        {
          timerInfo->timerExtra2 += MEADOW_TIMER_16_BIT_OVERFLOW;
        }
      }
    break;

    //------------------------------------------------------------
    case 0x06:    // (illegal) Rising and Falling together, no
      timerInfo->timerDEBUG = 6;
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      syslog(1, "$$$> FreqDC Illegal ISR state 0x06\n");
    break;

    case 0x07:    // (illegal) Rising and Falling plus UIF
      // This case exists when the duty cycle is very small (< 0.5%) or very
      // large (> 99.5%)
      timerInfo->timerDEBUG = 7;
      timStatusReg &= ~GTIM_SR_CC1IF;
      timStatusReg &= ~GTIM_SR_CC2IF;
      timStatusReg &= ~GTIM_SR_UIF;
      timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
      // syslog(1, "$$$> FreqDC Illegal ISR state 0x07\n");
    break;
  }

  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A0, false);    
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A1, false);    
  stm32_gpiowrite(MEADOW_DEBUG_PIN_V2_A2, false);    

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_freq_duty(struct timerInfo_s *timerData)
{
  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo)
{
  // Just feed pulse train into appropriate GPIO

  int i = 0;

  if(timerInfo->timerWidth == 16)
  {
    // Scan looking for valid data
    for(i = 0; i < 32; i++)
    {
      if(timerInfo->timerCount1 > 0 && timerInfo->timerCount2 > 0)
        break;

      usleep(1 * 1000);
    }
  }

  if(timerInfo->timerCount1 > 0 && timerInfo->timerCount2 > 0)
  {
    double dutyCycle = (double)(timerInfo->timerCount2 * 100.0)/(double)timerInfo->timerCount1;
    double freq = (double)(timerInfo->timerFreq)/(double)timerInfo->timerCount1;

    syslog(1, "===> Freq:%06.4fHz, DC:%02.2f%%, CCR1:%06lu, CCR2:%06lu, NotUsed:%03u (i:%d)\n",
              freq, dutyCycle,
              timerInfo->timerCount1,
              timerInfo->timerCount2,
              timerInfo->timerDEBUG, i);
  }
  else
  {
    syslog(1, "+++> Invalid data                CCR1:%06lu, CCR2:%06lu, NotUsed:%03u (i:%d).\n",
              timerInfo->timerCount1,
              timerInfo->timerCount2,
              timerInfo->timerDEBUG, i);
  }

  return OK;
}

//=============================================================
// Frequency and duty cycle measurement.
int meadow_timer_init_freq_and_dutycycle(struct timerInfo_s *timerInfo)
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
  uint32_t timerBase = timerInfo->timerBase;

  timerInfo->timerDectSync = MEADOW_TIMER_FREQ_DC_SYNC_ERROR;
  timerInfo->timerDEBUG = 0;

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

  return OK;
}
