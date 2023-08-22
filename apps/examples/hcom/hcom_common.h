/****************************************************************************
 * \apps\examples\hcom\hcom_common.h
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
#ifndef __CONFIGS_MEADOW_HOST_COM_HCOM_COMMON__H
#define __CONFIGS_MEADOW_HOST_COM_HCOM_COMMON__H

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
#include <nuttx/userspace.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_dnld_shared.h>
#include <meadow/meadow_thread_config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

//---------------------------------------------------------------------
// These define how long the receive thread waits before "waking up." It
// prevents a failed download from hanging the system for a long time.
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS 15
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS 60
#define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (5 * 60)    // 5 minutes
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (1 * 60 * 60) // once an hour report hcom thread running
#define HCOM_RECV_TIMEOUT_ACTIVE_SECONDS 10

#define HCOM_CONNECTION_TIMEOUT_STARTUP 250 * 1000    // At startup we connect quickly
#define HCOM_CONNECTION_TIMEOUT_RUNNING 5000 * 1000   // If no host connection at first wait longer
// How many fast connection attempts during startup before falling to a slower rate
// After 5 seconds realize host isn't there. After this use a slower rate
#define HCOM_CONNECTION_STARTUP_ATTEMPTS ((1000000 / HCOM_CONNECTION_TIMEOUT_STARTUP) * 5) // 5 seconds

//---------------------------------------------------------------------
// Select the approprate UART for meadow NSH
#if defined (CONFIG_SYSTEM_NSH)
  // #define HCOM_DIAG_NSH_SERIAL_DEVICE "/dev/ttyS0"  // This is UART1
  // #define HCOM_DIAG_NSH_SERIAL_DEVICE "/dev/ttyS1"  // This is UART4
  // Note: /dev/ttyS2 is UART5 used to download to ESP32
  #define HCOM_DIAG_NSH_SERIAL_DEVICE "/dev/ttyS3"  // This is UART6
#endif

//---------------------------------------------------------------------
// Several of the meadow device names are defined here
#define HCOM_COMMUNICATIONS_DEVICE_NAME "/dev/ttyACM0"
#define HCOM_TRACE_RAMLOG_DEVICE_NAME "/dev/ramlog"
#define HCOM_MONO_STDOUT_REDIRECT_FIFO "/dev/monostdout"
#define HCOM_MONO_STDERR_REDIRECT_FIFO "/dev/monostderr"
#define HCOM_MONO_REMOTE_DBG_SOCKET_NAME "/dev/monodbg"
#define HCOM_MONO_REMOTE_DBG_CMD_LINE_DEBUG "--debug"
#define HCOM_MONO_REMOTE_DBG_CMD_LINE_SD "--debugger-agent=transport=socket-fd,address=%d"

//---------------------------------------------------------------------
#define HCOM_CIR_BUFFER_MAX_PACKETS 4
// Allow for multiple message to be buffered
#define HCOM_CIRCULAR_BUF_MEM_SIZE (HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE * \
                  HCOM_CIR_BUFFER_MAX_PACKETS)

//----------------------------------------------------------------
// Trace level constants
#define HCOM_TRACE_LEVEL_DEFAULT 0
#define HCOM_TRACE_LEVEL_NOTICE 1
#define HCOM_TRACE_LEVEL_NOTICE_INFO 2
#define HCOM_TRACE_LEVEL_NOTICE_INFO_DEBUG 3

#define HCOM_TRACE_MASK_DEFAULT 0x1f
#define HCOM_TRACE_MASK_NOTICE 0x3f
#define HCOM_TRACE_MASK_NOTICE_INFO 0x7f
#define HCOM_TRACE_MASK_NOTICE_INFO_DEBUG 0xff

//----------------------------------------------------------------
// Cell network logs
#define HCOM_CELL_DEBUG_LOGS 1

#ifndef __ASSEMBLY__

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

 /****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

// hcom_startup_manager
void hcom_startup_mgr_release_sem(void);
void hcom_startup_mgr_release_sem_err(int semaphoreRet);
void hcom_manager_shutdown(void);

// USB CDC/ACM send receive host messages
int hcom_host_recv_setup(void);
void hcom_host_recv_shutdown(void);
int hcom_host_recv_receiving_loop(void);
const char *hcom_host_recv_get_device_name(void);

// Functions related to sending to the HOST (CLI)
int hcom_host_send_setup(void);
void hcom_host_send_shutdown(void);
void hcom_host_send_header_msg(uint16_t requestType, uint32_t userData,
        char *sourceFileName, int sourceLineNumber);
void hcom_host_send_binary_data_msg(uint16_t requestType, uint32_t userData,
          uint8_t *bytes,size_t msgLength, char *sourceFileName,
          int sourceLineNumber);
void hcom_host_send_simple_string_msg(uint16_t requestType, uint32_t userData,
          char *shortText,char *sourceFileName, int sourceLineNumber);
int hcom_host_send_raw_string_msg(uint16_t requestType, uint32_t userData,
          char *shortText,size_t msgLength, char *sourceFileName,
          int sourceLineNumber);
// Matches typedef in /nuttx/include/meadow/hcom_shared_common.h
// typedef int (* send_host_std_msg_data)(HcomProtoHdrMsg_t *hdrMsg,
//   size_t totalMsgLen, char *sourceFileName, int sourceLineNumber);
int hcom_host_send_std_msg_data(HcomProtoHdrMsg_t *hdrMsg,
        size_t totalMsgLen, char *sourceFileName, int sourceLineNumber);

int hcom_host_enq_deq_setup(void);
void hcom_host_enq_deq_shutdown(void);
bool hcom_host_enq_deq_clear_buffer(void);
// void hcom_host_enq_deq_dbg_info(void);               // Code for testing
int hcom_host_enq_deq_enqueue_rcvd_data(uint8_t recvBuff[], const ssize_t recvByteCnt);
int hcom_host_enq_deq_dequeue_packet(uint8_t *packet_dest_buf, size_t *packetLength);

// -----------------------------------------------
// Received message are first processed using these functions
int hcom_host_process_setup(void);
void hcom_host_process_shutdown(void);
int hcom_host_process_free_dnld_share_mem(void);
bool hcom_host_process_is_stm32f7_dnld_active(void);

int hcom_host_watchdog_dnld_timer_initialize(void);
int hcom_host_watchdog_dnld_timer_set_delay(time_t sec);
int hcom_host_watchdog_dnld_timer_delete(void);
int hcom_esp32_exec_flash_file(uint8_t *, uint32_t, uint32_t, char *);

void hcom_host_route_request_by_cmd_type(const HcomProtoHdrMsg_t *hcomMsg,
      const size_t packetSize, const uint32_t userData,
      const uint16_t requestType, hcom_dnld_shared_t *dnldShared);  
int hcom_host_route_setup(void);
void hcom_host_route_shutdown(void);

// -----------------------------------------------
// Execute Request for download add and delete
int hcom_file_dnld_stm32f7_setup(void);
void hcom_file_dnld_stm32f7_file_begin(const HcomProtoHdrMsg_t *hdrMsg,
      hcom_dnld_shared_t *dnldShared);
void hcom_file_dnld_stm32f7_recvd_file_data(const HcomProtoDataMsg_t *dataMsg,
      const size_t packetSize, hcom_dnld_shared_t *dnldShared);
void hcom_file_dnld_stm32f7_file_end(hcom_dnld_shared_t *dnldShared);
void hcom_file_delete_stm32f7_file_by_name(hcom_dnld_shared_t *dnldShared);
void hcom_file_delete_stm32f7_file_by_name_internal(hcom_dnld_shared_t *dnldShared);

int hcom_file_dnld_esp32_setup(void);
bool hcom_file_dnld_esp32_is_active(void);
void hcom_file_dnld_esp32_set_to_inactive(void);
void hcom_file_dnld_esp32_file_begin(const HcomProtoHdrMsg_t *hdrMsg);
void hcom_file_dnld_esp32_recvd_file_data(const HcomProtoDataMsg_t *dataMsg,
      const size_t packetSize);
void hcom_file_dnld_esp32_file_end(uint32_t user_data);

// -----------------------------------------------
// Execute Request for uploading a file
int hcom_file_upld_proc_setup(void);
void hcom_file_upld_proc_initial_bytes_in_file(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize, uint32_t partitionId);
void hcom_file_upld_proc_start_file_upload(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize, uint32_t partitionId);
void hcom_file_upld_proc_begin_file_uploading(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize, uint32_t partitionId);
void hcom_file_upld_proc_abort_file_upload(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize, uint32_t partitionId);

// -----------------------------------------------
// File System functions
int hcom_file_write_setup(void);
void hcom_file_write_shutdown(void);
int hcom_file_write_open_active_file(hcom_dnld_shared_t *dnldShared);
int hcom_file_write_to_active_file(hcom_dnld_shared_t *dnldShared,
      const uint8_t *fileWriteData, const size_t fileWriteSize);
int hcom_file_write_close_active_file(hcom_dnld_shared_t *dnldShared);

// -----------------------------------------------
// File listing functions
int hcom_file_lists_files_in_partition(uint32_t partitionId);
int hcom_file_lists_files_and_crc_in_partition(uint32_t partitionId);
int hcom_file_lists_all_dev_dir_and_files_start(uint32_t userData);

// -----------------------------------------------
// File directory functions
int hcom_file_dir_nested_dev_dir_and_files_start(void);

// -----------------------------------------------
// File download misc functions
uint32_t hcom_file_misc_calc_crc_for_file(char *completeFilePath, off_t *fileSize,
        uint32_t *blockSizeKB, int *detectError);
uint32_t hcom_file_misc_calc_crc_for_file_fd(int fd, char *completeFilePath,
        off_t *fileSize, uint32_t *blockSizeKB, int *detectError);

// -----------------------------------------------
// Mono related
int hcom_mono_ctrl_mono_main_setup(void);
bool hcom_mono_ctrl_is_mono_enabled(void);
int hcom_mono_ctrl_start_mono_main(void);
int hcom_pppd_start(void);
int meadow_cell_scanner(char *response);
int hcom_mono_ctrl_mono_appears_to_be_running(void);
void hcom_mono_ctrl_disable_mono(uint32_t userData);
void hcom_mono_ctrl_enable_mono(uint32_t userData);
void hcom_mono_ctrl_report_mono_enabled_state(uint32_t userData);

// mono stdout & stderr to host
int hcom_mono_stderr_read_setup(void);
void hcom_mono_stderr_read_shutdown(void);
int hcom_mono_stdout_read_setup(void);
void hcom_mono_stdout_read_shutdown(void);
int hcom_mono_stdout_redirect(void);
int hcom_mono_stderr_redirect(void);

// mono Visual Studio interactions
int hcom_mono_remote_dbg_setup(void);
void hcom_mono_remote_dbg_shutdown(void);
bool hcom_mono_remote_dbg_is_active(void);

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
void hcom_mono_remote_dbg_recv_host_sending_to_mono(const HcomProtoHdrMsg_t *hdrMsg,
          size_t packetSize, uint32_t userData);
void hcom_mono_remote_dbg_enable(uint32_t userData);
#endif

// -----------------------------------------------
// Comms support, COBS encode and receive circular buffer
size_t hcom_host_cobs_encoder(uint8_t source[], size_t startingOffset, size_t length, uint8_t encoded[]);
size_t hcom_host_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[]);

// common utils
int hcom_common_utils_setup(void);
void hcom_common_utils_shutdown(void);
uint64_t hcom_utils_get_current_time64_ns(void);
int hcom_common_utils_snprintf_chk(FAR char *buf, size_t size, char *fileName, int lineNumb,
        FAR const IPTR char *fmt, ...);

// -----------------------------------------------
// Utility Requests
int hcom_misc_rqst_setup(void);
void hcom_misc_rqst_get_device_info(uint32_t userData);
void hcom_misc_rqst_get_device_name(uint32_t userData);
void hcom_misc_rqst_enter_dfu_mode(uint32_t user_data);

// -----------------------------------------------
// Access to battery backed registers
uint32_t hcom_bbreg_read_bbr_and_right_justify(uint32_t bitMask);
uint32_t hcom_bbreg_read_bbr(void);
void hcom_bbreg_write_bbr(uint32_t value);
void hcom_bbreg_set_bbr_bits(uint32_t value);
void hcom_bbreg_clear_bbr_bits(uint32_t value);
void hcom_bbreg_clear_bbr_bits_alt(int alt_access_fd, uint32_t value);
void hcom_bbreg_clear_then_set_bbr_bits(uint32_t clearBits, uint32_t setBits);
bool hcom_bbreg_is_bbr_bits_set_n_clear(uint32_t value);
bool hcom_bbreg_is_bbr_bit_set(uint32_t value);

// -----------------------------------------------
// HCOM nx (nuttx) access allows low-level access to operating system resources
int hcom_via_nx_upd_setup(void);
int hcom_via_nx_upd_driver_open(void);
int hcom_via_nx_set_any_reg(uint32_t address, uint32_t value);
int hcom_via_nx_set_bbr(uint32_t value);
int hcom_via_nx_get_bbr(uint32_t *value);
int hcom_via_nx_update_bbr(uint32_t clearBits, uint32_t setBits);
int hcom_via_nx_update_bbr_alt(int alt_access_fd, uint32_t clearBits, uint32_t setBits);
int hcom_via_nx_host_restart_meadow(void);
int hcom_via_nx_only_restart_meadow(void);
int hcom_via_nx_put_meadow_into_dfu_mode(void);
int hcom_via_nx_get_mcu_id(uint8_t uniqueId[12]);
int hcom_via_nx_get_mcu_ser_numb(char mcuSerNumb[16]);
void hcom_via_nx_restore_uart_reconfig(uint32_t uartId);
uint32_t hcom_via_nx_get_hw_version(void);
uint32_t hcom_via_nx_get_hw_version_alt(int alt_access_fd);
int hcom_via_nx_esp32_enter_prog_mode(void);
void hcom_via_nx_mono_has_started(void);
size_t hcom_via_nx_provide_cli_trace_transport(char *buff, size_t bufLen);
size_t hcom_via_nx_provide_host_text_transport(uint16_t *requestType,
        char *buff, size_t bufLen);
int hcom_via_nx_esp32_restart_esp32(void);
int hcom_via_nx_start_espcp_running(void);
void hcom_via_nx_diag_fd_inode(int fd);
void hcom_via_nx_diag_fd_inode_read(int fd, struct inode **inodeOut);
int hcom_via_nx_execute_espcp_tests(uint32_t);
int hcom_via_nx_copy_mono_runtime_to_ram(void);

void hcom_via_nx_forward_cli_cmd_to_nx(uint16_t hcomCmd, uint32_t userData);
bool hcom_via_nx_is_mounted(uint32_t partitionId);

int hcom_via_nx_execute_rtc_set_clock(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize);
int hcom_via_nx_execute_rtc_set_wakeup_time(const HcomProtoHdrMsg_t *hdrMsg,
        const size_t packetSize);
int hcom_via_nx_update_OS1(void);
int hcom_via_nx_update_OS2(void);
int hcom_via_nx_get_update_state(uint8_t flag);
int hcom_via_nx_set_update_state(uint8_t flag, uint8_t state);
int hcom_via_nx_register_pwr_mgmt_callback(pwr_mgmt_notify_callback callback);
int hcom_via_nx_register_host_msg_send_callback(send_host_std_msg_data hostCallback);

// -----------------------------------------------
// Methods found in meadow_utils.c
int meadow_copy_mono_runtime_to_ram(void);

// -----------------------------------------------
// These all deal with syslog message, related to syslog tracing
// priority and building the final syslog message
int hcom_diag_logging_setup(void);
void hcom_diag_logging_shutdown(void);
int hcom_diag_logging_get_syslog_mask(void);
void hcom_diag_logging_change_trace_level(uint32_t userData);

int hcom_trace_to_cli_setup(void);
void hcom_trace_to_cli_enable_command(uint32_t userData);
void hcom_trace_to_cli_disable_command(uint32_t userData);
void hcom_trace_to_cli_disable_cleanup(uint32_t userData);

// These are syslog message helpers used throughout HCOM
void hcom_logging_syslog(int priority, FAR const IPTR char *fmt, ...);
void hcom_logging_syslog_x(int priority, FAR const IPTR char *fmt, ...);
void hcom_logging_safe_ramlog(int priority, FAR const IPTR char *fmt, va_list args);
int hcom_logging_syslog_mask_init(void);

//-------------------------------------------------------
// Ramlog to host
#if defined (CONFIG_RAMLOG_SYSLOG)
int hcom_diag_trace_to_cli_setup(void);
void hcom_diag_trace_to_cli_shutdown(void);
#endif

void hcom_diag_trace_forward_to_host(uint32_t userData);
void hcom_diag_trace_do_not_send_to_host(uint32_t userData);

int hcom_host_text_transport_setup(void);

int hcom_diag_misc_setup(void);
int hcom_diag_nsh_support_setup(void);
void hcom_diag_misc_launch_nsh(uint32_t userData);

void hcom_diag_print_buffer(const uint8_t packetBuffer[],
          const int bufLen, uint8_t logPriority);
void hcom_diag_print_buffer_x(const uint8_t buffer[], const int bufLen, uint8_t msgPriority,
      void (*logger)(int priority, const char *string, ...));

void hcom_diag_misc_build_info_from_recvd_msg(uint8_t buffer[],
          const int bufLen, bool isEncoded);
void hcom_diag_misc_build_info_from_send_msg(uint8_t buffer[],
          const int bufLen, bool isEncoded);
void hcom_diag_decode_recvd_message_type(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize);
void hcom_diag_decode_sending_message_type(const uint8_t *hostRawMsg,
        const uint16_t hostRqstType, const size_t packetSize);
void hcom_via_nx_exec_diag_app_cmd(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize);

//-------------------------------------------------------
// Testing utilities
void hcom_developer_tests_developer(uint16_t level, uint32_t value);

int MonoVsRemoteDebugTestSetup(int argc, char *argv[]);

void hcom_bbr_tests(uint32_t);

void diag_misc_tests_snprintf_on_nuttx(uint32_t userData);

void hcom_meadow_sqlite_tests(uint32_t userData);
void hcom_meadow_diag_gpio_tests(uint32_t userData);
void meadow_dir_mgmt_tests(uint32_t userData);

void diag_misc_tests_overload_mcu(uint32_t userData);
void diag_ethernet_chat_server(uint32_t userData);

void tensorflow_tests_hello_world(uint32_t userData);

// This macro calls a function adding file and line info. I kept the entire
// macro on a single line to reduce line number confusion. The ## is needed
// for those cases when the caller doesn't supply any additional arguments.
#define snprintf_chk(Buf, Len, Fmt, ...) hcom_common_utils_snprintf_chk(Buf, Len, __FILE__, __LINE__, Fmt, ##__VA_ARGS__ )

//------------------------------------------------
#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_HOST_COM_HCOM_COMMON__H
