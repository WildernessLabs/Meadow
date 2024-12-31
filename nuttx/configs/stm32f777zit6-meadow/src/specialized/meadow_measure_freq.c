/****************************************************************************
 * nuttx/configs/stm32f777zit6-meadow/src/specialized/meadow_calc_freq_dc.c
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

// This module, uses timers to calculate frequency

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>
#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"
#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"
#include "specialized/meadow_measure_freq_local.h"
#include <meadow/meadow_hw_version.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_measure_freq_shared.h>

//=====================================================
// Diagnostic
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>
// #pragma message "(--) meadow_measure_freq.c"
// #pragma GCC optimize("O0")    // Prevent compiler from changing the code
#define MEADOW_MEASURE_FREQ_INCLUDE_REG_DUMP (0)
// Diagnostic

/****************************************************************************
 * Private Data
 ****************************************************************************/
// This array contains timer information most of which is fixed by the
// STM32F7's hardware. It contains each F7 timer and a flag for useability
// (TIM1 and TIM8 are not usable). The 'Acv' (i.e. Active channels) byte
// contains 4 bits representing active/inuse channels within the timer.
static mdwFreqTimerInfo_t mdwFreqTimerInfoArray[] =
{
            //   |--- bit-field---|---------------------- Fixed by hardware -----------------------|--------- Runtime Data ---------|
            //   #  wid max apb use    Base Addr       Clk Timer Enable      IRQ Vector    Alt Func OvF Acv  Chan1 Chan2 Chan3 Chan4
  /* TIM1   */  {1 , 0,  1,  0, 0, STM32_TIM1_BASE,  0,                   0,               GPIO_AF1, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM2   */  {2 , 1,  0,  0, 1, STM32_TIM2_BASE,  RCC_APB1ENR_TIM2EN,  STM32_IRQ_TIM2,  GPIO_AF1, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM3   */  {3 , 0,  0,  0, 1, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM4   */  {4 , 0,  0,  0, 1, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM5   */  {5 , 1,  0,  0, 1, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5,  GPIO_AF2, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM8   */  {8 , 0,  1,  0, 0, STM32_TIM8_BASE,  0,                   0,               GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM9   */  {9 , 0,  1,  1, 1, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9,  GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM10  */  {10, 0,  1,  1, 1, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM11  */  {11, 0,  1,  1, 1, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM12  */  {12, 0,  0,  0, 1, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, GPIO_AF9, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM13  */  {13, 0,  0,  0, 1, STM32_TIM13_BASE, RCC_APB1ENR_TIM13EN, STM32_IRQ_TIM13, GPIO_AF3, 0,  0, {NULL, NULL, NULL, NULL}},
  /* TIM14  */  {14, 0,  0,  0, 1, STM32_TIM14_BASE, RCC_APB1ENR_TIM14EN, STM32_IRQ_TIM14, GPIO_AF9, 0,  0, {NULL, NULL, NULL, NULL}}
};
#define MEADOW_FREQ_TOTAL_TIMERS_AVAILABLE (sizeof(mdwFreqTimerInfoArray)/sizeof(mdwFreqTimerInfo_t))

//----------------------------------------------------------------------------
// This table contains all of the STM32F7's timers and their valid GPIOs with
// channel. It is used for F7v1 and F7v2 and CCM. It should be able to verify
// any timer, GPIO combination.
mdwFreqChanPortPin_t validStm32F7GpioArray[][11] = 
{
  /* Tim 1 */
  {
    {GPIO_PORTA | GPIO_PIN8,  1},   // PA8
    {GPIO_PORTE | GPIO_PIN9,  1},   // PE9
    {GPIO_PORTA | GPIO_PIN9,  2},   // PA9
    {GPIO_PORTE | GPIO_PIN11, 2},   // PE11
    {GPIO_PORTA | GPIO_PIN10, 3},   // PA10
    {GPIO_PORTE | GPIO_PIN13, 3},   // PE13
    {GPIO_PORTA | GPIO_PIN11, 4},   // PA11
    {GPIO_PORTE | GPIO_PIN14, 4},   // PE14
    {0xff, 0}
  },

  /* Tim 2 */
  {
    {GPIO_PORTA | GPIO_PIN0,  1},   // PA0
    {GPIO_PORTA | GPIO_PIN15, 1},   // PA15
    {GPIO_PORTA | GPIO_PIN1,  2},   // PA1
    {GPIO_PORTB | GPIO_PIN3,  2},   // PB3
    {GPIO_PORTA | GPIO_PIN2,  3},   // PA2
    {GPIO_PORTB | GPIO_PIN10, 3},   // PB10
    {GPIO_PORTA | GPIO_PIN3,  4},   // PA3
    {GPIO_PORTB | GPIO_PIN11, 4},   // PB11
    {0xff, 0}
  },

  /* Tim 3 */
  {
    {GPIO_PORTA | GPIO_PIN6, 1},    // PA6
    {GPIO_PORTC | GPIO_PIN6, 1},    // PC6
    {GPIO_PORTB | GPIO_PIN4, 1},    // PB4
    {GPIO_PORTA | GPIO_PIN7, 2},    // PA7
    {GPIO_PORTC | GPIO_PIN7, 2},    // PC7
    {GPIO_PORTB | GPIO_PIN5, 2},    // PB5
    {GPIO_PORTB | GPIO_PIN0, 3},    // PB0
    {GPIO_PORTC | GPIO_PIN8, 3},    // PC8
    {GPIO_PORTB | GPIO_PIN1, 4},    // PB1
    {GPIO_PORTC | GPIO_PIN9, 4},    // PC9
    {0xff, 0}
  },
  
  /* Tim 4 */
  {
    {GPIO_PORTD | GPIO_PIN12, 1},    // PD12
    {GPIO_PORTB | GPIO_PIN6,  1},    // PB6
    {GPIO_PORTD | GPIO_PIN13, 2},    // PD13
    {GPIO_PORTB | GPIO_PIN7,  2},    // PB7
    {GPIO_PORTD | GPIO_PIN14, 3},    // PD14
    {GPIO_PORTB | GPIO_PIN8,  3},    // PB8
    {GPIO_PORTD | GPIO_PIN15, 4},    // PD15
    {GPIO_PORTB | GPIO_PIN9,  4},    // PB9
    {0xff, 0}
  },

  /* Tim 5 */
  {
    {GPIO_PORTA | GPIO_PIN0,  1},    // PA0
    {GPIO_PORTH | GPIO_PIN10, 1},    // PH10
    {GPIO_PORTA | GPIO_PIN1,  2},    // PA1
    {GPIO_PORTH | GPIO_PIN11, 2},    // PH11
    {GPIO_PORTA | GPIO_PIN2,  3},    // PA2
    {GPIO_PORTH | GPIO_PIN12, 3},    // PH12
    {GPIO_PORTA | GPIO_PIN3,  4},    // PA3
    {GPIO_PORTI | GPIO_PIN0,  4},    // PI0
    {0xff, 0}
  },

  /* Tim 6 No GPIOs */
    {{0xff, 0}},

  /* Tim 7 No GPIOs */
    {{0xff, 0}},

  /* Tim 8 */
  {
    {GPIO_PORTC | GPIO_PIN6, 1},    // PC6
    {GPIO_PORTI | GPIO_PIN5, 1},    // PI5
    {GPIO_PORTC | GPIO_PIN7, 2},    // PC7
    {GPIO_PORTI | GPIO_PIN6, 2},    // PI6
    {GPIO_PORTC | GPIO_PIN8, 3},    // PC8
    {GPIO_PORTI | GPIO_PIN7, 3},    // PI7
    {GPIO_PORTC | GPIO_PIN9, 4},    // PC9
    {GPIO_PORTI | GPIO_PIN2, 4},    // PI2
    {0xff, 0}
  },

  /* Tim 9 */
  {
    {GPIO_PORTE | GPIO_PIN5, 1},    // PE5
    {GPIO_PORTA | GPIO_PIN2, 1},    // PA2
    {GPIO_PORTE | GPIO_PIN6, 2},    // PE6
    {GPIO_PORTA | GPIO_PIN3, 2},    // PA3
    {0xff, 0}
  },

  /* Tim 10 */
  {
    {GPIO_PORTF | GPIO_PIN6, 1},    // PF6
    {GPIO_PORTB | GPIO_PIN8, 1},    // PB8
    {0xff, 0}
  },

  /* Tim 11 */
  {
    {GPIO_PORTF | GPIO_PIN7, 1},    // PF7
    {GPIO_PORTB | GPIO_PIN9, 1},    // PB9
    {0xff, 0}
  },

  /* Tim 12 */
  {
    {GPIO_PORTH | GPIO_PIN6,  1},    // PH6
    {GPIO_PORTB | GPIO_PIN14, 1},    // PB14
    {GPIO_PORTH | GPIO_PIN9,  2},    // PH9
    {GPIO_PORTB | GPIO_PIN15, 2},    // PB15
    {0xff, 0}
  },

  /* Tim 13 */
  {
    {GPIO_PORTF | GPIO_PIN8, 1},    // PF8
    {GPIO_PORTA | GPIO_PIN6, 1},    // PA6
    {0xff, 0}
  },

  /* Tim 14 */
  {
    {GPIO_PORTF | GPIO_PIN9, 1},    // PF9
    {GPIO_PORTA | GPIO_PIN7, 1},    // PA7
    {0xff, 0}
  },
};

//-------------------------------------------------
// Used to verify port and pin availability on F7FeatherV1
static uint8_t validF7v1GpioArray[][5] =
{
  /* TIM1 No GPIO exposed */
  {0xff},
  /* TIM2  No GPIO exposed */
  {0xff},
  
  /* TIM3  */
  {GPIO_PORTC | GPIO_PIN6,  // PC6, D02
   GPIO_PORTC | GPIO_PIN7,  // PC7, D05
   GPIO_PORTB | GPIO_PIN0,  // PB0, D06
   GPIO_PORTB | GPIO_PIN1,  // PB1, D09 (or PC9, D11)
   0xff},

  /* TIM4 */
  {GPIO_PORTB | GPIO_PIN6,  // PB6, D08
   GPIO_PORTB | GPIO_PIN7,  // PB7, D07
   GPIO_PORTB | GPIO_PIN8,  // PB8, D03
   GPIO_PORTB | GPIO_PIN9,  // PB9, D04
   0xff},

  // /* TIM5 */
  {GPIO_PORTH | GPIO_PIN10,  // PH10, D10
   0xff},

  /* TIM6  No GPIO exposed */
  {0xff},
  /* TIM7  No GPIO exposed */
  {0xff},
  /* TIM8  No GPIO exposed */
  {0xff},

  /* TIM9  */
  {GPIO_PORTA | GPIO_PIN3,  // PA3, A02
   0xff},

  /* TIM10 */
  {GPIO_PORTB | GPIO_PIN8,  // PB8, D03
   0xff},

  /* TIM11 */
  {GPIO_PORTB | GPIO_PIN9,  // PB9, D04
   0xff},

  /* TIM12 */
  {GPIO_PORTB | GPIO_PIN14,  // PB14, D12
   GPIO_PORTB | GPIO_PIN15,  // PB15, D13
   0xff},

  /* TIM13  No GPIO exposed */
  {0xff},

  /* TIM14 */
  {GPIO_PORTA | GPIO_PIN7,  // PA7, A03
   0xff},
};

//-------------------------------------------------
// Used to verify port and pin availability on F7FeatherV2
static uint8_t validF7v2GpioArray[][5] =
{
  /* TIM1 No GPIO exposed */
  {0xff},

  /* TIM2 No GPIO exposed */
  {0xff},

  /* TIM3 */
  {GPIO_PORTB | GPIO_PIN4,  // PB4, D05
   GPIO_PORTC | GPIO_PIN7,  // PC7, D10
   GPIO_PORTB | GPIO_PIN0,  // PB0, A03
   GPIO_PORTB | GPIO_PIN1,  // PB1, A04
   0xff},

  /* TIM4 D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19,0xff},
  /* TIM4 */
  {GPIO_PORTB | GPIO_PIN8,  // PB6, D08
   GPIO_PORTB | GPIO_PIN7,  // PB7, D07
   GPIO_PORTB | GPIO_PIN8,  // PB8, D03
   GPIO_PORTB | GPIO_PIN9,  // PB9, D04
   0xff},

  /* TIM5 */
  {GPIO_PORTH | GPIO_PIN10,  // PH10, D02
   GPIO_PORTA | GPIO_PIN3,   // PA3, A02
   0xff},

  /* TIM6 No GPIO exposed */
  {0xff},
  /* TIM7 No GPIO exposed */
  {0xff},
  /* TIM8 No GPIO exposed */
  {0xff},
  /* TIM9 No GPIO exposed */
  {0xff},

  /* TIM10 */
  {GPIO_PORTB | GPIO_PIN8,  // PB8, D03
   0xff},

  /* TIM11 */
  {GPIO_PORTB | GPIO_PIN9,  // PB9, D04
   0xff},

  /* TIM12 */
  {GPIO_PORTB | GPIO_PIN14,  // PB14, D12
   GPIO_PORTB | GPIO_PIN15,  // PB15, D13
   0xff},

  /* TIM13 No GPIO exposed */
  {0xff},

  /* TIM14 No GPIO exposed */
  {0xff},
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static int meadow_measure_freq_isr(int irq, void *context, void *arg);
static int meadow_measure_freq_unconfigure(mdwFreqCfgTimer_t *mdwCfgTimerChan);
static int meadow_measure_freq_cfg_timer_hardware(mdwFreqTimerInfo_t *mdwFreqTimerInfo);
static int meadow_measure_freq_cfg_channel_hardware(mdwFreqTimerInfo_t *mdwFreqTimerInfo,
          mdwFreqChanData_t *mdwFreqChanData);
#if MEADOW_MEASURE_FREQ_INCLUDE_REG_DUMP > 0
static void meadow_measure_freq_diag_dump_timer_regs(char *label,
            mdwFreqTimerInfo_t *mdwFreqTimerInfo);
#endif
/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR is called for all frequency measurement interrupts.
// There is a unique ISR vector for each timer. But, each timer must process
// 1 - 4 inputs. On entry we don't know which input(s) is/are involved.
int meadow_measure_freq_isr(int irq, void *context, void *arg)
{
  mdwFreqTimerInfo_t *mdwFreqTimerInfo = (mdwFreqTimerInfo_t *)arg;
  uint32_t timerBase = mdwFreqTimerInfo->timerBase;

  // Get the current status to determine why ISR called
  uint16_t timStatusReg = getreg16(timerBase + STM32_GTIM_SR_OFFSET);

  //----------------------------------------------------------
  // Check UIF (Update Interrupt Flag), which indicates CNT changed.
  if(timStatusReg & GTIM_SR_UIF)
  {
    timStatusReg &= ~GTIM_SR_UIF;

    // We need to account for the timer's CNT overflow and use it in the
    // individual input calculations.
    // Note: with a 96 MHz clock a 16-bit timer will rollover every 683
    // microseconds and at 960 kHz it will rollover every 68.3 milliseconds,
    // about 14.6 times/second.
    //
    // What is done here is we have both 16/32-bit timer counts. Which isn't
    // enough. Therefore, we use another 32-bit uint32_t for the overflow
    // value. This approach will effectively add 32-bits to each timers size.
    //
    // This overflow value is for the entire timer and is used each channels
    // in doing calculations.
    mdwFreqTimerInfo->timerOverflow++;
  }

  // Any other interrupts to handle?
  if(timStatusReg == 0)
  {
    // Since there aren't we can exit
    putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
    return OK;
  }

  //----------------------------------------------------------
  // Channel 1
  if(timStatusReg & GTIM_SR_CC1IF)
  {
    timStatusReg &= ~GTIM_SR_CC1IF;

    // Ignore if channel not configured
    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_1)
    {
      // Channel data pointer
      mdwFreqChanData_t *mdwFreqChanData = 
              mdwFreqTimerInfo->mdwFreqChanData[FREQ_CHAN_DATA_OFFSET_CHAN_1];
      if(mdwFreqChanData == NULL)
      {
        syslog(LOG_ERR, "%s@%d-Error:Channel 1 is NULL\n", __FILE__, __LINE__);
        return -ENXIO;  // No such device or addres
      }

      // Read the captured value. This resets reg value to 0.
      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR1_OFFSET);

      // From this point the code is identical for all channels
      // If using duty cycle, read the input GPIO, otherwise only raising
      // edges seen.
      if(mdwFreqChanData->useDutyCycle)
      {
        // With duty cycle we must read the GPIO's input state to determine
        // if this is raising or falling edge.
        if(stm32_gpioread(mdwFreqChanData->inputConfig))
        {
          // Raising edge with duty cycle
          // The end of the previous edge is now the beginning of this count.
          // These are captured so that when the calculations are executed
          // it is free to be unconcerned about changing values.
          // Remember that when an ISR occurs, everything else stops,
          // including the code that does the calculations.
          mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
          mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
          mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
          mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

          // Capture end count and current overflow count
          mdwFreqChanData->endResultCnt = capturedCount;
          mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

          // The following is for calculating average frequency
          mdwFreqChanData->gpioCountForAvg++;
        }
        else
        {
          // Falling edge, only for duty cycle
          mdwFreqChanData->midCaptureCnt = capturedCount;
          mdwFreqChanData->midCaptureOvr = mdwFreqTimerInfo->timerOverflow;
        }
      }
      else
      {
        // Raising edge without duty cycle is end of previous capture and the
        // beginning of a new capture
        mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
        mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
        mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
        mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

        // End is now
        mdwFreqChanData->endResultCnt = capturedCount;
        mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

        // The following is for calculating average frequency
        mdwFreqChanData->gpioCountForAvg++;
      }
    }
  }

  //----------------------------------------------------------
  // Channel 2
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_2)
    {
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_CHAN_DATA_OFFSET_CHAN_2];
      if(mdwFreqChanData == NULL)
      {
        syslog(LOG_ERR, "%s@%d-Error:Channel 2 is NULL\n", __FILE__, __LINE__);
        return -ENXIO;  // No such device or addres
      }

      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR2_OFFSET);

      if(mdwFreqChanData->useDutyCycle)
      {
        if(stm32_gpioread(mdwFreqChanData->inputConfig))
        {
          mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
          mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
          mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
          mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

          mdwFreqChanData->endResultCnt = capturedCount;
          mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

          mdwFreqChanData->gpioCountForAvg++;
        }
        else
        {
          mdwFreqChanData->midCaptureCnt = capturedCount;
          mdwFreqChanData->midCaptureOvr = mdwFreqTimerInfo->timerOverflow;
        }
      }
      else
      {
        mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
        mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
        mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
        mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

        mdwFreqChanData->endResultCnt = capturedCount;
        mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

        mdwFreqChanData->gpioCountForAvg++;
      }
    }
  }

  //----------------------------------------------------------
  // Channel 3
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    timStatusReg &= ~GTIM_SR_CC3IF;

    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_3)
    {
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_CHAN_DATA_OFFSET_CHAN_3];
      if(mdwFreqChanData == NULL)
      {
        syslog(LOG_ERR, "%s@%d-Error:Channel 3 is NULL\n", __FILE__, __LINE__);
        return -ENXIO;  // No such device or addres
      }

      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR3_OFFSET);

      if(mdwFreqChanData->useDutyCycle)
      {
        if(stm32_gpioread(mdwFreqChanData->inputConfig))
        {
          mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
          mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
          mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
          mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

          mdwFreqChanData->endResultCnt = capturedCount;
          mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

          mdwFreqChanData->gpioCountForAvg++;
        }
        else
        {
          mdwFreqChanData->midCaptureCnt = capturedCount;
          mdwFreqChanData->midCaptureOvr = mdwFreqTimerInfo->timerOverflow;
        }
      }
      else
      {
        mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
        mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
        mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
        mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

        mdwFreqChanData->endResultCnt = capturedCount;
        mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

        mdwFreqChanData->gpioCountForAvg++;
      }
    }
  }

  //----------------------------------------------------------
  // Channel 4
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;

    if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_4)
    {
      mdwFreqChanData_t *mdwFreqChanData = 
                mdwFreqTimerInfo->mdwFreqChanData[FREQ_CHAN_DATA_OFFSET_CHAN_4];
      if(mdwFreqChanData == NULL)
      {
        syslog(LOG_ERR, "%s@%d-Error:Channel 4 is NULL\n", __FILE__, __LINE__);
        return -ENXIO;  // No such device or addres
      }

      uint32_t capturedCount = getreg32(timerBase + STM32_GTIM_CCR4_OFFSET);

      if(mdwFreqChanData->useDutyCycle)
      {
        if(stm32_gpioread(mdwFreqChanData->inputConfig))
        {
          mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
          mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
          mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
          mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

          mdwFreqChanData->endResultCnt = capturedCount;
          mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

          mdwFreqChanData->gpioCountForAvg++;
        }
        else
        {
          mdwFreqChanData->midCaptureCnt = capturedCount;
          mdwFreqChanData->midCaptureOvr = mdwFreqTimerInfo->timerOverflow;
        }
      }
      else
      {
        mdwFreqChanData->bgnResultCnt = mdwFreqChanData->endResultCnt;
        mdwFreqChanData->bgnResultOvr = mdwFreqChanData->endResultOvr;
        mdwFreqChanData->midResultCnt = mdwFreqChanData->midCaptureCnt;
        mdwFreqChanData->midResultOvr = mdwFreqChanData->midCaptureOvr;

        mdwFreqChanData->endResultCnt = capturedCount;
        mdwFreqChanData->endResultOvr = mdwFreqTimerInfo->timerOverflow;

        mdwFreqChanData->gpioCountForAvg++;
      }
    }
  }

  // Clear the timer's status register
  putreg16(timStatusReg, timerBase + STM32_GTIM_SR_OFFSET);
  return OK;
}

//=============================================================
// Find the RCC clock for enable for the selected timer
static uint32_t meadow_measure_freq_get_apb_clock(
          mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  if(mdwFreqTimerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
// Timers use 2 different clock sources
static uint32_t meadow_measure_freq_get_max_clock(
          const mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  if(mdwFreqTimerInfo->timerMaxClk)
    return STM32_APB2_TIM1_CLKIN;
  else
    return STM32_APB1_TIM2_CLKIN;
}

//=============================================================
// Disable the selected timer
static void meadow_measure_freq_disable(const uint32_t timerBase)
{
  uint16_t regval = getreg16(timerBase + STM32_BTIM_CR1_OFFSET);
  regval &= ~ATIM_CR1_CEN;
  putreg16(regval, timerBase + STM32_BTIM_CR1_OFFSET);
}

//=============================================================
// Enable the selected timer
static void meadow_measure_freq_enable(const uint32_t timerBase)
{
  // Enable timer Counter
  uint16_t cr1Val = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  cr1Val |= GTIM_CR1_CEN;
  
  // Re-initialize the counter and generates an update of the registers
  uint16_t egrVal = getreg16(timerBase + STM32_GTIM_EGR_OFFSET);
  egrVal |= GTIM_EGR_UG;

  putreg16(egrVal, timerBase + STM32_GTIM_EGR_OFFSET);
  putreg16(cr1Val, timerBase + STM32_GTIM_CR1_OFFSET);
}

//=====================================================================
// The following returns the specified timer's information or NULL
static mdwFreqTimerInfo_t *meadow_measure_freq_chk_get_timer_info(
          const uint32_t timerNumb)
{
  mdwFreqTimerInfo_t *timerInfo = &(mdwFreqTimerInfoArray[timerNumb - 1]);
  if(timerInfo->timerUsable)
    return timerInfo;

  return NULL;
}

//=====================================================================
// Returns the correct bit field definition based on the timer's channel
static uint8_t meadow_measure_freq_get_channel_bit(const uint32_t timerChan)
{
  switch(timerChan)
  {
    case FREQ_CHANNEL_NUMBER_CHAN_1:
      return ACTIVE_CHAN_BITFIELD_1;

    case FREQ_CHANNEL_NUMBER_CHAN_2:
      return ACTIVE_CHAN_BITFIELD_2;

    case FREQ_CHANNEL_NUMBER_CHAN_3:
      return ACTIVE_CHAN_BITFIELD_3;

    case FREQ_CHANNEL_NUMBER_CHAN_4:
      return ACTIVE_CHAN_BITFIELD_4;

    default:
      return 0;
  }
}

//==================================================================
// Get the current time in nanoseconds. This is used for the average
// frequency calculations.
static uint64_t meadow_measure_freq_get_current_time(void)
{
  int ret;
  struct tm rtcTime;
  long nsecs;
  long prevNsecs;
  uint64_t returnTime;

  // This function reads the date, time and sub-seconds from the MCU's
  // hardware into a struct tm. However, the STM32F77X Errata warns about a
  // possible problem in ES0334-Rev 9 2.12.1 related to the RTC calendar
  // register not locked properly. Therefore, we'll implement the workaround
  // by reading the nsec twice and compare, if different repeat till equal.
#ifdef CONFIG_STM32F7_HAVE_RTC_SUBSECONDS
  do
  {
    ret = up_rtc_getdatetime_with_subseconds(&rtcTime, &prevNsecs);
    if(ret < 0)
    {
      return ret;
    }

    // Read a second time per Errata
    ret = up_rtc_getdatetime_with_subseconds(&rtcTime, &nsecs);
    if(ret < 0)
    {
      return ret;
    }

    // If they match we have good values
    if(prevNsecs == nsecs)
      break;
      
  } while (1);
#else
  // This function is used if no sub-seconds.
  ret = up_rtc_getdatetime(&rtcTime)
  nsecs = 0;
#endif

  returnTime = mktime(&rtcTime);
  returnTime *= 1000 * 1000 * 1000;
  returnTime += nsecs;

  return returnTime;
}

//=============================================================
// This function will evaluate the GPIO based on 3 tables containing the
// valid GPIOs. For CCM only one table is checked. For F7v1 and F7v2
// The a version specific table is checked, and then the CCM table.
// Returns the channel, 1-4 unless not found, then returns 0.
static uint8_t meadow_measure_freq_get_chan_tim_port_pin(
          const uint32_t timerNumb, uint8_t portAndPin)
{
  int entry;
  int timerOffset = timerNumb - 1;

  if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
  {
    // Verify port & pin are valid for FeatherV1 hardware
    entry = 0;
    while(validF7v1GpioArray[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validF7v1GpioArray[timerOffset][entry])
      {
        break;    // So far good
      }
      entry++;
    }

    if(validF7v1GpioArray[timerOffset][entry] == 0xff)
     return 0;
  }
  else if (meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2)
  {
    // Verify port & pin are valid for FeatherV2 hardware
    entry = 0;
    while(validF7v2GpioArray[timerOffset][entry] != 0xff)
    {
      if(portAndPin == validF7v2GpioArray[timerOffset][entry])
      {
        break;    // So far good
      }
      entry++;
    }
    
    if(validF7v2GpioArray[timerOffset][entry] == 0xff)
      return 0;
  }
  else if (meadow_hw_version_get() != MEADOW_F7_HW_VERSION_NUMB_CCMV2)
  {
    // Unsupported device type
    syslog(1, "%s@%d- Unknown device type\n", __FILE__, __LINE__);
    return 0;
  }

  // All hardware types reach here to find the timer channel and verify the
  // port and pin are legal.
  entry = 0;
  while(validStm32F7GpioArray[timerOffset][entry].portPin != 0xff)
  {
    if(portAndPin == validStm32F7GpioArray[timerOffset][entry].portPin)
    {
      // Found valid channel
      return validStm32F7GpioArray[timerOffset][entry].chan;
    }
    entry++;
  }

  return 0; // Channel not found
}

/****************************************************************************
 * Public Function
 ****************************************************************************/
// Called by Meadow.Core to configure a timer channel
// Timer numbers range from 1 - 14. However, some are not defined.
int meadow_measure_freq_configure(mdwFreqCfgTimer_t *mdwCfgTimerChan)
{
  int ret;
  uint32_t timerNumber = mdwCfgTimerChan->timerNumber;
  uint32_t channelNumber = mdwCfgTimerChan->channelNumber;
  uint8_t portAndPin = (uint8_t)(mdwCfgTimerChan->portAndPin & 0x000000ff);
  uint32_t inputGpioConfig;
  uint32_t channelOffset = channelNumber - 1;
  bool timerNeedsConfig;
  bool chanNeedsDuty;

  if(timerNumber > 14 || timerNumber < 1)
  {
    syslog(LOG_ERR, "%s@%d-Error:Timer must be 1 - 14\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_CONF_TIM_NUMB_ILLEGAL;
  }
  
  if(channelNumber > 4 || channelNumber < 1)
  {
    syslog(LOG_ERR, "%s@%d-Error:Channel must be 1 - 4\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_CONF_CHAN_NUMB_ILLEGAL;
  }

  // 0 = illegal
  // 1 = Configure without Duty Cycle (reduces interrupts by 50%),
  // 2 = Configure with Duty Cycle,
  // 3 = Unconfigure
  switch(mdwCfgTimerChan->configOption)
  {
    case MEADOW_MEAS_FREQ_CONF_OPTION_NO_DC:
      chanNeedsDuty = false;
      break;

    case MEADOW_MEAS_FREQ_CONF_OPTION_WITH_DC:
      chanNeedsDuty = true;
      break;
     
    case MEADOW_MEAS_FREQ_CONF_OPTION_UNCFG:
      // Unconfigure - Also removes Timer if no channels remain
      ret = meadow_measure_freq_unconfigure(mdwCfgTimerChan);
      return ret;   // Done with unconfigure

    default:
      syslog(LOG_ERR, "%s@%d-Error:Unknown configure option:%llu\n",
                __FILE__, __LINE__, mdwCfgTimerChan->configOption);
      return MEADOW_MEAS_FREQ_CONF_UNDEFINED_OPTION;   // Function not implemented
  }

  // Insure a valid timer, channel, port+pin combination was supplied.
  // Timers have, at most, 4 channels, each representing 1 GPIO. For the
  // specified timer we need to verify a proper port and pin.
  uint8_t chanValid4TimerPortPin = meadow_measure_freq_get_chan_tim_port_pin(
            timerNumber, portAndPin);
  if(chanValid4TimerPortPin == 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:The Port and Pin, not valid for timer %d\n",
              __FILE__, __LINE__, timerNumber);
    return MEADOW_MEAS_FREQ_CONF_PORT_PIN_NOT_FOR_TIM;
  }

  // Is this the channel the user wanted?
  if(channelNumber != chanValid4TimerPortPin)
  {
    syslog(LOG_ERR, "%s@%d-Error:The Port/Pin/Timer/Channel combination, not valid\n",
              __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_CONF_PORT_PIN_TIM_CHAN_INVALID;
  }

  // Check if this timer is useable and get a pointer if it is
  mdwFreqTimerInfo_t *mdwFreqTimerInfo =
            meadow_measure_freq_chk_get_timer_info(timerNumber);
  if(mdwFreqTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Timer %ld unusable\n",
              __FILE__, __LINE__, timerNumber);
    return MEADOW_MEAS_FREQ_CONF_TIM_NOT_USABLE;
  }

#if MEADOW_MEASURE_FREQ_INCLUDE_REG_DUMP > 0
  meadow_measure_freq_diag_dump_timer_regs(
            "Before Configuration", mdwFreqTimerInfo);
#endif

  // Are any channels already active? If not, timer needs to be initialized
  if(mdwFreqTimerInfo->chanActiveBits == 0)
    timerNeedsConfig = true;
  else
    timerNeedsConfig = false;

  // This timer is usable, but is this channel already being used?
  uint8_t chanBit = meadow_measure_freq_get_channel_bit(channelNumber);
  if(mdwFreqTimerInfo->chanActiveBits & chanBit)
  {
    syslog(LOG_ERR, "%s@%d-Error:Timer %d, channel %d already in use\n",
              __FILE__, __LINE__, timerNumber, channelNumber);
    return MEADOW_MEAS_FREQ_CONF_TIM_CHAN_IN_USE;
  }

  // Allocate a struct for each new channel on a timer
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset] =
            (mdwFreqChanData_t*)zalloc(sizeof(mdwFreqChanData_t));
  if(mdwFreqTimerInfo->mdwFreqChanData[channelOffset] == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Allocation for mdwFreqChanData_s\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_CONF_CHAN_MEM_ALLOC_FAILED;
  }

  // Start populating the channel structure
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->useDutyCycle = chanNeedsDuty;

  // Init time and count used to calculate average frequency
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->gpioCountForAvg = 0;
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->startTimeForAvg =
            meadow_measure_freq_get_current_time();

  // Indicate this channel is being used
  mdwFreqTimerInfo->chanActiveBits |= chanBit;    // Set channel bit

  // Build the GPIO input configuration for Nuttx GPIO processing
  inputGpioConfig = MEADOW_TIMER_GPIO_CONST | portAndPin | \
            mdwFreqTimerInfo->timerAltFunc;

  // // Diagnostic
  // syslog(1, "-->%s@%d-TIM%lu, Chn:%lu, input Pin defn:0x%02x (P%c%d), Pin defn+AF:0x%08lx\n",
  //           __FILE__, __LINE__, timerNumber, channelNumber, portAndPin,
  //           ((portAndPin) >> 4) + 'A', portAndPin & 0x0f, inputGpioConfig);
  // // Diagnostic

  // Valid GPIO so configure input for channel.
  ret = stm32_configgpio(inputGpioConfig);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:stm32_configgpio() returned:%ld\n",
              __FILE__, __LINE__, ret);
    free(mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
    return MEADOW_MEAS_FREQ_CONF_CONFIGGPIO_ERR;   // Not supported
  }

  // Save GPIO configuration
  mdwFreqTimerInfo->mdwFreqChanData[channelOffset]->inputConfig = \
            inputGpioConfig;

  // Initialized the F7's channel hardware for this GPIO as input
  ret = meadow_measure_freq_cfg_channel_hardware(mdwFreqTimerInfo,
          mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:Channel hardware init failed:%d\n",
              __FILE__, __LINE__, ret);
    free(mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
    return MEADOW_MEAS_FREQ_CONF_INIT_CHAN_HW_FAIL;
  }

  // Initialized the F7's timer hardware
  if(timerNeedsConfig)
  {
    // Initialized the F7's timer hardware
    ret = meadow_measure_freq_cfg_timer_hardware(mdwFreqTimerInfo);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Error:Timer hardware init failed:%d\n",
                __FILE__, __LINE__, ret);
      free(mdwFreqTimerInfo->mdwFreqChanData[channelOffset]);
      return MEADOW_MEAS_FREQ_CONF_INIT_TIM_HW_FAIL;
    }
  }
#if MEADOW_MEASURE_FREQ_INCLUDE_REG_DUMP > 0
  meadow_measure_freq_diag_dump_timer_regs(
          "After Configuration", mdwFreqTimerInfo);
#endif

  return MEADOW_MEAS_FREQ_CONF_SUCCESSFUL;
}

//============================================================
// Configuration of timer channel hardware registers.
int meadow_measure_freq_cfg_channel_hardware(
          mdwFreqTimerInfo_t *mdwFreqTimerInfo,
          mdwFreqChanData_t *mdwFreqChanData)
{
  uint32_t regVal32;
  uint16_t ccerRegVal;    // Capture/Compare Enable Register
  uint16_t dierRegVal;    // DMA/Interrupt Enable Register
  uint32_t timerBase = mdwFreqTimerInfo->timerBase;
  bool useDC = mdwFreqChanData->useDutyCycle;

  ccerRegVal = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  dierRegVal = getreg16(timerBase + STM32_GTIM_DIER_OFFSET);
  
  // From STM32F7 Ref man: "Note: CC1S bits are writable only when the
  //    channel is OFF (CC1E = 0 in TIMx_CCER)."
  // We'll just set the entire CCER register to 0.
  putreg16((int)0, timerBase + STM32_GTIM_CCER_OFFSET);

  // Enable timer overrun (overflow)
  dierRegVal |= GTIM_DIER_UIE;

  // Enable the timer inputs for all active channels (all were disabled
  // earlier)
  if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_1)
  {
    // Capture/compare mode reg 1, chan 1
    regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
    regVal32 &= ~(GTIM_CCMR1_CC1S_MASK);
    regVal32 |= 0x00000001;       // 1 = 01, set bits 1:0
    putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

    // Set the input triggers, rising, falling or both for channel
    if(useDC)
      ccerRegVal |= (GTIM_CCER_CC1P | GTIM_CCER_CC1NP);   // 11 both edges
    else
      ccerRegVal &= ~(GTIM_CCER_CC1P | GTIM_CCER_CC1NP);  // 00 rising only
    ccerRegVal |= GTIM_CCER_CC1E;   // Enable capture channel 1
    dierRegVal |= GTIM_DIER_CC1IE;  // DMA/Interrupt enable
  }

  if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_2)
  {
    // Capture/compare mode reg 1, chan 2
    regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
    regVal32 &= ~(GTIM_CCMR1_CC2S_MASK);
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

    if(useDC)
      ccerRegVal |= (GTIM_CCER_CC2P | GTIM_CCER_CC2NP);   // 11 both edges
    else
      ccerRegVal &= ~(GTIM_CCER_CC2P | GTIM_CCER_CC2NP);  // 00 rising only
    ccerRegVal |= GTIM_CCER_CC2E;   // Enable capture channel 2
    dierRegVal |= GTIM_DIER_CC2IE;  // DMA/Interrupt enable
  }

  if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_3)
  {
    // Capture/compare mode reg 2, chan 3
    regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
    regVal32 &= ~(GTIM_CCMR2_CC3S_MASK);
    regVal32 |= 0x00000001;   // 1 = 01, set bits 1:0
    putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

    if(useDC)
      ccerRegVal |= (GTIM_CCER_CC3P | GTIM_CCER_CC3NP);   // 11 both edges
    else
      ccerRegVal &= ~(GTIM_CCER_CC3P | GTIM_CCER_CC3NP);  // 00 rising only
    ccerRegVal |= GTIM_CCER_CC3E;   // Enable capture channel 3
    dierRegVal |= GTIM_DIER_CC3IE;  // DMA/Interrupt enable
  }

  if(mdwFreqTimerInfo->chanActiveBits & ACTIVE_CHAN_BITFIELD_4)
  {
    // Capture/compare mode reg 2, chan 4
    regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
    regVal32 &= ~(GTIM_CCMR2_CC4S_MASK);
    regVal32 |= 0x00000100;   // 0x0100 = 0100, set bits 9:8
    putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

    if(useDC)
      ccerRegVal |= (GTIM_CCER_CC4P | GTIM_CCER_CC4NP);   // 11 both edges
    else
      ccerRegVal &= ~(GTIM_CCER_CC4P | GTIM_CCER_CC4NP);  // 00 rising only
    ccerRegVal |= GTIM_CCER_CC4E;   // Enable capture channel 4
    dierRegVal |= GTIM_DIER_CC4IE;  // DMA/Interrupt enable
  }

  // Clear all interrupt sources and then set the needed ones in the
  // DMA/Interrupt Enable Register.
  // Note: Unsupported Advanced timers 1 & 8 add ATIM_DIER_COMIE,
  // ATIM_DIER_BIE and ATIM_DIER_COMDE.
  modifyreg16(timerBase + STM32_GTIM_DIER_OFFSET,
          (GTIM_DIER_TDE   | GTIM_DIER_CC4DE | GTIM_DIER_CC3DE | GTIM_DIER_CC2DE |
           GTIM_DIER_CC1DE | GTIM_DIER_UDE   | GTIM_DIER_TIE   | GTIM_DIER_CC4IE |
           GTIM_DIER_CC3IE | GTIM_DIER_CC2IE | GTIM_DIER_CC1IE | GTIM_DIER_UIE),
          dierRegVal);

  // Capture/Compare Enable Register 
  putreg16(ccerRegVal, timerBase + STM32_GTIM_CCER_OFFSET);

  return OK;
}

//=============================================================
// Configuration of timer registers.
int meadow_measure_freq_cfg_timer_hardware(mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  int ret;
  uint32_t apbClock;
  uint32_t regVal32;
  uint32_t timerBase = mdwFreqTimerInfo->timerBase;
  
  // Slave mode control register
  regVal32 = getreg32(timerBase + STM32_GTIM_SMCR_OFFSET);
  regVal32 &= ~(GTIM_SMCR_ECE | GTIM_SMCR_DISAB | GTIM_SMCR_SMS);
  putreg32(regVal32, timerBase + STM32_GTIM_SMCR_OFFSET);

  // To enable the timer we needed to know which clock enable register to use.
  // And we need to know which bit to set in the register
  apbClock = meadow_measure_freq_get_apb_clock(mdwFreqTimerInfo);
  modifyreg32(apbClock, 0, mdwFreqTimerInfo->timerClkEn);

  // Must be between 0 and 0xffff. Set the prescaler value of 0 to allow
  // highest speed, allowed by MEADOW_FREQ_CLOCK_FREQ.
  // A prescaler value of 1 will divide the clock by 2.
  uint16_t prescaler = (meadow_measure_freq_get_max_clock(mdwFreqTimerInfo)/ \
            (MEADOW_FREQ_CLOCK_FREQ) - 1);
  putreg16(prescaler, timerBase + STM32_GTIM_PSC_OFFSET);

  // The value put into the ARR is maximum allowed for the timer. Either
  // 32-bit or 16-bit ARR register.
  uint32_t maxARRValue = mdwFreqTimerInfo->timerWidth ==
            MEADOW_FREQ_TIMER_WIDTH_16 ? 0xffff : 0xffffffff;
  putreg32(maxARRValue, timerBase + STM32_GTIM_ARR_OFFSET);

  // Control Register 1
  uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
  regval |= GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

  // All interrupts are handled by the same ISR code, but each timer has a
  // different interrupt vector.
  ret = irq_attach(mdwFreqTimerInfo->timerIrqVec,
            meadow_measure_freq_isr,  // ISR address
            mdwFreqTimerInfo);        // Argument to ISR
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Enable timer
  meadow_measure_freq_enable(timerBase);

  // Lastly enable IRQ
  up_enable_irq(mdwFreqTimerInfo->timerIrqVec);

  return OK;
}

//=============================================================
// Called to unconfigure a timer
// It needs channelNumber etc.
int meadow_measure_freq_unconfigure(mdwFreqCfgTimer_t *mdwCfgTimerChan)
{
  int ret;
  uint32_t apbClock;
  uint32_t regVal32;
  uint32_t channelOffset = mdwCfgTimerChan->channelNumber - 1;
  uint16_t ccerRegVal;    // Capture/Compare Enable Register
  uint16_t dierRegVal;    // DMA/Interrupt Enable Register

  mdwFreqTimerInfo_t *mdwFreqTimerInfo =
            meadow_measure_freq_chk_get_timer_info(mdwCfgTimerChan->timerNumber);
  if(mdwFreqTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Couldn't get TimerInfo via timer number:%lu\n",
          __FILE__, __LINE__, mdwCfgTimerChan->timerNumber);
    return MEADOW_MEAS_FREQ_UNCFG_TIMER_ACCESS_NULL;
  }

  uint32_t timerBase = mdwFreqTimerInfo->timerBase;

  // Stop the timer's clock so values can be written to it's registers
  apbClock = meadow_measure_freq_get_apb_clock(mdwFreqTimerInfo);
  modifyreg32(apbClock, mdwFreqTimerInfo->timerClkEn, 0);

  // Is the channel configured?
  uint8_t channelBit = meadow_measure_freq_get_channel_bit(
          mdwCfgTimerChan->channelNumber);
  if((mdwFreqTimerInfo->chanActiveBits & channelBit) == 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:Unconfigure, channel not configured\n",
              __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_UNCFG_CHAN_NOT_CONFIG;
  }

  mdwFreqChanData_t *mdwFreqChanData =
            mdwFreqTimerInfo->mdwFreqChanData[channelOffset];
  if(mdwFreqChanData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Channel data NULL\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_UNCFG_NO_CHANNEL;
  }

  // Unconfigure channel hardware
  ccerRegVal = getreg16(timerBase + STM32_GTIM_CCER_OFFSET);
  dierRegVal = getreg16(timerBase + STM32_GTIM_DIER_OFFSET);
  channelBit = mdwFreqTimerInfo->chanActiveBits;

  // Modifies the requested channel
  switch(mdwCfgTimerChan->channelNumber)
  {
    case FREQ_CHANNEL_NUMBER_CHAN_1:
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
      regVal32 &= ~(GTIM_CCMR1_CC1S_MASK);
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

      channelBit &= ~ACTIVE_CHAN_BITFIELD_1;  // Clear channel field
      // Reset Capture/Compare Enable Register
      ccerRegVal &= ~(GTIM_CCER_CC1E | GTIM_CCER_CC1P | GTIM_CCER_CC1NP);
      dierRegVal &= ~GTIM_DIER_CC1IE;         // DMA/Interrupt disable
      break;

    case FREQ_CHANNEL_NUMBER_CHAN_2:
      // Capture/compare mode reg 1, chan 2
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET);
      regVal32 &= ~(GTIM_CCMR1_CC2S_MASK);
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR1_OFFSET);

      channelBit &= ~ACTIVE_CHAN_BITFIELD_2;
      ccerRegVal &= ~(GTIM_CCER_CC2E | GTIM_CCER_CC2P | GTIM_CCER_CC2NP);
      dierRegVal &= ~GTIM_DIER_CC2IE;
      break;

    case FREQ_CHANNEL_NUMBER_CHAN_3:
      // Capture/compare mode reg 2, chan 3
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
      regVal32 &= ~(GTIM_CCMR2_CC3S_MASK);
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

      channelBit &= ~ACTIVE_CHAN_BITFIELD_3;
      ccerRegVal &= ~(GTIM_CCER_CC3E | GTIM_CCER_CC3P | GTIM_CCER_CC3NP);
      dierRegVal &= ~GTIM_DIER_CC3IE;
      break;

    case FREQ_CHANNEL_NUMBER_CHAN_4:
      // Capture/compare mode reg 2, chan 4
      regVal32 = getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET);
      regVal32 &= ~(GTIM_CCMR2_CC4S_MASK);
      putreg32(regVal32, timerBase + STM32_GTIM_CCMR2_OFFSET);

      channelBit &= ~ACTIVE_CHAN_BITFIELD_4;
      ccerRegVal &= ~(GTIM_CCER_CC4E | GTIM_CCER_CC4P | GTIM_CCER_CC2NP);
      dierRegVal &= ~GTIM_DIER_CC4IE;
      break;
  }

  // Update the new register values
  putreg16(ccerRegVal, timerBase + STM32_GTIM_CCER_OFFSET);
  putreg16(dierRegVal, timerBase + STM32_GTIM_DIER_OFFSET);

  // Save the remaining active channels, if there are any.
  mdwFreqTimerInfo->chanActiveBits = channelBit;

  // Unconfigure no longer needed GPIO
  stm32_unconfiggpio(mdwFreqChanData->inputConfig);

  // If there are no active channels, unconfigure the timer itself.
  // It's clock was already turned off.
  if(channelBit == 0)
  {
    syslog(LOG_INFO, "-->%s@%d-Removing Timer, no remaining channels\n",
              __FILE__, __LINE__);

    // Stop interrupts
    up_disable_irq(mdwFreqTimerInfo->timerIrqVec);

    // Since no remaining channels, stop calling the ISR for overflows
    dierRegVal &= ~GTIM_DIER_UIE;
    putreg16(dierRegVal, timerBase + STM32_GTIM_DIER_OFFSET);

    // No need to auto-reload CNT
    uint16_t regval = getreg16(timerBase + STM32_GTIM_CR1_OFFSET);
    regval &= ~GTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
    putreg16(regval, timerBase + STM32_GTIM_CR1_OFFSET);

    // Detach ISR
    ret = irq_detach(mdwFreqTimerInfo->timerIrqVec);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-irq_detach failed, ret:%d, errno:%d\n",
            __FILE__, __LINE__, ret, errno);
      return MEADOW_MEAS_FREQ_UNCFG_IRQ_DETACH_ERR;
    }

    // Disable timer
    meadow_measure_freq_disable(mdwFreqTimerInfo->timerBase);

    // Clear modifiable timer fields
    mdwFreqTimerInfo->timerOverflow  = 0;
    mdwFreqTimerInfo->chanActiveBits = 0;
  }
  else
  {
    // Still active channels so restart the timer clock
    apbClock = meadow_measure_freq_get_apb_clock(mdwFreqTimerInfo);
    modifyreg32(apbClock, 0, mdwFreqTimerInfo->timerClkEn);
  }

  // Free channel runtime memory
  free(mdwFreqChanData);
  mdwFreqChanData = NULL;

  return MEADOW_MEAS_FREQ_UNCFG_SUCCESSFUL;
}

//================================================================
// Return Frequency and Duty Cycle information to caller.
int meadow_measure_freq_return_freq_info(mdwFreqReturnData_t *returnData)
{
  // 64 bytes of stack space
  double dblFrequency;
  double dblDutyCycle;
  double dblTotalInputCount;
  double dblAvgFreq;
  uint64_t halfCycle;
  uint64_t fullCycle;
  uint64_t regOvrFloTimSize;
  uint64_t bgnCapture;
  uint64_t midCapture;
  uint64_t endCapture;
  uint64_t totalCaptureTime;

  // Verify that provided timer and channel are valid
  if(returnData->timerNumber > 14 || returnData->timerNumber < 1)
  {
    syslog(LOG_ERR, "%s@%d-Timer must be 1 - 14\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_INVALID_TIMER_NUMB;
  }

  if(returnData->channelNumber > 4 || returnData->channelNumber < 1)
  {
    syslog(LOG_ERR, "%s@%d-Channel must be 1 - 4\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_INVALID_CHANNEL_NUMB;
  }

  mdwFreqTimerInfo_t *mdwFreqTimerInfo =
            meadow_measure_freq_chk_get_timer_info(returnData->timerNumber);
  if(mdwFreqTimerInfo == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Couldn't get TimerInfo from timer number:%lu\n",
          __FILE__, __LINE__, returnData->timerNumber);
    return MEADOW_MEAS_FREQ_READ_TIMER_ACCESS_NULL;
  }

  // syslog(1, "-->%s@%d-Returning data-for timer:%lu, channel:%lu, active channels:0x%02x\n",
  //         __FILE__, __LINE__,
  //         returnData->timerNumber, returnData->channelNumber,
  //         mdwFreqTimerInfo->chanActiveBits);

  // Is the channel configured?
  uint8_t channelBit = meadow_measure_freq_get_channel_bit(
          returnData->channelNumber);
  if((mdwFreqTimerInfo->chanActiveBits & channelBit) == 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:Unconfigured channel accessed\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_CHAN_NOT_CONFIG;
  }

  mdwFreqChanData_t *mdwFreqChanData =
            mdwFreqTimerInfo->mdwFreqChanData[returnData->channelNumber - 1];
  if(mdwFreqChanData == NULL)
  {
    syslog(LOG_ERR, "%s@%d-Error:Channel data NULL\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_CHANNEL_DATA_NULL;
  }

  // Does this timer have any channels?
  if(mdwFreqTimerInfo->chanActiveBits == 0)
  {
    // This should be impossible state
    syslog(LOG_ERR, "%s@%d-Error:No channels active\n", __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_NO_CHANS_ACTIVE;
  }

  // Any activity since last this code was last executed?
  if(mdwFreqChanData->gpioCountForAvg == 0)
  {
    returnData->dutyCycleX1000  = 0;
    returnData->frequencyX1000  = 0;
    returnData->avgFreqX1000    = 0;
    returnData->gpioCountForAvg = 0;

    // Reset capture time for next average
    mdwFreqChanData->startTimeForAvg = meadow_measure_freq_get_current_time();
  
    syslog(LOG_ERR, "%s@%d-Error:No freq input since last read\n",
              __FILE__, __LINE__);
    return MEADOW_MEAS_FREQ_READ_FREQ_INPUT_NOT_DETECTED;
  }

  //-------------------------------------------------------
  // We need the timer's width to do the overflow math
  if(mdwFreqTimerInfo->timerWidth)
    regOvrFloTimSize = MEADOW_FREQ_32_BIT_OVERFLOW_COUNT;
  else
    regOvrFloTimSize = MEADOW_FREQ_16_BIT_OVERFLOW_COUNT;

  uint64_t ovrFloStartCnt = mdwFreqChanData->bgnResultOvr;

  // Add in the overflow counts
  bgnCapture = mdwFreqChanData->bgnResultCnt + \
        ((mdwFreqChanData->bgnResultOvr - ovrFloStartCnt) * regOvrFloTimSize);
  midCapture = mdwFreqChanData->midResultCnt + \
        ((mdwFreqChanData->midResultOvr - ovrFloStartCnt) * regOvrFloTimSize);
  endCapture = mdwFreqChanData->endResultCnt + \
        ((mdwFreqChanData->endResultOvr - ovrFloStartCnt) * regOvrFloTimSize);
  
  // Normalize from the beginning
  fullCycle = endCapture - bgnCapture;
  halfCycle = midCapture - bgnCapture;

  // To preserve resolution, use floating point math.
  // Duty Cycle is the ratio of the full cycle count and the cycle count
  // before the trailing edge was detected.
  if(mdwFreqChanData->useDutyCycle)
    dblDutyCycle = (((double)halfCycle) * 100.0) / ((double)fullCycle);
  else
    dblDutyCycle = 0;
    
  // The dblFrequency is the timer's clock divided by the cycle count.
  dblFrequency = ((double)MEADOW_FREQ_CLOCK_FREQ) / ((double)fullCycle);

  // Average frequency since last calculated
  totalCaptureTime =
            meadow_measure_freq_get_current_time() - \
            mdwFreqChanData->startTimeForAvg;

  // Convert time in nanoseconds to fractional seconds
  double dblTotalCaptureTime =\
            ((double) totalCaptureTime) / (1000.0 * 1000.0 * 1000.0);
  dblTotalInputCount = (double)mdwFreqChanData->gpioCountForAvg;
  dblAvgFreq = ((double)dblTotalInputCount) / dblTotalCaptureTime;

  if(mdwFreqChanData->useDutyCycle)
    returnData->dutyCycleX1000  = (dblDutyCycle * 1000.0);
  else
    returnData->dutyCycleX1000  = 0;

  returnData->frequencyX1000    = (dblFrequency * 1000.0);
  returnData->avgFreqX1000      = (dblAvgFreq   * 1000.0);
  returnData->gpioCountForAvg   = (uint32_t)dblTotalInputCount;

  // Reset capture counts and Meadow time for next average
  mdwFreqChanData->gpioCountForAvg = 0;
  mdwFreqChanData->startTimeForAvg = meadow_measure_freq_get_current_time();

  return MEADOW_MEAS_FREQ_READ_SUCCESSFUL;
}

#if MEADOW_MEASURE_FREQ_INCLUDE_REG_DUMP > 0
//=================================================================
// Dump all timer registers
void meadow_measure_freq_diag_dump_timer_regs(char *label,
          mdwFreqTimerInfo_t *mdwFreqTimerInfo)
{
  uint32_t timerBase = mdwFreqTimerInfo->timerBase;

  syslog(1, "\nTimer:%lu Register Dump-%s\n", mdwFreqTimerInfo->timerNumb, label);
  syslog(1, "\tCR1:\t0x%08x\tCR2:\t0x%08x\tSMCR:\t0x%08x\tDIER:\t0x%08x\n",
          getreg16(timerBase + STM32_GTIM_CR1_OFFSET),
          getreg16(timerBase + STM32_GTIM_CR2_OFFSET),
          getreg32(timerBase + STM32_GTIM_SMCR_OFFSET),
          getreg16(timerBase + STM32_GTIM_DIER_OFFSET));

  syslog(1, "\tSR:\t0x%08x\tEGR:\t0x%08x\tCCMR1:\t0x%08x\tCCMR2:\t0x%08x\n",
          getreg16(timerBase + STM32_GTIM_SR_OFFSET),
          getreg16(timerBase + STM32_GTIM_EGR_OFFSET),
          getreg32(timerBase + STM32_GTIM_CCMR1_OFFSET),
          getreg32(timerBase + STM32_GTIM_CCMR2_OFFSET));

  syslog(1, "\tCCER:\t0x%08x\tCNT:\t0x%08x\tPSC:\t0x%08x\tARR:\t0x%08x\n",
          getreg16(timerBase + STM32_GTIM_CCER_OFFSET),
          getreg32(timerBase + STM32_GTIM_CNT_OFFSET),
          getreg16(timerBase + STM32_GTIM_PSC_OFFSET),
          getreg32(timerBase + STM32_GTIM_ARR_OFFSET));

  syslog(1, "\tCCR1:\t0x%08x\tCCR2:\t0x%08x\tCCR3:\t0x%08x\tCCR4:\t0x%08x\n",
          getreg32(timerBase + STM32_GTIM_CCR1_OFFSET),
          getreg32(timerBase + STM32_GTIM_CCR2_OFFSET),
          getreg32(timerBase + STM32_GTIM_CCR3_OFFSET),
          getreg32(timerBase + STM32_GTIM_CCR4_OFFSET));

  syslog(1, "\tDCR:\t0x%08x\tDMAR:\t0x%08x\tOR:\t0x%08x\n",
          getreg16(timerBase + STM32_GTIM_DCR_OFFSET),
          getreg16(timerBase + STM32_GTIM_DMAR_OFFSET),
          getreg16(timerBase + STM32_GTIM_OR_OFFSET));
}
#endif
