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

#include<meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Update the following for each release build
#define HCOM_DEVICE_INFO_PRODUCT "Meadow by Wilderness Labs"
#define HCOM_DEVICE_INFO_MODEL "F7Micro"
#define HCOM_DEVICE_INFO_MEADOW_OS_VERSION "0.4.0"
#define HCOM_DEVICE_INFO_PROCESSOR_TYPE "STM32F777IIK6"
#define HCOM_DEVICE_INFO_COPROCESSOR_TYPE "ESP32"
#define HCOM_DEVICE_INFO_COPROCESSOR_OS_VERSION "0.0.1"
#define HCOM_DEVICE_INFO_MONO_VERSION "0.0.0.1"

//---------------------------------------------------------------------
// Thread priorities and names
// Note: pthreads cannot be named. The name below are only for
// error messages ect.
#define HCOM_THREAD_PRIORITY_HCOM_RECEIVE 240
#define HCOM_THREAD_NAME_HCOM_RECEIVE "HcomRecv"
#define HCOM_THREAD_STACKSIZE_HCOM_RECEIVE 65536

// Insure hcom recv thread runs before esp32 recv
#define HCOM_THREAD_PRIORITY_ESP32_RECEIVE (HCOM_THREAD_PRIORITY_HCOM_RECEIVE - 1)
#define HCOM_THREAD_NAME_ESP32_RECEIVE "EspRecv"
#define HCOM_THREAD_STACKSIZE_ESP32_RECEIVE 2048

// This thread reads stdout and forwards to the Host 
#define HCOM_THREAD_PRIORITY_STDERR_REDIRECT 120
#define HCOM_THREAD_NAME_STDOUT_REDIRECT "MonoOut"
#define HCOM_THREAD_STACKSIZE_STDOUT_REDIRECT 2048

// This thread reads stderr and forwards to the Host 
#define HCOM_THREAD_PRIORITY_STDOUT_REDIRECT 120
#define HCOM_THREAD_NAME_STDERR_REDIRECT "MonoErr"
#define HCOM_THREAD_STACKSIZE_STDERR_REDIRECT 2048

// This thread is used for remote debugging mono apps
#define HCOM_THREAD_PRIORITY_REMOTE_DBG 120
#define HCOM_THREAD_NAME_REMOTE_DBG "RemoteDbg"
#define HCOM_THREAD_STACKSIZE_REMOTE_DBG 2048

// The ramlog is part of nuttx and contains syslog text
#define HCOM_THREAD_PRIORITY_TRACE_RAMLOG 120
#define HCOM_THREAD_NAME_TRACE_RAMLOG "RamlogRead"
#define HCOM_THREAD_STACKSIZE_TRACE_RAMLOG 2048

//---------------------------------------------------------------------
// These define how long the receive thread waits before "waking up"
// p-m DON'T FORGET
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS 15
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS 60
#define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (5 * 60)    // 5 minutes
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (1 * 60 * 60) // once an hour report hcom thread running
#define HCOM_RECV_TIMEOUT_ACTIVE_SECONDS 5

#define HCOM_CONNECTION_TIMEOUT_STARTUP 250 * 1000    // At startup we connect quickly
#define HCOM_CONNECTION_TIMEOUT_RUNNING 5000 * 1000   // If no host connection at first wait longer
// How many fast connection attempts during startup before falling to a slower rate
// After 5 seconds realize host isn't there. After this use a slower rate
#define HCOM_CONNECTION_STARTUP_ATTEMPTS ((1000000 / HCOM_CONNECTION_TIMEOUT_STARTUP) * 5) // 5 seconds

//---------------------------------------------------------------------
#define HCOM_COMMUNICATIONS_DEVICE_NAME "/dev/ttyACM0"
#define HCOM_TRACE_RAMLOG_DEVICE_NAME "/dev/ramlog"
#define HCOM_MONO_STDOUT_REDIRECT_FIFO "/dev/monostdout"
#define HCOM_MONO_STDERR_REDIRECT_FIFO "/dev/monostderr"
#define HCOM_MONO_REMOTE_DBG_SOCKET_NAME "/dev/monodbg"
#define HCOM_MONO_REMOTE_DBG_CMD_LINE_SD "--dbgSD"
//---------------------------------------------------------------------
#define HCOM_CIR_BUFFER_MAX_PACKETS 4
// Based on the encoding scheme (COTS), after encoding there will usually be 2-3 bytes added. One that
// prepends the message and the delimiter of '0'. For messages longer than 254 bytes, another byte may
// be added every 254 bytes.

// Allow for 4 max sized message to be buffered
#define HCOM_CIRCULAR_BUF_MEM_SIZE (HCOM_PROTOCOL_SAFE_PACKET_BUF_SIZE * HCOM_CIR_BUFFER_MAX_PACKETS)

// Host text message buffer sizes for text messages
#define HCOM_DECODE_XMIT_RQST_TYPE_LEN 48
#define HCOM_SHORT_HOST_STRING_BUFF_LENGTH 128                  // automatic variable
// This is the maximum length of a message that can be in a single packet
#define HCOM_LARGE_HOST_STRING_BUFF_LENGTH HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN
#define HCOM_MAX_HOST_STRING_BUFF_LENGTH 2048                   // allocate
// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less
#define HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

//----------------------------------------------------------------
// Circular buffer 
struct host_com_cir_buffer_s
{
  uint8_t *bottom;    // bottom of buffer
  uint8_t *top;       // top end of buffer
  uint8_t *head;      // add data here
  uint8_t *tail;      // remove from here
  uint8_t delimiter;  // custom message delimiter
};

// Circular buffer return values
enum hcom_comms_recv_buffer_return
{
  HCOM_CIR_BUF_INIT_OK,
  HCOM_CIR_BUF_INIT_FAILED,

  HCOM_CIR_BUF_ADD_SUCCESS,
  HCOM_CIR_BUF_ADD_WONT_FIT,
  HCOM_CIR_BUF_ADD_BAD_ARG,

  HCOM_CIR_BUF_GET_FOUND_MSG,
  HCOM_CIR_BUF_GET_NONE_FOUND,
  HCOM_CIR_BUF_GET_DEST_NO_ROOM
};

//--------------------------------------------------------------------
// This enum defines the current processing activity for a data packet
// download.
// The protocol could be modified so that each data packet contains this
// information. This would allow more than one operation to be processed
// at a time.
// To do this the protocol would need to be enhanced so that start download
// command carried an additional field to identify the "series" a particular
// data packet belonged to. Each data packet would be unuque and the
// sequence numbers 1-n would be unique for each series.
enum hcom_download_data_packet_action
{
  HcomDnldActionNone,
  HcomDnldActionMeadowFileXfer, // Could be expanded to specify file type (e.g. mscorlib.dll)
  HcomDnldActionEsp32FileXfer,
};

// Used for writing and deleting files
#define HCOM_INVALID_PARTITION_ID_VALUE 0xffffffff

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

  int hcom_host_send_setup(void);
  void hcom_host_send_shutdown(void);
  void hcom_host_send_header_msg(uint16_t requestType, uint32_t userData,
          char *sourceFileName, int sourceLineNumber);
  void hcom_host_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
          char *sourceFileName, int sourceLineNumber);
  int hcom_host_send_raw_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
          size_t msgLength, char *sourceFileName, int sourceLineNumber);

  int hcom_host_parse_setup(void);
  void hcom_host_parse_shutdown(void);
  int hcom_host_parse_save_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt);

  void hcom_host_route_request_by_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize);
  int hcom_host_route_setup(void);
  void hcom_host_route_shutdown(void);

  // -----------------------------------------------
  // Execute Request for downloaded file
  int hcom_file_dnld_proc_setup(void);
  bool hcom_file_dnld_proc_is_active(void);
  void hcom_file_dnld_restore_to_inactive_state(void);
  void hcom_file_dnld_proc_begin(const uint8_t *recvPacketData,
      const size_t recvPacketDataSize,uint32_t partitionId, uint16_t requestType);
  void hcom_file_dnld_proc_end(uint32_t user_data);
  void hcom_file_dnld_proc_recvd_file_data(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb);
  void hcom_file_write_del_remove_file_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize, uint32_t user_data);

  // -----------------------------------------------
  // File commands
  int hcom_file_write_del_setup(void);
  void hcom_file_write_del_shutdown(void);
  int hcom_file_write_del_open_active_file(const uint32_t partitionId, const char *mountPoint, const char *fileName);
  int hcom_file_write_del_add_to_active_file(const uint8_t *fileWriteData, const size_t fileWriteSize);
  int hcom_file_write_del_close_active_file(void);

  int hcom_file_lists_files_in_partition(uint32_t partitionId);
  int hcom_file_lists_files_and_crc_in_partition(uint32_t partitionId);
  int hcom_file_lists_all_dev_dir_and_files_start(uint32_t userData);

  // -----------------------------------------------
  // Mono related
  // hcom_mono_control
  int hcom_mono_ctrl_mono_main_setup(void);
  bool hcom_mono_ctrl_is_mono_enabled(void);
  int hcom_mono_ctrl_start_mono_main(void);
  int hcom_mono_ctrl_mono_appears_to_be_running(void);
  void hcom_mono_ctrl_disable_mono(uint32_t userData);
  void hcom_mono_ctrl_enable_mono(uint32_t userData);
  void hcom_mono_ctrl_report_mono_enabled_state(uint32_t userData);

  // mono stdout & stderr to host
  int hcom_mono_stderr_read_setup(void);
  void hcom_mono_stderr_read_shutdown(void);
  int hcom_mono_stdout_read_setup(void);
  void hcom_mono_stdout_read_shutdown(void);

  // mono Visual Studio interactions
  int hcom_mono_remote_dbg_setup(void);
  void hcom_mono_remote_dbg_shutdown(void);
  bool hcom_mono_remote_dbg_is_active(void);

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
  void hcom_mono_remote_dbg_recv_host_sending_to_mono(const uint8_t *recvPayload, size_t recvPayloadSize, uint32_t userData);
  void hcom_mono_remote_dbg_enable(uint32_t userData);
#endif

  // -----------------------------------------------
  // Comms support, COBS encode and receive circular buffer
  size_t hcom_host_cobs_encoder(uint8_t source[], size_t startingOffset, size_t length, uint8_t encoded[]);
  size_t hcom_host_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[]);

  int hcom_cirbuf_init(struct host_com_cir_buffer_s *hcom_cbuf, size_t totalCapacity, uint8_t delimiter);
  size_t hcom_cirbuf_avail_space(struct host_com_cir_buffer_s *hcom_cbuf);
  int hcom_cirbuf_add_bytes(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *newBytes, uint32_t bytesToAdd);
  int hcom_cirbuf_get_next_packet(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *packetBuffer,
                                  size_t packetBufferSize, size_t *packetLength);
  int hcom_cirbuf_release_memory(struct host_com_cir_buffer_s *hcom_cbuf);

  int hcom_common_utils_setup(void);
  void hcom_common_utils_shutdown(void);

  // bool hcom_utils_boot_time_qemu_check(void);
  void hcom_utils_dbg_gpio_1led_update(bool ledOn);
  void hcom_utils_dbg_gpio_8bit_update(uint8_t newValue, bool ledOn);

  // -----------------------------------------------
  // Utility Requests
  int hcom_misc_rqst_setup(void);
  void hcom_misc_rqst_get_device_info(uint32_t userData);
  void hcom_misc_rqst_enter_dfu_mode(uint32_t user_data);

  // -----------------------------------------------
  // Access to battery backed registers
  uint32_t hcom_bbreg_read_bbr_and_right_justify(uint32_t bitMask);
  uint32_t hcom_bbreg_read_bbr(void);
  void hcom_bbreg_write_bbr(uint32_t value);
  void hcom_bbreg_set_bbr_bits(uint32_t value);
  void hcom_bbreg_clear_bbr_bits(uint32_t value);
  void hcom_bbreg_clear_bbr_bits_mono(int nx_access_fd, uint32_t value);
  void hcom_bbreg_clear_then_set_bbr_bits(uint32_t clearBits, uint32_t setBits);
  bool hcom_bbreg_is_bbr_bits_set_n_clear(uint32_t value);
  bool hcom_bbreg_is_bbr_bit_set(uint32_t value);

  // -----------------------------------------------
  // HCOM nx (nuttx) access allows low-level access to operating system resources
  int hcom_via_nx_upd_setup(void);
  int hcom_via_nx_get_fd(void);
  int hcom_via_nx_upd_driver_open(void);
  void hcom_via_nx_upd_driver_close(int nx_access_fd);
  int hcom_via_nx_set_bbr(int nx_access_fd, uint32_t value);
  int hcom_via_nx_get_bbr(int nx_access_fd, uint32_t *value);
  int hcom_via_nx_update_bbr(int nx_access_fd, uint32_t clearBits, uint32_t setBits);
  int hcom_via_nx_restart_meadow(int nx_access_fd);
  int hcom_via_nx_get_mcu_id(int nx_access_fd, uint8_t uniqueId[12]);
  int hcom_via_nx_get_mcu_ser_numb(int nx_access_fd, char mcuSerNumb[16]);
  int hcom_via_nx_esp32_enter_prog_mode(int nx_access_fd);
  void hcom_via_nx_restore_uart_reconfig(int nx_access_fd, uint32_t uartId);
  int hcom_via_nx_esp32_restart_esp32(int nx_access_fd);
  void hcom_via_nx_diag_fd_inode(int nx_access_fd, int fd);
  void hcom_via_nx_diag_fd_inode_read(int nx_access_fd, int fd, struct inode **inodeOut);
  int hcom_via_nx_gpio_config(int nx_access_fd, int gpioHcomId, uint8_t configValue);
  int hcom_via_nx_gpio_write(int nx_access_fd, int gpioHcomId, uint8_t cmdValue);
  int hcom_via_nx_diag_gpio_config(int nx_access_fd, int gpioHcomId, uint8_t configValue);
  int hcom_via_nx_diag_gpio_write(int nx_access_fd, int gpioHcomId, uint8_t cmdValue);
  int hcom_via_nx_diag_gpio_write_byte(int nx_access_fd, uint8_t byteValue, uint8_t rangeId);

  void hcom_via_nx_forward_cli_cmd_to_nx(int nx_access_fd, uint16_t hcomCmd, uint32_t userData);
  bool hcom_via_nx_is_mounted(int nx_access_fd, uint32_t partitionId);
  // These exist and work, however, direct registry access is currently
  // not supported.
  // int hcom_via_nx_set_register(uint32_t address, uint32_t value);
  // int hcom_via_nx_get_register(uint32_t address, uint32_t *value);
  // int hcom_via_nx_update_register(uint32_t address, uint32_t clearBits, uint32_t setBits);

  // -----------------------------------------------
  int hcom_diag_logging_setup(void);
  void hcom_diag_logging_shutdown(void);
  int hcom_diag_logging_get_syslog_mask(void);
  void hcom_diag_logging_change_trace_level(uint32_t userData);
  void hcom_logging_syslog(int priority, FAR const IPTR char *fmt, ...);
  void hcom_logging_syslog_x(int priority, FAR const IPTR char *fmt, ...);
  void hcom_logging_safe_ramlog(int priority, FAR const IPTR char *fmt, va_list args);
  int hcom_logging_syslog_mask_init(void);

  //-------------------------------------------------------
  // Ramlog to host/uart
#if defined (CONFIG_RAMLOG_SYSLOG)
  int hcom_diag_trace_ramlog_setup(void);
  void hcom_diag_trace_ramlog_shutdown(void);
  void hcom_diag_trace_ramlog_mono_started(void);
#endif
  void hcom_diag_trace_forward_to_host(uint32_t userData);
  void hcom_diag_trace_do_not_send_to_host(uint32_t userData);
  void hcom_diag_trace_forward_to_uart1(uint32_t userData);
  void hcom_diag_trace_do_not_send_to_uart1(uint32_t userData);

  int hcom_diag_misc_setup(void);
#if HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD > 0
  void hcom_diag_misc_launch_nsh(uint32_t userData);
#endif

  void hcom_diag_misc_print_buffer(const uint8_t packetBuffer[], const int bufLen, uint8_t logPriority);
  void hcom_diag_misc_build_info_from_recvd_msg(uint8_t buffer[], const int bufLen, bool isEncoded);
  void hcom_diag_misc_build_info_from_send_msg(uint8_t buffer[], const int bufLen, bool isEncoded);

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
  int hcom_diag_gpio_setup(void);
  int hcom_diag_gpio_config_all_as_output(void);
  int hcom_diag_gpio_config_one_output(int gpioHcomId);
  int hcom_diag_gpio_output_cmd_led(int ledNumber, bool turnOn);
  int hcom_diag_gpio_write_byte(uint8_t byteValue, uint8_t rangeId);
#endif
  
  //-------------------------------------------------------
  // Testing utilities
  void hcom_developer_tests_developer_1(uint32_t userData);
  void hcom_developer_tests_developer_2(uint32_t userData);
  void hcom_developer_tests_developer_3(uint32_t userData);
  void hcom_developer_tests_developer_4(uint32_t userData);

#if HCOM_INCLUDE_BATTERY_BACKED_REG_TEST > 0
void hcom_bbr_tests(void);
#endif

#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
int MonoVsRemoteDebugTestSetup(int argc, char *argv[]);
#endif

#if defined(CONFIG_EXAMPLES_SQLITE_TESTS)
void hcom_meadow_sqlite_tests(uint32_t userData);
#endif

//------------------------------------------------
#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_HOST_COM_HCOM_COMMON__H
