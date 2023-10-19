/****************************************************************************
 * nuttx\include\meadow\meadow_syscall_support.h
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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

#ifndef __CONFIG_MEADOW_SRC_MEADOW_SYSCALL_SUPPORT__H
#define __CONFIG_MEADOW_SRC_MEADOW_SYSCALL_SUPPORT__H

#include <nuttx/config.h>

#include <sys/types.h>

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/


/************************************************************************************
 * Public Functions
 ************************************************************************************/

// Meadow ADC
int meadow_adc_configure(uint8_t gpioList[], uint32_t gpioCount, double *userDataBuf);
int meadow_adc_read_values(void);
int meadow_adc_read_temp_vbat(double *batteryVoltage, double *temperatureValue);

#endif // __CONFIG_MEADOW_SRC_MEADOW_SYSCALL_SUPPORT__H