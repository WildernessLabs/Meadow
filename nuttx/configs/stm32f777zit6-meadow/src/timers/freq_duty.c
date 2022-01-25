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

#include <nuttx/config.h>
// #include <arch/board/board.h>

// #include <string.h>
// #include <stdbool.h>
// #include <assert.h>
// #include <debug.h>
// #include <errno.h>

// #include "chip.h"
// #include "fcntl.h"
// #include <nuttx/semaphore.h>
// #include <nuttx/arch.h>

// #include "stm32f777zit6-meadow.h"

// #include <sys/ioctl.h>
// #include <nuttx/timers/timer.h>
// #include "stm32_tim.h"

// // PeterM - still needed?
// #include <nuttx/kthread.h>
// #include <meadow/meadow_hw_version.h>

#include "meadow_timers.h"

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
 * Private Data
 ****************************************************************************/
// This is the beginning of a list of Meadow Timer Configuration (mtc) settings.
// These will eventually be set, directly or indirectly. by the .Net programmer.

static int debugCount = 0;  // DEBUG

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct timerInfo_s *_timerData[MEADOW_TIMERS_NUMB_OF_TIMERS];

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
  if(!mtcFreqDutyCycle)
    return OK;
  
  debugCount++;    // DEBUG CODE

  // Why are we here? Check the timer's Status Register
  struct timerInfo_s *timerInfo = (struct timerInfo_s *)arg;
  uint32_t timerBase = timerInfo->timerBase;
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  timerInfo->timerStartEdge = 0;
  
  //--------------------------------------------
  // Capture/Compare 1 signifies that input rising edge encountered.
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    // Clear this interrupt bit
    timStatusReg &= ~GTIM_SR_CC1IF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Grab the current CCR1 value and, if it's a 16-bit register, add the
    // overflow totals.
    if(timerInfo->timerWidth == 16)
    {
      timerInfo->timerCount1 = (uint32_t)getreg16(timerBase + STM32_GTIM_CCR1_OFFSET);
      timerInfo->timerCount1 += timerInfo->timerExtra1 + MEADOW_TIMER_CORRECTION_COUNT;
      timerInfo->timerExtra1 = 0;      // Reset CNT overflow counts
    }
    else
    {
      timerInfo->timerCount1 = (uint32_t)getreg32(timerBase + STM32_GTIM_CCR1_OFFSET)\
                + MEADOW_TIMER_CORRECTION_COUNT;
    }
    
    timerInfo->timerStartEdge = 1;

    // Collect CNT overflow for CCR2 until falling edge
    timerInfo->timerColCC2 = 1;
  }

  //---------------------------------------------------
  // Capture/Compare 2 interrupt signifies an input falling edge. This
  // signifies that the timer has copied CNT into CCR2.
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    timerInfo->timerColCC2 = 0;

    // Falling edge means we're done considering CCR2's value so we don't 
    // need to worry about CNT overflow with respect to CCR2.
    if(timerInfo->timerWidth == 16)
    {
      timerInfo->timerCount2 = (uint32_t)getreg16(timerBase + STM32_GTIM_CCR2_OFFSET);
      timerInfo->timerCount2 += timerInfo->timerExtra2 + MEADOW_TIMER_CORRECTION_COUNT;
      timerInfo->timerExtra2 = 0;
    }
    else
    {
      timerInfo->timerCount2 = (uint32_t)getreg32(timerBase + STM32_GTIM_CCR2_OFFSET) \
                + MEADOW_TIMER_CORRECTION_COUNT;
    }
  }

  //--------------------------------------------
  // Since only counting up, UIF means the CNT register overflowed or CNT was
  // reset to zero.
  // Note: At a clock speed of 96MHz, this interrupt is called every 683
  // microsec. However, with a 96MHz clock an input frequency around 1464.8Hz
  // will be fast enough to not overflow the CNT registers 65536 limit.
  // UIF bit set when the first rising edge is detected which is when the
  // CNT is reset to zero, and at every overflow.
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);

    // Ignore first interrupt because it's not an overflow but occurs when CNT
    // is reset to zero. And we we don't need it if using a 32-bit register
    if(timerInfo->timerStartEdge == 0 && timerInfo->timerWidth == 16)
    {
      timerInfo->timerExtra1 += MEADOW_TIMER_16_BIT_OVERFLOW;

      // Handle all CNT overflows, but only until falling edge for CCR2.
      if(timerInfo->timerColCC2)
        timerInfo->timerExtra2 += MEADOW_TIMER_16_BIT_OVERFLOW;
    }
  }
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_timer_setup_freq_duty(struct timerInfo_s *timerData)
{
  _timerData[0] = timerData;
  return OK;
}

//================================================================
// Test code for gated pulse width
int meadow_timer_test_freq_and_dutycycle(struct timerInfo_s *timerInfo)
{
  // Just feed square wave into appropriate GPIO

  // Get the values in capture/compare register 1 and 2
  if(timerInfo->timerCount1 > 1 && timerInfo->timerCount2 > 1)
  {
    double dutyCycle = (double)(timerInfo->timerCount2 * 100.0)/(double)timerInfo->timerCount1;
    double freq = (double)(timerInfo->timerFreq)/(double)timerInfo->timerCount1;

    syslog(1, "===> Freq:%06.4fHz, DC:%02.2f%%, Cnt1:%lu, Cnt2:%lu\n",
              freq, dutyCycle,
              timerInfo->timerCount1,
              timerInfo->timerCount2);
  }
  else
  {
    syslog(1, "+++> Invalid data CCR1:%lu, CCR2:%lu. Any zeros are bad.\n",
              timerInfo->timerExtra1, timerInfo->timerExtra2);
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
  // registers 1 and 2. For 1 compare/capture register it is configured to 
  // for rising edge interrupt and falling edge for ccr2. By using the
  // count between interrupts for one CCR's (i.e. rising to rising edges)
  // the frequency can be found by counting between interrupts from rising to
  // falling the duty cycle can be found.

  int ret;
  uint16_t regVal16;
  uint32_t regVal32;
  uint32_t timerBase = timerInfo->timerBase;

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
  
  syslog(1, "--> FreqDC-Setting up interrupt sources\n");
  // Clear all interrupt sources and set the ones we want we need
  // DMA/Interrupt enable register (DIER)
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET, 
          GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
          GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
          GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE,
          // Advanced timers 1 & 8 ATIM_DIER_COMIE | ATIM_DIER_BIE | ATIM_DIER_COMDE
          GTIM_DIER_CC1IE | GTIM_DIER_CC2IE | GTIM_DIER_UIE);

  // All Frequency/Duty Cycle interupts are handled by same isr
  ret = irq_attach(timerInfo->timerIrqVec, meadow_timer_isr_freq_dutycycle, timerInfo);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  syslog(1, "---> Enabling IRQ up_enable_irq\n");

  // Nuttx handles the interrupts at the lowest level
  up_enable_irq(timerInfo->timerIrqVec);

  return OK;
}
