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

#define FMC_NADDRCONFIGS 26
#define FMC_NDATACONFIGS 16

/*
 * FMC registers base
*/
#define SDRAM_FMC_BASE	0xA0000140
#define FMC_SDCR1	(SDRAM_FMC_BASE+0)
#define FMC_SDCR2	(SDRAM_FMC_BASE+4)

#define FMC_SDTR1	(SDRAM_FMC_BASE+8)
#define FMC_SDTR2	(SDRAM_FMC_BASE+12)

#define FMC_SDCMR	(SDRAM_FMC_BASE+16)
#define FMC_SDRTR	(SDRAM_FMC_BASE+20)
#define FMC_SDSR	(SDRAM_FMC_BASE+24)

#define FMC_SDSR_BUSY	(1 << 5)
#define FMC_BANK1	(1 << 4)

#define TMRD(x) (x << 0)  /* Load Mode Register to Active */
#define TXSR(x) (x << 4)  /* Exit Self-refresh delay */
#define TRAS(x) (x << 8)  /* Self refresh time */
#define TRC(x)  (x << 12) /* Row cycle delay */
#define TWR(x)  (x << 16) /* Recovery delay */
#define TRP(x)  (x << 20) /* Row precharge delay */
#define TRCD(x) (x << 24) /* Row to column delay */

#define FMC_BUSY_WAIT()		do { \
		__asm__ __volatile__ ("dsb" : : : "memory"); \
		while (FMC_SDSR & FMC_SDSR_BUSY) \
			; \
	} while (0)

/************************************************************************************
 * Public Data
 ************************************************************************************/

/* GPIO configurations common to most external memories */

static const uint32_t g_addressconfig[FMC_NADDRCONFIGS] =
{
  GPIO_FMC_A0,  GPIO_FMC_A1 , GPIO_FMC_A2,  GPIO_FMC_A3,  GPIO_FMC_A4 , GPIO_FMC_A5,
  GPIO_FMC_A6,  GPIO_FMC_A7,  GPIO_FMC_A8,  GPIO_FMC_A9,  GPIO_FMC_A10, GPIO_FMC_A11,
  GPIO_FMC_A12, GPIO_FMC_A13, GPIO_FMC_A14, GPIO_FMC_A15,

  GPIO_FMC_SDCKE0_2, GPIO_FMC_SDNE0_2, GPIO_FMC_SDNWE_2, GPIO_FMC_SDCLK,
  GPIO_FMC_NBL0, GPIO_FMC_NBL1, GPIO_FMC_BA0, GPIO_FMC_BA1,
  GPIO_FMC_SDNRAS, GPIO_FMC_SDNCAS //(GPIO_ALT|GPIO_AF12|GPIO_SPEED_100MHz|GPIO_PORTB|GPIO_PIN7), /* SDNCAS is on PB7, but we should have it on G15 */ 
};

static const uint32_t g_dataconfig[FMC_NDATACONFIGS] =
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

  stm32_extmemgpios(g_addressconfig, FMC_NADDRCONFIGS);
  stm32_extmemgpios(g_dataconfig, FMC_NDATACONFIGS);

  /* Enable AHB clocking to the FMC */

  regval  = getreg32(STM32_RCC_AHB3ENR);
  regval |= RCC_AHB3ENR_FMCEN;
  putreg32(regval, STM32_RCC_AHB3ENR);

  putreg32(/* FMC_SDCR1_SDCLK_1 */ 0x00000800 |
	   /* FMC_SDCR1_RBURST */  0x00001000 |
	   /* FMC_SDCR1_RPIPE_1 */ 0x00004000 |
	   /* FMC_SDCR1_NR_0 */    0x00000004 |
	   /* FMC_SDCR1_MWID_0 */  0x00000010 |
	   /* FMC_SDCR1_NB */      0x00000040 |
	   /* FMC_SDCR1_CAS */     0x00000180,
	   FMC_SDCR1);

  /* FMC_SDCR1_CAS sets the CAS latency to 3
   * TRCD: 3
   * TRP: 3
   * TWR: 3 (unknown but >= TRCD)
   * TRC: 9
   * TRAS: 6
   * TXSR: 10 
   * TMRD: 2 
   */
  putreg32(TRCD(3) |
           TRP(3) |
	   TWR(3) |
	   TRC(9) |
	   TRAS(6) |
	   TXSR(10) |
	   TMRD(2),
	   FMC_SDTR1);

  FMC_BUSY_WAIT();
  putreg32(1 | FMC_BANK1 | (1 << 5), FMC_SDCMR);
  for (count = 0; count < 10000; count++) ;    /* Delay */
  FMC_BUSY_WAIT();
  putreg32(2 | FMC_BANK1 | (1 << 5), FMC_SDCMR);
  FMC_BUSY_WAIT();
  putreg32(3 | FMC_BANK1 | (4 << 5), FMC_SDCMR);
  FMC_BUSY_WAIT();
  putreg32(4 | FMC_BANK1 | (1 << 5) | (0x231 << 9), FMC_SDCMR);
  FMC_BUSY_WAIT();

  putreg32((41 << 1), FMC_SDRTR);
  FMC_BUSY_WAIT();

  {
	void *base = (void *)0xc0000000;
	uint32_t size = 0x800000;
	void *cur = base;

	while (cur < base+size) {
		*(uint32_t*)cur = 0xdddddddd;
		*(uint32_t*)(0x20000000) = cur;
		if (*(uint32_t*)cur != 0x0)
			while(1);
		cur += 4;
	}

	cur = base;
	while (cur < base+size) {
		*(uint32_t*)(0x20001000) = cur;
		if (*(uint32_t*)cur != 0x0)
			while(1);
		cur += 4;
	}
  }
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
