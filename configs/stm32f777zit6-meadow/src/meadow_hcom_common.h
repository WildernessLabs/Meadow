/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow_hcom_common.h
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
//#include <sched.h>
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

#ifndef OK
#define OK 0
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_COMMUNICATIONS_DEVICE_NAME "/dev/ttyACM0"
#define HCOM_INVALID_PARTITION_ID_VALUE 0xffffffff

// + 2 so errors can be detected (1 for null, 1 for overrun).
// TODO - revist these values
#define HCOM_MAX_FILE_PATH_NAME_LENGTH 128 + 2 // MAX_PATH    // see nuttx/include/limits.h
#define HCOM_MIN_RECOMMENDED_CONFIG_SMARTFS_MAXNAMLEN 32
#define HCOM_FLASH_FILE_PARTITION_COUNT_MAX 8

#ifdef CONFIG_FS_SMARTFS
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "smartfs"
#define HCOM_FILE_MOUNT_POINT_SOURCE "/dev/smart" // assumes partitioning
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"
#endif

#ifdef CONFIG_FS_NXFFS
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "nxffs"
#define HCOM_FILE_MOUNT_POINT_SOURCE NULL // Some file systems don't need block device
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"
#endif

#ifdef CONFIG_FS_LITTLEFS
#define HCOM_FILE_MOUNT_FILE_SYS_TYPE "littlefs"
#define HCOM_FILE_MOUNT_POINT_SOURCE "/dev/little0"
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"
#endif

// These define how long the host receive thread waits before "waiking up"
#define HCOM_RECV_TIMEOUT_DEFAULT 1 * 60 * 5 //DEBUGGING * 60    // once an hour
#define HCOM_RECV_TIMEOUT_ACTIVE 5           // seconds

#define HCOM_CONNECTION_TIMEOUT_STARTUP 50 * 1000   // At startup we connect quickly
#define HCOM_CONNECTION_TIMEOUT_RUNNING 5000 * 1000 // If no host connection at first wait longer
// How many fast connection attempts during startup before falling to a slower rate
#define HCOM_CONNECTION_STARTUP_ATTEMPTS (1000000 / HCOM_CONNECTION_TIMEOUT_STARTUP) * 5 // 5 seconds

// This defines the largest block of data to be sent/received
#define HCOM_PACKET_MAX_SIZE 256
#define HCOM_CIR_BUFFER_MAX_PACKETS 4
// After encoding, there will usually be 2-3 bytes added. One that prepends the message
// and the delimiter of '0'. For messages longer than 254 bytes, another byte may be
// added every 254 bytes.
#define HCOM_SAFE_PACKET_BUF_SIZE (HCOM_PACKET_MAX_SIZE + 4 + (HCOM_PACKET_MAX_SIZE / 254))
#define HCOM_CIRCULAR_BUF_MEM_SIZE (HCOM_SAFE_PACKET_BUF_SIZE * HCOM_CIR_BUFFER_MAX_PACKETS)

// Circular buffer return values
enum hcom_recv_buffer_return
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
// Protocol support
#define HCOM_PROTOCOL_REQUEST_HDR_SEQ_NUMBER 0

// Unique to SIMPLE header type
#define HCOM_PROTOCOL_REQUEST_REQD_HDR_SEQ_OFFSET 0
#define HCOM_PROTOCOL_REQUEST_REQD_HDR_RQST_TYPE_OFFSET 2
#define HCOM_PROTOCOL_REQUEST_REQD_HDR_USER_DATA_OFFSET 4
#define HCOM_PROTOCOL_REQUEST_REQD_HDR_LENGTH 8

// Unique to FILE header type
#define HCOM_PROTOCOL_REQUEST_FILE_HDR_FILE_SIZE_OFFSET 0
#define HCOM_PROTOCOL_REQUEST_FILE_HDR_FILE_CHKSM_OFFSET 4
#define HCOM_PROTOCOL_REQUEST_FILE_HDR_FILENAME_OFFSET 8

// This enum defines the current processing activity for a data packet
// the protocol COULD be modified so that each data packet contains
// this information. This would also allow more than one operation
// to be processed at the same time.
// To do this the protocol would need to be enhanced so that command carried
// an additional field to identify the "series" a particular data packet
// belonged to. For each command a unique series number would exist and the 
// the sequence numbers 1-n would be unique for each series.
enum hcom_current_recv_action
{
  CurrentHcomDataPacketActionNone,
  CurrentHcomDataPacketActionExtFileXfer // Could be expanded to specify file type (e.g. mscorlib.dll)
};

//-------------------------------------------------------------
// The following are the hcom protocol message types
// The upper 8-bits are used to determine the header type
#define HCOM_REQUEST_HEADER_TYPE_MASK 0xff00

  enum HcomRqstHeaderTypes
  {
    HCOM_REQUEST_HEADER_TYPE_UNDEFINED = 0x0000,
    // Simple request types, include 4-byte user data
    HCOM_REQUEST_HEADER_TYPE_SIMPLE = 0x0100,
    // File related types includes 4-byte user data (used for the
    // destination partition id), 4-byte file size, 4-byte checksum and
    // variable length destition file name.
    HCOM_REQUEST_HEADER_TYPE_FILE = 0x0200,
  };

  // Messages sent to Meadow board
  enum HcomMeadowRequestType
  {
    HCOM_MDOW_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_REQUEST_HEADER_TYPE_UNDEFINED,

    HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS  = 0x01 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL      = 0x02 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS   = 0x03 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_END_FILE_TRANSFER       = 0x04 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_RESET_PRIMARY_MCU       = 0x05 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH     = 0x06 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_PARTITION_FLASH_FS      = 0x07 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MOUNT_FLASH_FS          = 0x08 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS     = 0x09 | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_BULK_FLASH_ERASE        = 0x0a | HCOM_REQUEST_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_ENTER_DFU_MODE          = 0x0b | HCOM_REQUEST_HEADER_TYPE_SIMPLE,

    HCOM_MDOW_REQUEST_START_FILE_TRANSFER     = 0x01 | HCOM_REQUEST_HEADER_TYPE_FILE,
    HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME     = 0x02 | HCOM_REQUEST_HEADER_TYPE_FILE,
  };

#ifndef __ASSEMBLY__

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

  struct host_com_cir_buffer_s
  {
    uint8_t *bottom; // bottom of buffer
    uint8_t *top;    // top end of buffer
    uint8_t *head;   // add data here
    uint8_t *tail;   // remove from here
  };

  /****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

  // Manager
  int hcom_manager_setup(FAR struct mtd_dev_s *mtd);
  void hcom_manager_shutdown(void);
  int hcom_manager_create_worker_thread(void);

  // Receiver
  int hcom_receiver_setup(int fd);
  void hcom_receiver_shutdown(void);
  void hcom_receiver_receive_data_thread(void);
  bool hcom_receiver_is_currently_active(void);

  // Transmitter
  int hcom_transmitter_setup(int fd);
  void hcom_transmitter_shutdown(void);
  int hcom_transmitter_send_text(FAR char xmitBuffer[], size_t xmitLength);
  int hcom_transmitter_send_data(FAR const uint8_t xmitBuffer[], size_t xmitLength);

  // Parse request
  int hcom_parse_request_setup(void);
  int hcom_parse_request_and_process(const uint8_t *packet, const size_t packetSize);

  // Execute host request
  int hcom_execute_request_action_setup(FAR struct mtd_dev_s *mtd);
  void hcom_execute_data_packet(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb);
  void hcom_execute_request_flash_file_xfer_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize, uint32_t user_data);
  void hcom_execute_request_flash_file_xfer_end(uint32_t user_data);
  void hcom_execute_request_flash_bulk_erase(uint32_t user_data);
  void hcom_execute_request_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize, uint32_t user_data);
  void hcom_execute_request_flash_fs_format(uint32_t user_data);
  void hcom_execute_request_mcu_restart(uint32_t user_data);
  void hcom_execute_request_enter_dfu_mode(uint32_t user_data);
  void hcom_execute_request_flash_verify_erase(uint32_t user_data);
  void hcom_execute_request_flash_fs_partition(uint32_t user_data);
  void hcom_execute_request_flash_fs_mount(uint32_t user_data);
  void hcom_execute_request_flash_fs_initialize(uint32_t userData);
  void hcom_execute_request_flash_fs_create(uint32_t userData);
  void hcom_execute_request_change_trace_level(uint32_t userData);

  // File processing
  int hcom_file_processing_setup(void);
  void hcom_file_processing_shutdown(void);
  int hcom_file_processing_open(const uint32_t partitionId, const char *mountPoint, const char *fileName);
  int hcom_file_processing_write(const uint8_t *fileWriteData, const size_t fileWriteSize);
  int hcom_file_processing_close(void);
  int hcom_file_processing_delete_file(const uint32_t partitionId, const char *mountPoint, const char *fileName);

  // File system helper
  int hcom_fs_helper_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *entire_flash_mtd, uint32_t numbOfPartitions);
  int hcom_fs_helper_init_fs_partitions(FAR struct mtd_dev_s *full_block_mtd, uint32_t partitionCount);
  int hcom_fs_helper_verify_erased_flash(FAR struct mtd_dev_s *full_block_mtd);
  int hcom_fs_helper_initialize_fs(uint32_t partitionId);
  int hcom_fs_helper_format_smartfs(uint32_t partitionId);
  int hcom_fs_helper_mount_partitioned_fs(const char *sourceDevice, const char *targetDevice,
                                          const char *fileSystemType, uint32_t partitionId);
  bool hcom_fs_helper_is_fs_mounted(uint32_t partitionId);
  int hcom_fs_helper_setup(void);
  void hcom_fs_helper_shutdown(void);

  // Comms support
  size_t hcom_com_support_cobs_encoder(uint8_t source[], size_t startingOffset, size_t length, uint8_t encoded[]);
  size_t hcom_com_support_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[]);
  int hcom_cirbuf_init(struct host_com_cir_buffer_s *hcom_cbuf, size_t totalCapacity);
  size_t hcom_cirbuf_avail_space(struct host_com_cir_buffer_s *hcom_cbuf);
  int hcom_cirbuf_add_bytes(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *newBytes, uint32_t bytesToAdd);
  int hcom_cirbuf_get_next_packet(struct host_com_cir_buffer_s *hcom_cbuf, uint8_t *packetBuffer,
                                  size_t packetBufferSize, size_t *packetLength);
  int hcom_cirbuf_release_memory(struct host_com_cir_buffer_s *hcom_cbuf);

  // Common Utils
#define HCOM_DIAG_LOG_DEFAULT 0
#define HCOM_DIAG_LOG_NOTICE 1
#define HCOM_DIAG_LOG_NOTICE_INFO 2
#define HCOM_DIAG_LOG_NOTICE_INFO_DEBUG 3

//#define HCOM_MAGIC_REGISTER_DFU_MODE_ADDR ((uint32 *) STM32_RTC_BK0R)// ((uint32_t *) 0x40002850)
#define HCOM_MAGIC_NUMBER_DFU_MODE_ADDR ((uint32_t *) 0x20020000)
//#define HCOM_MAGIC_NUMBER_DFU_MODE_VALUE 0xdff7dff7   // Just some number
#define HCOM_MAGIC_NUMBER_DFU_MODE_VALUE1 0xabcdef12   // Just some number
#define HCOM_MAGIC_NUMBER_DFU_MODE_VALUE2 0x3456789a   // Just some number
#define HCOM_MAGIC_NUMBER_DFU_MODE_VALUE3 0xbcdef123   // Just some number
#define HCOM_MAGIC_NUMBER_DFU_MODE_VALUE4 0x456789ab   // Just some number

  void f7syslog(int priority, FAR const IPTR char *fmt, ...);
  void hcom_diag_print_buffer(const uint8_t packetBuffer[], const int bufLen, uint8_t logPriority);

#endif // __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // __CONFIGS_MEADOW_SRC_MEADOW_HOSTCOM__H