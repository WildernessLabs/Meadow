/************************************************************************************
 * configs/stm32f777zit6-meadow/src/stm32_extmem.c
 *
 *   Copyright (C) 2017 Geoff Norton. All rights reserved.
 *   Author: Geoff Norton <geoff@wildernesslabs.co>
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

#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"

#include <arch/board/board.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#ifndef CONFIG_STM32F7_FMC
#  warning "FMC is not enabled"
#endif

#define STM32_FMC_NADDRCONFIGS 26
#define STM32_FMC_NDATACONFIGS 16

/*
 * FMC registers base
*/
#define STM32_SDRAM_FMC_BASE	0xA0000140
#define STM32_FMC_SDCR1        (STM32_SDRAM_FMC_BASE+0)
#define STM32_FMC_SDCR2        (STM32_SDRAM_FMC_BASE+4)

#define STM32_FMC_SDTR1        (STM32_SDRAM_FMC_BASE+8)
#define STM32_FMC_SDTR2        (STM32_SDRAM_FMC_BASE+12)

#define STM32_FMC_SDCMR        (STM32_SDRAM_FMC_BASE+16)
#define STM32_FMC_SDRTR        (STM32_SDRAM_FMC_BASE+20)
#define STM32_FMC_SDSR         (STM32_SDRAM_FMC_BASE+24)

/* Control register SDCR */
#define STM32_FMC_SDCR_RPIPE_SHIFT	13	/* RPIPE bit shift */
#define STM32_FMC_SDCR_RBURST_SHIFT	12	/* RBURST bit shift */
#define STM32_FMC_SDCR_SDCLK_SHIFT	10	/* SDRAM clock divisor shift */
#define STM32_FMC_SDCR_WP_SHIFT		9	/* Write protection shift */
#define STM32_FMC_SDCR_CAS_SHIFT	7	/* CAS latency shift */
#define STM32_FMC_SDCR_NB_SHIFT		6	/* Number of banks shift */
#define STM32_FMC_SDCR_MWID_SHIFT	4	/* Memory width shift */
#define STM32_FMC_SDCR_NR_SHIFT		2	/* Number of row address bits shift */
#define STM32_FMC_SDCR_NC_SHIFT		0	/* Number of col address bits shift */

/* Timings register SDTR */
#define STM32_FMC_SDTR_TMRD_SHIFT	0	/* Load mode register to active */
#define STM32_FMC_SDTR_TXSR_SHIFT	4	/* Exit self-refresh time */
#define STM32_FMC_SDTR_TRAS_SHIFT	8	/* Self-refresh time */
#define STM32_FMC_SDTR_TRC_SHIFT	12	/* Row cycle delay */
#define STM32_FMC_SDTR_TWR_SHIFT	16	/* Recovery delay */
#define STM32_FMC_SDTR_TRP_SHIFT	20	/* Row precharge delay */
#define STM32_FMC_SDTR_TRCD_SHIFT	24	/* Row-to-column delay */


#define STM32_FMC_SDCMR_NRFS_SHIFT	5

#define STM32_FMC_SDCMR_MODE_NORMAL		0
#define STM32_FMC_SDCMR_MODE_START_CLOCK	1
#define STM32_FMC_SDCMR_MODE_PRECHARGE		2
#define STM32_FMC_SDCMR_MODE_AUTOREFRESH	3
#define STM32_FMC_SDCMR_MODE_WRITE_MODE		4
#define STM32_FMC_SDCMR_MODE_SELFREFRESH	5
#define STM32_FMC_SDCMR_MODE_POWERDOWN		6

#define STM32_FMC_SDCMR_BANK_1		(1 << 4)
#define STM32_FMC_SDCMR_BANK_2		(1 << 3)

#define STM32_FMC_SDCMR_MODE_REGISTER_SHIFT	9

#define STM32_FMC_SDSR_BUSY			(1 << 5)

#define STM32_FMC_BUSY_WAIT()		do { \
		__asm__ __volatile__ ("dsb" : : : "memory"); \
		while (STM32_FMC_SDSR & STM32_FMC_SDSR_BUSY) \
			; \
	} while (0)

#define STM32F7_CPU_RAM_FREQ_DIV	2

#define STM32_SDRAM_CLKEN 1
#define STM32_SDRAM_PALL 2
#define STM32_SDRAM_REFRESH 3
#define STM32_SDRAM_MODEREG 4

/************************************************************************************
 * Public Data
 ************************************************************************************/

/* GPIO configurations common to most external memories */

static const uint32_t g_addressconfig[STM32_FMC_NADDRCONFIGS] =
{
  GPIO_FMC_A0,  GPIO_FMC_A1 , GPIO_FMC_A2,  GPIO_FMC_A3,  GPIO_FMC_A4 , GPIO_FMC_A5,
  GPIO_FMC_A6,  GPIO_FMC_A7,  GPIO_FMC_A8,  GPIO_FMC_A9,  GPIO_FMC_A10, GPIO_FMC_A11,
  GPIO_FMC_A12, GPIO_FMC_A13, GPIO_FMC_A14, GPIO_FMC_A16,

  GPIO_FMC_SDCKE1, GPIO_FMC_SDNE1, GPIO_FMC_SDNWE, GPIO_FMC_NBL0,
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

  regval  = getreg32(STM32_RCC_AHB3ENR);
  regval |= RCC_AHB3ENR_FMCEN;
  putreg32(regval, STM32_RCC_AHB3ENR);

  putreg32(
		  STM32F7_CPU_RAM_FREQ_DIV << STM32_FMC_SDCR_SDCLK_SHIFT| // frequency divider
		  3 << STM32_FMC_SDCR_CAS_SHIFT				| // cas latency
		  1 << STM32_FMC_SDCR_NB_SHIFT				| // num banks
		  1 << STM32_FMC_SDCR_MWID_SHIFT			| // width
		  2 << STM32_FMC_SDCR_NR_SHIFT				| // num rows
		  0 << STM32_FMC_SDCR_NC_SHIFT				| // num cols
		  0 << STM32_FMC_SDCR_RPIPE_SHIFT			| // hclk cycle delay
		  0 << STM32_FMC_SDCR_RBURST_SHIFT,
	   STM32_FMC_SDCR1);

  /*
  putreg32(
		  16 << STM32_FMC_SDTR_TRCD_SHIFT	|
		  16 << STM32_FMC_SDTR_TRP_SHIFT		| 
		  16 << STM32_FMC_SDTR_TWR_SHIFT		|
		  16 << STM32_FMC_SDTR_TRC_SHIFT		|
		  16 << STM32_FMC_SDTR_TRAS_SHIFT	|
		  16 << STM32_FMC_SDTR_TXSR_SHIFT	|
		  16 << STM32_FMC_SDTR_TMRD_SHIFT,
	   STM32_FMC_SDTR1);
	   */
		  
  /* SDRAM Initialization sequence */

  stm32_sdramcommand(0x11);      /* Clock enable command */
  for (count = 0; count < 10000; count++) ;    /* Delay */
  stm32_sdramcommand(0x12);       /* Precharge ALL command */
  stm32_sdramcommand(0xf3);    /* Auto refresh command */
  stm32_sdramcommand(0x00044014);    /* Mode Register program */

  /* Set refresh count
   *
   * FMC_CLK = 143Mhz
   * Refresh_Rate = 7.81us
   * Counter = (FMC_CLK * Refresh_Rate) - 20
   */

  putreg32(1116 << 1, STM32_FMC_SDRTR);

  regval = getreg32(STM32_FMC_SDCR1);
  regval &= 0xFFFFFDFF;
  putreg32(regval, STM32_FMC_SDCR1);
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
