  /****************************************************************************
 * \include\meadow\hcom_bbreg_defn.h
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
 ****************************************************************************/
#ifndef __INCLUDE_MEADOW_HCOM_BBREG_DEFN__H
#define __INCLUDE_MEADOW_HCOM_BBREG_DEFN__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

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

// Define our Battery Backed Registers. There are 32 (0-31) in the stm32f7.
// STM32_RTC_BKnnR is defined in chip/stm32_rtcc.h.
//
// WARNING: Don't use CONFIG_STM32F7_RTC_MAGIC_REG (default is STM32_RTC_BK0R).
// It is used by Nuttx in nuttx/arch/arm/src/stm32f7/stm32_rtc.c. Where it is
// used to indicate that RTC is initialized. Search file for 'RTC_MAGIC_REG' or
// 'STM32_RTC_BKR(CONFIG_STM32F7_RTC_MAGIC_REG)' in
// /arch/arm/src/stm32f7/stm32_rtc.h
//
// This register stores the following bit fields. Most are so user
// preferences can survive a restart.
#define HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER  (STM32_RTC_BK31R)

#define HCOM_NX_MEADOW_RESET_REASON_BBR         (STM32_RTC_BK30R)

#define HCOM_BBREG_RESTART_ALL_32_BITS_MASK 0xffffffff
// This mask defines the syslog level
#define HCOM_BBREG_RESTART_SYSLOG_CONFIG_VALUE_MASK 0x000000ff
// This bit indicates if the restart was initiated by hcom command
// AKA HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG
#define HCOM_BBREG_RESTART_INITIATED_BY_HOST_CMD_BIT 0x000100
// This bit indicates if we are to send trace messages to the host PC
#define HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT 0x00000200
// This bit indicates if we are to send trace messages to the uart1
#define HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT 0x00000400
// Used to display host and uart1 routing at startup
#define HCOM_BBREG_TRACE_MSG_TO_HOST_AND_UART1_BIT_MASK (HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT | HCOM_BBREG_ROUTE_TRACE_MSG_TO_UART1_BIT)
// This bit indicates if mono should be started during startup
#define HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT 0x00000800
// The last time mono was started did it run?
#define HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT 0x00001000
// This bit indicates if the debugging server should run after restart
#define HCOM_BBREG_MONO_DEBUGGING_START_BIT 0x00002000
// This bit indicates if we are to send profiler output binaries (.mlpd) to the uart1
#define HCOM_BBREG_ROUTE_PROFILER_BINS_TO_UART1_BIT 0x00004000

#endif  //__INCLUDE_MEADOW_HCOM_BBREG_DEFN__H
