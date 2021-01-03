/****************************************************************************
 * \include\meadow\hcom_protocol.h
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
#ifndef __INCLUDE_MEADOW_HCOM_PROTOCOL__H
#define __INCLUDE_MEADOW_HCOM_PROTOCOL__H

#include <nuttx/config.h>
#include <nuttx/compiler.h>

#include <stdint.h>

//--------------------------------------------------------------------
// HCOM protocol
// This protocol consists of a header followed by optional data. The header
// is defined by the '#define HCOM_PROTOCOL_REQUEST_HEADER_XXX_XXX' entries
// below.
struct HcomProtocolHeader_s
{
  // Sequence Number (2-bytes)
  // The first 2-byte field is the 'Sequence Number'. This field is used for 2
  // purposes. If it's value is zero (0), it indicates that this message is a
  // non-data message. Non-data messages always contain a header and optionally,
  // additional related information.
  // This is called a "simple" message type. Most messages fit this category.
  //
  // If the sequence number is > 0 it indicates it's a data packet. A data packet
  // must have been proceeded by an earlier non=data packet defining how the, soon
  // to follow data packets are to be used.
  // A data packet's only requirement is that the sequence number is = 0. The
  // remainder of the packet is available for data. Currently, the only features
  // that use data packets are file downloads.
  // After the last data packet a non-data packet indicates the end of the data
  uint16_t seqNumber;

  // Version (2-bytes)
  // The second header field is a 2-byte 'Version' field. This value is updated
  // for each change or enhancment to the protocol.
  // The version field is considered a single number which is incremented for each
  // protocol change, not each new Request Type.
  uint16_t version;

  // Request Type (2-bytes)
  // The third header field is a 2-byte 'Request Type' which defines the type of
  // message. Each message type has a unique definition.
  uint16_t rqstType;

  // Extra (2-bytes)
  // The fourth header field is a 2-byte long and called Extra Data. However, it is
  // no longer used and can be considered 'future'.
  uint16_t extraData;

  // User Data (4-bytes)
  // The fifth and last header field is a 4-byte 'User Data' field which can be
  // used for any purpose specified by the Request Type.
  uint32_t userData;
} __attribute__((packed));

// There is no length field. Since the packet boundaries are delimited and the
// header is fixed length. Therefore, any additional datas length is easily
// determined.
//
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

// This defines the largest packet of data to be sent/received
#define HCOM_PROTOCOL_PACKET_MAX_SIZE 512
#define HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN (HCOM_PROTOCOL_PACKET_MAX_SIZE - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH)
#define HCOM_PROTOCOL_SAFE_PACKET_BUF_SIZE (HCOM_PROTOCOL_PACKET_MAX_SIZE + (HCOM_PROTOCOL_PACKET_MAX_SIZE/254) + 8)

#define HCOM_PROTOCOL_PACKET_DELIMITER_VALUE (0x00)

// Unique to FILE type data field definitions
#define HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH 32
#define HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET 44

// The following are the hcom protocol message types
// The upper 8-bits are used to determine the header type
#define HCOM_PROTOCOL_HEADER_MAJOR_TYPE_MASK 0xff00
#define HCOM_PROTOCOL_HEADER_MINOR_TYPE_MASK 0x00ff

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
    HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU     = 0x05 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
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
    HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME     = 0x1c | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END    = 0x1d | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_MONO_START_DBG_SESSION  = 0x1e | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,
    HCOM_MDOW_REQUEST_GET_DEVICE_NAME         = 0x1f | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,

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
    HCOM_MDOW_REQUEST_DEBUGGING_DEBUGGER_DATA = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY,
  };

  // Messages sent from meadow to host
  enum HcomHostRequestType
  {
    HCOM_HOST_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED,

    // Simple types
    HCOM_HOST_REQUEST_HEADER_MESSAGE          = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE,    // Just the header
    
    // Simple with some text message
    HCOM_HOST_REQUEST_TEXT_REJECTED           = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_ACCEPTED           = 0x02 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_CONCLUDED          = 0x03 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_ERROR              = 0x04 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_INFORMATION        = 0x05 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_LIST_HEADER        = 0x06 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_LIST_MEMBER        = 0x07 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_CRC_MEMBER         = 0x08 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_MONO_STDOUT        = 0x09 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_DEVICE_INFO        = 0x0A | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_TRACE_MSG          = 0x0B | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_RECONNECT          = 0x0C | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    HCOM_HOST_REQUEST_TEXT_MONO_STDERR        = 0x0D | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT,
    // Simple with mono debug data
    HCOM_HOST_REQUEST_DEBUGGING_MONO_DATA     = 0x01 | HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY,
  };

#endif  // __INCLUDE_MEADOW_HCOM_PROTOCOL__H