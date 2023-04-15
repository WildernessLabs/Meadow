/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow-mint-interrupt.c
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

#warning "(--) Peter working here"

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>

#include <nuttx/config.h>

#include <nuttx/fs/fs.h>
#include <arch/board/board.h>
#include <nuttx/mqueue.h>
#include <nuttx/signal.h>
#include <nuttx/drivers/pwm.h>
#include <nuttx/spi/spi.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32_pwm.h"
#include "stm32_i2c.h"
#include "stm32f777zit6-meadow.h"
#include "stm32_spi.h"

#include <dirent.h>

#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"

#include <nuttx/clock.h>    // for testing

#include "meadow-upd.h"
#include <meadow/meadow_hw_version.h>

//============================================================
// Arbitrary value
#define MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS (32)

// DEVELOPER NOTE:
// Debounce recognizes the first state transition and then ignores anything after
//  that for a period of time.
// Glitch filtering ignores the first state transition and waits a period of time
//  and then looks at state to make sure the result is stable

#define MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG (0)    // 0 > will include

// A free STM32F7 timer
#define MEADOW_INTERRUPT_STM32F7_TIMER_NUMBER (10)
// #define MEADOW_INTERRUPT_STM32F7_TIMER_NUMBER (14)

// This is the threshold any glitch duration greater than this value
// will use milliseconds timing instead of 100 usec timing.
#define MEADOW_INTERRUPT_GLITCH_TIME_USE_MS (200)    // 200 == 20 milliseconds

#if CONFIG_USEC_PER_TICK == 1000
#define MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR (10)
#else
#define MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR (1)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _firstTimeConfig = true;
static struct stm32_tim_dev_s *_periodicTimer;

enum meadowInterruptProcState_e
{
  meadow_int_state_uncfg,           // not configured
  meadow_int_state_wait_gpio_isr,   // waiting for an interrupt from a gpio
  meadow_int_state_mon_glitch,      // gpio is being monitored for glitch timeout
  meadow_int_state_mon_debounce,    // gpio is being monitored for debounce timeout
  meadow_int_state_mon_no_delay     // gpio with no glitch or debounce delay
};

enum RequestedInterruptMode_e
{
  rqstdintmode_none,
  rqstdintmode_rising,
  rqstdintmode_falling,
  rqstdintmode_both
};

enum GlitchAndDebouceReturnValues_e
{
  gad_ret_keepwaiting,
  gad_ret_validtransition,
  gad_ret_break,
};

// All possible input data registers addresses, needed for ISR access to GPIO state
static uint32_t inputDataRegAddrs[] = 
{
  STM32_GPIOA_IDR,
  STM32_GPIOB_IDR,
  STM32_GPIOC_IDR,
  STM32_GPIOD_IDR,
  STM32_GPIOE_IDR,
  STM32_GPIOF_IDR,
  STM32_GPIOG_IDR,
  STM32_GPIOH_IDR,
  STM32_GPIOI_IDR,
  STM32_GPIOJ_IDR,
  STM32_GPIOK_IDR,
};
#define MEADOW_HW_INPUT_DATA_REGS_TOTAL (sizeof(inputDataRegAddrs) / sizeof(uint32_t))

// The following struct defines the informtion needed for debounce and glitch operation
struct interruptPinMap_s
{
  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinId;

  // Address of the "Input Data Register" that holds GPIO port state bits
  uint32_t IDRAddress;

  // CurrentProcessState - tracks the current processing state for this GPIO
  // defined by an entry in meadowInterruptProcState_e enum
  uint8_t CurrentProcessState;

  // Contains the configured interrupt mode: None = 0, Rising = 1, Falling = 2 and Both = 3;
  // Note: for mode 'None' configuration is not sent from Meadow.Core.
  uint8_t GpioInterruptMode;
  // LastKnownGpioState - Last known GPIO state (often last reported to Meadow.Core)
  uint8_t LastKnownGpioState;    // 1 = high, 0 = low, 0xff = unknown

  // The number of debounce timer timeouts needed to satisfy user's config
  int32_t DebounceRequestedDuration;
  // This counter counts down to zero, this ends the debounce period allowing
  // new GPIO interrupts to be received
  uint32_t DebounceDownCounter;

  // The number of valid states that must be the same to be stable
  uint32_t GlitchRequestedDuration;   // Supplied by Meadow.Core
  // The current number of states that have been the same
  uint32_t GlitchTimeoutsCounter;
  // The previous GPIO state that we must be matched to be declared stable
  uint8_t GlitchPrevGpioState;
  // TimeProcessingBegan is used for glitch timing when the glitch duration
  // is beyond a certain limit and for diagnostics.
  uint32_t TimeProcessingBegan;
};

// This is a list of the GPIOs currently being timed. The entries in this list
// are very short lived, begin added as soon as the GPIO ISR is called and
// removed as soon as the glitch or debounce period has elapsed.
static struct interruptPinMap_s * activeGpiosBeingTimed[MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS];

// This count indicates the number of GPIOs active being processed, i.e. the
// number of entries in the activeGpiosBeingTimed array.
static int _timedGpioPinCount;

// IT LOOKS LIKE _timedGpioPinCount AND numbGpioAreBeingTimed WILL ALWAYS HAVE
// THE SAME VALUE. _timedGpioPinCount IS INCREMENTED BY mint_add_gpio_to_timed_list
// AND numbGpioAreBeingTimed IS ALWAYS INCREMENTED BY THE
// mint_add_gpio_to_timed_list CALLER. DECREMENT IS THE SAME PATTERN.

// Increment by GPIO ISR, decremented by timeout expired. Indicates the number
// of GPIOs currently being timed.
static volatile int numbGpioAreBeingTimed = 0;

// Incremented and decrementd by config to indicate how many GPIOs are either
// glitch or debounce GPIOs (i.e. able to be timed).
static volatile int numbGpioConfigCanBeTimed = 0;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int mint_gpio_interrupt(int irq, void *context, void *arg);
static int mint_config_interrupt_prep_timer(int stm32_timer_numb);
static inline int mint_forward_interrupt_to_core(struct interruptPinMap_s *gpioElementAddr, uint8_t state);
static int mint_process_gpio_debounce(struct interruptPinMap_s *gpioElementAddr);
static int mint_process_gpio_glitch(struct interruptPinMap_s *gpioElementAddr);
static int mint_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioElementAddr);
static inline uint8_t mint_read_current_gpio_state(struct interruptPinMap_s *gpioElementAddr);
static int mint_periodic_timeout_isr(int irq, void *context, void *arg);
static void mint_add_gpio_to_timed_list(struct interruptPinMap_s *gpioElementAddr);
static int mint_remove_gpio_to_timed_list(struct interruptPinMap_s *gpioElementAddr);

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This ISR handles the case where a GPIO needs no delay, we notify Meadow.Core
// of the interrupt immediately
static int mint_gpio_interrupt_no_delay(int irq, void *context, void *arg)
{
  struct interruptPinMap_s *gpioElementAddr = (struct interruptPinMap_s *)arg;

  // Properly setup?
  if(gpioElementAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioElementAddr == NULL\n", __FILE__, __LINE__);
    return OK;
  }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)-0x%02x (P%c%d)- No delay interrupt received, state:%d (5)\n",
            gpioElementAddr->PinId,
            ((gpioElementAddr->PinId) >> 4) + 'A', gpioElementAddr->PinId & 0x0f,
            gpioElementAddr->CurrentProcessState);
#endif

  if(gpioElementAddr->CurrentProcessState != meadow_int_state_mon_no_delay)
  {
    syslog(LOG_ERR, "%s@%d-PinId:0x%02x No delay interrupt but not meadow_int_state_mon_no_delay\n",
            __FILE__, __LINE__, gpioElementAddr->PinId);
    return OK;
  }

  // Configured for neither glitch or debounce delay, so send ASAP.
  uint8_t currentState = mint_read_current_gpio_state(gpioElementAddr);
  mint_forward_interrupt_to_core(gpioElementAddr, currentState);

  return OK;
}

//===========================================================================
// There has been a transition of a GPIO pin that we've been ask to monitor.
// This ISR is only configured to be called for glitch or debounce configurations.
// This needs to be remembered when looking at this code.
static int mint_gpio_interrupt(int irq, void *context, void *arg)
{  
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(GPIO isr)-received interrupt. numbGpioConfigCanBeTimed:%d\n", numbGpioConfigCanBeTimed);
#endif

  // Is there any work to do?
  if(numbGpioConfigCanBeTimed < 1)
    return OK;

  struct interruptPinMap_s *gpioElementAddr = (struct interruptPinMap_s *)arg;

  // Properly setup?
  if(gpioElementAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioElementAddr == NULL\n",
            __FILE__, __LINE__);
    return OK;
  }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)-0x%02x (P%c%d)- received interrupt, with delay\n",
            gpioElementAddr->PinId,
            ((gpioElementAddr->PinId) >> 4) + 'A', gpioElementAddr->PinId & 0x0f);
#endif

  // Unless waiting for gpio interrupt, ignore because there's nothing to do
  if(gpioElementAddr->CurrentProcessState != meadow_int_state_wait_gpio_isr)
  {
    // The meadow_int_state_mon_glitch and meadow_int_state_mon_debounce states
    // exit here. Why? Because these states indicate that this GPIO is
    // currently actively being monitored. (i.e., being timed). So, for this
    // GPIO there's nothing to do.

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)--PinId:0x%02x ignored\n", gpioElementAddr->PinId);
#endif

    return OK;
  }

  // Since this ISR is only called for glitch and debounce interrupts, we can
  // safely deduce that this must be an interrupt for a GPIO not currently
  // being monitored. Since an interrupt has occured we'll begin monitoring
  // it by adding to the list of GPIOs that are being timed.
  // All the items in the timed list will be checked at each periodic timer
  // ISR.
  mint_add_gpio_to_timed_list(gpioElementAddr);

  // Depending on the configuration we will save the appropriate information
  // for glitch or debounce.
  // Note: Minimal processing is done here, just setting up for the
  // timer to do the the real work after a delay.
  if(gpioElementAddr->GlitchRequestedDuration > 0)
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)--PinId:0x%02x, Process glitch\n", gpioElementAddr->PinId);
#endif

    // Setup for glitch which always runs before debounce, if debounce configured
    gpioElementAddr->GlitchTimeoutsCounter = 0;
    gpioElementAddr->CurrentProcessState = meadow_int_state_mon_glitch;
    numbGpioAreBeingTimed++;
    STM32_TIM_SETMODE(_periodicTimer, STM32_TIM_MODE_UP);
  }
  else if(gpioElementAddr->DebounceRequestedDuration > 0)
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)--PinId:0x%02x, Process debounce\n", gpioElementAddr->PinId);
#endif
    // Setup for debounce monitoring
    gpioElementAddr->DebounceDownCounter = gpioElementAddr->DebounceRequestedDuration;
    gpioElementAddr->CurrentProcessState = meadow_int_state_mon_debounce;
    numbGpioAreBeingTimed++;
    STM32_TIM_SETMODE(_periodicTimer, STM32_TIM_MODE_UP);
  }
  else
  {
    // Both Glitch and Debounce durations have reached zero and the interrupt mode
    // is set, so read the state and send it to Meadow.Core
    uint8_t currentState = mint_read_current_gpio_state(gpioElementAddr);
    if(currentState == gpioElementAddr->LastKnownGpioState)
      return OK;

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(GPIO isr)--PinId:0x%02x, Glitch and Debounce reached zero. Notify as:0x%02x (was:0x%02x)\n",
        gpioElementAddr->PinId, newState, gpioElementAddr->LastKnownGpioState);
#endif
    if(mint_forward_interrupt_to_core(gpioElementAddr, currentState) == OK)
      gpioElementAddr->LastKnownGpioState = currentState;
  }

  return OK;
}

//===============================================================
// This function is called every 100 microseconds when the timer is running
int mint_periodic_timeout_isr(int irq, void *context, void *arg)
{
  int result;
  struct interruptPinMap_s *gpioElementAddr;

  // Acknowledge timer interrupt. Fortunately at this point we don't
  // care about which GPIO this is for.
  STM32_TIM_ACKINT(_periodicTimer, GTIM_SR_UIF);

  // Timer only needed if at least one gpio configured and active
  if(numbGpioConfigCanBeTimed == 0 || numbGpioAreBeingTimed == 0)
  {
    return OK;
  }

  // Check all active GPIOs
  int offset = 0;
  while(activeGpiosBeingTimed[offset] != NULL)
  {
    gpioElementAddr = activeGpiosBeingTimed[offset++];

    // Monitoring Debounce or Glitch?
    if(gpioElementAddr->CurrentProcessState == meadow_int_state_mon_debounce)
    {
      //----- Debounce Filtering -----
      result = mint_process_gpio_debounce(gpioElementAddr);
      if(result == gad_ret_keepwaiting)
        continue;

      if(result == gad_ret_break)
        break;
    }

    if(gpioElementAddr->CurrentProcessState == meadow_int_state_mon_glitch)
    {
      //----- Glitch Filtering -----
      result = mint_process_gpio_glitch(gpioElementAddr);
      if(result == gad_ret_keepwaiting)
        continue;

      // We reached the end of the glitch filtering time requested
      // Should we switch to debounce?
      if(gpioElementAddr->DebounceRequestedDuration > 0 && result == gad_ret_validtransition)
      {
        // Debounce is also configured for > 0 duration and this interrupt is not a "glitch"
        gpioElementAddr->DebounceDownCounter = gpioElementAddr->DebounceRequestedDuration;
        gpioElementAddr->CurrentProcessState = meadow_int_state_mon_debounce;
        continue;
      }
      else
      {
        // Terminate glitch capture for this GPIO since nothing else to do
        mint_remove_gpio_to_timed_list(gpioElementAddr);
        numbGpioAreBeingTimed--;
        gpioElementAddr->CurrentProcessState = meadow_int_state_wait_gpio_isr;
        if(numbGpioAreBeingTimed == 0)
          break;    // Cannot be more work to do, so quit loop
      }
    }

    // Should never reach here as only debounce and glitch filtering need to
    // be timed.
    syslog(LOG_ERR, "Invalid state %d detected\n",
              gpioElementAddr->CurrentProcessState);
    usleep(20 * 1000);
    PANIC();
  }

  // Stop Timer if no GPIO needs timing
  if(numbGpioAreBeingTimed == 0)
  {
    int ret = STM32_TIM_SETMODE(_periodicTimer, STM32_TIM_MODE_DISABLED);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-udp-(time isr)--STM32_TIM_SETMODE failed:%d\n",
                __FILE__, __LINE__, ret);
    }
  }

  return OK;
}

//========================================================================
// Process debounce
int mint_process_gpio_debounce(struct interruptPinMap_s *gpioElementAddr)
{
  if(gpioElementAddr->DebounceDownCounter == gpioElementAddr->DebounceRequestedDuration)
  {
    // This is the first time to process this debounce filter request

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(deb)-0x%02x DEBOUNCE Starting\n", gpioElementAddr->PinId);
    gpioElementAddr->TimeProcessingBegan = clock_systimer();
#endif

    // In the case both Glitch and Debounce are requested, Gliitch has
    // already sent the interrupt, if it's going to. Only if Glitch does
    // send an interrupt and debounce duration is > 0, will Debounce keep
    // new interrupts inactive until debounce duration has elasped.
    if(gpioElementAddr->GlitchRequestedDuration == 0)
    {
      // Glitch filtering not configured but Debounce was, so send
      // interrupt now.
      mint_meadow_debounce_notification_logic(gpioElementAddr);
    }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    else
    {
      syslog(LOG_INFO, "mint-(deb)-0x%02x Debounce started following Glitch\n", gpioElementAddr->PinId);
    }
#endif
  }

  // This is the start of Debounce processing. Step one is to notify the Meadow.Foundation
  // by sending an interrupt, unless one already sent by glitch filtering.

  // For Debounce this is all that needs to be done, wait for time to pass.
  // Decrement the timeout count. When it reaches 0, re-enbled the gpio interrupts.
  gpioElementAddr->DebounceDownCounter--;
  if(gpioElementAddr->DebounceDownCounter > 0)
    return gad_ret_keepwaiting;   // Not done yet

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  uint32_t procTime = clock_systimer() - gpioElementAddr->TimeProcessingBegan;
  syslog(LOG_INFO, "mint-(deb)-0x%02x Debounce proccessing ended in %d ms\n",
            gpioElementAddr->PinId, procTime);
#endif

  // Finished with debounce timing. Since the GPIO state should now be
  // stable, save it for next time, especially for InterruptMode.Both
  gpioElementAddr->LastKnownGpioState = mint_read_current_gpio_state(gpioElementAddr);

  // Terminate this GPIO's capture. Postpone this process state transition as
  // late as possible
  mint_remove_gpio_to_timed_list(gpioElementAddr);
  numbGpioAreBeingTimed--;
  gpioElementAddr->CurrentProcessState = meadow_int_state_wait_gpio_isr;
  if(numbGpioAreBeingTimed == 0)
    return gad_ret_break;       // Cannot be more work to do, so quit loop too

  return gad_ret_keepwaiting;   // Go to next gpio
}

//========================================================================
// Glitch filtering is done here.
int mint_process_gpio_glitch(struct interruptPinMap_s *gpioElementAddr)
{
  if(gpioElementAddr->GlitchTimeoutsCounter == 0)
  {
    // First time to process glitch
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch Starting\n", gpioElementAddr->PinId);
#endif
    gpioElementAddr->TimeProcessingBegan = clock_systimer();
  }

  gpioElementAddr->GlitchTimeoutsCounter++;

  // If GlitchRequestedDuration is greater than
  // MEADOW_INTERRUPT_GLITCH_TIME_USE_MS we use time based, not count
  // based to establish completion. This is because using counts is more
  // accurate for short delays (having 100 usec resolution). And long delays
  // accumulate an increasing error. Also, if GlitchRequestedDuration is
  // greater than MEADOW_INTERRUPT_GLITCH_TIME_USE_MS we'll only read
  // the GPIO state every millisecond not every timer interrupt.
  if(gpioElementAddr->GlitchRequestedDuration > MEADOW_INTERRUPT_GLITCH_TIME_USE_MS)
  {
    // Using sys time so check about every millisecond not every 100 microseconds.
    if(gpioElementAddr->GlitchTimeoutsCounter % MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR != 0)
      return gad_ret_keepwaiting;
  }

  uint8_t currentState = mint_read_current_gpio_state(gpioElementAddr);

  if(currentState != gpioElementAddr->GlitchPrevGpioState)
  {
    // GPIO state is different from last, save the new state and reset time
    gpioElementAddr->TimeProcessingBegan = clock_systimer();
    gpioElementAddr->GlitchTimeoutsCounter = 0;

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch state changed was:%d now:%d\n",
          gpioElementAddr->PinId, gpioElementAddr->GlitchPrevGpioState, currentState);
#endif
    gpioElementAddr->GlitchPrevGpioState = currentState;
    return gad_ret_keepwaiting;
  }

  // GPIO State stable since last check?
  if(gpioElementAddr->GlitchRequestedDuration > MEADOW_INTERRUPT_GLITCH_TIME_USE_MS)
  {
    // Check base on sys clock
    uint32_t elapedTimeMs = clock_systimer() - gpioElementAddr->TimeProcessingBegan;
    if(elapedTimeMs < gpioElementAddr->GlitchRequestedDuration/MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR)
      return gad_ret_keepwaiting;   // Not done yet
  }
  else
  {
    // Check based on 100 usec timer
    if(gpioElementAddr->GlitchTimeoutsCounter < gpioElementAddr->GlitchRequestedDuration)
      return gad_ret_keepwaiting;   // Need to keep checking
  }

  // Finished checking
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  uint32_t totalTime = clock_systimer() - gpioElementAddr->TimeProcessingBegan;
  syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch completed in %d ms, timeouts:%d\n",
          gpioElementAddr->PinId, totalTime, gpioElementAddr->GlitchTimeoutsCounter);
#endif

  // We've found a stable state.
  // Need to determine whether a interrupt notification is needed
  if(gpioElementAddr->LastKnownGpioState != currentState)
  {
    bool isRising = gpioElementAddr->LastKnownGpioState < currentState;
    switch(gpioElementAddr->GpioInterruptMode)
    {
      case rqstdintmode_both:
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying Meadow.Core, rqstdintmode_both\n", gpioElementAddr->PinId);
#endif
        mint_forward_interrupt_to_core(gpioElementAddr, currentState);
        break;

      case rqstdintmode_falling:
        if(!isRising)
        {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying, Falling and config rqstdintmode_falling\n", gpioElementAddr->PinId);
#endif
          mint_forward_interrupt_to_core(gpioElementAddr, currentState);
        }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        else
        {
          syslog(LOG_INFO, "mint_(glitch)-0x%02x ignoring, Rising but config rqstdintmode_falling\n", gpioElementAddr->PinId);
        }
#endif
        break;

      case rqstdintmode_rising:
        if(isRising)
        {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying, Rising and config rqstdintmode_rising\n", gpioElementAddr->PinId);
#endif
          mint_forward_interrupt_to_core(gpioElementAddr, currentState);
        }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        else
        {
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Ignoring, Falling but config rqstdintmode_rising\n", gpioElementAddr->PinId);
        }
#endif
        break;

      case rqstdintmode_none:
      default:
        syslog(LOG_ERR, "%s@%d-0x%02x unexpected case:%d\n",
                  __FILE__, __LINE__, gpioElementAddr->PinId, gpioElementAddr->GpioInterruptMode);
        break;
    }
  }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  else
  {
    // gpioElementAddr->LastKnownGpioState == currentState i.e. no change. It's a glitch, ignore it
    syslog(LOG_INFO, "mint_(glitch)-0x%02x No GPIO state change-Ignore\n", gpioElementAddr->PinId);
  }
#endif

  gpioElementAddr->LastKnownGpioState = currentState;
  return gad_ret_validtransition;
}

//==================================================================
// This code is only used for notifying Meadow.Core for debounce
int mint_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioElementAddr)
{
  uint8_t newState;
  
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(debounce)-0x%02x Debounce alone, no Glitch\n", gpioElementAddr->PinId);
#endif

  switch(gpioElementAddr->GpioInterruptMode)
  {
    // Perfect debounce filtering is not possible due to the MCU not providing the GPIO
    // state that originally generated the interrupt. Therefore, we must assume that the
    // user is using it correctly and they understand what applications are appropriate.
    // All this is because we cannot accurately read the GPIO state at the instant we receive
    // the interrupt notification, it may have changed. If the desired interrupt mode is
    // 'rising' or 'falling' we assume the state based on the configuration, depending on
    // the MCU to only send the requested types. In the case of 'both' we have no choice
    // but to assume that the last known state was valid and send the opposite state.
    case rqstdintmode_both:
      // Assume the opposite state, that's the best we can do.
      // The LastKnownGpioState was updated after the debounce timeout period.
      newState = gpioElementAddr->LastKnownGpioState == 1 ? 0 : 1;
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_both \n", gpioElementAddr->PinId, newState);
#endif
      mint_forward_interrupt_to_core(gpioElementAddr, newState);
      break;

    case rqstdintmode_falling:
      newState = 0;      // Assume high to low transition
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_falling\n", gpioElementAddr->PinId, 0);
#endif
      mint_forward_interrupt_to_core(gpioElementAddr, newState);
      break;

    case rqstdintmode_rising:
      newState = 1;      // Assume low to high transition
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_rising\n", gpioElementAddr->PinId, 1);
#endif
      mint_forward_interrupt_to_core(gpioElementAddr, 1);  
      break;

    case rqstdintmode_none:
    default:
      syslog(LOG_ERR, "%s@%d-0x%02x unexpected case:%d\n",
              __FILE__, __LINE__, gpioElementAddr->PinId, gpioElementAddr->GpioInterruptMode);
      break;
  }
  return OK;
}

//===============================================================
// Forward interrupt info to Meadow.Core
int mint_forward_interrupt_to_core(struct interruptPinMap_s *gpioElementAddr, uint8_t state)
{
  int ret;
  extern mqd_t s_int_queue;

  // Forward to Meadow.Core
  char queue_buffer[QUEUE_MSG_SIZE];
  queue_buffer[0] = gpioElementAddr->PinId;
  queue_buffer[1] = state;

  ret = mq_send(s_int_queue, queue_buffer, QUEUE_MSG_SIZE, 0);
  if(ret < 0)
  {
    if(errno == ENOMEM)
    {
      syslog(LOG_ERR, "0x%02x Queue overflow (too fast?)\n", gpioElementAddr->PinId);
    }
    else
    {
      syslog(LOG_ERR, "%s@%d-0x%02x mq_send failed:%d, errno:%d\n", __FILE__, __LINE__, 
          gpioElementAddr->PinId, ret, get_errno());
    }
  }

  return ret;
}

//===============================================================
uint8_t mint_read_current_gpio_state(struct interruptPinMap_s *gpioElementAddr)
{
  uint8_t pinNumb = gpioElementAddr->PinId & 0x0f;
  uint32_t idrRegisterValues = *((uint32_t *)(gpioElementAddr->IDRAddress));
  return (idrRegisterValues & (1 << pinNumb)) > 0 ? 1 : 0;
}

//===============================================================
// When an interrupt occurs that must be monitored it is added to this
// list. This list is used in the periodic ISR to time glitch and debounce
// activity. Once the timing has completed it is removed from this list.
void mint_add_gpio_to_timed_list(struct interruptPinMap_s *gpioElementAddr)
{
  // Find first empty slot
  for(int i = 0; i < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; i++)
  {
    if(activeGpiosBeingTimed[i] == NULL)
    {
      activeGpiosBeingTimed[i] = gpioElementAddr;
      _timedGpioPinCount++;
      break;
    }
  }
}

//===============================================================
// Removes from the timer isr
int mint_remove_gpio_to_timed_list(struct interruptPinMap_s *gpioElementAddr)
{
  int activeOffset;

  // Find the entry specified
  for(activeOffset = 0; activeOffset < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; activeOffset++)
  {
    if(activeGpiosBeingTimed[activeOffset] == gpioElementAddr)
      break;
  }

  // Was the item in the list?
  if(activeOffset == MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS)
    return -1;  // Not found

  // Keep tally of entries
  _timedGpioPinCount--;

  // Is this the only entry?
  if(activeOffset == _timedGpioPinCount)
  {
    // Remove the last entry in the array
    activeGpiosBeingTimed[activeOffset] = NULL;
    return OK;
  }
 
  // Compress the list by moving the last entry in the list to the slot we
  // are about to remove and then clearing the last slot.
  activeGpiosBeingTimed[activeOffset] = activeGpiosBeingTimed[_timedGpioPinCount];
  activeGpiosBeingTimed[_timedGpioPinCount] = NULL;
  return OK;
}

//========================================================
// Timer settup is here. This should only be called once
// to prepare both timers for operation.
static int mint_config_interrupt_prep_timer(int stm32_timer_numb)
{
  int ret;
  struct stm32_tim_dev_s *initTimer;

  // For 100 microsec
  // (--) pwm this should be related to STM32_APB2_TIM10_CLKIN and
  // not hardcoded!!!!!
  uint32_t frequency = 1920000;
  uint32_t period = 192 - 1;
  xcpt_t isrHandler = mint_periodic_timeout_isr;

  // For future reference
  // -- for 1 microsec --
  // frequency = STM32_APB2_TIM10_CLKIN;
  // period = 192 - 1;
  // -- for 1 millisec --
  // frequency = STM32_APB2_TIM10_CLKIN / 100; // = 1,920,000 MHz
  // period = 1920 - 1;                        // = 1 millisec
  
  initTimer = stm32_tim_init(stm32_timer_numb);
  if(initTimer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-stm32_tim_init returned NULL\n",
          __FILE__, __LINE__);
    return OK;
  }
  
  // This determines the prescaler value 0 - 65535. Nuttx looks up the
  // desired frequency and calculates the correct clock divisor.
  STM32_TIM_SETCLOCK(initTimer, frequency);

  // Increasing period decreases the frequency
  // Sets the Auto Reload Register value
  STM32_TIM_SETPERIOD(initTimer, period);

  // arg (third parameter) is a pointer that's returned in the isr handler
  ret = STM32_TIM_SETISR(initTimer, isrHandler, NULL, 0);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-STM32_TIM_SETISR failed:%d\n",
          __FILE__, __LINE__, ret);
    return ret;
  }

  // Prevent interrupts until needed
  STM32_TIM_SETMODE(initTimer, STM32_TIM_MODE_DISABLED);

  // Finish set up
  STM32_TIM_ACKINT(initTimer, GTIM_SR_UIF);
  STM32_TIM_ENABLEINT(initTimer, GTIM_DIER_UIE);

  _periodicTimer = initTimer;
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from meadow-upd.c to configure or remove a gpio for monitoring
int mint_config_interrupt(struct mint_gpio_int_config* cfg)
{
  int ret;
  struct interruptPinMap_s *gpioElementAddr;
  uint8_t pinDesignation = cfg->port << 4 | cfg->pin;

  if(_firstTimeConfig)
  {
    _timedGpioPinCount = 0;
    _firstTimeConfig = false;

    // Setup the timer once, the first time
    ret = mint_config_interrupt_prep_timer(MEADOW_INTERRUPT_STM32F7_TIMER_NUMBER);
    if(ret < 0)
    {
      syslog(LOG_ERR, "mint-(cfg)---mint_config_interrupt_prep_timer failed\n");
      return -1;
    }
  }

  // Allocate memory for this GPIO's configuration and data storage needs
  gpioElementAddr = (struct interruptPinMap_s *) zalloc(sizeof (struct interruptPinMap_s));
  if(gpioElementAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // We must set a few elements in the struct for this configuration
  gpioElementAddr->PinId = pinDesignation;
  gpioElementAddr->IDRAddress = inputDataRegAddrs[cfg->port << 4];
  gpioElementAddr->CurrentProcessState = meadow_int_state_uncfg;
  gpioElementAddr->LastKnownGpioState = 0xff;

  if(cfg->enable)
  {
    // Get the current GPIO state which may be used when processing
    // interrupts
    gpioElementAddr->LastKnownGpioState = mint_read_current_gpio_state(gpioElementAddr);
    gpioElementAddr->GlitchPrevGpioState = gpioElementAddr->LastKnownGpioState;

    // Note: the available configurations are 0.0 (none), 0.1 - 1000 millisec.
    // Foundation.Core will supply a value of 0, 1 - 10000. Since the timer 
    // is set at 100 usec then the count provided is the same as the number
    // of timer timeouts received.

    // Set both Debounce and Glitch delay times
    gpioElementAddr->DebounceRequestedDuration = cfg->debounceDuration;
    gpioElementAddr->GlitchRequestedDuration = cfg->glitchDuration;      
    gpioElementAddr->GlitchTimeoutsCounter = 0;

    // none = 0, rising = 1, falling = 2 & both = 3 (must match F7GPIOManager_interrupts.cs
    // in WireInterrupt()
    gpioElementAddr->GpioInterruptMode = (cfg->risingEdge & 0x01) | (cfg->fallingEdge & 0x01) << 1;

    // cfgset contains 20-bits of data. It is required by the Nuttx stm32_gpiosetevent
    // function. If the 20 bits of data are not correct, this Nuttx function will
    // reconfigure the GPIO based on whatever the data is in cfgset.
    // See stm32_gpio.h for more information.
    // Inputs: MMUU .... ...X PPPP BBBB
    // MM = Mode for input (this is 00)
    // UU = pull up, pull down or float
    // X  = is external interrupt selection, stm32_gpiosetevent sets this
    uint32_t cfgset = (pinDesignation & 0x000000ff);   // Set Port and Pin and clear MM

    switch(cfg->resistorMode)
    {
      case 0: // Float
        cfgset |= GPIO_FLOAT;
        break;    // 0 = do nothing
      case 1: // Pull up
        cfgset |= GPIO_PULLUP;
        break;
      case 2: // Pull down
        cfgset |= GPIO_PULLDOWN;
        break;
    }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(cfg)- 0x%02x (P%c%d)-Cfg Enabled-LKS:%d, GLDuration:%d, DBDuration:%d, InterruptMode:%d, cfgset:0x%08x\n",
              gpioElementAddr->PinId,
              ((gpioElementAddr->PinId) >> 4) + 'A', gpioElementAddr->PinId & 0x0f,
              gpioElementAddr->LastKnownGpioState,
              gpioElementAddr->GlitchRequestedDuration,
              gpioElementAddr->DebounceRequestedDuration,
              gpioElementAddr->GpioInterruptMode,
              cfgset);
#endif

    // Tell Nuttx about interrupt parameters
    if(gpioElementAddr->GlitchRequestedDuration > 0)
    {
      // For Glitch we must receive both rising and falling or we cannot keep
      // LastKnownGpioState accurate. After a stable state is reached we save
      // this value. Without this we couldn't send rising and falling correctly
      // to Meadow.Core
      ret = stm32_gpiosetevent(
      cfgset,               // special gpio for call
      1,                    // risingEdge,
      1,                    // fallingEdge,
      0,                    // event
      mint_gpio_interrupt,   // function to call
      gpioElementAddr);     // gpio element address

      // If not already configured
      if(gpioElementAddr->CurrentProcessState == meadow_int_state_uncfg)
          numbGpioConfigCanBeTimed++;

      gpioElementAddr->CurrentProcessState = meadow_int_state_wait_gpio_isr;
    }
    else if(gpioElementAddr->DebounceRequestedDuration > 0)
    {
      // For Debounce we cannot know the GPIOs state for certain when we receive
      // the interrupt notification, so we rely on the MCU only sending interrupts
      // based on the rising and falling configuration. For Both (rising and falling)
      // we assume that the state is the opposite of the last know state.
      ret = stm32_gpiosetevent(
      cfgset,               // special gpio for call
      cfg->risingEdge,      // risingEdge,
      cfg->fallingEdge,     // fallingEdge,
      0,                    // event
      mint_gpio_interrupt,   // function to call
      gpioElementAddr);     // gpio element address

      // If not already configured
      if(gpioElementAddr->CurrentProcessState == meadow_int_state_uncfg)
          numbGpioConfigCanBeTimed++;

      gpioElementAddr->CurrentProcessState = meadow_int_state_wait_gpio_isr;
    }
    else
    {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "udp-(cfg)-0x%02x (P%c%d)--Config NO delay Interrupt\n", gpioElementAddr->PinId,
                  ((gpioElementAddr->PinId) >> 4) + 'A', gpioElementAddr->PinId & 0x0f);
#endif

      // Configured with neither debounch nor glitch filtering.
      // For no delay, as above, we cannot know the GPIOs state for certain when we
      // receive the interrupt notification. So, we immediately read the state.
      // and hope for the best.
      ret = stm32_gpiosetevent(
      cfgset,               // special gpio for call
      cfg->risingEdge,      // risingEdge,
      cfg->fallingEdge,     // fallingEdge,
      0,                    // event
      mint_gpio_interrupt_no_delay,  // different function to call
      gpioElementAddr);     // gpio element address

      gpioElementAddr->CurrentProcessState = meadow_int_state_mon_no_delay;
    }
  }
  else
  {
    // Requested to remove a GPIO from being monitored
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "udp-(cfg)-0x%02x (P%c%d)--Removing GPIO\n", gpioElementAddr->PinId,
                ((gpioElementAddr->PinId) >> 4) + 'A', gpioElementAddr->PinId & 0x0f);
#endif

    // Small chance but it might be active
    mint_remove_gpio_to_timed_list(gpioElementAddr);

    // Tell Nuttx to forget about this interrupt
    ret = stm32_gpiosetevent(
        gpioElementAddr->PinId,
        0, 0, 0, NULL, NULL);

    if(gpioElementAddr->CurrentProcessState != meadow_int_state_mon_no_delay)
      numbGpioConfigCanBeTimed--;   // Keep track only of timed gpios

    gpioElementAddr->CurrentProcessState = meadow_int_state_uncfg;

    free(gpioElementAddr);
  }

  return ret;
}
