/************************************************************************************
 * configs/stm32f429i-disco/src/stm32_extmem.c
 *
 *   Copyright (C) 2013 Ken Pettit. All rights reserved.
 *   Author: Ken Pettit <pettitkd@gmail.com>
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
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include "chip.h"
#include "up_arch.h"

#include "stm32_fmc.h"
#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"

#include <arch/board/board.h>

// Diagnostic only
// #define USE_MEADOW_DEBUG_HELPERS
#undef USE_MEADOW_DEBUG_HELPERS
//#include <meadow/meadow_debug_helpers.h>

#define DEBUG_PIN_V2_D06  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN13)
#define DEBUG_PIN_V2_D07  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN7)
#define DEBUG_PIN_V2_D08  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN6)
#define DEBUG_PIN_V2_D09  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)
#define DEBUG_PIN_V2_D10  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#ifndef CONFIG_STM32F7_FMC
#  warning "FMC is not enabled"
#endif

#if STM32F7_NGPIO < 6
#  error "Required GPIO ports not enabled"
#endif

#define STM32_FMC_NADDRCONFIGS 23
#define STM32_FMC_NDATACONFIGS 16

#define STM32_SDRAM_CLKEN     FMC_SDRAM_MODE_CMD_CLK_ENABLE | FMC_SDRAM_CMD_BANK_1

#define STM32_SDRAM_PALL      FMC_SDRAM_MODE_CMD_PALL | FMC_SDRAM_CMD_BANK_1

// FMC_SDRAM_AUTO_REFRESH_SHIFT defines the bits representing the number
// of Auto-refreshs
#define STM32_SDRAM_AUTO_REFRESH   FMC_SDRAM_MODE_CMD_AUTO_REFRESH | FMC_SDRAM_CMD_BANK_1 |\
                                    (3 << FMC_SDRAM_AUTO_REFRESH_SHIFT)

#define STM32_SDRAM_SELF_REFRESH  FMC_SDRAM_MODE_CMD_SELF_REFRESH | FMC_SDRAM_CMD_BANK_1
#define STM32_SDRAM_SELF_REFRESH_2  FMC_SDRAM_MODE_CMD_SELF_REFRESH |\
                                    FMC_SDRAM_CMD_BANK_1 | FMC_SDRAM_CMD_BANK_2

#define STM32_SDRAM_MODEREG   FMC_SDRAM_MODE_CMD_LOAD_MODE | FMC_SDRAM_CMD_BANK_1 |\
                                FMC_SDRAM_MODEREG_BURST_LENGTH_1 | \
                                FMC_SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |\
                                FMC_SDRAM_MODEREG_CAS_LATENCY_3 |\
                                FMC_SDRAM_MODEREG_WRITEBURST_MODE_SINGLE

#define STM32_SDRAM_RTTN_NORMAL FMC_SDRAM_MODE_CMD_SELF_REFRESH |\
                                FMC_SDRAM_CMD_BANK_1

/************************************************************************************
 * Public Data
 ************************************************************************************/

/* GPIO configurations common to most external memories */

static const uint32_t g_addressconfig[STM32_FMC_NADDRCONFIGS] =
{
  GPIO_FMC_A0,  GPIO_FMC_A1 , GPIO_FMC_A2,  GPIO_FMC_A3,  GPIO_FMC_A4 , GPIO_FMC_A5,
  GPIO_FMC_A6,  GPIO_FMC_A7,  GPIO_FMC_A8,  GPIO_FMC_A9,  GPIO_FMC_A10, GPIO_FMC_A11,
  GPIO_FMC_A12,

  GPIO_FMC_SDCKE0_3, GPIO_FMC_SDNE0_3, GPIO_FMC_SDNWE_3, GPIO_FMC_NBL0,
  GPIO_FMC_SDNRAS, GPIO_FMC_NBL1,  GPIO_FMC_BA0,   GPIO_FMC_BA1,
  GPIO_FMC_SDCLK,  GPIO_FMC_SDNCAS
};

static const uint32_t g_dataconfig[STM32_FMC_NDATACONFIGS] =
{
  GPIO_FMC_D0,  GPIO_FMC_D1 , GPIO_FMC_D2,  GPIO_FMC_D3,  GPIO_FMC_D4 , GPIO_FMC_D5,
  GPIO_FMC_D6,  GPIO_FMC_D7,  GPIO_FMC_D8,  GPIO_FMC_D9,  GPIO_FMC_D10, GPIO_FMC_D11,
  GPIO_FMC_D12, GPIO_FMC_D13, GPIO_FMC_D14, GPIO_FMC_D15
};

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: stm32_extmemgpios
 *
 * Description:
 *   Initialize GPIOs for external memory usage
 *
 ************************************************************************************/

static void stm32_extmemgpios(const uint32_t *gpios, int ngpios)
{
  int i;

  /* Configure GPIOs */

  for (i = 0; i < ngpios; i++)
    {
      stm32_configgpio(gpios[i]);
    }
}

/************************************************************************************
 * Name: stm32_sdramcommand
 *
 * Description:
 *   Initialize data line GPIOs for external memory access
 *
 ************************************************************************************/

static void stm32_sdramcommand(uint32_t command)
{
  uint32_t  regval;
  volatile  uint32_t timeout = 0xFFFF;

  regval = getreg32( STM32_FMC_SDSR ) & 0x00000020;
  while ((regval != 0) && timeout-- > 0)
    {
      regval = getreg32( STM32_FMC_SDSR ) & 0x00000020;
    }
  putreg32(command, STM32_FMC_SDCMR);
  timeout = 0xFFFF;
  regval = getreg32( STM32_FMC_SDSR ) & 0x00000020;
  while ((regval != 0) && timeout-- > 0)
    {
      regval = getreg32( STM32_FMC_SDSR ) & 0x00000020;
    }
}

/************************************************************************************
 * Name: stm32_enablefmc
 *
 * Description:
 *  enable clocking to the FMC module
 *
 ************************************************************************************/

void stm32_enablefmc(void)
{
  uint32_t regval;
  volatile int count;

  /* Enable GPIOs as FMC / memory pins */

  stm32_extmemgpios(g_addressconfig, STM32_FMC_NADDRCONFIGS);
  stm32_extmemgpios(g_dataconfig, STM32_FMC_NDATACONFIGS);

  /* Enable AHB clocking to the FMC */

  regval  = getreg32( STM32_RCC_AHB3ENR);
  regval |= RCC_AHB3ENR_FMCEN;
  putreg32(regval, STM32_RCC_AHB3ENR);

  /* Configure and enable the SDRAM bank1
   *
   *   FMC clock = 180MHz/2 = 90MHz
   *   90MHz = 11,11 ns
   *   All timings from the datasheet for Speedgrade -7 (=7ns)
   */

  putreg32(FMC_SDRAM_CR_RPIPE_1 |
           FMC_SDRAM_CR_SDCLK_2X |
           FMC_SDRAM_CR_CASLAT_3 |
           FMC_SDRAM_CR_BANKS_4 |
           FMC_SDRAM_CR_WIDTH_16 |
           FMC_SDRAM_CR_ROWBITS_13 |
           FMC_SDRAM_CR_COLBITS_9,
      STM32_FMC_SDCR1);

  putreg32(FMC_SDRAM_CR_RPIPE_1 |
           FMC_SDRAM_CR_SDCLK_2X |
           FMC_SDRAM_CR_CASLAT_3 |
           FMC_SDRAM_CR_BANKS_4 |
           FMC_SDRAM_CR_WIDTH_16 |
           FMC_SDRAM_CR_ROWBITS_13 |
           FMC_SDRAM_CR_COLBITS_9,
      STM32_FMC_SDCR2);

  putreg32((2 << FMC_SDRAM_TR_TRCD_SHIFT) |  /* tRCD min = 15ns */
           (2 << FMC_SDRAM_TR_TRP_SHIFT) |   /* tRP  min = 15ns */
           (2 << FMC_SDRAM_TR_TWR_SHIFT) |   /* tWR      = 2CLK */
           (7 << FMC_SDRAM_TR_TRC_SHIFT) |   /* tRC  min = 63ns */
           (4 << FMC_SDRAM_TR_TRAS_SHIFT) |  /* tRAS min = 42ns */
           (7 << FMC_SDRAM_TR_TXSR_SHIFT) |  /* tXSR min = 70ns */
           (2 << FMC_SDRAM_TR_TMRD_SHIFT),   /* tMRD     = 2CLK */
      STM32_FMC_SDTR1);

  /* SDRAM Initialization sequence */

  stm32_sdramcommand(STM32_SDRAM_CLKEN);          /* Clock enable command */
  for (count = 0; count < 10000; count++) ;       /* Delay */
  stm32_sdramcommand(STM32_SDRAM_PALL);           /* Precharge ALL command */
  stm32_sdramcommand(STM32_SDRAM_AUTO_REFRESH);   /* Auto refresh command */
  stm32_sdramcommand(STM32_SDRAM_MODEREG);        /* Mode Register program */

  /* Set refresh count
   *
   * FMC_CLK = 90MHz
   * Refresh_Rate = 7.81us
   * Counter = (FMC_CLK * Refresh_Rate) - 20
   */

  putreg32(683 << 1, STM32_FMC_SDRTR);

  /* Disable write protection */

  regval = getreg32(STM32_FMC_SDCR1);
  putreg32(regval & 0xFFFFFDFF, STM32_FMC_SDCR1);
}

/************************************************************************************
 * Name: stm32_disablefmc
 *
 * Description:
 *  enable clocking to the FMC module
 *
 ************************************************************************************/

void stm32_disablefmc(void)
{
  uint32_t regval;

  /* Disable AHB clocking to the FMC */

  regval  = getreg32(STM32_RCC_AHB3ENR);
  regval &= ~RCC_AHB3ENR_FMCEN;
  putreg32(regval, STM32_RCC_AHB3ENR);
}

/************************************************************************************
 * Name: stm32_enter_normal_mode_fmc
 *
 * Description:
 *  puts FMC module into normal mode
 *
 ************************************************************************************/
void stm32_enter_normal_mode_fmc(void)
{
  stm32_sdramcommand(STM32_SDRAM_RTTN_NORMAL);
}

/************************************************************************************
 * Name: stm32_enter_self_refresh_fmc
 *
 * Description:
 *  puts FMC module into self-refresh mode and waits for it to be be not busy.
 *
 ************************************************************************************/
void stm32_enter_self_refresh_fmc(void)
{
  volatile uint32_t timeout = 0xFFFF;
  uint32_t regval;

  stm32_gpiowrite(DEBUG_PIN_V2_D06, false);
  stm32_gpiowrite(DEBUG_PIN_V2_D07, false);
  stm32_gpiowrite(DEBUG_PIN_V2_D08, false);
  stm32_gpiowrite(DEBUG_PIN_V2_D09, false);
  stm32_gpiowrite(DEBUG_PIN_V2_D10, false);

  // syslog(1, "===> Sending self-refresh command to SDRAM\n");

  // ??????????????????? MAYBE NOT NEEDED TBD?????
  // Send the 'Precharge ALL' command. I spent a lot of time looking and I
  // found no mention of needing this before the self-refresh command
  // (PeterM-5Jul22)
  // Per F7 ref man the self-refresh command sends a PALL before the self-
  // refresh command
  // stm32_sdramcommand(STM32_SDRAM_PALL);

  // Self-refresh command
  stm32_sdramcommand(STM32_SDRAM_SELF_REFRESH);

  // Wait till busy flag is cleared
  regval = getreg32(STM32_FMC_SDSR) & 0x00000020;
  while ((regval != 0) && timeout-- > 0)
  {
    regval = getreg32(STM32_FMC_SDSR) & 0x00000020;
  }

  //-----------------------
  regval = getreg32(STM32_FMC_SDSR);
  switch((regval >> 1) & 0x00000003)
  {
    case 0:   // Normal
      stm32_gpiowrite(DEBUG_PIN_V2_D06, true);
    break;
    case 1:   // Self-refresh
      stm32_gpiowrite(DEBUG_PIN_V2_D07, true);
    break;
    case 2:   // Power-down
      stm32_gpiowrite(DEBUG_PIN_V2_D08, true);
    break;
    default: // Illegal
      stm32_gpiowrite(DEBUG_PIN_V2_D09, true);
    break;
  }
  //-----------------------

  // syslog(1, "===> EXIT Sending self-refresh command\n");
  // usleep(20 * 1000);
}

/****************************************************************************************************
 * Name: stm32_check_sdram_status_fmc
 *
 * Description:
 *  check the status for one sdran bank
 * Return:
 *  00: Normal Mode
 *  01: Self-refresh mode
 *  10: Power-down mode
 ****************************************************************************************************/

int stm32_check_sdram_status_fmc(int bank)
{
  uint32_t regval;
  regval = getreg32(STM32_FMC_SDSR);

  if(bank == 1)
  {
    return (regval >> 1) & 0x00000003;
  }

  if(bank == 2)
  {
    return (regval >> 3) & 0x00000003;
  }
  
  return -1;
}
