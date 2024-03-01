/****************************************************************************
 * timing.h
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

#include <dwt.h>
#include <nvic.h>
#include <etm.h>

// Also see https://cwiki.apache.org/confluence/display/NUTTX/Critical+Section+Monitor
// at label 'Simple ARMv7-M Platform-Specific Timers'

// volatile uint32_t *DWT_CONTROL = (uint32_t *) 0xE0001000;
// volatile uint32_t *DWT_CYCCNT = (uint32_t *) 0xE0001004;
// volatile uint32_t *DEMCR = (uint32_t *) 0xE000EDFC;
// volatile uint32_t *LAR  = (uint32_t *) 0xE0001FB0;   // <-- added lock access register
// 

// *DEMCR = *DEMCR | 0x01000000;     // enable trace
// *NVIC_DEMCR = *NVIC_DEMCR | NVIC_DEMCR_TRCENA;


// *LAR = 0xC5ACCE55;                // <-- added unlock access to DWT (ITM, etc.)registers 
// *ETM_ETMLAR = 0xC5ACCE55;


// *DWT_CYCCNT = 0;                  // clear DWT cycle counter

// *DWT_CONTROL = *DWT_CONTROL | 1;  // enable DWT cycle counter
// *DWT_CONTROL = *DWT_CONTROL | DWT_CTRL_CYCCNTENA_MASK;

// *DWT_CONTROL = *DWT_CONTROL & ~DWT_CTRL_CYCCNTENA_MASK;
