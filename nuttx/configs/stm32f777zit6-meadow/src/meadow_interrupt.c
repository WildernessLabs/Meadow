/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_interrupt.c
 * 
 *   Copyright (C) 2020, 2021, 2023, 2024 Wilderness Labs. All rights reserved.
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

// DEVELOPER NOTE:
// Debounce recognizes the first state transition and then ignores anything
//  after that for a period of time.
// Glitch filtering ignores the first state transition and waits a period of
//  time and then looks at state to make sure the result is stable

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <string.h>
#include <stdbool.h>
#include <arch/board/board.h>
#include <arch/arch.h>
#include <nuttx/mqueue.h>
#include <errno.h>
#include "chip.h"
#include "stm32f777zit6-meadow.h"
#include <nuttx/timers/timer.h>
#include "stm32_tim.h"
#include <chip/stm32f76xx77xx_rcc.h>
#include <nuttx/clock.h>    // for testing
#include <nuttx/arch.h>
#include "meadow-upd.h"
#include "hcom_nx/hcom_nx_common.h"
#include "meadow_interrupt.h"
#include <meadow/meadow_hw_version.h>

#if defined (CONFIG_MEADOW_INTERRUPT_TESTS)
#pragma message "(--) meadow_interrupt.c"
#endif

// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

//============================================================
// Set == 0 to disable diagnostic output via syslog
// Set == 1 for I/O and config diagnostic output
// Set == 2 or > to output all diagnostic output
#define MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG (0)

// Arbitrary, large value, defining the maximum number of gpios that can be
// monitored.
#define MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS (32)

// Timer 7 is a basic timer with fewer features than other timer types
#define MEADOW_INTERRUPT_STM32F7_TIMER_NUMBER (7)

// Prescaler is be between 0 and 0xffff. A prescaler value of 0 to will
// not divide the input clock and a prescaler value of 1 will divide
// the clock by 2 etc.
// Timer 7's input clock is 96 MHz (1/2 of the STM32_SYSCLK_FREQUENCY speed).
// (prescaler + 1) * (auto reload register + 1) = TimerClock / frequency.
// The following values will give us a timer overflow interrupt every 0.1
// millisecond, which is the desired frequency
#define MEADOW_INTERRUPT_RUNNING_PSC (959)
#define MEADOW_INTERRUPT_RUNNING_ARR (9)

// This is a threshold at which any glitch duration greater than this value
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
static char *thisFile = __FILE__;

static mqd_t mint_mqd;
static bool _firstTimeConfig = true;

// Current GPIO interrupt state
enum MeadowInterruptProcState_e
{
  mint_state_uncfg,           // not configured
  mint_state_wait_gpio_chg,   // waiting for an interrupt from a gpio
  mint_state_mon_glitch,      // gpio is being monitored for glitch timeout
  mint_state_mon_debounce,    // gpio is being monitored for debounce timeout
  mint_state_mon_no_delay     // gpio with no glitch or debounce delay
};

enum RequestedInterruptMode_e
{
  rqstdintmode_none,
  rqstdintmode_rising,
  rqstdintmode_falling,
  rqstdintmode_both
};

enum GlitchDebouceReturnValues_e
{
  glit_debo_ret_check_next_gpio,
  glit_debo_ret_no_timed_remain,
};

// All F7 possible input data registers addresses, used for ISR access to GPIO
// state value.
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

// The following struct defines the information needed for debounce and glitch
// processing
struct interruptPinMap_s
{
  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinId;              // Supplied by configuration

  // What is the configuration type for this?
  // 0 = remove, 1 = new, 2 = low-power sleep wakeup
  uint8_t InputUsage;         // Supplied by configuration

  // Address of correct "Input Data Register" which holds GPIO's hardware port
  // state bits
  uint32_t IDRAddress;        // Calculated during configuration

  // CurrentProcessState - tracks the current processing state for this GPIO
  // defined by an entry in MeadowInterruptProcState_e enum
  uint8_t CurrentProcessState;

  // Contains the configured interrupt mode: None = 0, Rising = 1, Falling = 2
  // and Both = 3.
  // Note: for mode 'None' configuration is not sent from Meadow.Core.
  uint8_t GpioRiseFallValue;   // Supplied by configuration

  // LastKnownGpioState - Last known GPIO state (often last reported).
  uint8_t LastKnownGpioState;    // 1 = high, 0 = low, 0xff = unknown

  // The number of debounce timer timeouts needed to satisfy user's config
  int32_t DebounceConfiguredDuration;   // Supplied by configuration

  // This counter counts the requested timeouts down to zero, this ends the
  // debounce period allowing new GPIO interrupts to be received and processing
  // to be repeated.
  uint32_t DebounceDownCounter;

  // The number of valid states that must be the same to declare this GPIO's
  // state to be stable.
  uint32_t GlitchConfiguredDuration;   // Supplied by configuration

  // The current number of times the state of the GPIO has been the same
  // without any state transitions.
  uint32_t GlitchTimeoutsCounter;

  // The previous GPIO state that we must be matched to be declared stable
  uint8_t GlitchPrevGpioState;

  // TimeProcessingBegan is used for glitch timing when the glitch duration
  // is beyond a certain limit and for diagnostics.
  uint32_t TimeProcessingBegan;
};

// This is the list of the GPIOs currently being timed. The entries in this
// list are very short lived, begin added as soon as the GPIO ISR is called and
// removed as soon as the glitch or debounce period has elapsed.
static struct interruptPinMap_s *_allGpiosBeingTimed[MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS];

// This list keeps the address of all the allocated structures so they can be
// removed when the GPIO is removed
static struct interruptPinMap_s *_allConfiguredGpios[MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS];

// Indicates the total number of GPIOs currently being timed.
static volatile int _allGpiosBeingTimedCnt = 0;

// Incremented and decrementd during configuration to indicate how many GPIOs
// need to be timed (only glitch and debounce GPIOs need to be timed).
static volatile int _totalGpiosCanBeTimed = 0;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int mint_gpio_no_delay_isr(int irq, void *context, void *arg);
static int mint_gpio_need_delay_isr(int irq, void *context, void *arg);
static int mint_config_interrupt_prep_timer(int stm32_timer_numb);
static inline int mint_forward_interrupt_to_core(struct interruptPinMap_s *gpioInfoAddr, uint8_t state);
static int mint_process_gpio_debounce(struct interruptPinMap_s *gpioInfoAddr);
static int mint_process_gpio_glitch(struct interruptPinMap_s *gpioInfoAddr);
static int mint_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioInfoAddr);
static inline uint8_t mint_read_current_gpio_state(struct interruptPinMap_s *gpioInfoAddr);
static int mint_isr_periodic(int irq, void *context, void *arg);
static void mint_add_to_timed_list_and_incr(struct interruptPinMap_s *gpioInfoAddr);
static void mint_remove_from_timed_list_and_decr(struct interruptPinMap_s *gpioInfoAddr);
static void meadow_timer_enable(uint32_t timerBase);
static int mint_config_interrupt_remove(struct mint_gpio_int_config* cfg,
          struct interruptPinMap_s *gpioInfoAddr);
static int mint_config_interrupt_new(struct mint_gpio_int_config* cfg,
          struct interruptPinMap_s *gpioInfoAddr, uint32_t cfgset);

// Basic timers need to turn on/off the timer a different way 
static void turn_periodic_timer_on(void);
static void turn_periodic_timer_off(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// Initialization for meadow interrupt queue for writing
int meadow_interrupt_setup(void)
{
  // Open the mq used to pass interrupts to Meadow.Core
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = MINT_MSG_QUEUE_MAX_MSGS;
  attr.mq_msgsize = SIZE_OF_MINT_CORE_MSG;
  attr.mq_curmsgs = 0;

  mint_mqd = mq_open(MINT_MSG_QUEUE_NAME, O_WRONLY | O_CREAT, 0660, &attr);
  if (mint_mqd == (mqd_t)-1)
  {
    int errcode = get_errno();
    syslog(LOG_ERR, "%s@%d-mq_open failed: %d\n", __FILE__, __LINE__, errcode);
    return -errcode;
  }

  return OK;
}

//=============================================================================
void meadow_interrupt_shutdown(void)
{
  extern mqd_t mint_mqd;
  mq_close(mint_mqd);
}

//=============================================================================
// This ISR handles the case where a GPIO needs no delay, we notify Meadow.Core
// of the interrupt immediately
int mint_gpio_no_delay_isr(int irq, void *context, void *arg)
{
  DEBUG_SET_HIGH(DEBUG_PIN_V2_A1);

  struct interruptPinMap_s *gpioInfoAddr = (struct interruptPinMap_s *)arg;

  // Properly setup?
  if(gpioInfoAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioInfoAddr == NULL\n", __FILE__, __LINE__);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
    return OK;
  }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(no delay)-0x%02x (P%c%d)- A 'no_delay' interrupt received, state:%d (5)\n",
            gpioInfoAddr->PinId,
            ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f,
            gpioInfoAddr->CurrentProcessState);
#endif

  if(gpioInfoAddr->CurrentProcessState != mint_state_mon_no_delay)
  {
    syslog(LOG_ERR, "%s@%d-PinId:0x%02x 'no_delay' interrupt but not mint_state_mon_no_delay\n",
            __FILE__, __LINE__, gpioInfoAddr->PinId);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
    return OK;
  }

  if(gpioInfoAddr->InputUsage == gpio_intrpt_cfg_type_wakeup)
  {
    // Execute the generic code that restarts the F7's internal clocks etc.
    // and then return. The thread that put things into sleep mode will do the
    // rest of the things needed to fully restore normal operations.
    int ret = pwrmgmt_isr_gpio_wakeup_code();
    DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
    return ret;
  }

  // Configured for neither glitch or debounce delay, so send ASAP.
  uint8_t currentState = mint_read_current_gpio_state(gpioInfoAddr);
  mint_forward_interrupt_to_core(gpioInfoAddr, currentState);

  DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
  return OK;
}

//=============================================================================
// This ISR is called for all GPIO state changes for GPIOs needing glitch or
// debounce filtering and a transition of one of those GPIO pins has occurred.
// GPIOs not needing glitch nor debounce filtering use a different ISR.
int mint_gpio_need_delay_isr(int irq, void *context, void *arg)
{  
  struct interruptPinMap_s *gpioInfoAddr = (struct interruptPinMap_s *)arg;

  DEBUG_SET_HIGH(DEBUG_PIN_V2_A2);

  // Is this GPIO setup?
  if(gpioInfoAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioInfoAddr == NULL. This indicates coding problem\n",
            __FILE__, __LINE__);
    return OK;
  }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(delay)-0x%02x (P%c%d)- received delay targeted interrupt\n",
            gpioInfoAddr->PinId,
            ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif

  // If not waiting for GPIO state change, ignore. This will be only GPIOs
  // for which glitch and/or debounce timing is actively being timed already.
  if(gpioInfoAddr->CurrentProcessState != mint_state_wait_gpio_chg)
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(delay)--PinId:0x%02x ignored, not waiting for GPIO change\n", gpioInfoAddr->PinId);
#endif

    DEBUG_SET_LOW(DEBUG_PIN_V2_A2);
    return OK;
  }

  //---------------------------------------------------------------------------
  // The remainder of this code will set things up so this GPIO can be
  // filtered. After being setup this GPIO will be ignored until the glitch /
  // debounce filtering is finished.
  if(gpioInfoAddr->GlitchConfiguredDuration > 0)
  {
    // Setup for ---glitch monitoring---
    DEBUG_SET_HIGH(DEBUG_PIN_V2_A3);

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(delay)--PinId:0x%02x, Process glitch\n", gpioInfoAddr->PinId);
#endif

    // If both glitch and debounce configured, glitch always runs before
    // debounce. After the glitch filtering is finished then the debounce
    // filtering will begin.
    gpioInfoAddr->GlitchTimeoutsCounter = 0;
    gpioInfoAddr->CurrentProcessState = mint_state_mon_glitch;

    mint_add_to_timed_list_and_incr(gpioInfoAddr);

    // Start timer if not running
    turn_periodic_timer_on();
  }
  else if(gpioInfoAddr->DebounceConfiguredDuration > 0)
  {
    // Setup for ---debounce monitoring---
    DEBUG_SET_HIGH(DEBUG_PIN_V2_A3);
    
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(delay)--PinId:0x%02x, Process debounce\n", gpioInfoAddr->PinId);
#endif

    gpioInfoAddr->DebounceDownCounter = gpioInfoAddr->DebounceConfiguredDuration;
    gpioInfoAddr->CurrentProcessState = mint_state_mon_debounce;

    mint_add_to_timed_list_and_incr(gpioInfoAddr);

    // Start timer if not running
    turn_periodic_timer_on();
  }
  else
  {
    // The configured duration for both glitch and debounce duration is == 0?
    // This is illegal as this ISR is only for GPIOs using glitch or debounce
    // filtering which is indicated by their configured duration being > 0.
    syslog(LOG_ERR, "%s@%d-Wrong ISR mint_gpio_need_delay_isr called\n", __FILE__, __LINE__);
    usleep(20 * 1000);
    ASSERT(false);
  }

  DEBUG_SET_LOW(DEBUG_PIN_V2_A2);
  return OK;
}

//===============================================================
// This ISR is called every 100 microseconds when the timer is running, which
// is only when it needs to run to time reporting an interrupt.
int mint_isr_periodic(int irq, void *context, void *arg)
{
  int result;
  struct interruptPinMap_s *gpioInfoAddr = (struct interruptPinMap_s *)arg;

  DEBUG_SET_HIGH(DEBUG_PIN_V2_A0);

  // Acknowledge the timer interrupt.
  uint16_t timStatusReg = getreg16(STM32_TIM7_BASE + STM32_BTIM_SR_OFFSET);
  timStatusReg &= ~BTIM_SR_UIF;
  putreg16(timStatusReg, STM32_TIM7_BASE + STM32_BTIM_SR_OFFSET);

  // In the case where the timer was disable, but the call to this function
  // was already in flight, this code will prevent any unwanted side-effects.
  if(_allGpiosBeingTimedCnt == 0)
  {
    DEBUG_SET_LOW(DEBUG_PIN_V2_A0);
    return OK;
  }

  // Check all GPIOs currently being timed.
  int offset = 0;
  while(_allGpiosBeingTimed[offset] != NULL)
  {
    gpioInfoAddr = _allGpiosBeingTimed[offset++];

    if(gpioInfoAddr->CurrentProcessState == mint_state_mon_debounce)
    {
      //----- Debounce Filtering -----
      result = mint_process_gpio_debounce(gpioInfoAddr);
      if(result == glit_debo_ret_check_next_gpio)
        continue;     // At least 1 active GPIO to time

      // Last GPIO to be timed just finished
      if(result == glit_debo_ret_no_timed_remain)
        break;
    }

    if(gpioInfoAddr->CurrentProcessState == mint_state_mon_glitch)
    {
      //----- Glitch Filtering -----
      result = mint_process_gpio_glitch(gpioInfoAddr);
      if(result == glit_debo_ret_check_next_gpio)
        continue;     // At least 1 active GPIO to time

      // Last GPIO to be timed just finished
      if(result == glit_debo_ret_no_timed_remain)
        break;
    }

    // Should never reach here as only debounce and glitch filtering need to
    // be timed.
    syslog(LOG_ERR, "Invalid process state %d detected\n",
              gpioInfoAddr->CurrentProcessState);
    usleep(20 * 1000);
    PANIC();
  }

  // Stop Timer if no GPIO needs timing
  if(_allGpiosBeingTimedCnt == 0)
  {
    turn_periodic_timer_off();
  }

  DEBUG_SET_LOW(DEBUG_PIN_V2_A0);

  return OK;
}

//========================================================================
// Process debounce here.  Called by periodic timeout.
int mint_process_gpio_debounce(struct interruptPinMap_s *gpioInfoAddr)
{
  if(gpioInfoAddr->DebounceDownCounter == gpioInfoAddr->DebounceConfiguredDuration)
  {
    // This is the first time to process this debounce filter request

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(debo)-0x%02x DEBOUNCE Starting\n", gpioInfoAddr->PinId);
    gpioInfoAddr->TimeProcessingBegan = clock_systimer();
#endif

    // In the case both Glitch and Debounce are requested, Glitch has
    // already sent the interrupt, if it's going to. Only if Glitch does
    // send an interrupt and debounce duration is > 0, will Debounce keep
    // new interrupts inactive until debounce duration has elasped.
    if(gpioInfoAddr->GlitchConfiguredDuration == 0)
    {
      // Glitch filtering not configured but Debounce was, so send
      // interrupt now.
      mint_meadow_debounce_notification_logic(gpioInfoAddr);
    }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    else
    {
      syslog(LOG_INFO, "mint-(debo)-0x%02x Debounce started following Glitch\n", gpioInfoAddr->PinId);
    }
#endif
  }

  // This is the start of Debounce processing.
  // Step one is to notify the Meadow.Core by sending an interrupt, unless one
  // already sent by glitch filtering. Ignoring all GPIO state transitions
  // until the specified amount of time has elasped.

  // Decrement the timeout count. When it reaches 0, re-enbled the gpio interrupts.
  gpioInfoAddr->DebounceDownCounter--;
  if(gpioInfoAddr->DebounceDownCounter > 0)
    return glit_debo_ret_check_next_gpio;   // Not done yet

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
  uint32_t procTime = clock_systimer() - gpioInfoAddr->TimeProcessingBegan;
  syslog(LOG_INFO, "mint-(debo)-0x%02x Debounce proccessing ended in %d ms\n",
            gpioInfoAddr->PinId, procTime);
#endif

  // Finished with debounce timing. Since the GPIO state should now be stable,
  // save it for next time, especially for rqstdintmode_both
  gpioInfoAddr->LastKnownGpioState = mint_read_current_gpio_state(gpioInfoAddr);

  // Terminate this GPIO's capture by removing from the list and changing the
  // processing state.
  mint_remove_from_timed_list_and_decr(gpioInfoAddr);

  // Reset the state so the next GPIO state changes will begin the timing
  // process once again.
  gpioInfoAddr->CurrentProcessState = mint_state_wait_gpio_chg;

  if(_allGpiosBeingTimedCnt == 0)
    return glit_debo_ret_no_timed_remain;  // No more work to do, so quit loop too

  return glit_debo_ret_check_next_gpio;   // Go to next gpio
}

//========================================================================
// Glitch filtering is processed here. Called by periodic timeout.
int mint_process_gpio_glitch(struct interruptPinMap_s *gpioInfoAddr)
{
  if(gpioInfoAddr->GlitchTimeoutsCounter == 0)
  {
    // First time to process glitch or restarting after glitch detected
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch Starting\n", gpioInfoAddr->PinId);
#endif
    gpioInfoAddr->TimeProcessingBegan = clock_systimer();
  }

  gpioInfoAddr->GlitchTimeoutsCounter++;

  // If GlitchConfiguredDuration is greater than
  // MEADOW_INTERRUPT_GLITCH_TIME_USE_MS we use time based, not count
  // based to establish completion. This is because using counts is more
  // accurate for short delays (having 100 usec resolution). And long delays
  // accumulate an increasing error. Also, if GlitchConfiguredDuration is
  // greater than MEADOW_INTERRUPT_GLITCH_TIME_USE_MS we'll only read
  // the GPIO state every millisecond not every timer interrupt.
  if(gpioInfoAddr->GlitchConfiguredDuration > MEADOW_INTERRUPT_GLITCH_TIME_USE_MS)
  {
    // Using sys time so check about every millisecond not every 100 microseconds.
    if(gpioInfoAddr->GlitchTimeoutsCounter % MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR != 0)
      return glit_debo_ret_check_next_gpio;
  }

  uint8_t currentState = mint_read_current_gpio_state(gpioInfoAddr);

  if(currentState != gpioInfoAddr->GlitchPrevGpioState)
  {
    // GPIO state has changed from previous, save the new state and restart
    // the timer.
    gpioInfoAddr->TimeProcessingBegan = clock_systimer();
    gpioInfoAddr->GlitchTimeoutsCounter = 0;
    gpioInfoAddr->GlitchPrevGpioState = currentState;

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
    syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch state changed was:%d now:%d\n",
          gpioInfoAddr->PinId, gpioInfoAddr->GlitchPrevGpioState, currentState);
#endif
    return glit_debo_ret_check_next_gpio;   // Keep waiting
  }

  // GPIO State stable since last check?
  if(gpioInfoAddr->GlitchConfiguredDuration > MEADOW_INTERRUPT_GLITCH_TIME_USE_MS)
  {
    // Check elapsed time based on sys clock
    uint32_t elapedTimeMs = clock_systimer() - gpioInfoAddr->TimeProcessingBegan;
    if(elapedTimeMs < gpioInfoAddr->GlitchConfiguredDuration/MEADOW_INTERRUPT_TICK_MILLISEC_FACTOR)
      return glit_debo_ret_check_next_gpio;   // Not done yet
  }
  else
  {
    // Check elapsed time based on 100 usec timer
    if(gpioInfoAddr->GlitchTimeoutsCounter < gpioInfoAddr->GlitchConfiguredDuration)
      return glit_debo_ret_check_next_gpio;   // Need to keep checking
  }

  // We've found a stable state!
  // Finished checking, GPIO state stable for filter time.
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
  uint32_t totalTime = clock_systimer() - gpioInfoAddr->TimeProcessingBegan;
  syslog(LOG_INFO, "mint-(glitch)-0x%02x Glitch completed in %d ms, timeouts:%d\n",
          gpioInfoAddr->PinId, totalTime, gpioInfoAddr->GlitchTimeoutsCounter);
#endif

  // Need to determine if an interrupt notification needs to be sent to
  // Meadow.Core.
  if(gpioInfoAddr->LastKnownGpioState != currentState)
  {
    bool isRising = gpioInfoAddr->LastKnownGpioState < currentState;
    switch(gpioInfoAddr->GpioRiseFallValue)
    {
      case rqstdintmode_both:
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
        syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying Meadow.Core, rqstdintmode_both\n", gpioInfoAddr->PinId);
#endif
        mint_forward_interrupt_to_core(gpioInfoAddr, currentState);
        break;

      case rqstdintmode_falling:
        if(!isRising)
        {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying, Falling and config rqstdintmode_falling\n", gpioInfoAddr->PinId);
#endif
          mint_forward_interrupt_to_core(gpioInfoAddr, currentState);
        }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
        else
        {
          syslog(LOG_INFO, "mint_(glitch)-0x%02x ignoring, Rising but config rqstdintmode_falling\n", gpioInfoAddr->PinId);
        }
#endif
        break;

      case rqstdintmode_rising:
        if(isRising)
        {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Notifying, Rising and config rqstdintmode_rising\n", gpioInfoAddr->PinId);
#endif
          mint_forward_interrupt_to_core(gpioInfoAddr, currentState);
        }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
        else
        {
          syslog(LOG_INFO, "mint_(glitch)-0x%02x Ignoring, Falling but config rqstdintmode_rising\n", gpioInfoAddr->PinId);
        }
#endif
        break;

      default:
      case rqstdintmode_none:
        syslog(LOG_ERR, "%s@%d-0x%02x illegal interrupt mode:%d\n",
                  __FILE__, __LINE__, gpioInfoAddr->PinId, gpioInfoAddr->GpioRiseFallValue);
        break;
    }
  }
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  else
  {
    // gpioInfoAddr->LastKnownGpioState == currentState i.e. no change. It's a glitch, ignore it
    syslog(LOG_INFO, "mint_(glitch)-0x%02x No GPIO state change-Ignore\n", gpioInfoAddr->PinId);
  }
#endif

  gpioInfoAddr->LastKnownGpioState = currentState;

  //---------------------------------------------------------------------------
  // We reached the end of the glitch filtering time requested. What should be
  // done? Should we switch to debounce filtering or end filtering for this
  // GPIO.
  if(gpioInfoAddr->DebounceConfiguredDuration > 0)
  {
    // Debounce is configured to follow the glitch filtering
    gpioInfoAddr->DebounceDownCounter = gpioInfoAddr->DebounceConfiguredDuration;
    gpioInfoAddr->CurrentProcessState = mint_state_mon_debounce;

   // Move to next GPIO (the debounce will be processed next time)
   return glit_debo_ret_check_next_gpio;
  }
  else
  {
    // Remove this GPIO from being timed
    mint_remove_from_timed_list_and_decr(gpioInfoAddr);

    // Reset the state so the next GPIO state change will begin the timing
    // process again.
    gpioInfoAddr->CurrentProcessState = mint_state_wait_gpio_chg;

    if(_allGpiosBeingTimedCnt == 0)
      return glit_debo_ret_no_timed_remain;  // No more work to do, so quit loop too
  }

  return glit_debo_ret_check_next_gpio;
}

//==================================================================
// This code is only used for notifying Meadow.Core for debounce
int mint_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioInfoAddr)
{
  uint8_t newState;
  
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
  syslog(LOG_INFO, "mint-(debounce)-0x%02x Debounce alone, no Glitch\n", gpioInfoAddr->PinId);
#endif

  switch(gpioInfoAddr->GpioRiseFallValue)
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
      newState = gpioInfoAddr->LastKnownGpioState == 1 ? 0 : 1;
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_both \n", gpioInfoAddr->PinId, newState);
#endif
      mint_forward_interrupt_to_core(gpioInfoAddr, newState);
      break;

    case rqstdintmode_falling:
      newState = 0;      // Assume high to low transition
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_falling\n", gpioInfoAddr->PinId, 0);
#endif
      mint_forward_interrupt_to_core(gpioInfoAddr, newState);
      break;

    case rqstdintmode_rising:
      newState = 1;      // Assume low to high transition
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 1
      syslog(LOG_INFO, "mint-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_rising\n", gpioInfoAddr->PinId, 1);
#endif
      mint_forward_interrupt_to_core(gpioInfoAddr, 1);
      break;

    case rqstdintmode_none:
    default:
      syslog(LOG_ERR, "%s@%d-0x%02x unexpected case:%d\n",
              __FILE__, __LINE__, gpioInfoAddr->PinId, gpioInfoAddr->GpioRiseFallValue);
      break;
  }
  return OK;
}

//===============================================================
// Forward interrupt info to Meadow.Core
// This function will forward:
// 1. PinId - upper 4-bits GPIO port, lower 4-bits GPIO pin
// 2. State - 0 = false, 1 = true
// 3. Ticks - Tick count since OS started (Note: ticks is subject to change)
int mint_forward_interrupt_to_core(struct interruptPinMap_s *gpioInfoAddr,
  uint8_t state)
{
  int ret;
  mint_send_int_core_t mint_send_msg;

  // Timing must be finished
  DEBUG_SET_LOW(DEBUG_PIN_V2_A3);

  // Forward interrupt info to Meadow.Core
  // The first 2 bytes are the same if we add interrupt time or not
  mint_send_msg.gpioPinId = gpioInfoAddr->PinId;
  mint_send_msg.gpioState = state;

  // WIP - WHAT TIME INFO TO SEND?
  // THIS MUST BE RESOLVED WITH INPUT FROM THE MEADOW.CORE TEAM.
  // 
  // THE FOLLOWING IS EXPERIMENTAL CODE.
  //
  // struct tm tmTime = {0};
  // time_t secTime;         // uint32_t
  // long int nanoseconds;   // int32_t
  // uint64_t secTime;
  // int64_t nanoseconds;
  // int64_t secPlusMs;
  // int64_t timeUS;

  // #ifdef CONFIG_STM32F7_HAVE_RTC_SUBSECONDS
  //   ret = up_rtc_getdatetime_with_subseconds(&tmTime,
         // (long int *)&nanoseconds);
  // #endif

  //   // Convert struct tm to seconds
  //   secTime = (time_t)mktime(&tmTime);
  //   secPlusMs = (secTime * 1000   ) + (nanoseconds / (1000 * 1000));
  //   // timeUS = (secTime * 1000000) + (nanoseconds / 1000);
  //
  //   syslog(1, "------------------------------------\n");
  //   syslog(1, "Sizes:secTime:%lu, nanoseconds:%lu, secPlusMs:%lu\n",
  //     sizeof(secTime), sizeof(nanoseconds), sizeof(secPlusMs));
  //
  //   Wed Oct  1 17:21:03 2025
  //   syslog(1, "UTC:%s", ctime((time_t *) &secTime));
  //   syslog(1, "nanoseconds :%020lld\n", nanoseconds);
  //   syslog(1, "milliseconds:%020lld\n", nanoseconds / (1000 * 1000));
  //   syslog(1, "Seconds     :%020lld\n", secTime);
  //   syslog(1, "Sec+MilliSec:%020lld, Sec:%lld, MS::%lld\n",
  // syslog(1, "Sec+MicroSec:%020lld\n", timeUS);
  //     secPlusMs, secPlusMs / 1000, secPlusMs % (1000 * 1000));

  mint_send_msg.interruptTicks = clock_systimer();

  ret = mq_send(mint_mqd, (char *) &mint_send_msg,
    SIZE_OF_MINT_CORE_MSG, 0);
  if(ret < 0)
  {
    if(errno == ENOMEM)
    {
      syslog(LOG_ERR, "0x%02x Queue overflow (too fast?)\n", gpioInfoAddr->PinId);
    }
    else
    {
      syslog(LOG_ERR, "%s@%d-0x%02x mq_send failed:%d, errno:%d\n", __FILE__, __LINE__, 
          gpioInfoAddr->PinId, ret, get_errno());
    }
  }

  return ret;
}

//===============================================================
uint8_t mint_read_current_gpio_state(struct interruptPinMap_s *gpioInfoAddr)
{
  uint8_t pinNumb = gpioInfoAddr->PinId & 0x0f;
  uint32_t idrRegisterValues = *((uint32_t *)(gpioInfoAddr->IDRAddress));
  return (idrRegisterValues & (1 << pinNumb)) > 0 ? 1 : 0;
}

//===============================================================
// When an interrupt occurs that must be monitored it is added to this
// list. This list is used by the periodic ISR to time glitch and debounce
// activity. Once the timing has completed it is removed from this list.
void mint_add_to_timed_list_and_incr(struct interruptPinMap_s *gpioInfoAddr)
{
  // Find first empty slot
  for(int i = 0; i < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; i++)
  {
    if(_allGpiosBeingTimed[i] == NULL)
    {
      _allGpiosBeingTimed[i] = gpioInfoAddr;
      _allGpiosBeingTimedCnt++;
      break;
    }
  }
}

//===============================================================
// We need to save the address of all allocated memory so it can be freed
// when the GPIO is removed.
static int mint_add_new_gpio_to_allocation_list(
            struct interruptPinMap_s *gpioInfoAddr)
{
  for(int i = 0; i < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; i++)
  {
    // Find unused slot
    if(_allConfiguredGpios[i] == NULL)
    {
      _allConfiguredGpios[i] = gpioInfoAddr;
      return OK;
    }
  }
  return -ENOSPC;   // No space
}

//===============================================================
// When removing a GPIO we must find it's memory so it can be freed
static int mint_free_gpio_in_allocation_list(uint8_t pinId)
{
  for(int i = 0; i < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; i++)
  {
    // If NULL continue
    if(_allConfiguredGpios[i] == NULL)
      continue;

    // Match the allocation
    if(_allConfiguredGpios[i]->PinId == pinId)
    {
      free(_allConfiguredGpios[i]);
      _allConfiguredGpios[i] = NULL;
      return OK;
    }
  }

  return -ENODATA;
}

//===============================================================
// Removes from the timer isr
void mint_remove_from_timed_list_and_decr(struct interruptPinMap_s *gpioInfoAddr)
{
  int activeOffset;

  // Find the entry specified
  for(activeOffset = 0; activeOffset < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; activeOffset++)
  {
    if(_allGpiosBeingTimed[activeOffset] == gpioInfoAddr)
      break;
  }

  // Was the item in the list?
  if(activeOffset == MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS)
    return;  // Not found

  // Keep tally of entries
  _allGpiosBeingTimedCnt--;

  // Is this the only entry?
  if(activeOffset == _allGpiosBeingTimedCnt)
  {
    // Remove the last entry in the array
    _allGpiosBeingTimed[activeOffset] = NULL;
    return;
  }
 
  // Compress the list by moving the last entry in the list to the slot we
  // are about to remove and then clearing the last slot.
  _allGpiosBeingTimed[activeOffset] = _allGpiosBeingTimed[_allGpiosBeingTimedCnt];
  _allGpiosBeingTimed[_allGpiosBeingTimedCnt] = NULL;
  return;
}

//=============================================================================
// Timer setup is here. This should only be called once to prepare timer for
// for periodic operation.
// Note: Timers 6 & 7 are Basic Timers. Timer 6 can be used to measure CPU idle
// time and Timer 7 is used here. Since neither timer has any GPIO this means
// other timers can be used for GPIO related work.
static int mint_config_interrupt_prep_timer(int stm32_timer_numb)
{
  int ret;
  
  // Setup the clock enable
  modifyreg32(STM32_RCC_APB1ENR, 0, RCC_APB1ENR_TIM7EN);
  
  // Set prescaler and auto reload register to determine timer interrupt period
  putreg16(MEADOW_INTERRUPT_RUNNING_PSC, STM32_TIM7_BASE + STM32_BTIM_PSC_OFFSET);
  putreg16(MEADOW_INTERRUPT_RUNNING_ARR, STM32_TIM7_BASE + STM32_BTIM_ARR_OFFSET);

  uint16_t regval = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
  regval |= BTIM_CR1_ARPE;    // Auto Reload Pre-Load enable bit
  putreg16(regval, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);

  // Timer 7 only supports UIE interrupt
  putreg16(BTIM_DIER_UIE, STM32_TIM7_BASE + STM32_BTIM_DIER_OFFSET);

  // The ISR for periodic interupt for timing, will roll-over every 65536
  // counts.
  ret = irq_attach(STM32_IRQ_TIM7, mint_isr_periodic, NULL);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-irq_attach failed, ret:%d, errno:%d\n",
          __FILE__, __LINE__, ret, errno);
    return ret;
  }

  // Clear interrupt bit
  uint16_t timStatusReg = getreg16(STM32_TIM7_BASE + STM32_GTIM_SR_OFFSET);
  timStatusReg &= ~BTIM_SR_UIF;
  putreg16(timStatusReg, STM32_TIM7_BASE + STM32_GTIM_SR_OFFSET);

  up_enable_irq(STM32_IRQ_TIM7);

  meadow_timer_enable(STM32_TIM7_BASE);

  return OK;
}

//=============================================================
// Enables the timer
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

//=====================================================================
// Setting the Counter Enable bit
static void turn_periodic_timer_on(void)
{
  uint16_t cr1Val = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
  cr1Val |= BTIM_CR1_CEN;   // counter enable
  putreg16(cr1Val, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
}

//=====================================================================
static void turn_periodic_timer_off(void)
{
  uint16_t cr1Val = getreg16(STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
  cr1Val &= ~BTIM_CR1_CEN;   // counter enable
  putreg16(cr1Val, STM32_TIM7_BASE + STM32_BTIM_CR1_OFFSET);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from meadow-upd.c to configure or remove a gpio for monitoring
int mint_config_interrupt(struct mint_gpio_int_config* cfg)
{
  int ret;
  struct interruptPinMap_s *gpioInfoAddr;
  if(cfg->port > 15|| cfg->pin > 15)
  {
    syslog(LOG_ERR, "%s@%d-ERROR: mint_config_interrupt port/pin error\n",
       thisFile, __LINE__);
    return -EINVAL;
  }
  uint8_t pinDesignation = cfg->port << 4 | cfg->pin;

  // If a new GPIO interrupt must have at least one 'edge' defined
  if(cfg->configType == gpio_intrpt_cfg_type_new && \
     cfg->risingEdge == 0 && cfg->fallingEdge == 0)
  {
    syslog(LOG_ERR, "%s@%d-ERROR: mint_config_interrupt edge config type.\n",
       thisFile, __LINE__);
    return -EINVAL;
  }

  // If configured for wakeup, cannot support glitch or debounce filtering 
  // because while in low-power mode the clocks are all stopped.
  if(cfg->configType == gpio_intrpt_cfg_type_wakeup && \
     (cfg->debounceDuration != 0 || cfg->glitchDuration != 0))
  {
    syslog(LOG_ERR, "%s@%d-ERROR: Both glitch and debounce must be 0 for wakeup\n",
       thisFile, __LINE__);
    return -EINVAL;
  }

  if(_firstTimeConfig)
  {
    _allGpiosBeingTimedCnt = 0;
    _firstTimeConfig = false;

    for(int i = 0; i < MEADOW_INTERRUPT_MAX_SUPPORTED_GPIOS; i++)
    {
      _allConfiguredGpios[i] = NULL;
    }

    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A0);    // True while in periodic isr
    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A1);    // True while in no delay isr
    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A2);    // True while in delay isr
    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A3);
    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A4);
    DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_A5);
    
    DEBUG_SET_LOW(DEBUG_PIN_V2_A0);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A1);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A2);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A3);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A4);
    DEBUG_SET_LOW(DEBUG_PIN_V2_A5);

    // Setup the timer once, the first time
    ret = mint_config_interrupt_prep_timer(MEADOW_INTERRUPT_STM32F7_TIMER_NUMBER);
    if(ret < 0)
    {
      syslog(LOG_ERR, "mint-(cfg)-mint_config_interrupt_prep_timer failed, ret:%d\n", ret);
      return ret;
    }
  }

  // Allocate memory for this GPIO's configuration and data storage needs
  gpioInfoAddr = (struct interruptPinMap_s *) zalloc(sizeof (struct interruptPinMap_s));
  if(gpioInfoAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  // We must set a few elements in the struct for this configuration
  gpioInfoAddr->PinId = pinDesignation;

  // Find the correct offset for this GPIO
  gpioInfoAddr->IDRAddress = inputDataRegAddrs[cfg->port];
  gpioInfoAddr->CurrentProcessState = mint_state_uncfg;
  gpioInfoAddr->LastKnownGpioState = 0xff;
  gpioInfoAddr->InputUsage = cfg->configType;

  // Setup the Nuttx cfgset for this point to be configured by Nuttx
  uint32_t cfgset = (pinDesignation & 0x000000ff);   // Set Port and Pin and clear MM

  switch (cfg->configType)
  {
  case gpio_intrpt_cfg_type_remove:
    ret = mint_config_interrupt_remove(cfg, gpioInfoAddr);
    break;

  case gpio_intrpt_cfg_type_new:
    ret = mint_config_interrupt_new(cfg, gpioInfoAddr, cfgset);
    break;

  case gpio_intrpt_cfg_type_wakeup:
    ret = mint_config_interrupt_new(cfg, gpioInfoAddr, cfgset);
    break;

  default:
    syslog(LOG_ERR, "%s@%d-ERROR:Illegal configType:%lu\n",
      thisFile, __LINE__, cfg->configType);
    break;
  }

  return ret;
}

//========================================================================
// Remove an existing interrupt entry
int mint_config_interrupt_remove(struct mint_gpio_int_config* cfg,
          struct interruptPinMap_s *gpioInfoAddr)
{
  int ret;

  // Disable - remove a GPIO from being monitored
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Removing GPIO\n", gpioInfoAddr->PinId,
              ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif

  // Small chance but it might be actively timing
  mint_remove_from_timed_list_and_decr(gpioInfoAddr);

  // Tell Nuttx to forget about this interrupt
  ret = stm32_gpiosetevent(gpioInfoAddr->PinId, 0, 0, 0, NULL, NULL);

  if(gpioInfoAddr->CurrentProcessState != mint_state_mon_no_delay)
    _totalGpiosCanBeTimed--;   // Keep track only of timed gpios

  gpioInfoAddr->CurrentProcessState = mint_state_uncfg;

  // This call will free the memory allocated when this GPIO was originally
  // configured
  ret = mint_free_gpio_in_allocation_list(gpioInfoAddr->PinId);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-mint_free_gpio_in_allocation_list returned, ret:%d\n",
              __FILE__, __LINE__, ret);
    // Have reported error, might as well finish removing GPIO
  }

  // Free the memory was only allocated a moment ago by the caller to this
  // function.
  free(gpioInfoAddr);
  return ret;
}

//========================================================================
// Add a new interrupt entry
int mint_config_interrupt_new(struct mint_gpio_int_config* cfg,
          struct interruptPinMap_s *gpioInfoAddr, uint32_t cfgset)
{
  int ret;

  // Save the allocated memory so it can be freed when/if GPIO interrupt is
  // disposed of.
  ret = mint_add_new_gpio_to_allocation_list(gpioInfoAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "mint-(cfg)-mint_add_new_gpio_to_allocation_list, ret:%d\n", ret);
    return ret;
  }

  // Get the current GPIO state which may be used when processing
  // the interrupt.
  gpioInfoAddr->LastKnownGpioState = mint_read_current_gpio_state(gpioInfoAddr);
  gpioInfoAddr->GlitchPrevGpioState = gpioInfoAddr->LastKnownGpioState;

  // Set both Debounce and Glitch delay times
  // Note: the available configurations are 0.0 (none), 0.1 - 1000 millisec.
  // Foundation.Core will supply a value of 0, 1 - 10000. Since the timer 
  // is set at 100 usec then the count provided is the same as the number
  // of timer timeouts received.
  gpioInfoAddr->DebounceConfiguredDuration = cfg->debounceDuration;
  gpioInfoAddr->GlitchConfiguredDuration = cfg->glitchDuration;
  gpioInfoAddr->GlitchTimeoutsCounter = 0;

  // none = 0, rising = 1, falling = 2 & both = 3 (must match F7GPIOManager_interrupts.cs
  // in WireInterrupt()
  gpioInfoAddr->GpioRiseFallValue = (cfg->risingEdge & 0x01) | (cfg->fallingEdge & 0x01) << 1;

  // cfgset contains 20-bits of data. It is required by the Nuttx stm32_gpiosetevent
  // function. If the 20 bits of data are not correct, this Nuttx function will
  // reconfigure the GPIO based on whatever the data is in cfgset.
  // See stm32_gpio.h for more information.
  // Inputs: MMUU .... ...X PPPP BBBB
  // MM = Mode for input (this is 00)
  // UU = pull up, pull down or float
  // X  = configure as EXTI interrupt. Event or ISR set by stm32_gpiosetevent
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
    default:
      syslog(LOG_ERR, "%s@%d-ERROR:Illegal resistorMode:%lu\n",
        thisFile, __LINE__, cfg->resistorMode);
      break;
  }

#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "mint-(cfg)- 0x%02x (P%c%d)-Cfg Enabled-LKS:%d, GLDuration:%d, DBDuration:%d, RiseFallValue:%d, cfgset:0x%08x\n",
            gpioInfoAddr->PinId,
            ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f,
            gpioInfoAddr->LastKnownGpioState,
            gpioInfoAddr->GlitchConfiguredDuration,
            gpioInfoAddr->DebounceConfiguredDuration,
            gpioInfoAddr->GpioRiseFallValue,
            cfgset);
#endif

  // Tell Nuttx about interrupt parameters
  if(gpioInfoAddr->GlitchConfiguredDuration > 0)
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Config Glitch\n", gpioInfoAddr->PinId,
                ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif
    // Glitch - we must receive both rising and falling or we cannot keep
    // LastKnownGpioState accurately. After a stable state is reached we save
    // this value. Without this we couldn't send rising and falling correctly
    // to Meadow.Core
    // This call also includes stm32_configgpio() to configure the GPIO
    ret = stm32_gpiosetevent(
    cfgset,                     // Nuttx cfgset
    1,                          // risingEdge,
    1,                          // fallingEdge,
    0,                          // event
    mint_gpio_need_delay_isr,   // Need delay GPIO ISR
    gpioInfoAddr);              // GPIO information address

    // If not already configured
    if(gpioInfoAddr->CurrentProcessState == mint_state_uncfg)
        _totalGpiosCanBeTimed++;

    gpioInfoAddr->CurrentProcessState = mint_state_wait_gpio_chg;
  }
  else if(gpioInfoAddr->DebounceConfiguredDuration > 0)
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Config Debounce\n", gpioInfoAddr->PinId,
                ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif
    // Debounce - we cannot know the GPIOs state for certain when we receive
    // the interrupt notification, so we rely on the MCU only sending interrupts
    // based on the rising and falling configuration. For Both (rising and falling)
    // we assume that the state is the opposite of the last know state.
    // This call also includes stm32_configgpio() to configure the GPIO
    ret = stm32_gpiosetevent(
    cfgset,                     // Nuttx cfgset
    cfg->risingEdge,            // risingEdge,
    cfg->fallingEdge,           // fallingEdge,
    0,                          // event
    mint_gpio_need_delay_isr,   // Need delay GPIO ISR
    gpioInfoAddr);              // GPIO information address

    // If not already configured
    if(gpioInfoAddr->CurrentProcessState == mint_state_uncfg)
        _totalGpiosCanBeTimed++;

    gpioInfoAddr->CurrentProcessState = mint_state_wait_gpio_chg;
  }
  else
  {
#if MEADOW_INTERRUPT_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "mint-(cfg)-0x%02x (P%c%d)--Config NO delay\n", gpioInfoAddr->PinId,
                ((gpioInfoAddr->PinId) >> 4) + 'A', gpioInfoAddr->PinId & 0x0f);
#endif
    // No filtering required. Also used for low-power sleep wakeup.
    // For no delay, as with debounce, we cannot know the GPIOs state for
    // certain when we receive the interrupt notification. So, we immediately
    // read the state and hope for the best.
    // This call also includes stm32_configgpio() to configure the GPIO
    ret = stm32_gpiosetevent(
    cfgset,                   // Nuttx cfgset
    cfg->risingEdge,          // risingEdge,
    cfg->fallingEdge,         // fallingEdge,
    0,                        // event
    mint_gpio_no_delay_isr,   // No delay GPIO ISR
    gpioInfoAddr);            // GPIO information address

    gpioInfoAddr->CurrentProcessState = mint_state_mon_no_delay;
  }
  return ret;
}
