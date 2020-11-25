 /****************************************************************************
 * \nuttx\include\meadow\hcom_gpio_defn_diag.h
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

#ifndef __INCLUDE_MEADOW_HCOM_GPIO_DEFN_DIAG__H
#define __INCLUDE_MEADOW_HCOM_GPIO_DEFN_DIAG__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
//=================================================================
// These for testing only
#endif

// Offsets of above, used on the /apps side when using hcom_nx_udp
#define HCOM_NX_DIAG_GPIO_A0    0
#define HCOM_NX_DIAG_GPIO_A1    1
#define HCOM_NX_DIAG_GPIO_A2    2
#define HCOM_NX_DIAG_GPIO_A3    3
#define HCOM_NX_DIAG_GPIO_A4    4
#define HCOM_NX_DIAG_GPIO_A5    5
#define HCOM_NX_DIAG_GPIO_SCK   6
#define HCOM_NX_DIAG_GPIO_MOSI  7
#define HCOM_NX_DIAG_GPIO_MISO  8
#define HCOM_NX_DIAG_GPIO_D00   9
#define HCOM_NX_DIAG_GPIO_D01   10
#define HCOM_NX_DIAG_GPIO_D02   11
#define HCOM_NX_DIAG_GPIO_D03   12
#define HCOM_NX_DIAG_GPIO_D04   13
#define HCOM_NX_DIAG_GPIO_D05   14
#define HCOM_NX_DIAG_GPIO_D06   15
#define HCOM_NX_DIAG_GPIO_D07   16
#define HCOM_NX_DIAG_GPIO_D08   17
#define HCOM_NX_DIAG_GPIO_D09   18
#define HCOM_NX_DIAG_GPIO_D10   19
#define HCOM_NX_DIAG_GPIO_D11   20
#define HCOM_NX_DIAG_GPIO_D12   21
#define HCOM_NX_DIAG_GPIO_D13   22
#define HCOM_NX_DIAG_GPIO_D14   23
#define HCOM_NX_DIAG_GPIO_D15   24

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
#endif    // __INCLUDE_MEADOW_HCOM_GPIO_DEFN_DIAG__H