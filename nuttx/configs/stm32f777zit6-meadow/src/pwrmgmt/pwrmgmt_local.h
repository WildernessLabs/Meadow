/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/pwrmgmt/pwrmgmt_local.h
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#ifndef __INCLUDE_MEADOW_POWER_MGMT_LOCAL__H
#define __INCLUDE_MEADOW_POWER_MGMT_LOCAL__H

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

// Only set this to 1 for testing
#if HCOM_INCLUDE_PWR_MGMT_TESTS_IN_BUILD > 0
#define PWRMGMT_CLK_SHOW_RTC_TIME_FOR_TESTING (1) // 1 oe 0
#else
#define PWRMGMT_CLK_SHOW_RTC_TIME_FOR_TESTING (0) // leave 0
#endif

#define PWRMGMT_CAL_LSI_THREAD_NAME "LSI Calibrate"
#define PWRMGMT_CAL_LSI_THREAD_PRIORITY (120)
#define PWRMGMT_CAL_LSI_THREAD_STACKSIZE  (2048)

// This are defined here because they are not in Nuttx. In Nuttx they are
// hardcoded in stm32_rtc.c
#define PWRMGMT_CLK_HSE_DIV_A_FACTOR_FOR_1_MHZ (124)    // STMicro's AN4759 table 7
#define PWRMGMT_CLK_HSE_DIV_S_FACTOR_FOR_1_MHZ (7999)   // STMicro's AN4759 table 7

// Miscellaneous functions
void pwrmgmt_rtc_dumpregs(FAR const char *msg);
void rtc_wprunlock(void);
void rtc_wprlock(void);
int rtc_enterinit(void);
void rtc_exitinit(void);
int rtc_synchwait(void);
void pwrmgmt_rtc_resume(void);

// Internal to power management
int meadow_pwr_mgmt_enter_stop(bool lowestPwr);
int meadow_pwr_mgmt_enter_sleep(void);
int meadow_pwr_mgmt_enter_standby(void);

int pwrmgmt_init_lsi_calib(void);
int pwrmgmt_init_rtc_clk_switch(void);
uint32_t pwrmgmt_get_lsi_calib_rtc_clk_value(void);

#if PWRMGMT_CLK_SHOW_RTC_TIME_FOR_TESTING > 0
void pwrmgmt_set_dbg_clk_switched_flag(bool dbgClkSwitched);
#endif

#endif  // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#endif // __INCLUDE_MEADOW_POWER_MGMT_LOCAL__H
