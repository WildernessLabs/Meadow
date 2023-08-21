/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\dig_to_ana_conv_tests.c
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

#include <nuttx/config.h>
#include <sys/types.h>
// nuttx/arch/arm/src/common/up_arch.h
#include <nuttx/arch.h>   // up_enable_irq
#include "up_arch.h"      // getreg32 & putreg32
#include <arch/stm32f7/chip.h>
#include "stm32_gpio.h"
#include <meadow/hcom_shared_common.h>

// #include "chip.h"
// #include "stm32_rcc.h"
// #include "stm32_tim.h"
// #include "stm32_dma.h"
// #include "stm32_adc.h"

// FOR TESTING BBR
#include "chip/stm32_rtcc.h"

#if defined (CONFIG_DAC_TESTS)
#warning "(--) Hacking dac tests.c"

// Diagnostic always as this is test code
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#pragma GCC optimize "Og"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
#include <stdio.h>
#include <math.h>
#include "chip/stm32f76xx77xx_memorymap.h"

#define QUICK_MISC_PIN_V2_A00_DAC_1  (GPIO_ANALOG | GPIO_FLOAT | GPIO_PORTA | GPIO_PIN4)
#define QUICK_MISC_PIN_V2_A01_DAC_2  (GPIO_ANALOG | GPIO_FLOAT | GPIO_PORTA | GPIO_PIN5)

#define CONFIG_DAC_TESTS_FIXED_PI 3.14159265
#define CONFIG_DAC_TESTS_FIXED_N 128

// There seems to be very poor DAC support for STM32f7 in Nuttx
// I pieced together the following. Basically, modified data from header files
// The following header needs this defined
#define STM32_DAC_BASE      0x40007400
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

/************************************************************************************
 * Private Data
 ************************************************************************************/

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

// 8-bit
// //1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234, 1234,
//   0,    6,    12,   18,   24,   31,   37,   43,   49,   55,   61,   68,   74,   79,   85,   91,
//   97,   103,  109,  114,  120,  125,  131,  136,  141,  146,  151,  156,  161,  166,  171,  175,
//   180,  184,  188,  193,  197,  201,  204,  208,  212,  215,  218,  221,  224,  227,  230,  233,
//   235,  237,  240,  242,  244,  245,  247,  248,  250,  251,  252,  253,  253,  254,  254,  254,
//   255,  254,  254,  254,  253,  253,  252,  251,  250,  248,  247,  245,  244,  242,  240,  237,
//   233,  230,  227,  224,  221,  218,  215,  212,  208,  204,  201,  197,  193,  188,  184,  180,
//   175,  171,  166,  161,  156,  151,  146,  141,  136,  131,  125,  120,  114,  109,  103,  97,
//   91,   85,   79,   74,   68,   61,   55,   49,   43,   37,   31,   24,   18,   12,   6,    0
// };

// 12-bit sin wave loopup table with 128 elements looks to be alittle shifted
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

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

static void adc_dac_test_initialize_dac_1(void);
static void getSinTable(void);

/************************************************************************************
 * Public Functions
 ************************************************************************************/
void meadow_kt_dac_tests(uint32_t userData)
{
  static int onlyOnce = false;

  syslog(1, "%s@%d-Entered meadow_kt_adc_tests, userData:%lu\n", __FILE__, __LINE__, userData);

//   DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D01);
//   DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D02);
//   DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D03);
//   DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D04);
  
//   DEBUG_SET_LOW(DEBUG_PIN_V2_D01);
//   DEBUG_SET_LOW(DEBUG_PIN_V2_D02);
//   DEBUG_SET_LOW(DEBUG_PIN_V2_D03);
//   DEBUG_SET_LOW(DEBUG_PIN_V2_D04);

  switch(userData)
  {
    case 1:
      if(onlyOnce)
      {
        syslog(1, "Only once\n");
      }
      else
      {
        onlyOnce = true;
        // Initialize only ADC1 to start with
        adc_dac_test_initialize_dac_1();
      }
      break;

    case 2:
        getSinTable();
      break;

    default:
      syslog(1, "Undefined test for meadow_kt_dac_tests, userData:%lu\n", userData);
      break;
  }
}

/************************************************************************************
 * Private Functions
 ************************************************************************************/

// Just getting started and needed to switch to another project
//-----------------------------------------------------------
static void getSinTable()
{
  int i;
  uint16_t sinValue;
// IF USED NEED '/n' at end
  for (i = 0; i < CONFIG_DAC_TESTS_FIXED_N; i++) {
      sinValue = (uint16_t)(2047 * sin(2 * CONFIG_DAC_TESTS_FIXED_PI * i / CONFIG_DAC_TESTS_FIXED_N) + 2048);
      syslog(1, "%u, ", sinValue);
  }
}

//-----------------------------------------------------------
// DAC tests
static void adc_dac_test_initialize_dac_1(void)
{
  int ret;

  // Per Ref Man 16.2 end Note - Set PA4 to analog input before using for DAC
  // Configure input point
  ret = stm32_configgpio(QUICK_MISC_PIN_V2_A00_DAC_1);
  if(ret < 0)
  {
    syslog(1, "Error#1 in adc_dac_test_initialize_dac_1.\n ret:%d errno:%d\n", ret, errno);
  }

  // Enable DAC1
  putreg32(DAC_CR_EN1, STM32_DAC_CR);
  
}

#endif  // #if defined (CONFIG_DAC_TESTS)
