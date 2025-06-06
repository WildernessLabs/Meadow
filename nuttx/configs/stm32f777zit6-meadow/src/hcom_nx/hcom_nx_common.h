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
#include <limits.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

#include <sys/mount.h>

#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_protocol.h>
#include <meadow/meadow_thread_config.h>
#include <meadow/meadow_os_persistent_data.h>

#include "../../bootloader/Core/Inc/ota_data.h"

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

#define HCOM_NX_FLASH_FILE_PARTITION_COUNT_MAX 8

// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less.
// For a buffer large enough for the path PLUS file name we this length
#define HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

#ifdef CONFIG_MTD_PARTITION
// Any number 2 - 8
#define HCOM_NX_NUMBER_OF_FS_PARTITIONS 1
#else
// 1 if no partitions in use
#define HCOM_NX_NUMBER_OF_FS_PARTITIONS 1
#endif

// #define UPDATE_DIR "/meadow0/update/"
// #define UPDATE_APP_DIR UPDATE_DIR "app"
// #define UPDATE_OS_DIR UPDATE_DIR "os"
// #define ROLLBACK_DIR "/meadow0/rollback/"

#ifdef CONFIG_FS_LITTLEFS
#define HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE "littlefs"
#define HCOM_NX_FILE_MOUNT_FORCE_FORMAT "forceformat"
// Note:The following string must fit into a buffer whose size is defined
// by HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH
#define HCOM_NX_FILE_MOUNT_POINT_SOURCE "/dev/little"
#endif

#define HCOM_TRACE_RAMLOG_DEVICE_NAME "/dev/ramlog"

// Which timing method should be built in Meadow, Wakeup Timer or RTC Alarm.
// RTC Alarm can sleep for up to 28 days - 1 second, where as the Wakeup Timer
// can sleep for 65535 seconds (18.2 hours).
// Define only 1
#define PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM
// #define PWRMGMT_LOW_PWR_EXIT_USE_WAKEUP_TIMER

// Support ISO-8601 time stardard
// This #define may not control all ISO-8601 code, as maybe it should
#define HCOM_INCLUDE_ISO8601_SUPPORT 0

// This define controls if the way the external flash chip is divided for
// Meadow is shown at startup.
#define HCOM_NX_EX_FLASH_SHOW_FLASH_STATS (1)

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/
  int hcom_nx_upd_initialize(void);
  int hcom_nx_utils_startup_handling_of_trace_level(void);

  // Common Utils
  void hcom_nx_common_utils_host_restart_meadow(void);
  void hcom_nx_common_utils_only_restart_meadow(void);

  // Route text to host PC (CLI)
  int hcom_nx_route_text_to_host_setup(void);
  int hcom_nx_route_text_to_host(uint16_t requestType, char *msgBuff,
          size_t msgLen);
  size_t hcom_nx_text_to_host_transport(uint16_t *requestType,
          char *buff, size_t buffLen);

  // HCOM command handling
  int hcom_nx_route_in_bound_cli_command(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_profiler_do_not_send_to_uart(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_profiler_forward_to_uart(struct hcom_nx_cmd_data *cmdData);

  // External flash
  int hcom_nx_exec_ex_flash_setup(FAR struct mtd_dev_s *mtd);
  int hcom_nx_exec_ex_flash_mono_flash(struct hcom_nx_cmd_data *cmd_data);
  int hcom_nx_exec_ex_flash_erase_ex_flash(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_ex_flash_verify_ex_flash(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_ex_flash_renew_file_system(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_ex_flash_OS_update_flash1(void);
  int hcom_nx_exec_ex_flash_OS_update_flash2(void);
  uint32_t hcom_nx_exec_ex_flash_get_block_size(void);
  int hcom_nx_exec_ex_flash_get_geometry(struct mtd_geometry_s *);
  int hcom_nx_exec_ex_flash_read_absolute_block(uint32_t blockNumber, void *destinationAddress);
  int  hcom_nx_exec_ex_flash_write_buffer_to_flash(uint8_t* data_buf, off_t size, off_t offset);
  int hcom_nx_exec_ex_flash_copy_blocks_to_memory(uint32_t startBlock, void *destinationAddress, uint32_t numberOfBlocks);
  int hcom_nx_exec_ex_flash_read_persistent_data(meadow_os_persistent_data_t *data);
  int hcom_nx_exec_ex_flash_write_persistent_data(meadow_os_persistent_data_t *data);
  uint32_t hcom_nx_exec_ex_flash_assert_data_location(void);
  int hcom_nx_exec_ex_flash_write_assertion_data(const char *data, uint32_t length);
  int hcom_nx_exec_ex_flash_read_assertion_data(const char *data);
  #if (HCOM_NX_EX_FLASH_SHOW_FLASH_STATS > 0)
  void hcom_nx_exec_ex_flash_syslog_external_flash_regions(FAR struct mtd_dev_s *mtd);
  #endif

  // Syslog tracing
  int hcom_nx_exec_trace_do_not_send_to_host(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_trace_do_send_to_host(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_trace_do_not_send_to_uart(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_trace_forward_to_uart(struct hcom_nx_cmd_data *cmdData);
#if defined (CONFIG_RAMLOG_SYSLOG)
  int hcom_nx_trace_msg_proc_setup(void);
  int hcom_nx_trace_msg_mono_started(void);
  void hcom_nx_trace_insure_correct_config (bool uartTracing, bool cliTracing, bool uartProfiling);
  size_t hcom_nx_trace_cli_trace_transport(char *buff, size_t bufLen);
#endif

  // Low-level file system
  int hcom_nx_create_fs_initialize(FAR struct mtd_dev_s *mtd);
  int hcom_nx_create_fs_mount(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId,
                                        const char *mountCommand);
bool hcom_nx_fs_is_mounted(uint32_t partitionId);
int hcom_nx_fs_1st_erase_sector_of_partition(uint32_t partitionId);

#ifdef CONFIG_FS_LITTLEFS
  int hcom_nx_create_littlefs_support_init_master(FAR struct mtd_dev_s *master_flash_mtd);
#ifdef CONFIG_MTD_PARTITION
  int hcom_nx_create_littlefs_init_1_part(uint32_t partitionId, struct mtd_dev_s *partMtd);
#endif
  int hcom_nx_create_littlefs_mount_format_1_part(uint32_t partitionId);
#endif

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
  // This functions allow mono to access power management

  // Public functions to manage power management
  int pwrmgmt_enter_stm32f7_stop_mode(uint32_t wakeupPeriod);
  int pwrmgmt_most_recent_wakeup_reason(void);
  int pwrmgmt_isr_gpio_wakeup_code(void);

  // Power Management Real-time clock hardware available to mono
  int pwrmgmt_mono_cmd_time_set_clock(const HcomProtoHdrMsg_t *hdrMsg, size_t packetSize);
  int pwrmgmt_mono_cmd_time_read_clock(struct hcom_nx_cmd_data *cmdData);
  #if HCOM_INCLUDE_ISO8601_SUPPORT > 0
    int pwrmgmt_mono_cmd_time_wakeup_period(const HcomProtoHdrMsg_t *hdrMsg, size_t packetSize);
  #endif

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

int hcom_nx_exec_test_sdcard_setup(void);

// Low-level QSPI flash tests
#if HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD > 0
  int hcom_nx_exec_test_qspi_flash_setup(FAR struct mtd_dev_s *mtd);
  int hcom_nx_exec_test_qspi_flash_write(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_test_qspi_flash_init(struct hcom_nx_cmd_data *cmdData);
  int hcom_nx_exec_test_qspi_flash_read(struct hcom_nx_cmd_data *cmdData);
#endif

// Access to battery backed registers
uint32_t hcom_nx_bbreg_read_bbr_and_right_justify(uint32_t bitMask);
uint32_t hcom_nx_bbreg_read_bbr(void);
void hcom_nx_bbreg_write_bbr(uint32_t value);
void hcom_nx_bbreg_set_bbr_bits(uint32_t value);
void hcom_nx_bbreg_clear_bbr_bits(uint32_t value);
void hcom_nx_bbreg_clear_then_set_bbr_bits(uint32_t clearBits, uint32_t setBits);
bool hcom_nx_bbreg_is_bbr_bits_set_n_clear(uint32_t value);
bool hcom_nx_bbreg_is_bbr_bit_set(uint32_t value);

// Configuration related
int hcom_nx_config_copy_for_user_mode(uint8_t *, int);

// Power Management/RTC
int meadow_parse_iso8601_date_time(char *isoDateTime, size_t isoDataTimeLen, struct tm *tmResult);
int meadow_parse_iso8601_utc_offset(char *isoDateTime, size_t isoDataTimeLen,
          int *utcTimeOffset, double *fractSec);
uint32_t meadow_parse_iso8601_time_period(const char *isoTimePeriod,
          time_t *secondsTillAlarm);

#define MEADOW_ISO_8601_PERIOD_FORMAT_LEAD_IN ('P')

// Diagnostic related 
int hcom_nx_diagnostic_app_execute(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t msgLen);
int meadow_idle_monitor_setup(void);

#if (HCOM_DIAG_MEADOW_OS_SLEEP_WAKE_FOREVER > 0)
int pwmmgmt_test_sleep_wake_only_setup(void);
#endif

  // Diagnostics
#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
  void hcom_nx_diag_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t msgPriority);
  void hcom_nx_diag_print_buffer_x(const uint8_t buffer[], const int bufLen, uint8_t msgPriority,
        void (*logger)(int priority, const char *string, ...));
#endif

// This macro and function simplify checking snprintf buffer overflow. The
// the function internally calls vsnprinf. The function
// hcom_nx_common_utils_snprintf_chk checks the return and outputs a syslog
// message if the message is truncated, if there's an error this is reported.
#define snprintf_chk(Buf, Len, Fmt, ...) hcom_nx_common_utils_snprintf_chk(Buf, Len, __FILE__, __LINE__, Fmt, ##__VA_ARGS__ )

int hcom_nx_common_utils_snprintf_chk(FAR char *buf, size_t size, char *fileName, int lineNumb,
          FAR const IPTR char *fmt, ...);

#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_SRC_HCOM_NX_COMMON__H
