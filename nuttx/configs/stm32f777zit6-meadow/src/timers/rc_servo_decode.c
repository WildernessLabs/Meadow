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

// This struct is allocated if needed
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

// These items are constants except for dataPtr which contains allocated memory.
struct rcServoInfo_s
{
  // The following are const, defined at build time
  const uint8_t timerNumb      : 4;      // 0 - 15 timer number
  const uint8_t timerWidth     : 1;      // 16-bit or 32-bit timer? 0 = 16-bits, 1 = 32-bits
  const uint8_t timerMaxClk    : 1;      // 0 = 96MHz (STM32_APB1_TIM2_CLKIN), 1 = 192MHz (STM32_APB2_TIM1_CLKIN)
  const uint8_t timerAPBClk    : 1;      // 0 = STM32_RCC_APB1ENR, 1 = STM32_RCC_APB2ENR
  const uint8_t timerFuture    : 1;
  const uint32_t timerBase;               // Unique for each timer
  const uint32_t timerClkEn;              // Bit of timer enable bit for APB1 or APB2
  const uint32_t timerIrqVec;             // Interrupt vector
  struct rcServoData_s *dataPtr;             // Points to the variable data array
};

// The items in this array, are determined by the environment and cannot be
// populated during configuration except by a lot of code.
static struct rcServoInfo_s rcServoInfoArray[] =
{
  // NOTE: DEFINED Channel REFLECT F7V2, ONLY TIM3 DIFFERENT IN F7V1
  //             |--- bit-field ---|
  //             #  wid max apb fut    Base Addr       Timer Clk Enable      IRQ Vector    Ptr
  /* TIM3   */  {3 , 0,  0,  0,  0, STM32_TIM3_BASE,  RCC_APB1ENR_TIM3EN,  STM32_IRQ_TIM3 , 0},
  /* TIM4   */  {4 , 0,  0,  0,  0, STM32_TIM4_BASE,  RCC_APB1ENR_TIM4EN,  STM32_IRQ_TIM4 , 0},
  /* TIM5   */  {5 , 1,  0,  0,  0, STM32_TIM5_BASE,  RCC_APB1ENR_TIM5EN,  STM32_IRQ_TIM5 , 0},
  /* TIM9   */  {9 , 0,  1,  1,  0, STM32_TIM9_BASE,  RCC_APB2ENR_TIM9EN,  STM32_IRQ_TIM9 , 0},
  /* TIM10  */  {10, 0,  1,  1,  0, STM32_TIM10_BASE, RCC_APB2ENR_TIM10EN, STM32_IRQ_TIM10, 0},
  /* TIM11  */  {11, 0,  1,  1,  0, STM32_TIM11_BASE, RCC_APB2ENR_TIM11EN, STM32_IRQ_TIM11, 0},
  /* TIM12  */  {12, 0,  0,  0,  0, STM32_TIM12_BASE, RCC_APB1ENR_TIM12EN, STM32_IRQ_TIM12, 0},
};

#define MEADOW_TIMER_RC_TOTAL_NUMB (sizeof(rcServoInfoArray) / sizeof(struct rcServoInfo_s))

// GPIOs are in there own table due to the need to change GPIO definitions 
// based on the F7 version number. Hopefully, if there's additional versions
// this will simplify the effort
struct rcServoGpio_s
{
  // In Nuttx pin is bits 3:0, port bits 7:4 and Alt Func 15:12
  uint8_t timerF7v1Gpio[4];  // GPIO for each timer channel
  uint8_t timerF7v2Gpio[4];  // GPIO for each timer channel
  uint16_t timerAltFunc;     // GPIO Alternate Function for each timer
};

// Same timers as above
static struct rcServoGpio_s rcServoGpioArray[] =
{
  //                            F7v1                                              F7v2                       Alt Func
  /* TIM3  D02, D05, D06,  D09  */ {{0x26,0x27,0x10,0x11}, /* D05, D10, A03,  A04  */ {0x14,0x27,0x10,0x11}, GPIO_AF2},
  /* TIM4  D08, D07, D03*, D04* */ {{0x16,0x17,0x18,0x19}, /* D08, D07, D03*, D04* */ {0x16,0x17,0x18,0x19}, GPIO_AF2},
  /* TIM5  D10,                 */ {{0x7a,0xff,0xff,0xff}, /* D02                  */ {0x7a,0xff,0xff,0xff}, GPIO_AF2},
  /* TIM9  A02,                 */ {{0x03,0xff,0xff,0xff}, /* A02                  */ {0x03,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM10 D03*,                */ {{0x18,0xff,0xff,0xff}, /* D03*                 */ {0x18,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM11 D04*,                */ {{0x19,0xff,0xff,0xff}, /* D04*                 */ {0x19,0xff,0xff,0xff}, GPIO_AF3},
  /* TIM12 D12, D13             */ {{0x1e,0x1e,0xff,0xff}, /* D12, D13             */ {0x1e,0x1f,0xff,0xff}, GPIO_AF3},
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
  struct rcServoInfo_s *timerInfo = (struct rcServoInfo_s *)arg;
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
    bool inputState = stm32_gpioread(timerInfo->dataPtr->timerGpioCfg[0]);
    if(timerInfo->dataPtr->timerPolarity1)
      inputState = !inputState;

    if(inputState)
    {
      timerInfo->dataPtr->timerLeadCnt1 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR1_OFFSET);
    }
    else
    {
      uint32_t prevLeadingCount = timerInfo->dataPtr->timerLeadCnt1;

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
        timerInfo->dataPtr->timerPulseW1 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        timerInfo->dataPtr->timerPulseW1 = 0;
      }

      // Set to 0 for leading edge detection
      timerInfo->dataPtr->timerLeadCnt1 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC2IF)
  {
    timStatusReg &= ~GTIM_SR_CC2IF;

    bool inputState = stm32_gpioread(timerInfo->dataPtr->timerGpioCfg[1]);
    
    if(timerInfo->dataPtr->timerPolarity2)
      inputState = !inputState;

    if(inputState)
    {
      timerInfo->dataPtr->timerLeadCnt2 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR2_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = timerInfo->dataPtr->timerLeadCnt2;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR2_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        timerInfo->dataPtr->timerPulseW2 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        timerInfo->dataPtr->timerPulseW2 = 0;
      }

      timerInfo->dataPtr->timerLeadCnt2 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC3IF)
  {
    timStatusReg &= ~GTIM_SR_CC3IF;

    bool inputState = stm32_gpioread(timerInfo->dataPtr->timerGpioCfg[2]);
    
    if(timerInfo->dataPtr->timerPolarity3)
      inputState = !inputState;

    if(inputState)
    {
      timerInfo->dataPtr->timerLeadCnt3 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR3_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = timerInfo->dataPtr->timerLeadCnt3;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR3_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        timerInfo->dataPtr->timerPulseW3 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        timerInfo->dataPtr->timerPulseW3 = 0;
      }

      timerInfo->dataPtr->timerLeadCnt3 = 0;
    }
  }

  //--------------------------------------------
  if(timStatusReg & GTIM_SR_CC4IF)
  {
    timStatusReg &= ~GTIM_SR_CC4IF;
  
    bool inputState = stm32_gpioread(timerInfo->dataPtr->timerGpioCfg[3]);

    if(timerInfo->dataPtr->timerPolarity4)
      inputState = !inputState;

    if(inputState)
    {
      timerInfo->dataPtr->timerLeadCnt4 = getreg16(timerInfo->timerBase + STM32_GTIM_CCR4_OFFSET);
    } 
    else
    {
      uint32_t prevLeadingCount = timerInfo->dataPtr->timerLeadCnt4;
      if(prevLeadingCount > 0)
      {
        uint32_t currentCount = getreg16(timerInfo->timerBase + STM32_GTIM_CCR4_OFFSET);
        if(currentCount < prevLeadingCount)
          currentCount += MEADOW_TIMER_16_BIT_OVERFLOW;

        uint32_t pulseCount = currentCount - prevLeadingCount;
        timerInfo->dataPtr->timerPulseW4 = ((pulseCount * 1000)/ \
                  (MEADOW_TIMER_RC_SERVO_CLK_FREQ/1000));
      }
      else
      {
        timerInfo->dataPtr->timerPulseW4 = 0;
      }

      timerInfo->dataPtr->timerLeadCnt4 = 0;
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

//=============================================================
// static uint8_t meadow_timer_get_timer_numb(struct rcServoInfo_s *timerInfo)
// {
//   return timerInfo->timerNumb;
// }

//=============================================================
// Not all timers are available this will return the correct pointer
static struct rcServoInfo_s * meadow_timer_get_timer_pointer(int timerNumb)
{
  for (int i = 0; i < MEADOW_TIMER_RC_TOTAL_NUMB; i++)
  {
    if(rcServoInfoArray[i].timerNumb == timerNumb)
    {
      return ( &(rcServoInfoArray[i]));
    }
  }

  return NULL;
}

//=============================================================
// Find the proper timer, version and channel for the GPIO Alt
// Function, Port and Pin for this timer.
static uint16_t meadow_timer_get_gpio_for_chan(int timerNumb, int channelOffset)
{
  for (int i = 0; i < MEADOW_TIMER_RC_TOTAL_NUMB; i++)
  {
    if(rcServoInfoArray[i].timerNumb == timerNumb)
    {
      if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
      {
        return rcServoGpioArray[i].timerF7v1Gpio[channelOffset] |
                  rcServoGpioArray[i].timerAltFunc;
      }
      else if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V2 ||
              meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_CCMV2)
      {
        return rcServoGpioArray[i].timerF7v2Gpio[channelOffset] |
                  rcServoGpioArray[i].timerAltFunc;
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
static uint32_t meadow_timer_get_apb_clock(struct rcServoInfo_s *timerInfo)
{
  if(timerInfo->timerAPBClk)
    return STM32_RCC_APB2ENR;
  else
    return STM32_RCC_APB1ENR;
}

//=============================================================
static uint32_t meadow_timer_get_max_clock(struct rcServoInfo_s *timerInfo)
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
int meadow_timer_setup_rc_servo_decode(int timerNumber)
{
  struct rcServoInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);

  if(timerInfo == NULL)
  {
    syslog(1, "meadow_timer_get_timer_pointer() returned NULL\n");
    return -ENXIO;
  }

  timerInfo->dataPtr = malloc(sizeof(struct rcServoData_s));
  memset(timerInfo->dataPtr, 0, sizeof(struct rcServoData_s));

  // Initialize GPIOs for this timer
  for(int i = 0; i < 4; i++)
  {
    uint32_t afPortPin = meadow_timer_get_gpio_for_chan(timerNumber, i);
    if(afPortPin != 0xffff)
    {
      timerInfo->dataPtr->timerGpioCfg[i] = MEADOW_TIMER_GPIO_CONST | afPortPin;
      stm32_configgpio(timerInfo->dataPtr->timerGpioCfg[i]);
    }
    else
    {
      syslog(1, "meadow_timer_setup_rc_servo_decode() no gpio at offset:%d\n", i);
      timerInfo->dataPtr->timerGpioCfg[i] = MEADOW_TIMER_RC_SERVO_BAD_GPIO;
    }
  }

  return OK;
}

//================================================================
// Test code for gated frequency and pulse width
int meadow_timer_test_rc_servo_decode(int timerNumber)
{
  struct rcServoInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  syslog(1, "+++> Pulse Width - Channel 1:%04lu, Channel 2:%04lu, Channel 3:%04lu, Channel 4:%04lu\n",
          timerInfo->dataPtr->timerPulseW1,
          timerInfo->dataPtr->timerPulseW2,
          timerInfo->dataPtr->timerPulseW3,
          timerInfo->dataPtr->timerPulseW4);
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

  struct rcServoInfo_s *timerInfo = meadow_timer_get_timer_pointer(timerNumber);
  if(timerInfo == NULL)
    return -ENXIO;      // Unsupported timer for this feature

  uint32_t timerBase = timerInfo->timerBase;

  bool chan1 = timerInfo->dataPtr->timerGpioCfg[0] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan2 = timerInfo->dataPtr->timerGpioCfg[1] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan3 = timerInfo->dataPtr->timerGpioCfg[2] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  bool chan4 = timerInfo->dataPtr->timerGpioCfg[3] == MEADOW_TIMER_RC_SERVO_BAD_GPIO ? false : true;
  
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
