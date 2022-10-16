/****************************************************************************
 * \include\meadow\hcom_nuttx_shared.h
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#ifndef __INCLUDE_HCOM_NUTTX_SHARED__H
#define __INCLUDE_HCOM_NUTTX_SHARED__H

#include <meadow/hcom_shared_common.h>

// This file is for item that need to be available to Hcom and Nuttx

// Update the following for each release build
#define HCOM_DEVICE_INFO_PRODUCT "Meadow"
#define HCOM_DEVICE_INFO_MODEL "F7Micro"
#define HCOM_DEVICE_INFO_MAJOR ###VERSION_MAJOR###
#define HCOM_DEVICE_INFO_MINOR ###VERSION_MINOR###
#define HCOM_DEVICE_INFO_REVISION ###VERSION_REVISION###
#define HCOM_DEVICE_INFO_BUILD ###VERSION_BUILD###
#define HCOM_DEVICE_INFO_BUILD_DAY ###BUILD_DAY###
#define HCOM_DEVICE_INFO_BUILD_MONTH ###BUILD_MONTH###
#define HCOM_DEVICE_INFO_BUILD_MONTH_NAME "###BUILD_MONTH_NAME###"
#define HCOM_DEVICE_INFO_BUILD_YEAR ###BUILD_YEAR###
#define HCOM_DEVICE_INFO_BUILD_HOUR ###BUILD_HOUR###
#define HCOM_DEVICE_INFO_BUILD_MINUTE ###BUILD_MINUTE###
#define HCOM_DEVICE_INFO_BUILD_SECOND ###BUILD_SECOND###
#define HCOM_DEVICE_INFO_BUILD_HASH "###BUILD_HASH###"
#define HCOM_DEVICE_INFO_GIT_REF "###MEADOW_GIT_REF###"
#define HCOM_VERSION_FORMAT_STRING "%d.%d.%d.%d, built %02d %s 20%02d %02d:%02d:%02d UTC (%s)"
#define HCOM_DEVICE_INFO_FULL_OS_VERSION "###VERSION_MAJOR###.###VERSION_MINOR###.###VERSION_REVISION###.###VERSION_BUILD### built ###BUILD_TWO_DIGIT_DAY### ###BUILD_MONTH_NAME### 20###BUILD_YEAR### ###BUILD_TWO_DIGIT_HOUR###:###BUILD_TWO_DIGIT_MINUTE###:###BUILD_TWO_DIGIT_SECOND### UTC (###BUILD_HASH###/###MEADOW_GIT_REF###)"
#define HCOM_DEVICE_INFO_PROCESSOR_TYPE "STM32F777IIK6"
#define HCOM_DEVICE_INFO_COPROCESSOR_TYPE "ESP32"

#define MEADOW_ENTER_DFU_MODE_MEMORY_ADDR (0x2004FFF0)
#define MEADOW_ENTER_DFU_MODE_MAGIC_NUMB (0x1c0ffee2)

int hcom_nx_common_utils_calculate_serial_numb(uint8_t mcu6ByteSerialNumb[], char mcu12CharSerialNumb[]);

#endif  // __INCLUDE_HCOM_NUTTX_SHARED__H