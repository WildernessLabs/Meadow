/****************************************************************************
 * meadow_os.h
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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
#ifndef __MEADOW_OS_H
#define __MEADOW_OS_H

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/

/**
 * The following list of reset codes are derived from RM0410 document from
 * STM.  The values are taken from the RCC-CSR register bits.  Specifically,
 * bits 24 - 31 inclusive.
 * 
 * The bit patterns below assume that the CSR register has been shifted right
 * by 24 bits.
 */

/**
 * @brief Set when either a brownout reset or a POR/PDR reset occurs.
 */
#define MEADOW_OS_RESET_BROWNOUT                0x02

/**
 * @brief Reset triggered by the NRST pin.
 */
#define MEADOW_OS_RESET_RESET_PIN               0x04

/**
 * @brief POR/PDR reset.
 */
#define MEADOW_OS_RESET_POWER_CYCLE             0x08

/**
 * @brief Software reset.
 */
#define MEADOW_OS_RESET_SOFTWARE                0x10

/**
 * @brief Independent watchdog reset from the Vdd domain.
 */
#define MEADOW_OS_RESET_INDEPENDENT_WATCHDOG    0x20

/**
 * @brief Windows watchdog reset.
 */
#define MEADOW_OS_RESET_WINDOW_WATCHDOG         0x40

/**
 * @brief Low power management reset.
 */
#define MEADOW_OS_RESET_LOW_POWER               0x80

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//
//  Configuration methods.
//
void meadow_os_config_free_resources(meadow_configuration_t *);
meadow_configuration_t *meadow_os_deep_copy_config(void);
//
//  Power cycle, and reset methods.
//
uint32_t meadow_os_power_cycle_count(void);
uint32_t meadow_os_reset_cycle_count(void);
uint32_t meadow_os_reset_reason(void);
uint32_t meadow_os_hardware_version(void);
int meadow_os_reset_update_counters(void);
//
//  Misc methods.
//
uint32_t meadow_os_native_protocol_version(void);
void meadow_os_raise_simple_exception(uint32_t);
void meadow_os_reset_board(int);
//
//  ESP coprocessor specific methods.
//
void meadow_os_espcp_reset(void);
uint32_t meadow_os_espcp_enter_programming_mode(void);
void meadow_os_espcp_monitor_process_line(char *);
int meadow_os_get_gateway_address(char *);
void meadow_os_coprocessor_deep_sleep(void);
int meadow_os_get_cell_script(char *script);
#endif /* __MEADOW_OS_H */