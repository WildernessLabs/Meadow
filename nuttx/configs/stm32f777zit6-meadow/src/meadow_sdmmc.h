/****************************************************************************************************
 * configs/stm32f777zit6-meadow/src/meadow_sdmmc.h
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
 *   Authors: Gregory Nutt <gnutt@nuttx.org>
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
 ****************************************************************************************************/

#ifndef __CONFIGS_MEADOW_SRC_MEADOW_SDMMC__H
#define __CONFIGS_MEADOW_SRC_MEADOW_SDMMC__H

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include "../include/board.h"    // defines GPIO_SDIO_NC

/****************************************************************************************************
 * SDCard Support
 ****************************************************************************************************/

// NCD because C=Card, D=Detection and N=because the pin is pulled to ground
// (i.e. not) when the SD card is inserted. Better would be 'nCD'.
#define HAVE_MEADOW_NCD   1

#if defined(CONFIG_STM32F7_SDMMC1) || defined(CONFIG_STM32F7_SDMMC2)
# define HAVE_SDIO
#endif

#if defined(CONFIG_DISABLE_MOUNTPOINT) || !defined(CONFIG_MMCSD_SDIO)
#  undef HAVE_SDIO
#endif

#if !defined(HAVE_SDIO) || !defined(GPIO_MEADOW_SDIO_NCD)
#  undef HAVE_MEADOW_NCD
#endif

#ifdef HAVE_MEADOW_NCD
static int stm32_ncd_interrupt(int irq, FAR void *context, void *arg);
#endif

#if defined(CONFIG_STM32F7_SDMMC1)
  #define SDIO_SLOTNO 0  /* Only one slot */
#endif

#if defined(CONFIG_STM32F7_SDMMC2)
  #define SDIO_SLOTNO 1  /* Only one slot */
#endif

#ifdef HAVE_SDIO
#  if defined(CONFIG_STM32F7_SDMMC1)
#    define GPIO_SDMMC1_NCD (GPIO_INPUT|GPIO_FLOAT|GPIO_EXTI | GPIO_PORTC | GPIO_PIN6)
#  endif

#  if defined(CONFIG_NSH_MMCSDSLOTNO) && (CONFIG_NSH_MMCSDSLOTNO != 0)
#    warning "Only one MMC/SD slot, slot 0"
#    define CONFIG_NSH_MMCSDSLOTNO SDIO_SLOTNO
#  endif

#  if defined(CONFIG_NSH_MMCSDMINOR)
#    define SDIO_MINOR CONFIG_NSH_MMCSDMINOR
#  else
#    define SDIO_MINOR 0
#  endif
#endif
  
#endif // __CONFIGS_MEADOW_SRC_MEADOW_SDMMC__H