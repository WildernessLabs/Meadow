 /****************************************************************************
 * configs\stm32f777zit6-meadow\src\hcom_nx\diag\hcom_nx_diag.h
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
 ****************************************************************************
 Diagnostic aids
 ****************************************************************************/

#ifndef __INCLUDE_MEADOW_HCOM_NX_DIAG__H
#define __INCLUDE_MEADOW_HCOM_NX_DIAG__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <arch/board/board.h>
#include "stm32_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if HCOM_COMMON_UTILS_GPIO_A0_MISO_DOUT > 0
//=================================================================
// These for testing only
#define MEADOW_DIAG_GPIO_A0___01_OUTPUT  (GPIO_OUTPUT | GPIO_PORTA | GPIO_PIN4 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_A1___02_OUTPUT  (GPIO_OUTPUT | GPIO_PORTA | GPIO_PIN5 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_A2___03_OUTPUT  (GPIO_OUTPUT | GPIO_PORTA | GPIO_PIN3 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_A3___04_OUTPUT  (GPIO_OUTPUT | GPIO_PORTA | GPIO_PIN7 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_A4___05_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN0 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_A5___06_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN1 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_SCK__07_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN10| GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_MOSI_08_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN5 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_MISO_09_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN11| GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#endif

#if HCOM_COMMON_UTILS_GPIO_D00_D08_DOUT > 0
//=================================================================
// These for testing only
#define MEADOW_DIAG_GPIO_D00__10_OUTPUT  (GPIO_OUTPUT | GPIO_PORTI | GPIO_PIN9 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D01__11_OUTPUT  (GPIO_OUTPUT | GPIO_PORTH | GPIO_PIN13| GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D02__12_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN6 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D03__13_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN8 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D04__14_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN9 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D05__15_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN7 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D06__16_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN0 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D07__17_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN7 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define MEADOW_DIAG_GPIO_D08__18_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN6 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#endif

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//=================================================================
#endif    // __INCLUDE_MEADOW_HCOM_NX_DIAG__H