/****************************************************************************
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
// This header file contains those items that must be shared by apps/hcom

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

// Diagnostic
#define HCOM_INCLUDE_DIAGNOSTIC_GPIO_CODE          1

#if HCOM_INCLUDE_DIAGNOSTIC_GPIO_CODE > 0
  #define HCOM_COMMON_UTILS_GPIO_A0_MISO_DOUT   1
#endif

#if HCOM_COMMON_UTILS_GPIO_A0_MISO_DOUT == 0
  #define HCOM_COMMON_UTILS_GPIO_A0_MISO_DOUT   0
#endif

#define HCOM_COMMON_UTILS_GPIO_D00_D08_DOUT   0

// This are used by hcom and hcom_nx.
// Note:In hcom_nx the numeric values define the order these appear
// in an array (they are used as offsets).
#define HCOM_GPIO_DIG_NX_ID_ESP_RESET  0
#define HCOM_GPIO_DIG_NX_ID_ESP_BOOT   1
#define HCOM_GPIO_DIG_NX_ID_BLUE_LED   2
#define HCOM_GPIO_DIG_NX_ID_A0___01    3
#define HCOM_GPIO_DIG_NX_ID_A1___02    4
#define HCOM_GPIO_DIG_NX_ID_A2___03    5
#define HCOM_GPIO_DIG_NX_ID_A3___04    6
#define HCOM_GPIO_DIG_NX_ID_A4___05    7
#define HCOM_GPIO_DIG_NX_ID_A5___06    8
#define HCOM_GPIO_DIG_NX_ID_SCK__07    9
#define HCOM_GPIO_DIG_NX_ID_MOSI_08    10
#define HCOM_GPIO_DIG_NX_ID_MISO_09    11

// Simplify the naming of the test GPIOs
#define HCOM_GPIO_1   HCOM_GPIO_DIG_NX_ID_A0___01
#define HCOM_GPIO_2   HCOM_GPIO_DIG_NX_ID_A1___02
#define HCOM_GPIO_3   HCOM_GPIO_DIG_NX_ID_A2___03
#define HCOM_GPIO_4   HCOM_GPIO_DIG_NX_ID_A3___04
#define HCOM_GPIO_5   HCOM_GPIO_DIG_NX_ID_A4___05
#define HCOM_GPIO_6   HCOM_GPIO_DIG_NX_ID_A5___06
#define HCOM_GPIO_7   HCOM_GPIO_DIG_NX_ID_SCK__07
#define HCOM_GPIO_8   HCOM_GPIO_DIG_NX_ID_MOSI_08
#define HCOM_GPIO_9   HCOM_GPIO_DIG_NX_ID_MISO_09

#endif  // __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H