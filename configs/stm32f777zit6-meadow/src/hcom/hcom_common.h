/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_common.h
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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
#ifndef __CONFIGS_MEADOW_SRC_MEADOW_HOSTCOM__H
#define __CONFIGS_MEADOW_SRC_MEADOW_HOSTCOM__H

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
#include <nuttx/kthread.h>

#include "chip/stm32f76xx77xx_memorymap.h"
#include "chip/stm32_rtcc.h"    // battery backed registers and ram

#include "hcom_mono_main.h"

#ifndef OK
  #define OK 0
#endif

#ifndef MIN
#  define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Update the following for each release build
#define HCOM_DEVICE_INFO_PRODUCT "Meadow by Wilderness Labs"
#define HCOM_DEVICE_INFO_MODEL "F7Micro"
#define HCOM_DEVICE_INFO_MEADOW_OS_VERSION "0.3.11"
#define HCOM_DEVICE_INFO_PROCESSOR_TYPE "STM32F777IIK6"
#define HCOM_DEVICE_INFO_COPROCESSOR_TYPE "ESP32"
#define HCOM_DEVICE_INFO_COPROCESSOR_OS_VERSION "0.0.1"
#define HCOM_DEVICE_INFO_MONO_VERSION "0.0.0.1"

//--------------------------------------------------------------------
// Diagnostic aids
#define HCOM_TASK_SHOW_CREATED_TASK_PID_NAME  0   // No effect on size
#define HCOM_COMMON_UTILS_GPIO_TEST_PROBE     0   // No effect on size
#define HCOM_COMMON_UTILS_DIAG_PRINT_BUFFER   0   // No effect on size

// The code not compiled by this #define could be removed
#define HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS

// To reduce Meadow.OS size "syslog(LOG_DEBUG, ...);" messages are optional
#define HCOM_COMMS_DEBUG 0
#if HCOM_COMMS_DEBUG > 0
#define hcom_comms_dbg(...) hcom_utils_f7syslog(__VA_ARGS__)
#define hcom_comms_dbg_x(...) hcom_utils_f7syslog_x(__VA_ARGS__)
#else
#define hcom_comms_dbg(...)
#define hcom_comms_dbg_x(...)
#endif

//---------------------------------------------------------------------
// Thread priorities
#define HCOM_THREAD_PRIORITY_HCOM_RECEIVE 120
#define HCOM_THREAD_NAME_HCOM_RECEIVE "HcomRecv"

// Insure hcom recv thread runs before esp32 recv
#define HCOM_THREAD_PRIORITY_ESP32_RECEIVE (HCOM_THREAD_PRIORITY_HCOM_RECEIVE - 1)
#define HCOM_THREAD_NAME_ESP32_RECEIVE "EspRecv"

// This pipe carries .Net Console.WriteLine output to Host via stdout
#define HCOM_THREAD_PRIORITY_STDOUT_PIPE 120
#define HCOM_THREAD_NAME_STDOUT_PIPE "MonoText"

// This thread is used for remote debugging mono apps
#define HCOM_THREAD_PRIORITY_REMOTE_DBG 120
#define HCOM_THREAD_NAME_REMOTE_DBG "RemoteDbg"

// The ramlog is created by nuttx and contains syslog text
#define HCOM_THREAD_PRIORITY_TRACE_RAMLOG 120
#define HCOM_THREAD_NAME_TRACE_RAMLOG "RamlogRead"

//---------------------------------------------------------------------

// These define how long the receive thread waits before "waking up"
// p-m DON'T FORGET
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS 15
#define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (5 * 60)    // 5 minutes
// #define HCOM_RECV_TIMEOUT_DEFAULT_SECONDS (1 * 60 * 60) // once an hour report hcom thread running
#define HCOM_RECV_TIMEOUT_ACTIVE_SECONDS 5

#define HCOM_CONNECTION_TIMEOUT_STARTUP 50 * 1000   // At startup we connect quickly
#define HCOM_CONNECTION_TIMEOUT_RUNNING 5000 * 1000 // If no host connection at first wait longer
// How many fast connection attempts during startup before falling to a slower rate
#define HCOM_CONNECTION_STARTUP_ATTEMPTS ((1000000 / HCOM_CONNECTION_TIMEOUT_STARTUP) * 5) // 5 seconds

#define HCOM_COMMUNICATIONS_DEVICE_NAME "/dev/ttyACM0"

#define HCOM_TRACE_RAMLOG_DEVICE_NAME "/dev/ramlog"

//---------------------------------------------------------------------
#define HCOM_FLASH_FILE_PARTITION_COUNT_MAX 8

#ifdef CONFIG_MTD_PARTITION
#define HCOM_NUMBER_OF_FS_PARTITIONS 1    // Any number 2 - 8
#else
#define HCOM_NUMBER_OF_FS_PARTITIONS 1    // 1 if no partitions in use
#endif

#define HCOM_FS_MONO_RAW_PARTITION_SIZE 0x200000 // 2MB
#define HCOM_FS_MONO_RUNTIME_FILENAME "Meadow.OS.Runtime.bin"

// "/meadow" is shared by all supported file systems
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"
#ifdef CONFIG_FS_SMARTFS
#define HCOM_MIN_EXPECTED_CONFIG_SMARTFS_MAXNAMLEN 32
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "smartfs"
#define HCOM_FILE_MOUNT_POINT_SOURCE "/dev/smart" // assumes partitioning
#endif

#ifdef CONFIG_FS_LITTLEFS
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "littlefs"
#define HCOM_FILE_MOUNT_POINT_SOURCE "/dev/little"
#define HCOM_FILE_MOUNT_FORCE_FORMAT "forceformat"
#endif

// This defines the largest packet of data to be sent/received
#define HCOM_PROTOCOL_PACKET_MAX_SIZE 512
#define HCOM_CIR_BUFFER_MAX_PACKETS 4
// Based on the encoding scheme (COTS), after encoding there will usually be 2-3 bytes added. One that
// prepends the message and the delimiter of '0'. For messages longer than 254 bytes, another byte may
// be added every 254 bytes.

// Somewhat bigger than necessary but better safe than sorry
#define HCOM_SAFE_PACKET_BUF_SIZE (HCOM_PROTOCOL_PACKET_MAX_SIZE + (HCOM_PROTOCOL_PACKET_MAX_SIZE/2))
#define HCOM_CIRCULAR_BUF_MEM_SIZE (HCOM_SAFE_PACKET_BUF_SIZE * HCOM_CIR_BUFFER_MAX_PACKETS)

// Host text message buffer sizes for text messages
#define HCOM_DECODE_XMIT_RQST_TYPE_LEN 48
#define HCOM_SHORT_HOST_STRING_BUFF_LENGTH 128                  // automatic variable
#define HCOM_MAX_HOST_STRING_BUFF_LENGTH 2048                   // allocate
// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less
#define HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

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

// This enum defines the current processing activity for a data packet
// download.
// The protocol could be modified so that each data packet contains this
// information. This would allow more than one operation to be processed
// at a time.
// To do this the protocol would need to be enhanced so that start download
// command carried an additional field to identify the "series" a particular
// data packet belonged to. Each data packet would be unuque and the
// sequence numbers 1-n would be unique for each series.
enum hcom_current_data_packet_activity
{
  CurrentHcomDataPacketActionNone,
  CurrentHcomDataPacketActionF7FileXfer, // Could be expanded to specify file type (e.g. mscorlib.dll)
  CurrentHcomDataPacketActionEsp32FileXfer,
};

//----------------------------------------------------------------
// Trace level constants
#define HCOM_TRACE_LEVEL_DEFAULT 0
#define HCOM_TRACE_LEVEL_NOTICE 1
#define HCOM_TRACE_LEVEL_NOTICE_INFO 2
#define HCOM_TRACE_LEVEL_NOTICE_INFO_DEBUG 3

//--------------------------------------------------------------------
// Redefine Battery Backed Registers so we know what's what
#define HCOM_BATTERY_BACKED_REG_SYSLOG_MASK   STM32_RTC_BK31R
#define HCOM_BATTERY_BACKED_REG_MONO_ACCESS   STM32_RTC_BK30R
#define HCOM_BATTERY_BACKED_REG_MONO_ACTION   STM32_RTC_BK29R

#define HCOM_BATTERY_BACKED_REG_BIT_FLAGS     STM32_RTC_BK28R
// This bit indicates if the restart was initiated by hcom command
#define HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG 0x00000001
// This bit indicates if we are to send trace messages to the host PC
#define HCOM_BBREG_TRACE_MSG_TO_HOST_BIT_FLAG 0x00000002
// This bit indicates if we are to send trace messages to the uart1
#define HCOM_BBREG_TRACE_MSG_TO_UART1_BIT_FLAG 0x00000004

//--------------------------------------------------------------------
// HCOM protocol
// This protocol consists of a header followed by optional data. The header
// is defined by the '#define HCOM_PROTOCOL_REQUEST_HEADER_XXX_XXX' entries
// below.
//
// Header Fields
// The first field is the 'Sequence Number'. This field is used for 2 purposes.
// If it's value is 0, it indicates that the entire message is in a single
// packet, containing header plus optionally, some data. This is called a "simple"
// message type. Most messages fit this category.
// If the sequence number is > 0 it indicates it's a data packet. A data packet
// must have been proceeded by a header whose optional data fields defined
// how the, soon coming, data packets are to be used. A data packet's only 
// requirement is that the sequence number is > 0. The remainder of the packet
// is available for data.
// Following the last data packet a message indicating the end must follow.
// This ending packet will have a sequence number of zero, just as the header
// did. Currently, this features is only used for sending file data.
//
// Non-data Messages
// As explained above the first 2-byte field has a value of zero (0).
//
// The second header field is a 2-byte 'Version' field. This value is updated
// for each change or enhancment to the protocol.
//
// The third header field is a 2-byte 'Request Type' which defines the type of
// message. Each message type has a unique definition.
//
// The fourth header field is a 2-byte that is for protocol use and called 'extraData'.
//
// The fifth and last header field is a 4-byte 'User Data' field which can used
// for any request specific purpose.
//
// There is no length field. Since the header is fixed length any additional data
// length is easily determined.
//
// Currently, the 2-byte version field is considered a single number which is
// incremented for each protocol change.
#define HCOM_PROTOCOL_HCOM_VERSION_NUMBER   ((uint16_t) 0x0006)
#define HCOM_PROTOCOL_VERSION_CRITICAL_MASK  ((uint16_t) 0xff00)
#define HCOM_PROTOCOL_VERSION_FEATURE_MASK  ((uint16_t) 0x00ff)

#define HCOM_PROTOCOL_REQUEST_HEADER_SIMPLE_SEQ_NUMBER 0

// This are the offsets to the header elements
#define HCOM_PROTOCOL_REQUEST_HEADER_SEQ_OFFSET 0
#define HCOM_PROTOCOL_REQUEST_HEADER_VERSION_OFFSET 2
#define HCOM_PROTOCOL_REQUEST_HEADER_CONTROL_OFFSET 4
#define HCOM_PROTOCOL_REQUEST_HEADER_RQST_TYPE_OFFSET 6
#define HCOM_PROTOCOL_REQUEST_HEADER_USER_DATA_OFFSET 8
#define HCOM_PROTOCOL_REQUEST_HEADER_LENGTH 12

#define HCOM_PROTOCOL_PACKET_DELIMITER_VALUE (0x00)

#define HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN (HCOM_PROTOCOL_PACKET_MAX_SIZE - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH)

// Unique to FILE type data field definitions
#define HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH 32
#define HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET 44

// The following are the hcom protocol message types
// The upper 8-bits are used to determine the header type
#define HCOM_PROTOCOL_HEADER_TYPE_MASK 0xff00

  enum HcomProtocolHeaderTypes
  {
    HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED = 0x0000,

    // Simple request types, include 4-byte user data. The User data field
    // is type dependent
    HCOM_PROTOCOL_HEADER_TYPE_SIMPLE = 0x0100,

    // File related types includes 4-byte user data (used for the destination
    // partition id), 4-byte file size, 4-byte checksum, 4-byte destination address
    // and variable length destination file name. Note: The  4-byte destination address
    // is currently only used for the STM32F7 to ESP32 downloads.
    HCOM_PROTOCOL_HEADER_TYPE_FILE_START = 0x0200,

    // Simple text. The text will fit in the header extension
    HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT = 0x0300,

    // Header followed by binary data. The size of the data can be up to
    // HCOM_PROTOCOL_PACKET_MAX_SIZE minus header size
    HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY = 0x0400,
  };

  // Messages sent from host to Meadow 
  enum HcomMeadowRequestType
  {
    HCOM_MDOW_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED,

    HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS  = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL      = 0x02 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS   = 0x03 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_END_FILE_TRANSFER       = 0x04 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_RESET_PRIMARY_MCU       = 0x05 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH     = 0x06 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_PARTITION_FLASH_FS      = 0x07 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MOUNT_FLASH_FS          = 0x08 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS     = 0x09 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_BULK_FLASH_ERASE        = 0x0a | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_ENTER_DFU_MODE          = 0x0b | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH      = 0x0c | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_LIST_PARTITION_FILES    = 0x0d | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC = 0x0e | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_DISABLE            = 0x0f | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_ENABLE             = 0x10 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_RUN_STATE          = 0x11 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION  = 0x12 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS     = 0x13 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_NO_TRACE_TO_HOST        = 0x14 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_SEND_TRACE_TO_HOST      = 0x15 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_END_ESP_FILE_TRANSFER   = 0x16 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_READ_ESP_MAC_ADDRESS    = 0x17 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_RESTART_ESP32           = 0x18 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_FLASH              = 0x19 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_SEND_TRACE_TO_UART      = 0x1a | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_NO_TRACE_TO_UART        = 0x1b | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,

    // Only used for testing
    HCOM_MDOW_REQUEST_DEVELOPER_1             = 0xf0 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_DEVELOPER_2             = 0xf1 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_DEVELOPER_3             = 0xf2 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_DEVELOPER_4             = 0xf3 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    // Testing QSPI flash
    HCOM_MDOW_REQUEST_S25FL_QSPI_INIT         = 0xf4 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_S25FL_QSPI_WRITE        = 0xf5 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_S25FL_QSPI_READ         = 0xf6 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,

    // The file types have the optional data field defined for sending file information
    HCOM_MDOW_REQUEST_START_FILE_TRANSFER     = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_FILE_START,
    HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME     = 0x02 | HCOM_PROTOCOL_HEADER_TYPE_FILE_START,
    HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER = 0x03 | HCOM_PROTOCOL_HEADER_TYPE_FILE_START,
    
    // This is a simple type with binary data
    HCOM_MDOW_REQUEST_DEBUGGER_MSG            = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY,
  };

  // Messages sent from meadow to host
  enum HcomHostRequestType
  {
    HCOM_HOST_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED,

    // Simple types
    HCOM_HOST_REQUEST_HEADER_MESSAGE          = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,    // Just the header
    // Simple with mono debug data
    HCOM_HOST_REQUEST_MONO_DEBUGGER_MSG       = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY,
    // Simple with some text message
    HCOM_HOST_REQUEST_TEXT_REJECTED           = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_ACCEPTED           = 0x02 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_CONCLUDED          = 0x03 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_ERROR              = 0x04 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_INFORMATION        = 0x05 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_LIST_HEADER        = 0x06 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_LIST_MEMBER        = 0x07 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_CRC_MEMBER         = 0x08 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_MONO_MSG           = 0x09 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_DEVICE_INFO        = 0x0A | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_TRACE_MSG          = 0x0B | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_RECONNECT          = 0x0C | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
  };

  struct HcomProtocolHeader_s
  {
    uint16_t seqNumber;
    uint16_t version;
    uint16_t rqstType;
    uint16_t extraData;
    uint32_t userData;
  } __attribute__((packed));


  struct host_com_cir_buffer_s
  {
    uint8_t *bottom;    // bottom of buffer
    uint8_t *top;       // top end of buffer
    uint8_t *head;      // add data here
    uint8_t *tail;      // remove from here
    uint8_t delimiter;  // custom message delimiter
  };

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

  // Startup Manager
  int hcom_manager_setup(FAR struct mtd_dev_s *mtd);
#if defined(CONFIG_STM32F7_PWR)
  int hcom_manager_syslog_mask_init(void);
#endif

  // USB CDC/ACM receive host messages
  int hcom_comms_recv_setup(void);
  void hcom_comms_recv_shutdown(void);
  int hcom_comms_recv_open_connection(void);
  int hcom_comms_recv_restart_concluded(void);
  int hcom_comms_recv_thread_loop(void);
  const char *hcom_comms_recv_get_device_name(void);

  // USB CDC/ACM send host messages
  int hcom_comms_send_setup(void);
  void hcom_comms_send_msg_shutdown(void);
  void hcom_comms_send_header_msg(uint16_t requestType, uint32_t userData,
          char *sourceFileName, int sourceLineNumber);
  void hcom_comms_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
          char *sourceFileName, int sourceLineNumber);
  int hcom_comms_send_raw_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
          size_t msgLength, char *sourceFileName, int sourceLineNumber);

  // Save and Parse request
  int hcom_save_parse_request_setup(void);
  void hcom_save_parse_request_shutdown(void);
  int hcom_comms_recv_process_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt);

  // Execute Request for downloaded file
  int hcom_exec_rqst_download_file_rqst_setup(void);
  bool hcom_exec_rqst_download_is_download_active(void);
  void hcom_exec_rqst_download_file_rqst_start(const uint8_t *recvPacketData,
      const size_t recvPacketDataSize,uint32_t partitionId, uint16_t requestType);
  void hcom_exec_rqst_download_file_rqst_end(uint32_t user_data);
  void hcom_exec_rqst_data_packet_recvd(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb);

  // Execute Flash file system related request
  int hcom_exec_flash_fs_setup(FAR struct mtd_dev_s *mtd);
  void hcom_exec_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize, uint32_t user_data);
  void hcom_exec_flash_fs_format(uint32_t partitionId);
  void hcom_exec_flash_fs_partition(uint32_t numbOfPartitions);
  void hcom_exec_flash_fs_mount(uint32_t user_data);
  void hcom_exec_flash_fs_initialize(uint32_t partitionId);
  void hcom_exec_flash_fs_create(uint32_t userData);
  void hcom_exec_flash_fs_return_file_list(uint32_t partitionId);
  void hcom_exec_flash_fs_part_renew_file_system(uint32_t partitionId);
  void hcom_exec_flash_fs_return_file_list_with_crc(uint32_t partitionId);

  // Execute Utility Request
  int hcom_exec_rqst_misc_setup(FAR struct mtd_dev_s *mtd);
  void hcom_exec_flash_fs_flash_bulk_erase(uint32_t user_data);
  void hcom_exec_flash_fs_flash_verify_erase(uint32_t user_data);
  void hcom_exec_rqst_misc_mcu_restart(uint32_t user_data);
  void hcom_exec_rqst_misc_enter_dfu_mode(uint32_t user_data);
  void hcom_exec_rqst_misc_change_trace_level(uint32_t userData);
  void hcom_exec_rqst_misc_enable_disable_nsh(uint32_t userData);

  void hcom_exec_rqst_misc_mono_disable(uint32_t userData);
  void hcom_exec_rqst_misc_mono_enable(uint32_t userData);
  void hcom_exec_rqst_misc_mono_run_state(uint32_t userData);
  void hcom_exec_rqst_misc_mono_flash(uint32_t userData);
  void hcom_exec_rqst_misc_get_device_info(uint32_t userData);

  // File commands
  int hcom_file_commands_setup(void);
  void hcom_file_commands_shutdown(void);
  bool hcom_file_commands_is_active_file(void);
  int hcom_file_commands_open_active_file(const uint32_t partitionId, const char *mountPoint, const char *fileName);
  int hcom_file_commands_write_to_active_file(const uint8_t *fileWriteData, const size_t fileWriteSize);
  int hcom_file_commands_close_active_file(void);
  uint32_t hcom_file_commands_calc_crc_for_file(char *completeFilePath);
  int hcom_file_commands_delete_by_name(const uint32_t partitionId, const char *mountPoint, const char *fileName);

  // File system helper
  int hcom_fs_setup(FAR struct mtd_dev_s *mtd);
  int hcom_fs_init_file_system(void);
  void hcom_fs_shutdown(void);
  int hcom_fs_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *master_flash_mtd, uint32_t numbOfPartitions);
  int hcom_fs_init_partitions(FAR struct mtd_dev_s *master_flash_mtd, uint32_t partitionCount);
  int hcom_fs_mount_file_system(const char *sourceDevice, const char *targetDevice,
                                          const char *fileSystemType, uint32_t partitionId, const char *mountCommand);
  bool hcom_fs_is_mounted(uint32_t partitionId);
  int hcom_fs_1st_erase_sector_of_partition(uint32_t partitionId);
  int hcom_fs_get_list_files_in_partition(uint32_t partitionIdn);
  int hcom_fs_get_list_files_in_partition_and_crc(uint32_t partitionId);
  int hcom_fs_initialize_proxy(uint32_t partitionId);
  int hcom_fs_format_proxy(uint32_t partitionId);

  // Support SmartFS
#ifdef CONFIG_FS_SMARTFS
  int hcom_fs_smartfs_setup(void);
  void hcom_fs_smartfs_shutdown(void);
  int hcom_fs_smartfs_init_part_fs(uint32_t partitionId, struct mtd_dev_s *partMtd);
  int hcom_fs_smartfs_mount_format(uint32_t partitionId);
  int hcom_fs_smartfs_format(int partitionId);
#endif

  // Support LittleFS
#ifdef CONFIG_FS_LITTLEFS
  int hcom_fs_littlefs_setup(void);
  int hcom_little_support_init_master_fs(FAR struct mtd_dev_s *master_flash_mtd);
  void hcom_fs_littlefs_shutdown(void);
  #ifdef CONFIG_MTD_PARTITION
  int hcom_fs_littlefs_init_part_fs(uint32_t partitionId, struct mtd_dev_s *partMtd);
  #endif
  int hcom_fs_littlefs_mount_format(uint32_t partitionId);
#endif

  // Comms support, COBS encode and receive circular buffer
  size_t hcom_comms_cobs_encoder(uint8_t source[], size_t startingOffset, size_t length, uint8_t encoded[]);
  size_t hcom_comms_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[]);
  int hcom_cirbuf_init(struct host_com_cir_buffer_s *hcom_cbuf, size_t totalCapacity, uint8_t delimiter);
  size_t hcom_cirbuf_avail_space(struct host_com_cir_buffer_s *hcom_cbuf);
  int hcom_cirbuf_add_bytes(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *newBytes, uint32_t bytesToAdd);
  int hcom_cirbuf_get_next_packet(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *packetBuffer,
                                  size_t packetBufferSize, size_t *packetLength);
  int hcom_cirbuf_release_memory(struct host_com_cir_buffer_s *hcom_cbuf);

  // mono pipe user messages
  int hcom_mono_pipe_setup(void);
  void hcom_mono_pipe_shutdown(void);

  // mono Visual Studio interactions
  int hcom_remote_dbg_setup(void);
  void hcom_remote_dbg_shutdown(void);
  void hcom_remote_dbg_recv_host_send_to_mono(const uint8_t *recvPayload, size_t recvPayloadSize, uint32_t userData);

  // Common Utils and persistent (battery backed) storage functions
  int hcom_utils_setup(void);
  void hcom_utils_shutdown(void);
  void hcom_utils_bbreg_write(uint32_t regNumber, uint32_t value);
  uint32_t hcom_utils_bbreg_read(uint32_t regNumber);
  bool hcom_utils_bbreg_is_bit_set_clear(uint32_t regNumber, uint32_t value);
  void hcom_utils_bbreg_set_bit(uint32_t regNumber, uint32_t value);
  bool hcom_utils_bbreg_is_bit_set(uint32_t regNumber, uint32_t value);
  void hcom_utils_bbreg_clear_bit(uint32_t regNumber, uint32_t value);
  void hcom_utils_diag_print_buffer(const uint8_t packetBuffer[], const int bufLen, uint8_t logPriority);
  bool hcom_utils_boot_time_qemu_check(void);
  void hcom_utils_boot_time_mono_check(void);
  bool hcom_utils_is_mono_disabled(void);
  void hcom_utils_f7syslog(int priority, FAR const IPTR char *fmt, ...);
  void hcom_utils_f7syslog_x(int priority, FAR const IPTR char *fmt, ...);
  void hcom_utils_safe_ramlog(int priority, FAR const IPTR char *fmt, va_list args);
  void hcom_utils_dbg_gpio_1led_update(bool ledOn);
  void hcom_utils_dbg_gpio_8bit_update(uint8_t newValue, bool ledOn);

  // Ramlog to host/uart
#if defined (CONFIG_RAMLOG_SYSLOG)
  int hcom_ramlog_trace_setup(void);
  void hcom_ramlog_trace_shutdown(void);
#endif
  void hcom_trace_send_trace_to_host(uint32_t userData);
  void hcom_trace_do_not_send_trace_to_host(uint32_t userData);
  void hcom_trace_send_trace_to_uart1(uint32_t userData);
  void hcom_trace_do_not_send_trace_to_uart1(uint32_t userData);

  // Testing utilities
  int hcom_exec_rqst_testing_setup(FAR struct mtd_dev_s *mtd);
  void hcom_exec_rqst_testing_flash_qspi_init(uint32_t userData);
  void hcom_exec_rqst_testing_flash_qspi_write(uint32_t userData);
  void hcom_exec_rqst_testing_flash_qspi_read(uint32_t userData);
  void hcom_exec_rqst_testing_developer_1(uint32_t userData);
  void hcom_exec_rqst_testing_developer_2(uint32_t userData);
  void hcom_exec_rqst_testing_developer_3(uint32_t userData);
  void hcom_exec_rqst_testing_developer_4(uint32_t userData);

#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_SRC_MEADOW_HOSTCOM__H
