/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\quick_misc_tests.c
 * 
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

#include "../hcom_nx/hcom_nx_common.h"

#include <meadow/hcom_shared_common.h>
// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

// Only build if configured
#if defined(CONFIG_QUICK_MISC_TESTS)

// Optionally build only desired test code
#define QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP 0
#define QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS 0

#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0 || \
    QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
#define QUICK_MISC_TESTS_AT_LEAST_ONE 1
#else
#define QUICK_MISC_TESTS_AT_LEAST_ONE 0
#endif

#if QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
#define QUICK_MISC_PIN_V2_A00_DAC_1  (GPIO_ANALOG | GPIO_FLOAT | GPIO_PORTA | GPIO_PIN4)
#define QUICK_MISC_PIN_V2_A01_DAC_2  (GPIO_ANALOG | GPIO_FLOAT | GPIO_PORTA | GPIO_PIN5)
#endif

#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0
#include "stm32_gpio.h"   // stm32_configgpio
#include "stm32_exti.h"   // STM32_EXTI_PR
#include "stm32_rcc.h"    // stm32_clockenable
#include "up_arch.h"      // putreg32
#include "nvic.h"         // NVIC access
#endif

#if QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
#include "up_arch.h"            // putreg32
#include "stm32_gpio.h"         // stm32_configgpio
#include "chip/stm32f76xx77xx_memorymap.h"

#include <stdio.h>
#include <math.h>
#define CONFIG_QUICK_MISC_TESTS_PI 3.14159265
#define CONFIG_QUICK_MISC_TESTS_N 128

// There seems to be very poor DAC support for STM32f7
// I pieced together the following. Basically, modified data from different
// header files.
// The following header needs this defined
#define STM32_DAC_BASE      0x40007400
// I could not find where STM32_NDAC was defined.
// Didn't work. Never mind I'll hard code.
#define STM32_NDAC 0
#include "../../../../arch/arm/src/stm32/chip/stm32_dac.h"

// Copied and modified from chip/stm32_dac.h since 
#  define STM32_DAC_CR           (STM32_DAC_BASE+STM32_DAC_CR_OFFSET)
#  define STM32_DAC_SWTRIGR      (STM32_DAC_BASE+STM32_DAC_SWTRIGR_OFFSET)
#  define STM32_DAC_DHR12R1      (STM32_DAC_BASE+STM32_DAC_DHR12R1_OFFSET)
#  define STM32_DAC_DHR12L1      (STM32_DAC_BASE+STM32_DAC_DHR12L1_OFFSET)
#  define STM32_DAC_DHR8R1       (STM32_DAC_BASE+STM32_DAC_DHR8R1_OFFSET)
#  define STM32_DAC_DHR12R2      (STM32_DAC_BASE+STM32_DAC_DHR12R2_OFFSET)
#  define STM32_DAC_DHR12L2      (STM32_DAC_BASE+STM32_DAC_DHR12L2_OFFSET)
#  define STM32_DAC_DHR8R2       (STM32_DAC_BASE+STM32_DAC_DHR8R2_OFFSET)
#  define STM32_DAC_DHR12RD      (STM32_DAC_BASE+STM32_DAC_DHR12RD_OFFSET)
#  define STM32_DAC_DHR12LD      (STM32_DAC_BASE+STM32_DAC_DHR12LD_OFFSET)
#  define STM32_DAC_DHR8RD       (STM32_DAC_BASE+STM32_DAC_DHR8RD_OFFSET)
#  define STM32_DAC_DOR1         (STM32_DAC_BASE+STM32_DAC_DOR1_OFFSET)
#  define STM32_DAC_DOR2         (STM32_DAC_BASE+STM32_DAC_DOR2_OFFSET)
#  define STM32_DAC_SR           (STM32_DAC_BASE+STM32_DAC_SR_OFFSET)

// Half-wave
// static const uint16_t sin_table[128] = {
// //1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234,
//   0,    63,   127,  191,  255,  319,  383,  447,  511,  575,  639,  703,  767,  831,  895,  959,
//   1023, 1087, 1151, 1215, 1279, 1343, 1407, 1471, 1535, 1599, 1663, 1727, 1791, 1855, 1919, 1983,
//   2047, 2111, 2175, 2239, 2303, 2367, 2431, 2495, 2559, 2623, 2687, 2751, 2815, 2879, 2943, 3007,
//   3071, 3135, 3199, 3263, 3327, 3391, 3455, 3519, 3583, 3647, 3711, 3775, 3839, 3903, 3967, 4031,
//   4095
// };
// static const uint16_t sin_table2[128] = {
//   0, 63, 127, 191, 255, 319, 383, 447, 511, 575, 639, 703, 767, 831, 895, 959, 1023, 1087, 1151, 1215, 1279, 1343, 1407, 1471, 1535, 1599, 1663, 1727, 1791, 1855, 1919, 1983, 2047, 2111, 2175, 2239, 2303, 2367, 2431, 2495, 2559, 2623, 2687, 2751, 2815, 2879, 2943, 3007, 3071, 3135, 3199, 3263, 3327, 3391, 3455, 3519, 3583, 3647, 3711, 3775, 3839, 3903, 3967, 4031, 4095
// };

//static const uint16_t sin_table[128] = {
  // 0, 63, 127, 191, 255, 319, 383, 447, 511, 575, 639, 703, 767, 831, 895, 959, 1023, 1087, 1151, 1215, 1279, 1343, 1407, 1471, 1535, 1599, 1663, 1727, 1791, 1855, 1919, 1983, 2047, 2111, 2175, 2239, 2303, 2367, 2431, 2495, 2559, 2623, 2687, 2751, 2815, 2879, 2943, 3007, 3071, 3135, 3199, 3263, 3327, 3391, 3455, 3519, 3583, 3647, 3711, 3775, 3839, 3903, 3967, 4031, 4095
// };
//   0,    6,    12,   18,   24,   31,   37,   43,   49,   55,   61,   68,   74,   79,   85,   91,
//   97,   103,  109,  114,  120,  125,  131,  136,  141,  146,  151,  156,  161,  166,  171,  175,
//   180,  184,  188,  193,  197,  201,  204,  208,  212,  215,  218,  221,  224,  227,  230,  233,
//   235,  237,  240,  242,  244,  245,  247,  248,  250,  251,  252,  253,  253,  254,  254,  254,
//   255, 254, 254, 254, 253, 253, 252, 251, 250, 248, 247, 245, 244, 242, 240, 237, 235, 233, 230, 227, 224, 221, 218, 215, 212, 208, 204, 201, 197, 193, 188, 184, 180, 175, 171, 166, 161, 156, 151, 146, 141, 136, 131, 125, 120, 114, 109, 103, 97, 91, 85, 79, 74, 68, 61, 55, 49, 43, 37, 31, 24, 18, 12, 6
// };
// 12-bit sin wave loopup table with 128 elements
// uint32_t SinLookupTable[] =
// {
//   2048, 2149, 2250, 2350, 2450, 2549, 2646, 2742, 2837, 2929, 3020, 3108, 3193, 3275, 3355, 3431,
//   3504, 3574, 3639, 3701, 3759, 3812, 3861, 3906, 3946, 3982, 4013, 4039, 4060, 4076, 4087, 4094,
//   4095, 4091, 4082, 4069, 4050, 4026, 3998, 3965, 3927, 3884, 3837, 3786, 3730, 3671, 3607, 3539,
//   3468, 3394, 3316, 3235, 3151, 3064, 2975, 2883, 2790, 2695, 2598, 2500, 2400, 2300, 2199, 2098,
//   1997, 1896, 1795, 1695, 1595, 1497, 1400, 1305, 1212, 1120, 1031, 944,  860,  779,  701,  627,
//   556,  488,  424,  365,  309,  258,  211,  168,  130,  97,   69,   45,   26,   13,   4,    0,
//   1,    8,    19,   35,   56,   82,   113,  149,  189,  234,  283,  336,  94,   456,  521,  591,
//   664,  740,  820,  902,  987,  1075, 1166, 1258, 1353, 1449, 1546, 1645, 1745, 1845, 1946, 2047
// };
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0
// D05 (PB4) for Version 2 Feather or CCM V1
// For Testing wanted a pin that was Px0-4 to more easily figure out interrupts
// and because these are a high priority interrupts.
#define QUICK_MISC_PIN_V2_D05  (GPIO_INPUT | GPIO_PULLDOWN | GPIO_PORTB | GPIO_PIN4)

// It uses STM32_IRQ_EXTI4 (26) [position in Table 46 is 10 + 16 = 26]
// NVIC_IRQ0_31_PEND_OFFSET (IRQ 0 - 31 pending alarm register address is
// 0xe000e200).
// NVIC_IRQ0_31_CLRPEND_OFFSET (for IRQ 0 - 31 this is the offset to clear
// pending in the NVIC). And its register address is 0xe000e280. And the
// NVIC interrupt clear pending bit is 0x04000000 (bit 26)

// For any Px4 GPIO
// Find values in these locations
// nuttx/arch/arm/include/stm32f7/stm32f76xx77xx_irq.h
// nuttx/arch/arm/src/armv7-m/nvic.h
// nuttx/arch/arm/include/stm32f7/irq.h
#define QUICK_MISC_TEST_GPIO_INPUT_NVIC_BIT (1 << STM32_IRQ_EXTI4)
#endif

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0
static void quick_misc_test_setup_interrupt_for_wakeup(void);
#endif

#if QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
static void quick_misc_test_initialize_dac_1(void);
static void outputSineTable(void);
#endif


/************************************************************************************
 * Public Functions
 ************************************************************************************/
void meadow_kt_quick_misc_tests(uint32_t userData)
{
#if QUICK_MISC_TESTS_AT_LEAST_ONE > 0
  static bool onlyOnce = true;
  syslog(1, "Received 'set developer -d 11 -v %lu'\n", userData);
#endif

  switch(userData)
  {
#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0
    case 1:
      if(onlyOnce)
      {
        onlyOnce = false;
        quick_misc_test_setup_interrupt_for_wakeup();
      }
      else
      {
        syslog(1, "Only once\n");
      }
      break;
    
    case 2:
      DEBUG_SET_HIGH(DEBUG_PIN_V2_D14);
      pwrmgmt_enter_stm32f7_stop_mode(10);    // Sleep 10 seconds
      DEBUG_SET_LOW(DEBUG_PIN_V2_D14);
      break;
#endif

#if QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
    case 3:   // Configure for DAC-1 to function
      if(onlyOnce)
      {
        onlyOnce = false;
        quick_misc_test_initialize_dac_1();
      }
      else
      {
        syslog(1, "Only once\n");
      }
    break;

    case 6:   // Configure for DAC-1 to function
    outputSineTable();
    break;
#endif

    default:
      syslog(1, "Undefined test for meadow_kt_quick_misc_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/
#if QUICK_MISC_TESTS_GPIO_DAC_EXPERIMENTS > 0
//-----------------------------------------------------------
static void outputSineTable()
{
  int i;
  uint16_t sinValue;

  for (i = 0; i < CONFIG_QUICK_MISC_TESTS_N; i++) {
      sinValue = (uint16_t)(2047 * sin(2 * CONFIG_QUICK_MISC_TESTS_PI * i / CONFIG_QUICK_MISC_TESTS_N) + 2048);
      syslog(1, "%u, ", sinValue);
  }
}

//-----------------------------------------------------------
// DAC tests
static void quick_misc_test_initialize_dac_1(void)
{
  int ret;

  // Per Ref Man 16.2 end Note - Set PA4 to analog input before using for DAC
  // Configure input point
  ret = stm32_configgpio(QUICK_MISC_PIN_V2_A00_DAC_1);
  if(ret < 0)
  {
    syslog(1, "Error#1 in quick_misc_test_initialize_dac_1. ret:%d errno:%d\n", ret, errno);
  }

  // Enable DAC1
  putreg32(DAC_CR_EN1, STM32_DAC_CR);
  
}
#endif

#if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0
//-----------------------------------------------------------
// GPIO LP Sleep wakeup tests
// Testing ISR
static int quick_misc_test_wakeup_stop_mode_isr(int irq, void *context, void *arg)
{
  DEBUG_SET_HIGH(DEBUG_PIN_V2_D15);
  // syslog(1, "==> ISR Wakeup Misc test code\n");

  // At this point the correct STM32_EXTI_PR bit has been cleared by Nuttx in
  // it's "first layer" ISR.
  // According to the Ref Man we still need to set a bit in the NVIC interrupt
  // clear pending register. However, experiimentation has show that this is
  // not necessary. Perhaps, Nuttx has already taken care of this?

  // Reconfigure the internal clocks. Restarts the clocks as defined in
  // board.h. These clocks are what run the entire MCU.
  stm32_clockenable();

  // Restart Nuttx Systick
  up_enable_irq(STM32_IRQ_SYSTICK);

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Clear the EXTI Pending Register for the RTC Alarm
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Clear the EXTI Pending Register for the Wakeup Timer
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);
#else
  #error "Select Power Management Low-Power scheme"
#endif

  // Don't leave ISR until the above have finished
  asm volatile ("dsb");

  DEBUG_SET_LOW(DEBUG_PIN_V2_D15);
  return OK;
}

// ============================================================================
// This test is used to determine if an interrupt can wakeup the F7 from a
// low-power mode. Specifically stop mode.
static void quick_misc_test_setup_interrupt_for_wakeup(void)
{
  // Setup D00 to generate an interrupt
  int ret;

  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D14);
  DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D15);
  DEBUG_SET_LOW(DEBUG_PIN_V2_D15);

  // Configure input point
  ret = stm32_configgpio(QUICK_MISC_PIN_V2_D05);
  if(ret < 0)
  {
    syslog(1, "Error#1 in quick_misc_test_setup_interrupt_for_wakeup. ret:%d errno:%d\n", ret, errno);
  }

  // Setup for interrupts
  ret = stm32_gpiosetevent(
  QUICK_MISC_PIN_V2_D05,            // Nuttx cfgset
  true,                             // risingEdge,
  false,                            // fallingEdge,
  false,   // quick_misc_test_wakeup_stop_mode_isr,  // event    <-- ADDED THIS 2:11
  quick_misc_test_wakeup_stop_mode_isr,  // ISR 
  NULL);                            // arg for ISR
  if(ret < 0)
  {
    syslog(1, "Error#2 in quick_misc_test_setup_interrupt_for_wakeup. ret:%d errno:%d\n", ret, errno);
  }
}
#endif    // #if QUICK_MISC_TESTS_GPIO_LP_SLEEP_WAKEUP > 0

#endif  // #if defined(CONFIG_QUICK_MISC_TESTS)

