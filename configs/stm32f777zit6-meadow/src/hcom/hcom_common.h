/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_common.h
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Update the following for each release build
#define HCOM_DEVICE_INFO_PRODUCT "Meadow by Wilderness Labs"
#define HCOM_DEVICE_INFO_MODEL "F7Micro"
#define HCOM_DEVICE_INFO_MEADOW_OS_VERSION "0.3.6"
#define HCOM_DEVICE_INFO_PROCESSOR_TYPE "STM32F777IIK6"
#define HCOM_DEVICE_INFO_COPROCESSOR_TYPE "ESP32"
#define HCOM_DEVICE_INFO_COPROCESSOR_OS_VERSION "0.0.1"
#define HCOM_DEVICE_INFO_MONO_VERSION "0.0.0.1"

#define HCOM_PROTOCOL_CURRENT_VERSION_NUMBER (0x0004)

//---------------------------------------------------------------------
// The code not compiled by this #define could be removed
#define HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS

#define HCOM_COMMUNICATIONS_DEVICE_NAME "/dev/ttyACM0"

#define HCOM_FLASH_FILE_PARTITION_COUNT_MAX 8

#ifdef CONFIG_MTD_PARTITION
#define HCOM_NUMBER_OF_FS_PARTITIONS 2    // Any number 2 - 8
#else
#define HCOM_NUMBER_OF_FS_PARTITIONS 1    // 1 if no partitions in use
#endif

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

// These define how long the host receive thread waits before "waiking up"
#define HCOM_RECV_TIMEOUT_DEFAULT 1 * 60 * 5 //DEBUGGING * 60    // once an hour
#define HCOM_RECV_TIMEOUT_ACTIVE 5           // seconds

#define HCOM_CONNECTION_TIMEOUT_STARTUP 50 * 1000   // At startup we connect quickly
#define HCOM_CONNECTION_TIMEOUT_RUNNING 5000 * 1000 // If no host connection at first wait longer
// How many fast connection attempts during startup before falling to a slower rate
#define HCOM_CONNECTION_STARTUP_ATTEMPTS (1000000 / HCOM_CONNECTION_TIMEOUT_STARTUP) * 5 // 5 seconds

// This defines the largest packet of data to be sent/received
#define HCOM_PROTOCOL_PACKET_MAX_SIZE 512
#define HCOM_CIR_BUFFER_MAX_PACKETS 4
// Based on the encoding scheme (COTS), after encoding there will usually be 2-3 bytes added. One that
// prepends the message and the delimiter of '0'. For messages longer than 254 bytes, another byte may
// be added every 254 bytes.
#define HCOM_SAFE_PACKET_BUF_SIZE (HCOM_PROTOCOL_PACKET_MAX_SIZE + 4 + (HCOM_PROTOCOL_PACKET_MAX_SIZE / 254))
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
// the protocol COULD be modified so that each data packet contains
// this information. This would allow more than one operation to be
// processed at the same time.
// To do this the protocol would need to be enhanced so that command carried
// an additional field to identify the "series" a particular data packet
// belonged to. For each command a unique series number would exist and the 
// the sequence numbers 1-n would be unique for each series.
enum hcom_current_recv_action
{
  CurrentHcomDataPacketActionNone,
  CurrentHcomDataPacketActionExtFileXfer // Could be expanded to specify file type (e.g. mscorlib.dll)
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

#define HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG 0x00000001
#define HCOM_BBREG_DIAG_MSG_TO_HOST_BIT_FLAG 0x00000002

//--------------------------------------------------------------------
// HCOM protocol
// This protocol consists of a header followed by optional data. The header
// is defined by the '#define HCOM_PROTOCOL_REQUEST_HEADER_XXX_XXX' entries
// below.
//
// The first field is the 'Sequence Number'. This field is used for 2 purposes.
// If it's value is 0, it indicates that the entire message is in a single
// packet, containing header and data. This is called a "simple" message type.
// Most messages fit this definition.
// If the sequence number is > 0 it indicates it's a data packet. A data packet
// must have been proceeded by a header whose optional data fields defined
// how the data packets are to be used. A data packet's only requirement is that
// the sequence number is > 0. The remainder of the packet is available for data.
// Following the last data packet a trailer must follow indicting the end.
// Currently, this features is only used by data packets is for copying files.
//
// The second header field is the 'Version' field. This value is updated for each
// change or enhancment to the protocol.
//
// The third header field is 16 2 bytes and after some refactoring is not used.
// Therefore it is 'future'. In the code this is referted to as protocol control.
//
// The fourth header field 'Request Type' which defines the type of message. Each
// message type must have a unique definition.
//
// The fifth and last header field is the 'User Data' field which the user can use
// for any desired purpose. Thus reducing the need for additional, message fields.
//
// There is generally no length field. Since the header is fixed length any additional
// data length is easily determined.

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
#define HCOM_PROTOCOL_REQUEST_HEADER_FILE_SIZE_OFFSET 0
#define HCOM_PROTOCOL_REQUEST_HEADER_FILE_CHKSM_OFFSET 4
#define HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET 8

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
    // partition id), 4-byte file size, 4-byte checksum and variable length
    // destination file name.
    HCOM_PROTOCOL_HEADER_TYPE_FILE = 0x0200,

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
    HCOM_MDOW_REQUEST_NO_DIAG_TO_HOST         = 0x14 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_SEND_SYSLOG_TO_HOST     = 0x15 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,

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
    HCOM_MDOW_REQUEST_START_FILE_TRANSFER     = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_FILE,
    HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME     = 0x02 | HCOM_PROTOCOL_HEADER_TYPE_FILE,
    
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
    HCOM_HOST_REQUEST_DEBUGGER_MSG            = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY,
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
    HCOM_HOST_REQUEST_TEXT_MEADOW_DIAG        = 0x0B | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_RECONNECT          = 0x0C | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
  };

  struct HcomProtocolHeader_s
  {
    uint16_t seqNumber;
    uint16_t version;
    uint16_t control;
    uint16_t rqstType;
    uint32_t userData;
  } __attribute__((packed));


  struct host_com_cir_buffer_s
  {
    uint8_t *bottom; // bottom of buffer
    uint8_t *top;    // top end of buffer
    uint8_t *head;   // add data here
    uint8_t *tail;   // remove from here
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

  // USB CDC/ACM host interface
  int hcom_comms_setup(void);
  void hcom_comms_shutdown(void);
  int hcom_comms_open_connection(void);
  int hcom_comms_handle_initial_connection(void);
  int hcom_comms_recv_thread_loop(void);
  int hcom_comms_transmit_to_host(FAR uint8_t xmitBuffer[], size_t xmitLength);
  bool hcom_comms_was_host_xmit_blocked(void);

  // Host message builder
  int hcom_comms_msg_builder_setup(void);
  void hcom_comms_msg_builder_shutdown(void);
  int hcom_comms_send_header_msg(uint16_t requestType, uint32_t userData);
  int hcom_comms_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText);
  int hcom_comms_send_raw_string_msg(uint16_t requestType, uint32_t userData, char *shortText, size_t msgLength);
  int hcom_comms_send_simple_buffer_msg(uint16_t requestType, uint16_t protocolCtrl, uint32_t userData, uint8_t *msgBuffer, size_t msgLen);

  // Save and Parse request
  int hcom_save_parse_request_setup(void);
  void hcom_save_parse_request_shutdown(void);
  int hcom_comms_recv_process_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt);

  // Execute Request for downloaded file
  int hcom_exec_rqst_download_file_rqst_setup(void);
  bool hcom_exec_rqst_download_is_download_active(void);
  void hcom_exec_rqst_download_file_rqst_start(const uint8_t *recvPacketData,
      const size_t recvPacketDataSize,uint32_t partitionId);
  void hcom_exec_rqst_download_file_rqst_end(uint32_t user_data);
  void hcom_exec_rqst_download_data_packet(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb);

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
  void hcom_exec_rqst_misc_no_diag_msg_to_host(uint32_t userData);
  void hcom_exec_rqst_misc_send_diag_to_host(uint32_t userData);

  void hcom_exec_rqst_misc_mono_disable(uint32_t userData);
  void hcom_exec_rqst_misc_mono_enable(uint32_t userData);
  void hcom_exec_rqst_misc_mono_run_state(uint32_t userData);
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
  int hcom_cirbuf_init(struct host_com_cir_buffer_s *hcom_cbuf, size_t totalCapacity);
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
  bool hcom_utils_bbreg_bit_test_and_clear(uint32_t regNumber, uint32_t value);
  void hcom_utils_bbreg_bit_set(uint32_t regNumber, uint32_t value);
  bool hcom_utils_bbreg_bit_test(uint32_t regNumber, uint32_t value);
  void hcom_utils_bbreg_bit_clear(uint32_t regNumber, uint32_t value);
  void hcom_utils_print_header(const uint8_t buffer[], const int bufLen, uint8_t logPriority);
  void hcom_utils_diag_print_buffer(const uint8_t packetBuffer[], const int bufLen, uint8_t logPriority);
  char* hcom_utils_decode_xmit_to_host(uint16_t requestType, char* requestTypeText);
  bool hcom_utils_boot_time_qemu_check(void);
  void hcom_utils_boot_time_mono_check(void);
  bool hcom_utils_is_mono_disabled(void);
  void f7syslog(int priority, FAR const IPTR char *fmt, ...);
  void f7syslog_x(int priority, FAR const IPTR char *fmt, ...);
  void f7syslog_host(int priority, FAR const IPTR char *fmt, ...);
  
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
