/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow-upd-interrupt.c
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

#include <string.h>

#include <nuttx/config.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
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

// DEVELOPER NOTE:
// Debounce recognizes the first state transition and then ignores anything after
//  that for a period of time.
// Glitch filtering ignores the first state transition and waits a period of time
//  and then looks at state to make sure the result is stable

#define MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG (0)    // 0 > will include

// A free STM32F7 timer
#define MEADOW_UPD_INTERRUPT_STM32F7_TIMER_NUMBER (10)

// This is the threshold any glitch duration greater than this value
// will use milliseconds timing instead of 100 usec timing.
#define MEADOW_UPD_GLITCH_TIME_TO_SWITCH_TO_MS (200)    // 200 == 20 milliseconds

#if CONFIG_USEC_PER_TICK == 1000
#define MEADOW_UPD_TICK_MILLISEC_FACTOR (10)   // GlitchRequestedDuration to milliseconds
#else
#define MEADOW_UPD_TICK_MILLISEC_FACTOR (1)    // GlitchRequestedDuration to milliseconds
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

uint32_t f7HardwareVersion;

enum updInterruptProcState_e
{
  updipstate_uncfg,           // not configured
  updipstate_wait_gpio_isr,   // waiting for an interrupt from a gpio
  updipstate_mon_glitch,      // gpio is being monitored for glitch
  updipstate_mon_debounce,    // gpio is being monitored for debounce
  updipstate_mon_no_delay     // gpio is being monitored with no glitch or debounce delay
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
  gadrv_keepwaiting,
  gadrv_validtransitiondetected,
  gadrv_break,
};

// The following struct defines the informtion needed for debounce and glitch operation
struct interruptPinMap_s
{
  // Represents the CPU Pin identifier (e.g. PD9, D=3 so 39)
  uint8_t PinId;

  // Address of the "Input Data Register" that holds GPIO port state bits
  uint32_t IDRAddress;

  // CurrentProcessState - tracks the current processing state for this GPIO
  // defined by an entry in updInterruptProcState_e enum
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

// Each row represents one Meadow GPIO
// On board Blue, Green and Red (PA0, PA1, PA2) of course excluded
// Note: This table is initialized with F7v1 values by the compiler. If this is wrong, at
// runtime the different values will be modified for the actual hardware version.
static struct interruptPinMap_s gpioDebounceData[] =
{
//                PinId     IDRAddress     Current State   GIM  LKS  DNT DDC GND STC PGS TTP
/* 00 A0   PA4*/  {0x04, STM32_GPIOA_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 01 A1   PA5*/  {0x05, STM32_GPIOA_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 02 A2   PA3*/  {0x03, STM32_GPIOA_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 03 A3   PA7*/  {0x07, STM32_GPIOA_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 04 A4   PC0*/  {0x20, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 05 A5   PC1*/  {0x21, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 06 SCK  PC10*/ {0x2A, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 07 COPI PB5*/  {0x15, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 08 CIPO PC11*/ {0x2B, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 09 D00  PI9*/  {0x89, STM32_GPIOI_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 10 D01  PH13*/ {0x7D, STM32_GPIOH_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 11 D02  PC6*/  {0x26, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 12 D03  PB8*/  {0x18, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 13 D04  PB9*/  {0x19, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 14 D05  PC7*/  {0x27, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 15 D06  PB0*/  {0x10, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 16 D07  PB7*/  {0x17, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 17 D08  PB6*/  {0x16, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 18 D09  PB1*/  {0x11, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 19 D10  PH10*/ {0x7A, STM32_GPIOH_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 20 D11  PC9*/  {0x29, STM32_GPIOC_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 21 D12  PB14*/ {0x1E, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 22 D13  PB15*/ {0x1F, STM32_GPIOB_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 23 D14  PG3*/  {0x63, STM32_GPIOG_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0},
/* 24 D15  PE3*/  {0x43, STM32_GPIOE_IDR, updipstate_uncfg, 0, 0xff,  0,  0,  0,  0,  0,  0} 
};

#define MEADOW_UPD_F7_SUPPORTED_GPIOS (25)

static struct interruptPinMap_s *gpiosBeingTimed[MEADOW_UPD_F7_SUPPORTED_GPIOS];
static int timedGpioPinCount;    // Independent from totalGpiosBeingTimed

//--------------------------------------------------------------
// Hardware version support
typedef struct
{
  // Represents the F7v1 Pin identifiers (e.g. PD9, D=3 so 0x39)
  const uint8_t F7v1PinId;  // Pin port & pin
  const uint8_t F7v2PinId;
  const uint32_t F7v2IDR;   // GPIO base register address
} meadow_hw_ver_gpio_pin_defns_t;

// Each row represents one Meadow GPIO available to the user. This table
// provides the gpio pin differences for the various Meadow Micro F7
// hardware versions.
// The GPIOs in this table are primarily for interrupt support.
static meadow_hw_ver_gpio_pin_defns_t meadow_hw_ver_gpio_pins[] =
{
//   Name   v1    v2     v1Pin  v2Pin v2 Gpio base Addr
/* 00 A0   PA4   PA4 */  {0x04, 0x04, STM32_GPIOA_IDR },
/* 01 A1   PA5   PA5 */  {0x05, 0x05, STM32_GPIOA_IDR },
/* 02 A2   PA3   PA3 */  {0x03, 0x03, STM32_GPIOA_IDR },
/* 03 A3   PA7   PB0 */  {0x07, 0x10, STM32_GPIOB_IDR },
/* 04 A4   PC0   PB1 */  {0x20, 0x11, STM32_GPIOB_IDR },
/* 05 A5   PC1   PC0 */  {0x21, 0x20, STM32_GPIOC_IDR },
/* 06 SCK  PC10  PC10*/  {0x2A, 0x2A, STM32_GPIOC_IDR },
/* 07 COPI PB5   PB5 */  {0x15, 0x15, STM32_GPIOB_IDR },
/* 08 COPO PC11  PC11*/  {0x2B, 0x2B, STM32_GPIOC_IDR },
/* 09 D00  PI9   PI9 */  {0x89, 0x89, STM32_GPIOI_IDR },
/* 10 D01  PH13  PH13*/  {0x7D, 0x7D, STM32_GPIOH_IDR },
/* 11 D02  PC6   PH10*/  {0x26, 0x7A, STM32_GPIOH_IDR },
/* 12 D03  PB8   PB8 */  {0x18, 0x18, STM32_GPIOB_IDR },
/* 13 D04  PB9   PB9 */  {0x19, 0x19, STM32_GPIOB_IDR },
/* 14 D05  PC7   PB4 */  {0x27, 0x14, STM32_GPIOB_IDR },
/* 15 D06  PB0   PB13*/  {0x10, 0x1D, STM32_GPIOB_IDR },
/* 16 D07  PB7   PB7 */  {0x17, 0x17, STM32_GPIOB_IDR },
/* 17 D08  PB6   PB6 */  {0x16, 0x16, STM32_GPIOB_IDR },
/* 18 D09  PB1   PC6 */  {0x11, 0x26, STM32_GPIOC_IDR },
/* 19 D10  PH10  PC7*/   {0x7A, 0x27, STM32_GPIOC_IDR },
/* 20 D11  PC9   PC9 */  {0x29, 0x29, STM32_GPIOC_IDR },
/* 21 D12  PB14  PB14*/  {0x1E, 0x1E, STM32_GPIOB_IDR },
/* 22 D13  PB15  PB15*/  {0x1F, 0x1F, STM32_GPIOB_IDR },
/* 23 D14  PG3   PB12*/  {0x63, 0x1C, STM32_GPIOB_IDR },
/* 24 D15  PE3   PG12*/  {0x43, 0x6C, STM32_GPIOG_IDR }
};

#define MEADOW_HW_VER_GPIO_PIN_DEFNS_SIZE \
  (sizeof(meadow_hw_ver_gpio_pins) / sizeof(meadow_hw_ver_gpio_pin_defns_t))

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int upd_gpio_interrupt(int irq, void *context, void *arg);
static int upd_config_interrupt_prep_timer(int stm32_timer_numb);
static inline int upd_forward_interrupt_to_core(struct interruptPinMap_s *gpioMapTblPtr, uint8_t state);
static int upd_process_gpio_debounce(struct interruptPinMap_s *gpioMapTblPtr);
static int upd_process_gpio_glitch(struct interruptPinMap_s *gpioMapTblPtr);
static int upd_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioMapTblPtr);
static inline uint8_t upd_read_current_gpio_state(struct interruptPinMap_s *gpioMapTblPtr);
static int upd_periodic_timeout_isr(int irq, void *context, void *arg);
static void upd_add_gpio_to_timed_list(struct interruptPinMap_s *gpioMapTblPtr);
static int upd_remove_gpio_to_timed_list(struct interruptPinMap_s *gpioMapTblPtr);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _firstTimeConfig = true;
static struct stm32_tim_dev_s *_periodicTimer;

// These are optimizations so we don't generate unnecessary interrupts
static volatile int numbGpioBeingTimed = 0;  // incremented and decrementd by config
static volatile int totalGpiosBeingTimed = 0;   // increment by ISR decremented by exit

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This handles the case where there is no delay
static int upd_gpio_interrupt_no_delay(int irq, void *context, void *arg)
{
  struct interruptPinMap_s *gpioMapTblPtr = (struct interruptPinMap_s *)arg;

  // Properly setup?
  if(gpioMapTblPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioMapTblPtr == NULL\n",
            __FILE__, __LINE__);
    return OK;
  }

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)-0x%02x (P%c%d)- No delay interrupt received, state:%d (5)\n",
            gpioMapTblPtr->PinId,
            ((gpioMapTblPtr->PinId) >> 4) + 'A', gpioMapTblPtr->PinId & 0x0f,
            gpioMapTblPtr->CurrentProcessState);
#endif

  if(gpioMapTblPtr->CurrentProcessState != updipstate_mon_no_delay)
  {
    syslog(LOG_ERR, "%s@%d-PinId:0x%02x No delay interrupt but not updipstate_mon_no_delay\n",
            __FILE__, __LINE__, gpioMapTblPtr->PinId);
    return OK;
  }

  // Configured for neither glitch or debounce delay, so send ASAP.
  uint8_t currentState = upd_read_current_gpio_state(gpioMapTblPtr);
  upd_forward_interrupt_to_core(gpioMapTblPtr, currentState);

  return OK;
}

//===========================================================================
// There has been a transition on a GPIO pin that we've been ask to delay
static int upd_gpio_interrupt(int irq, void *context, void *arg)
{  
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "upd-(GPIO isr)-received interrupt. numbGpioBeingTimed:%d\n", numbGpioBeingTimed);
#endif

  // Is there any work to do?
  if(numbGpioBeingTimed < 1)
    return OK;

  struct interruptPinMap_s *gpioMapTblPtr = (struct interruptPinMap_s *)arg;

  // Properly setup?
  if(gpioMapTblPtr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-gpioMapTblPtr == NULL\n",
            __FILE__, __LINE__);
    return OK;
  }

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)-0x%02x (P%c%d)- received interrupt\n",
            gpioMapTblPtr->PinId,
            ((gpioMapTblPtr->PinId) >> 4) + 'A', gpioMapTblPtr->PinId & 0x0f);
#endif

  // Unless waiting for gpio interrupt, ignore
  if(gpioMapTblPtr->CurrentProcessState != updipstate_wait_gpio_isr)
  {
    // The updipstate_uncfg, updipstate_mon_glitch and updipstate_mon_debounce states
    // exit here. When work is finished the state is returned to updipstate_wait_gpio_isr.
    // Therefore, switch bouncing will exits here too, assuming the delay is long enough
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)--PinId:0x%02x ignored\n", gpioMapTblPtr->PinId);
#endif
    return OK;
  }

  // This GPIO is not being processed so begin processing
  upd_add_gpio_to_timed_list(gpioMapTblPtr);

  // Check configuration to know what to do, glitch, debounce or just pass
  // through.
  // Note: Minimal processing is done here, just setup for the
  // timer to do the the real work.   
  if(gpioMapTblPtr->GlitchRequestedDuration > 0)
  {
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)--PinId:0x%02x, Process glitch\n", gpioMapTblPtr->PinId);
#endif
    // Glitch always runs before Debounce if Debounce configured
    gpioMapTblPtr->GlitchTimeoutsCounter = 0;
    gpioMapTblPtr->CurrentProcessState = updipstate_mon_glitch;
    totalGpiosBeingTimed++;
    STM32_TIM_SETMODE(_periodicTimer, STM32_TIM_MODE_UP);
  }
  else if(gpioMapTblPtr->DebounceRequestedDuration > 0)
  {
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)--PinId:0x%02x, Process debounce\n", gpioMapTblPtr->PinId);
#endif

    gpioMapTblPtr->DebounceDownCounter = gpioMapTblPtr->DebounceRequestedDuration;
    gpioMapTblPtr->CurrentProcessState = updipstate_mon_debounce;
    totalGpiosBeingTimed++;
    STM32_TIM_SETMODE(_periodicTimer, STM32_TIM_MODE_UP);
  }
  else
  {
    // Both Glitch and Debounce durations have reached zero and the interrupt mode
    // is set, so read the state and send it.
    uint8_t currentState = upd_read_current_gpio_state(gpioMapTblPtr);
    if(currentState == gpioMapTblPtr->LastKnownGpioState)
      return OK;

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(GPIO isr)--PinId:0x%02x, Glitch and Debounce reached zero. Notify as:0x%02x (was:0x%02x)\n",
        gpioMapTblPtr->PinId, newState, gpioMapTblPtr->LastKnownGpioState);
#endif
    if(upd_forward_interrupt_to_core(gpioMapTblPtr, currentState) == OK)
      gpioMapTblPtr->LastKnownGpioState = currentState;
  }

  return OK;
}

//===============================================================
// This function is called every 100 microseconds when the timer is running
int upd_periodic_timeout_isr(int irq, void *context, void *arg)
{
  int result;
  struct interruptPinMap_s *gpioMapTblPtr;

  // Acknowledge timer interrupt. Fortunately at this point we don't
  // care about which GPIO this is for.
  STM32_TIM_ACKINT(_periodicTimer, GTIM_SR_UIF);

  // Timer only needed if at least one gpio configured and active
  if(numbGpioBeingTimed == 0 || totalGpiosBeingTimed == 0)
  {
    return OK;
  }

  // Check all active GPIOs
  int offset = 0;
  while(gpiosBeingTimed[offset] != NULL)
  {
    gpioMapTblPtr = gpiosBeingTimed[offset++];

    // Monitoring Debounce or Glitch?
    if(gpioMapTblPtr->CurrentProcessState == updipstate_mon_debounce)
    {
      //----- Debounce Filtering -----
      result = upd_process_gpio_debounce(gpioMapTblPtr);
      if(result == gadrv_keepwaiting)
        continue;

      if(result == gadrv_break)
        break;
    }

    if(gpioMapTblPtr->CurrentProcessState == updipstate_mon_glitch)
    {
      //----- Glitch Filtering -----
      result = upd_process_gpio_glitch(gpioMapTblPtr);
      if(result == gadrv_keepwaiting)
        continue;

      // We reached the end of the glitch filtering
      // Should we switch to debounce?
      if(gpioMapTblPtr->DebounceRequestedDuration > 0 && result == gadrv_validtransitiondetected)
      {
        // Debounce is also configured for > 0 duration and this interrupt is not a "glitch"
        gpioMapTblPtr->DebounceDownCounter = gpioMapTblPtr->DebounceRequestedDuration;
        gpioMapTblPtr->CurrentProcessState = updipstate_mon_debounce;
        continue;
      }
      else
      {
        // Terminate glitch capture for this GPIO since nothing else to do
        upd_remove_gpio_to_timed_list(gpioMapTblPtr);
        totalGpiosBeingTimed--;
        gpioMapTblPtr->CurrentProcessState = updipstate_wait_gpio_isr;
        if(totalGpiosBeingTimed == 0)
          break;    // Cannot be more work to do, so quit loop
      }
    }

    // Should never reach here
    syslog(LOG_ERR, "Invalid state %d detected\n",
              gpioMapTblPtr->CurrentProcessState);
    usleep(20 * 1000);
    PANIC();
  }

  // Stop Timer if no GPIO needs it
  if(totalGpiosBeingTimed == 0)
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
int upd_process_gpio_debounce(struct interruptPinMap_s *gpioMapTblPtr)
{
  if(gpioMapTblPtr->DebounceDownCounter == gpioMapTblPtr->DebounceRequestedDuration)
  {
    // This is the first time to process this debounce filter request

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(deb)-0x%02x DEBOUNCE Starting\n", gpioMapTblPtr->PinId);
    gpioMapTblPtr->TimeProcessingBegan = clock_systimer();
#endif

    // In the case both Glitch and Debounce are requested, Gliitch has
    // already sent the interrupt, if it's going to. Only if Glitch does
    // send an interrupt and debounce duration is > 0, will Debounce keep
    // new interrupts inactive until debounce duration has elasped.
    if(gpioMapTblPtr->GlitchRequestedDuration == 0)
    {
      // Glitch filtering not configured but Debounce was, so send
      // interrupt now.
      upd_meadow_debounce_notification_logic(gpioMapTblPtr);
    }
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    else
    {
      syslog(LOG_INFO, "upd-(deb)-0x%02x Debounce started following Glitch\n", gpioMapTblPtr->PinId);
    }
#endif
  }

  // This is the start of Debounce processing. Step one is to notify the Meadow.Foundation
  // by sending an interrupt, unless one already sent by glitch filtering.

  // For Debounce this is all that needs to be done, wait for time to pass.
  // Decrement the timeout count. When it reaches 0, re-enbled the gpio interrupts.
  gpioMapTblPtr->DebounceDownCounter--;
  if(gpioMapTblPtr->DebounceDownCounter > 0)
    return gadrv_keepwaiting;   // Not done yet

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  uint32_t procTime = clock_systimer() - gpioMapTblPtr->TimeProcessingBegan;
  syslog(LOG_INFO, "upd-(deb)-0x%02x Debounce proccessing ended in %d ms\n",
            gpioMapTblPtr->PinId, procTime);
#endif

  // Finished with debounce timing. Since the GPIO state should now be
  // stable, save it for next time, especially for InterruptMode.Both
  gpioMapTblPtr->LastKnownGpioState = upd_read_current_gpio_state(gpioMapTblPtr);

  // Terminate this GPIO's capture. Postpone this process state transition as
  // late as possible
  upd_remove_gpio_to_timed_list(gpioMapTblPtr);
  totalGpiosBeingTimed--;
  gpioMapTblPtr->CurrentProcessState = updipstate_wait_gpio_isr;
  if(totalGpiosBeingTimed == 0)
    return gadrv_break;       // Cannot be more work to do, so quit loop too

  return gadrv_keepwaiting;   // Go to next gpio
}

//========================================================================
// Glitch filtering is done here.
int upd_process_gpio_glitch(struct interruptPinMap_s *gpioMapTblPtr)
{
  if(gpioMapTblPtr->GlitchTimeoutsCounter == 0)
  {
    // First time to process glitch
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(glitch)-0x%02x Glitch Starting\n", gpioMapTblPtr->PinId);
#endif
    gpioMapTblPtr->TimeProcessingBegan = clock_systimer();
  }

  gpioMapTblPtr->GlitchTimeoutsCounter++;

  // If GlitchRequestedDuration is greater than MEADOW_UPD_GLITCH_TIME_TO_SWITCH_TO_MS
  // we use time based, not count based to establish completion. This is because using
  // counts is more accurate for short delays (100 usec resolution). And long delays
  // accumulate an increasing error. Also, if GlitchRequestedDuration is greater than
  // MEADOW_UPD_GLITCH_TIME_TO_SWITCH_TO_MS we'll only read the GPIO state every
  // millisecond not every timer interrupt.
  if(gpioMapTblPtr->GlitchRequestedDuration > MEADOW_UPD_GLITCH_TIME_TO_SWITCH_TO_MS)
  {
    // Using sys time so check about every millisecond not every 100 microseconds.
    if(gpioMapTblPtr->GlitchTimeoutsCounter % MEADOW_UPD_TICK_MILLISEC_FACTOR != 0)
      return gadrv_keepwaiting;
  }

  uint8_t currentState = upd_read_current_gpio_state(gpioMapTblPtr);

  if(currentState != gpioMapTblPtr->GlitchPrevGpioState)
  {
    // GPIO state is different from last, save the new state and reset time
    gpioMapTblPtr->TimeProcessingBegan = clock_systimer();
    gpioMapTblPtr->GlitchTimeoutsCounter = 0;

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(glitch)-0x%02x Glitch state changed was:%d now:%d\n",
          gpioMapTblPtr->PinId, gpioMapTblPtr->GlitchPrevGpioState, currentState);
#endif
    gpioMapTblPtr->GlitchPrevGpioState = currentState;
    return gadrv_keepwaiting;
  }

  // GPIO State stable since last check?
  if(gpioMapTblPtr->GlitchRequestedDuration > MEADOW_UPD_GLITCH_TIME_TO_SWITCH_TO_MS)
  {
    // Check base on sys clock
    uint32_t elapedTimeMs = clock_systimer() - gpioMapTblPtr->TimeProcessingBegan;
    if(elapedTimeMs < gpioMapTblPtr->GlitchRequestedDuration/MEADOW_UPD_TICK_MILLISEC_FACTOR)
      return gadrv_keepwaiting;   // Not done yet
  }
  else
  {
    // Check based on 100 usec timer
    if(gpioMapTblPtr->GlitchTimeoutsCounter < gpioMapTblPtr->GlitchRequestedDuration)
      return gadrv_keepwaiting;   // Need to keep checking
  }

  // Finished checking
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  uint32_t totalTime = clock_systimer() - gpioMapTblPtr->TimeProcessingBegan;
  syslog(LOG_INFO, "upd-(glitch)-0x%02x Glitch completed in %d ms, timeouts:%d\n",
          gpioMapTblPtr->PinId, totalTime, gpioMapTblPtr->GlitchTimeoutsCounter);
#endif

  // We've found a stable state.
  // Need to determine whether a interrupt notification is needed
  if(gpioMapTblPtr->LastKnownGpioState != currentState)
  {
    bool isRising = gpioMapTblPtr->LastKnownGpioState < currentState;
    switch(gpioMapTblPtr->GpioInterruptMode)
    {
      case rqstdintmode_both:
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        syslog(LOG_INFO, "upd_(glitch)-0x%02x Notifying Meadow.Core, rqstdintmode_both\n", gpioMapTblPtr->PinId);
#endif
        upd_forward_interrupt_to_core(gpioMapTblPtr, currentState);
        break;

      case rqstdintmode_falling:
        if(!isRising)
        {
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
          syslog(LOG_INFO, "upd_(glitch)-0x%02x Notifying, Falling and config rqstdintmode_falling\n", gpioMapTblPtr->PinId);
#endif
          upd_forward_interrupt_to_core(gpioMapTblPtr, currentState);
        }
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        else
        {
          syslog(LOG_INFO, "upd_(glitch)-0x%02x ignoring, Rising but config rqstdintmode_falling\n", gpioMapTblPtr->PinId);
        }
#endif
        break;

      case rqstdintmode_rising:
        if(isRising)
        {
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
          syslog(LOG_INFO, "upd_(glitch)-0x%02x Notifying, Rising and config rqstdintmode_rising\n", gpioMapTblPtr->PinId);
#endif
          upd_forward_interrupt_to_core(gpioMapTblPtr, currentState);
        }
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
        else
        {
          syslog(LOG_INFO, "upd_(glitch)-0x%02x Ignoring, Falling but config rqstdintmode_rising\n", gpioMapTblPtr->PinId);
        }
#endif
        break;

      case rqstdintmode_none:
      default:
        syslog(LOG_ERR, "%s@%d-0x%02x unexpected case:%d\n",
                  __FILE__, __LINE__, gpioMapTblPtr->PinId, gpioMapTblPtr->GpioInterruptMode);
        break;
    }
  }
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  else
  {
    // gpioMapTblPtr->LastKnownGpioState == currentState i.e. no change. It's a glitch, ignore it
    syslog(LOG_INFO, "upd_(glitch)-0x%02x No GPIO state change-Ignore\n", gpioMapTblPtr->PinId);
  }
#endif

  gpioMapTblPtr->LastKnownGpioState = currentState;
  return gadrv_validtransitiondetected;
}

//==================================================================
// This code is only used for notifying Meadow.Core for debounce
int upd_meadow_debounce_notification_logic(struct interruptPinMap_s *gpioMapTblPtr)
{
  uint8_t newState;
  
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
  syslog(LOG_INFO, "upd-(debounce)-0x%02x Debounce alone, no Glitch\n", gpioMapTblPtr->PinId);
#endif

  switch(gpioMapTblPtr->GpioInterruptMode)
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
      newState = gpioMapTblPtr->LastKnownGpioState == 1 ? 0 : 1;
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "upd-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_both \n", gpioMapTblPtr->PinId, newState);
#endif
      upd_forward_interrupt_to_core(gpioMapTblPtr, newState);
      break;

    case rqstdintmode_falling:
      newState = 0;      // Assume high to low transition
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "upd-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_falling\n", gpioMapTblPtr->PinId, 0);
#endif
      upd_forward_interrupt_to_core(gpioMapTblPtr, newState);
      break;

    case rqstdintmode_rising:
      newState = 1;      // Assume low to high transition
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "upd-(debounce)-0x%02x--Notifying 0x%02x rqstdintmode_rising\n", gpioMapTblPtr->PinId, 1);
#endif
      upd_forward_interrupt_to_core(gpioMapTblPtr, 1);  
      break;

    case rqstdintmode_none:
    default:
      syslog(LOG_ERR, "%s@%d-0x%02x unexpected case:%d\n",
              __FILE__, __LINE__, gpioMapTblPtr->PinId, gpioMapTblPtr->GpioInterruptMode);
      break;
  }
  return OK;
}

//===============================================================
// Forward interrupt info to Meadow.Core
int upd_forward_interrupt_to_core(struct interruptPinMap_s *gpioMapTblPtr, uint8_t state)
{
  int ret;
  extern mqd_t s_int_queue;

  // Forward to Meadow.Core
  char queue_buffer[QUEUE_MSG_SIZE];
  queue_buffer[0] = gpioMapTblPtr->PinId;
  queue_buffer[1] = state;

  ret = mq_send(s_int_queue, queue_buffer, QUEUE_MSG_SIZE, 0);
  if(ret < 0)
  {
    if(errno == ENOMEM)
    {
      syslog(LOG_ERR, "0x%02x Queue overflow (too fast?)\n", gpioMapTblPtr->PinId);
    }
    else
    {
      syslog(LOG_ERR, "%s@%d-0x%02x mq_send failed:%d, errno:%d\n", __FILE__, __LINE__, 
          gpioMapTblPtr->PinId, ret, get_errno());
    }
  }

  return ret;
}

//===============================================================
uint8_t upd_read_current_gpio_state(struct interruptPinMap_s *gpioMapTblPtr)
{
  uint8_t pinNumb = gpioMapTblPtr->PinId & 0x0f;
  uint32_t idrRegisterValues = *((uint32_t *)(gpioMapTblPtr->IDRAddress));
  return (idrRegisterValues & (1 << pinNumb)) > 0 ? 1 : 0;
}

//===============================================================
// Adds are from the gpio isr
void upd_add_gpio_to_timed_list(struct interruptPinMap_s *gpioMapTblPtr)
{
  // Find first empty slot
  for(int i = 0; i < MEADOW_UPD_F7_SUPPORTED_GPIOS; i++)
  {
    if(gpiosBeingTimed[i] == NULL)
    {
      gpiosBeingTimed[i] = gpioMapTblPtr;
      timedGpioPinCount++;
      break;
    }
  }
}

//===============================================================
// Removes are from the timer isr
int upd_remove_gpio_to_timed_list(struct interruptPinMap_s *gpioMapTblPtr)
{
  int activeOffset;

  // Find the entry specified
  for(activeOffset = 0; activeOffset < MEADOW_UPD_F7_SUPPORTED_GPIOS; activeOffset++)
  {
    if(gpiosBeingTimed[activeOffset] == gpioMapTblPtr)
      break;
  }

  if(activeOffset == MEADOW_UPD_F7_SUPPORTED_GPIOS)
    return -1;  // Not found

  timedGpioPinCount--;
  if(activeOffset == timedGpioPinCount)
  {
    // Remove the last entry in the array
    gpiosBeingTimed[activeOffset] = NULL;
    return OK;
  }
 
  // Compress the list by moving the last entry to the now empty slot
  gpiosBeingTimed[activeOffset] = gpiosBeingTimed[timedGpioPinCount];
  gpiosBeingTimed[timedGpioPinCount] = NULL;
  return OK;
}

//========================================================
// Timer settup is here. This should only be called once
// to prepare both timers for operation.
static int upd_config_interrupt_prep_timer(int stm32_timer_numb)
{
  int ret;
  struct stm32_tim_dev_s *tempTimer;

  // for 100 microsec
  uint32_t frequency = STM32_APB2_TIM10_CLKIN / 100; // 1,920,000 MHz;
  uint32_t period = 192 - 1;
  xcpt_t isrHandler = upd_periodic_timeout_isr;

  // For future reference
  // -- for 1 microsec --
  // frequency = STM32_APB2_TIM10_CLKIN;
  // period = 192 - 1;
  // -- for 1 millisec --
  // frequency = STM32_APB2_TIM10_CLKIN / 100; // = 1,920,000 MHz
  // period = 1920 - 1;                        // = 1 millisec
  
  tempTimer = stm32_tim_init(stm32_timer_numb);
  if(tempTimer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-stm32_tim_init returned NULL\n",
          __FILE__, __LINE__);
    return OK;
  }
  
  // This determines the prescaler value 0 - 65535
  STM32_TIM_SETCLOCK(tempTimer, frequency);

  // Increasing period decreases the frequency
  STM32_TIM_SETPERIOD(tempTimer, period);

  // arg (third parameter) is a pointer that's returned in the isr handler
  ret = STM32_TIM_SETISR(tempTimer, isrHandler, NULL, 0);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-STM32_TIM_SETISR failed:%d\n",
          __FILE__, __LINE__, ret);
    return ret;
  }

  // Prevent interrupts until needed
  STM32_TIM_SETMODE(tempTimer, STM32_TIM_MODE_DISABLED);

  // Finish set up
  STM32_TIM_ACKINT(tempTimer, GTIM_SR_UIF);
  STM32_TIM_ENABLEINT(tempTimer, GTIM_DIER_UIE);

  _periodicTimer = tempTimer;
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from meadow-upd.c to configure a gpio for monitoring
int upd_config_interrupt(struct upd_gpio_int_config* cfg)
{
  struct interruptPinMap_s *gpioMapTblPtr;
  uint32_t designator = cfg->port << 4 | cfg->pin;
  uint32_t pinMapOffset;
  int ret;

  if(_firstTimeConfig)
  {
    // Do the following once, the first time
    ret = upd_config_interrupt_prep_timer(MEADOW_UPD_INTERRUPT_STM32F7_TIMER_NUMBER);
    if(ret < 0)
    {
      syslog(LOG_ERR, "upd-(cfg)---upd_config_interrupt_prep_timer failed\n");
      return -1;
    }

    f7HardwareVersion = meadow_hw_version_get();

    for(int i = 0; i < MEADOW_UPD_F7_SUPPORTED_GPIOS; i++)
    {
      // Clear out the active number of those being timed
      gpiosBeingTimed[i] = NULL;

      // To support multiple meadow hardware version, the gpio table is filled
      // with values correct for F7v1 at build time. If running on a different
      // hardware platform the table entries must be updated.
      if(f7HardwareVersion != MEADOW_F7_HW_VERSION_NUMB_F7V1)
      {
        if(meadow_hw_ver_gpio_pins[i].F7v1PinId != 
           meadow_hw_ver_gpio_pins[i].F7v2PinId)
        {
          // Over write the F7v1 defaults with the correct gpio information
          gpioDebounceData[i].PinId = meadow_hw_ver_gpio_pins[i].F7v2PinId;
          gpioDebounceData[i].IDRAddress = meadow_hw_ver_gpio_pins[i].F7v2IDR;
        }
      }
    }

    timedGpioPinCount = 0;

    _firstTimeConfig = false;
  }

  // Walk the gpio data array to find the desired entry
  for(pinMapOffset = 0; pinMapOffset < MEADOW_UPD_F7_SUPPORTED_GPIOS; pinMapOffset++)
  {
    if(gpioDebounceData[pinMapOffset].PinId == designator)
      break;
  }

  if(pinMapOffset == MEADOW_UPD_F7_SUPPORTED_GPIOS)
  {
    // GPIO not found in data table? This is not expected.
    syslog(LOG_ERR, "upd-(cfg)--No entry in table for 0x%02x\n", designator);
    return -ENODEV;
  }

  // Grab the correct entry's table address
  gpioMapTblPtr = &gpioDebounceData[pinMapOffset];

  if(cfg->enable)
  {
    // Get the current GPIO state which may be used when processing
    // interrupts
    gpioMapTblPtr->LastKnownGpioState = upd_read_current_gpio_state(gpioMapTblPtr);
    gpioMapTblPtr->GlitchPrevGpioState = gpioMapTblPtr->LastKnownGpioState;

    // Note: the available configurations are 0.0 (none), 0.1 - 1000 millisec.
    // Foundation.Core will supply a value of 0, 1 - 10000. Since the timer 
    // is set at 100 usec then the count provided is the same as the number
    // of timer timeouts received.

    // Set both Debounce and Glitch delay times
    gpioMapTblPtr->DebounceRequestedDuration = cfg->debounceDuration;
    gpioMapTblPtr->GlitchRequestedDuration = cfg->glitchDuration;      
    gpioMapTblPtr->GlitchTimeoutsCounter = 0;

    // none = 0, rising = 1, falling = 2 & both = 3 (must match F7GPIOManager_interrupts.cs
    // in WireInterrupt()
    gpioMapTblPtr->GpioInterruptMode = (cfg->risingEdge & 0x01) | (cfg->fallingEdge & 0x01) << 1;

    // cfgset contains 20-bits of data. It is required by the Nuttx stm32_gpiosetevent
    // function. If the 20 bits of data are not correct, this Nuttx function will
    // reconfigure the GPIO based on whatever the data is cfgset.
    // See stm32_gpio.h for more information.
    // Inputs: MMUU .... ...X PPPP BBBB
    // MM = Mode for input (this is 00)
    // UU = pull up, pull down or float
    // X  = is external interrupt selection, stm32_gpiosetevent sets this
    uint32_t cfgset = (designator & 0x000000ff);   // Set Port and Pin and clear MM

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

#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
    syslog(LOG_INFO, "upd-(cfg)- 0x%02x (P%c%d)-Cfg Enabled-LKS:%d, GLDuration:%d, DBDuration:%d, InterruptMode:%d, cfgset:0x%08x\n",
              gpioMapTblPtr->PinId,
              ((gpioMapTblPtr->PinId) >> 4) + 'A', gpioMapTblPtr->PinId & 0x0f,
              gpioMapTblPtr->LastKnownGpioState,
              gpioMapTblPtr->GlitchRequestedDuration,
              gpioMapTblPtr->DebounceRequestedDuration,
              gpioMapTblPtr->GpioInterruptMode,
              cfgset);
#endif

    // Tell Nuttx about interrupt parameters
    if(gpioMapTblPtr->GlitchRequestedDuration > 0)
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
      upd_gpio_interrupt,   // function to call
      gpioMapTblPtr);       // table entry pointer

      // If not already configured
      if(gpioMapTblPtr->CurrentProcessState == updipstate_uncfg)
          numbGpioBeingTimed++;
      gpioMapTblPtr->CurrentProcessState = updipstate_wait_gpio_isr;
    }
    else if(gpioMapTblPtr->DebounceRequestedDuration > 0)
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
      upd_gpio_interrupt,   // function to call
      gpioMapTblPtr);       // table entry pointer

      // If not already configured
      if(gpioMapTblPtr->CurrentProcessState == updipstate_uncfg)
          numbGpioBeingTimed++;
      gpioMapTblPtr->CurrentProcessState = updipstate_wait_gpio_isr;
    }
    else
    {
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
      syslog(LOG_INFO, "udp-(cfg)-0x%02x (P%c%d)--Config NO delay Interrupt\n", gpioMapTblPtr->PinId,
                  ((gpioMapTblPtr->PinId) >> 4) + 'A', gpioMapTblPtr->PinId & 0x0f);
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
      upd_gpio_interrupt_no_delay,  // different function to call
      gpioMapTblPtr);       // table entry pointer

      gpioMapTblPtr->CurrentProcessState = updipstate_mon_no_delay;
    }

    return ret;
  }

  // Disable a GPIO interrupt
#if MEADOW_UPD_INCLUDE_DIAGNOSTIC_SYSLOG > 0
syslog(LOG_INFO, "udp-(cfg)-0x%02x (P%c%d)--Disabling GPIO\n", gpioMapTblPtr->PinId,
            ((gpioMapTblPtr->PinId) >> 4) + 'A', gpioMapTblPtr->PinId & 0x0f);
#endif

  // Small chance but it might be active
  upd_remove_gpio_to_timed_list(gpioMapTblPtr);

  // Tell Nuttx to forget about this interrupt
  ret = stm32_gpiosetevent(
      gpioMapTblPtr->PinId,
      0, 0, 0, NULL, NULL);

  if(gpioMapTblPtr->CurrentProcessState != updipstate_mon_no_delay)
    numbGpioBeingTimed--;   // Keep track only of timed gpios

  gpioMapTblPtr->CurrentProcessState = updipstate_uncfg;

  return ret;
}
