/*************************************************************************
 * \include\meadow\hcom_shared_common.h
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
#ifndef __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
#define __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Private defines
 ****************************************************************************/
// This header file contains those items that must be shared between apps and
// nuttx sides

#ifndef OK
  #define OK 0
#endif

#ifndef MIN
#  define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define HCOM_NX_CMD_HOST_MSG_SIZE 128
#define HCOM_NX_CMD_LOG_MSG_SIZE  128

// Partition Id may postpend an to /meadow (i.e /meadow0)
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"

// Partitioning changes will effect the following
#ifdef CONFIG_MTD_PARTITION
#define MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/meadow0"
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow0/App.exe"
#else
#define MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/meadow"
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow/App.exe"
#endif

//==================================================
// hcom nx upd ioctl commands
// Augments the normal Nuttx LOG_XXXX list
#define LOG_NONE                         0xff

//--------------------------------------------------------------------
// These needed Meadow features can be excluded from a build
#define HCOM_VS_REMOTE_DEBUGGING_INCLUDE_IN_BUILD     1
#define HCOM_STDOUT_STDERR_REDIRECT_INCLUDE_IN_BUILD  1

//--------------------------------------------------------------------
// The following control things needed for diagnostics.
// When set to 1 the syslog mask is set for all tracing but debug
// and at startup syslog messages are routed to UART1 without
// the need to send the Uart1Trace command.
#define HCOM_FORCE_SYSLOG_MASK_F7_AND_UART1           0

// Allow the build to include the ability to print a buffer
// full of data, showing hex and ascii
#define HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE           0

// Allow the build to include code to decode a message about to
// be sent. AT THIS TIME THIS IS NOT FULLY IMPLEMENTED! RAN OUT
// OF TIME BEFORE VACATION.
#define HCOM_INCLUDE_DIAG_DECODE_MESSAGE_CODE         0

//--------------------------------------------------------------------
// The following controls building of tracing the hex information
// that is associated with some LOG_DEBUG messages throughout
// the code base.
#define HCOM_OUTPUT_DATA_BUFFER_INFO_VIA_SYSLOG       0
// NuttShell can be launched from CLI but currently it doesn't
// work because UART4 is reconfigured when mono starts running
// Requires nsh to be defined
#define HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD     0
// ESP32 can send cr/lf repeatedly very fast this causes this
// to be thrown away
#define HCOM_ESP32_PROCESS_CR_LF_ENDLESS_TEXT         0
// The F7's GPIOs can be used for diagnostics. Especially useful
// when debugging within the syslog code
#define HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE    0
// UART1 & UART 4 are sometimes used for diagnostic
// purposes. This define prevents these from being used
// by diagnostic code
#define HCOM_DIAG_GPIO_DIAGNOSTIC_PERSERVE_UARTS      0
 
//--------------------------------------------------------------------
// Test code
#define HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD      0 
#define HCOM_INCLUDE_BATTERY_BACKED_REG_TEST          0

//--------------------------------------------------------------------
// The code not compiled by this #define should be removed
// and removed from CLI at the same time
#define HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS  0

//---------------------------------------------------------------------
// Because it is difficult to discover the GPIO definition
// on the /apps side these provide a mapping between the
// GPIO definition and a numeric value that can easily be
// used on both nuttx and apps sides.
// The following can be used by hcom and hcom_nx.
// Note:In hcom_nx_upd.c the numeric values define the order
// these appear in an array (they are used as offsets).
#define HCOM_GPIO_DIG_NX_ID_ESP_RESET  0
#define HCOM_GPIO_DIG_NX_ID_ESP_BOOT   1
#define HCOM_GPIO_DIG_NX_ID_BLUE_LED   2

// Simplify naming of meadow GPIOs for diagnostics
#define HCOM_DIAG_GPIO_A0     0
#define HCOM_DIAG_GPIO_A1     1
#define HCOM_DIAG_GPIO_A2     2
#define HCOM_DIAG_GPIO_A3     3
#define HCOM_DIAG_GPIO_A4     4
#define HCOM_DIAG_GPIO_A5     5
#define HCOM_DIAG_GPIO_SCK    6
#define HCOM_DIAG_GPIO_MOSI   7
#define HCOM_DIAG_GPIO_MISO   8
#define HCOM_DIAG_GPIO_D00    9
#define HCOM_DIAG_GPIO_D01   10
#define HCOM_DIAG_GPIO_D02   11
#define HCOM_DIAG_GPIO_D03   12
#define HCOM_DIAG_GPIO_D04   13
#define HCOM_DIAG_GPIO_D05   14
#define HCOM_DIAG_GPIO_D06   15
#define HCOM_DIAG_GPIO_D07   16
#define HCOM_DIAG_GPIO_D08   17
#define HCOM_DIAG_GPIO_D09   18
#define HCOM_DIAG_GPIO_D10   19
#define HCOM_DIAG_GPIO_D11   20
#define HCOM_DIAG_GPIO_D12   21
#define HCOM_DIAG_GPIO_D13   22
#define HCOM_DIAG_GPIO_D14   23
#define HCOM_DIAG_GPIO_D15   24

#endif  // __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
