/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_common.h
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
#ifndef __CONFIGS_MEADOW_SRC_HCOM_NX_COMMON__H
#define __CONFIGS_MEADOW_SRC_HCOM_NX_COMMON__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <crc32.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/board.h>
#include <nuttx/mm/mm.h>
#include <nuttx/config.h>
#include <limits.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

#include <sys/mount.h>

#include <meadow/hcom_udp_shared.h>

#if defined (CONFIG_ARCH_CHIP_STM32F7)
#include "chip/stm32f76xx77xx_memorymap.h"
#include "chip/stm32_rtcc.h"    // battery backed registers and ram
#else
#error "This implementation only designed to work with STM32F7"
#endif

#ifndef __ASSEMBLY__

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#define HCOM_FLASH_FILE_PARTITION_COUNT_MAX 8

// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less
#define HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

#ifdef CONFIG_MTD_PARTITION
#define HCOM_NUMBER_OF_FS_PARTITIONS 1    // Any number 2 - 8
#else
#define HCOM_NUMBER_OF_FS_PARTITIONS 1    // 1 if no partitions in use
#endif

#define HCOM_FS_MONO_RAW_PARTITION_SIZE 0x200000 // 2MB
#define HCOM_FS_MONO_RUNTIME_FILENAME "Meadow.OS.Runtime.bin"

#ifdef CONFIG_FS_LITTLEFS
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "littlefs"
#define HCOM_FILE_MOUNT_POINT_SOURCE "/dev/little"
#define HCOM_FILE_MOUNT_FORCE_FORMAT "forceformat"
#endif

// Define Battery Backed Registers so we know what each one does
// This defines which BBR register to use. There are 32 (0-31) in
// the stm32f7. Currently only one is used.
// STM32_RTC_BK31R is defined in chip/stm32_rtcc.h
#define HCOM_MEADOW_BATTERY_BACKED_REGISTER STM32_RTC_BK31R

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

  int hcom_nx_upd_initialize(void);
  int hcom_utils_startup_handling_of_trace_level(void);

  // HCOM command handling
  int hcom_nx_route_cli_command(struct hcom_nx_cmd_data *cmdData);

  // External flash
  int hcom_exec_ex_flash_setup(FAR struct mtd_dev_s *mtd);
  int hcom_exec_ex_flash_mono_flash(struct hcom_nx_cmd_data *cmd_data);
  int hcom_exec_ex_flash_erase_ex_flash(struct hcom_nx_cmd_data *cmdData);
  int hcom_exec_ex_flash_verify_ex_flash(struct hcom_nx_cmd_data *cmdData);
  int hcom_exec_ex_flash_renew_file_system(struct hcom_nx_cmd_data *cmdData);

  // Low-level file system
  int hcom_create_fs_initialize(FAR struct mtd_dev_s *mtd);
  int hcom_create_fs_mount(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId,
                                        const char *mountCommand);
bool hcom_fs_is_mounted(uint32_t partitionId);
int hcom_fs_1st_erase_sector_of_partition(uint32_t partitionId);

#ifdef CONFIG_FS_LITTLEFS
  int hcom_create_littlefs_support_init_master(FAR struct mtd_dev_s *master_flash_mtd);
#ifdef CONFIG_MTD_PARTITION
  int hcom_create_littlefs_init_1_part(uint32_t partitionId, struct mtd_dev_s *partMtd);
#endif
  int hcom_create_littlefs_mount_format_1_part(uint32_t partitionId);
#endif

  // Diagnostics
#define HCOM_NX_DIAG_MISC_PRINT_BUFFER 0

#if HCOM_NX_DIAG_MISC_PRINT_BUFFER > 0
  void hcom_utils_diag_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t msgPriority);
#endif

#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_SRC_HCOM_NX_COMMON__H
