/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_common_utils.c
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
#include <nuttx/config.h>
#include "syslog.h"

#include "hcom_nx_common.h"
#include <meadow/hcom_bbreg_defn.h>

#include <assert.h>

#include <arch/board/board.h>
#include "stm32_gpio.h"

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

// NOTE: THIS EXACT CODE IS ALSO ON THE APPS SIDE
// //===================================================================
// // This is called during startup, before the hcom thread is created,
// // to check if we are running under the QEMU virtualization model.
// // 

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

//============================================================================
int hcom_nx_utils_startup_handling_of_trace_level()
{
  int syslogMask;

#if defined(CONFIG_STM32F7_PWR)
  // Check if this is a reboot or a power-on restart. The MCU at 
  // Power-on (unless there's a coin cell) clears all 32 battery
  // backed registers to 0.
  int bbrValue = getreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
  if(bbrValue == 0)
  {
    // Power-on restart
    // Set and save the syslog level to the default value
    syslogMask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
              LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING) |
              LOG_MASK(LOG_NOTICE) /*| LOG_MASK(LOG_INFO) | LOG_MASK(LOG_DEBUG) */;

    // Even though there are bits defined for other purposes, this works
    // because we know that the entire register is 0.
    putreg32(syslogMask, HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
  }
  else
  {
    // Rebooted - it's safe to use the battery backed registers values
    syslogMask = getreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
    syslogMask &= 0x000000ff;   // LS 8 bits are syslog mask
  }

  // Save for emergency debugging :-)
  // syslogMask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
  //             LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_NOTICE) | 
  //             LOG_MASK(LOG_INFO);  // | LOG_MASK(LOG_DEBUG);

  // Sets new mask and returns the previous syslog_mask
  int syslogMaskPrev = setlogmask(syslogMask);
  if (syslogMaskPrev < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setlogmask err:0x%08x\n", thisFile, __LINE__, syslogMaskPrev);
    return syslogMaskPrev;    // not old mask be error
  }
#else
#warning "CONFIG_STM32F7_PWR not defined\n"
  // Without battery backed registers the best we can do is defaults
  syslogMask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
              LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING);
  syslogMaskPrev = setlogmask(_syslogMask);
#endif
  return OK;
}

