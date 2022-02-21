/****************************************************************************
 * nuttx\include\meadow\meadow_hw_version.h
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

#ifndef __CONFIG_MEADOW_SRC_MEADOW_HARDWARE_VERSION__H
#define __CONFIG_MEADOW_SRC_MEADOW_HARDWARE_VERSION__H

#include <nuttx/config.h>

#include <sys/types.h>

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>
#include <nuttx/spi/qspi.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

// Define each Meadow version
#define MEADOW_F7_HW_VERSION_NUMB_ERROR (-1)
#define MEADOW_F7_HW_VERSION_NUMB_UNKNOWN (0)
// Known versions
#define MEADOW_F7_HW_VERSION_NUMB_F7V1 (1)
#define MEADOW_F7_HW_VERSION_NUMB_F7V2 (2)
#define MEADOW_F7_HW_VERSION_NUMB_CCMV2 (3)

// Define each Meadow version name
#define MEADOW_F7_HW_VERSION_TEXT_NAME_ERROR "Error"
#define MEADOW_F7_HW_VERSION_TEXT_NAME_UNKNOWN "Unknown"
#define MEADOW_F7_HW_VERSION_TEXT_NAME_F7v1 "F7v1"
#define MEADOW_F7_HW_VERSION_TEXT_NAME_F7v2 "F7v2"
#define MEADOW_F7_HW_VERSION_TEXT_NAME_CCMv2 "CCMv2"

// Size of each versions flash
#define MEADOW_F7_HW_VERSION_F7V1_FLASH_SIZE (33554432)   // 32 MB
#define MEADOW_F7_HW_VERSION_F7V2_FLASH_SIZE (67108864)   // 64 MB
#define MEADOW_F7_HW_VERSION_CCMV2_FLASH_SIZE (67108864)  // 64 MB

// GPIO results for various F7 versions
#define MEADOW_F7_HW_VERSION_GPIO_ID_F7V1_OR_F7V2 (0x0f)
#define MEADOW_F7_HW_VERSION_GPIO_ID_CCMV2 (0x01)

// Public functions
uint32_t meadow_hw_version_determine_ver(FAR struct qspi_dev_s *qspi);
uint32_t meadow_hw_version_flash_size(void);
uint32_t meadow_hw_version_get(void);

char *meadow_hw_version_string_return(void);

bool meadow_hw_verion_ethernet_support(void);
bool meadow_hw_verion_sdcard_support(void);

#endif // __CONFIG_MEADOW_SRC_MEADOW_HARDWARE_VERSION__H
