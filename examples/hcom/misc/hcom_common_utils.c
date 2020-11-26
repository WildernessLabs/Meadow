/****************************************************************************
 * \apps\examples\hcom\misc\hcom_common_utils.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <ctype.h>
#include "hcom_common.h"

#include <nuttx/config.h>
#include "syslog.h"

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_common_utils_setup()
{
  return OK;
}

//============================================================================
void hcom_common_utils_shutdown()
{
}

//===================================================================
// Returns the current time as a 64-bit number representing nanosec.
// Used for testing. Note: Only millisecond resolution.
uint64_t hcom_utils_get_current_time64(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}

//===================================================================
// Return true/false if match == value found by key in configuration file
bool hcom_utils_ini_cfg_is_match(char *fileName, char *section, char *key, char *match)
{
  int ret;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  ret = hcom_via_nx_ini_cfg_get_value(fileName, section, key,
        returnValueBuf, MEADOW_DEFAULT_INI_CFG_BUF_LEN);

  if(ret == OK && strcmp(returnValueBuf, match) == 0)
  {
    return true;
  }
  
  return false;
}

//===================================================================
// Return interger value found by key
int hcom_utils_ini_cfg_get_int_default(char *fileName, char *section, char *key, int defval)
{
  int ret;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  ret = hcom_via_nx_ini_cfg_get_value(fileName, section, key,
        returnValueBuf, MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  if(ret < 0)
  {
    syslog(LOG_DEBUG, "%s@%d-For section:%s, key:%s using default:, ret:%d\n", thisFile, __LINE__,
                    section, key, defval, ret);

    // Return default value
    return defval;
  }

  return atoi(returnValueBuf);
 }

//===================================================================
// NOTE: THIS EXACT CODE IS ALSO ON THE NUTTX SIDE
//
// This is called during startup, before the hcom thread is created,
// to check if we are running under the QEMU virtualization model.
// 
// #define QEMU_BOOT_INFO_MAGIC 0x12341234
// #define QEMU_BOOT_INFO_OFFSET_FROM_SDRAM_END 1024
// #define QEMU_BOOT_INFO_ADDRESS (CONFIG_HEAP2_BASE + CONFIG_HEAP2_SIZE - QEMU_BOOT_INFO_OFFSET_FROM_SDRAM_END)

// bool hcom_utils_boot_time_qemu_check()
// {
//     // As part of the booting process, QEMU writes a token value
//     // to the first page of SDRAM. This logic is implemented at
//     // qemu/hw/arm/meadow.c:meadow_machine_reset.

//     uint32_t *addr = (uint32_t *)QEMU_BOOT_INFO_ADDRESS; 
//     return *addr == QEMU_BOOT_INFO_MAGIC;
// }
