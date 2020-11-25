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

// Error returned value for functions that return an int
#define MEADOW_ERROR_RETURN_WHEN_INT_EXPECTED (0x80000000)    // Largest possible negative int

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
// Default name of meadow configuration file
#define MEADOW_DEFAULT_CONFIG_FILE_NAME "/meadow0/meadow.cfg"
#define MEADOW_DEFAULT_INI_CFG_BUF_LEN  32

// Errors from configuration file processing
#define MEADOW_CONFIG_ERROR_NO_KEY_FOUND -1
#define MEADOW_CONFIG_ERROR_CFG_FILE_OPEN -2
#define MEADOW_CONFIG_ERROR_PROVIDED_BUF_TOO_SMALL -3
#define MEADOW_CONFIG_ERROR_MEM_ALLOC_ERROR -4
#define MEADOW_CONFIG_ERROR_CFG_LINE_TOO_LONG -5
#define MEADOW_CONFIG_ERROR_CFG_FILE_READ_ERR -6
#define MEADOW_CONFIG_ERROR_NO_KEY_PROVIDED -7

//==================================================
// hcom nx upd ioctl commands
// Augments the normal Nuttx LOG_XXXX list
#define LOG_NONE                         0xff

//--------------------------------------------------------------------
// These needed Meadow features can be excluded from a build
// To enable/disable remote debugging use CONFIG_HCOM_MONO_REMOTE_DEBUGGING 
// To enable/disable stdout and stder use CONFIG_HCOM_MONO_STDERR_STDOUT

//--------------------------------------------------------------------
// The following control things needed for diagnostics.
// When set to 1 the syslog mask is set for all tracing but debug
// and at startup syslog messages are routed to UART1 without
// the need to send the Uart1Trace command.
// THIS CAN BE REMOVED. IT'S REPLACED BY CONFIGURATION FILE
#define HCOM_FORCE_SYSLOG_MASK_F7_AND_UART1           0

// Allow the build to include the ability to print a buffer
// full of data, showing hex and ascii
#define HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE           0

//--------------------------------------------------------------------
// The following controls building of tracing the hex information
// that is associated with some LOG_DEBUG messages throughout
// the code base.
// NOTE:Code and define could be removed no longer used
#define HCOM_OUTPUT_DATA_BUFFER_INFO_VIA_SYSLOG       0
// NuttShell can be launched from CLI but currently it doesn't
// work because UART4 is reconfigured when mono starts running
// Requires nsh to be defined
#define HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD     0
// The F7's GPIOs can be used for diagnostics. Especially useful
// when debugging within the syslog code or for timing
#define HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE    0
// UART1 & UART 4 are sometimes used for diagnostic
// purposes. This define prevents these from being configured
// as gpio outputs
#define HCOM_NX_DIAG_GPIO_DIAGNOSTIC_PERSERVE_UARTS   0
// Outputs to syslog the PID of each new thread
#define HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS    0

//-------------------------------------------------------------------
// Include/exclude test code
#define HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD      0
#define HCOM_INCLUDE_BATTERY_BACKED_REG_TEST          0
#define HCOM_INCLUDE_INI_CFG_TESTS_IN_BUILD           0


#endif  // __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
