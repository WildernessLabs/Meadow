/****************************************************************************
 * \include\meadow\hcom_protocol.h
 * 
 *   Copyright (C) 2019 - 2021 Wilderness Labs. All rights reserved.
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

// There is no length field. Since the packet boundaries are delimited and the
// header is fixed length. Therefore, any additional data length is easily
// determined.
#define HCOM_PROTOCOL_HCOM_VERSION_NUMBER   ((uint16_t) 0x0006)

// COBS needs a specific delimiter. Zero seems to be traditional.
#define HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE (0x00)

// What sequence number is used to identify a non-data message?
#define HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER (0)

// Note: while the MD5 hash is 128-bits (16-bytes), it is 32 character
// hex string from ESP32
#define HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH (32)

// Note: because 'sizeof' and 'offsetof' are process by the C preprocessor
// and not the compiler, these defines cannot be used in #if statements.

// Define the absolute maximum packet sizes for sent and receive. The length
// on the wire will be a bit longer because it's encoded.
#define HCOM_PROTOCOL_PACKET_MAX_SIZE 512

//--------------------------------------------------------------------
// The following structs define the HCOM Data Message
//--------------------------------------------------------------------
// This structure defines the data message header. Not much here
struct HcomProtocolDataHeader_s
{
  // If the sequence number is 1 - 65635 it indicates it's a data packet.
  // A data packet's only requirement is that the sequence number increments
  // starting with 1 and moving upward with each data packet.
  uint16_t seqNumber;

} __attribute__((packed));
typedef struct HcomProtocolDataHeader_s HcomProtocolDataHeader_t;

#define HCOM_PROTOCOL_DATA_MSG_HEADER_SIZE (sizeof(HcomProtocolDataHeader_t))

//--------------------------------------------------------------------
// This structure defines the data information. Not much here either
struct HcomProtocolDataInfo_s
{
  uint8_t binData[0];

} __attribute__((packed));
typedef struct HcomProtocolDataInfo_s HcomProtocolDataInfo_t;

#define HCOM_PROTOCOL_DATA_INFO_SIZE (sizeof(HcomProtocolDataInfo_t))
#define HCOM_PROTOCOL_DATA_INFO_BIN_DATA_OFF (offsetof(HcomProtocolDataInfo_t, binData))

//--------------------------------------------------------------------
// Complete HCOM data message
// PeterM-This should be removed and all messages use the standard message format
struct HcomProtocolDataMessage_s
{
  // After the header is binary data for a data message
  HcomProtocolDataHeader_t dataHeader;

  // Body just binary data
  HcomProtocolDataInfo_t dataInfo;

} __attribute__((packed));

typedef struct HcomProtocolDataMessage_s HcomProtocolDataMessage_t;

// WARNING: THIS TYPE OF MESSAGE IS NEVER USED TO SEND DATA TO HOST
#define HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF (offsetof(HcomProtocolDataMessage_t, dataInfo))
#define HCOM_PROTOCOL_DATA_MSG_MAX_DATA_LENGTH (HCOM_PROTOCOL_PACKET_MAX_SIZE - \
                                                HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF)

//--------------------------------------------------------------------
// The following are used to define HCOM Messages that can be sent/received
//--------------------------------------------------------------------
// This structure defines the additional information needed to initiate a file
// download, delete and other file related messages. Many of the following
// fields are not required nor needed for every message type.
struct HcomProtocolFileInfo_s
{
  // File length of the entire file
  uint32_t fileSize;

  // File checksum calculated by sender (not used by ESP32)
  uint32_t fileCheckSum;

  // File flash address (used only by ESP32)
  uint32_t fileFlashAddr;

  // The MD5 Hash is 32 char hex string (used only by ESP32)
  char fileMD5Hash[HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH];

  // File name (variable length field, not used by ESP32)
  char fileName[0];

} __attribute__((packed));

typedef struct HcomProtocolFileInfo_s HcomProtocolFileInfo_t;

#define HCOM_PROTOCOL_FILE_INFO_SIZE (sizeof(HcomProtocolFileInfo_t))
#define HCOM_PROTOCOL_FILE_INFO_NAME_OFF (offsetof(HcomProtocolFileInfo_t, fileName))

//--------------------------------------------------------------------
// This struct defines a command header. This type of header is used for most
// message types.
struct HcomProtocolStdHeader_s
{
  // If the sequence number is zero (0), it indicates that this is a non-data
  // message.Non-data messages always contain basic message related
  // information. Most messages fit this category.
  uint16_t seqNumber;

  // The second header field is the 'Version' field. This value is updated for each
  // breaking change to the protocol.
  // The version field is considered a single number which is incremented for each
  // protocol change.
  uint16_t version;

  // The third header field is the 'Request Type' which defines the type of
  // message. Each message type must have a unique request type. These are
  // defined below.
  uint16_t rqstType;

  // The forth header field is called Extra Data. However, it is no longer
  // used and can be considered 'future'.
  uint16_t extraData;

  // The fifth and last header field is the 'User Data' field. This field can
  // be used for any purpose specified by the Request Type.
  uint32_t userData;

} __attribute__((packed));
typedef struct HcomProtocolStdHeader_s HcomProtocolStdHeader_t;

#define HCOM_PROTOCOL_STD_HEADER_SIZE (sizeof(HcomProtocolStdHeader_t))

//--------------------------------------------------------------------
// Header only
struct HcomProtocolHdrMessage_s
{
  // This is the only thing in a header only message
  HcomProtocolStdHeader_t stdHeader;

} __attribute__((packed));

typedef struct HcomProtocolHdrMessage_s HcomProtocolHdrMessage_t;
#define HCOM_PROTOCOL_HEADER_MSG_LENGTH (sizeof(HcomProtocolHdrMessage_t))

//--------------------------------------------------------------------
// Header plus File Info
struct HcomProtocolFileMessage_s
{
  // This is the only thing in a header only message
  HcomProtocolStdHeader_t stdHeader;

  // Additional information relate to file downloads/uploads
  HcomProtocolFileInfo_t fileInfo;

} __attribute__((packed));

typedef struct HcomProtocolFileMessage_s HcomProtocolFileMessage_t;
#define HCOM_PROTOCOL_FILE_MSG_LENGTH (sizeof(HcomProtocolFileMessage_t))

//--------------------------------------------------------------------
// Header plus Text  Info
struct HcomProtocolTextMessage_s
{
  // This is the only thing in a header only message
  HcomProtocolStdHeader_t stdHeader;

  // Some 'simple' messages contain string information.
  char textData[0];

} __attribute__((packed));

typedef struct HcomProtocolTextMessage_s HcomProtocolTextMessage_t;
#define HCOM_PROTOCOL_TEXT_MSG_LENGTH (sizeof(HcomProtocolFileMessage_t))
#define HCOM_PROTOCOL_TEXT_MSG_START_OFF (offsetof(HcomProtocolTextMessage_t, textData))

//--------------------------------------------------------------------
// Header plus Binary Info
struct HcomProtocolBinMessage_s
{
  // This is the only thing in a header only message
  HcomProtocolStdHeader_t stdHeader;

  // Additional binary information for debugging or file data
  uint8_t binData[0];

} __attribute__((packed));

typedef struct HcomProtocolBinMessage_s HcomProtocolBinMessage_t;
#define HCOM_PROTOCOL_BIN_MSG_LENGTH (sizeof(HcomProtocolBinMessage_t))
#define HCOM_PROTOCOL_BIN_DATA_OFFSET (offsetof(HcomProtocolBinMessage_t, binData))

//--------------------------------------------------------------------
// What is the amount of space available in a message with only a header?
#define HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN (HCOM_PROTOCOL_PACKET_MAX_SIZE - \
          (HCOM_PROTOCOL_HEADER_MSG_LENGTH))

// This is the maximum length of a message that can fit in a single packet
#define HCOM_LARGE_HOST_STRING_BUFF_LENGTH  HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN

// Based on the encoding scheme (COTS), after encoding there will usually be
// 2-3 bytes added. One that prepends the message and the delimiter of '0'. For
// messages longer than 254 bytes, another byte may be added every 254 bytes.
// What would be a safe size for the receive buffer that can hold an encoded
// message? The COBS encoding can add 2 bytes every 254 bytes. Add a fudge
// factor of 8 for safety.
#define HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE (HCOM_PROTOCOL_PACKET_MAX_SIZE + \
          (HCOM_PROTOCOL_PACKET_MAX_SIZE/254) + 8)

//--------------------------------------------------------------------------
// HCOM Protocol message type definitions
//--------------------------------------------------------------------------
enum HcomProtocolMsgMajorTypes
{
  //When the time comes the following Major types should reflect the
  // name of the above structure is used to send it. The following are
  // close but some of the following are miscategorized
  HCOM_PROTOCOL_HEADER_UNDEFINED_TYPE = 0x0000,

  // The header of all mesasges include a 4-byte field called user data. The
  // User data field's meaning is determined by the message type
  
  // Header only request types,
  HCOM_PROTOCOL_HEADER_ONLY_TYPE = 0x0100,

  // File related types includes 4-byte user data (used for the destination
  // partition id), 4-byte file size, 4-byte checksum, 4-byte destination address
  // and variable length destination file name. Note: The  4-byte destination address
  // is currently only used for the STM32F7 to ESP32 downloads.
  HCOM_PROTOCOL_HEADER_FILE_START_TYPE = 0x0200,

  // Simple text is a header followed by text without a terminating NULL.
  HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE = 0x0300,

  // Simple binary is a header followed by binary data. The size of the data
  // can be up to HCOM_PROTOCOL_PACKET_MAX_SIZE minus header size
  HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE = 0x0400,
};

// Messages sent from host to Meadow 
enum HcomMeadowRequestType
{
  HCOM_MDOW_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_PROTOCOL_HEADER_UNDEFINED_TYPE,

  // No longer supported
  // HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS  = 0x01 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL      = 0x02 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS   = 0x03 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_END_FILE_TRANSFER       = 0x04 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU     = 0x05 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH     = 0x06 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  // No longer supported
  // HCOM_MDOW_REQUEST_PARTITION_FLASH_FS      = 0x07 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  // No longer supported
  // HCOM_MDOW_REQUEST_MOUNT_FLASH_FS          = 0x08 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  // No longer supported
  // HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS     = 0x09 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_BULK_FLASH_ERASE        = 0x0a | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_ENTER_DFU_MODE          = 0x0b | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH      = 0x0c | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_LIST_PARTITION_FILES    = 0x0d | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC = 0x0e | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_DISABLE            = 0x0f | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_ENABLE             = 0x10 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_RUN_STATE          = 0x11 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION  = 0x12 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS     = 0x13 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_NO_TRACE_TO_HOST        = 0x14 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_SEND_TRACE_TO_HOST      = 0x15 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_END_ESP_FILE_TRANSFER   = 0x16 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_READ_ESP_MAC_ADDRESS    = 0x17 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_RESTART_ESP32           = 0x18 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_FLASH              = 0x19 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_SEND_TRACE_TO_UART      = 0x1a | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_NO_TRACE_TO_UART        = 0x1b | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // >>> Breaking protocol change.
  // ToDo: This message is miscategorized should be HCOM_PROTOCOL_HEADER_FILE_START_TYPE
  // like HCOM_MDOW_REQUEST_START_FILE_TRANSFER.
  HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME     = 0x1c | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END    = 0x1d | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_START_DBG_SESSION  = 0x1e | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_GET_DEVICE_NAME         = 0x1f | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // >>> Breaking protocol change.
  // ToDo: This message is miscategorized should be HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE
  // since it is a header followed by text (the file name)
  HCOM_MDOW_REQUEST_GET_INITIAL_FILE_BYTES  = 0x20 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND  = 0x21 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_UPLOAD_ABORT_DATA_SEND  = 0x22 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // The file types have the optional data field defined for sending file information
  HCOM_MDOW_REQUEST_START_FILE_TRANSFER     = 0x01 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,
  HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME     = 0x02 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,
  HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER = 0x03 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,

  // These message are a header followed by text
  HCOM_MDOW_REQUEST_UPLOAD_INITIALIZE       = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  
  // This is a simple type with binary data
  HCOM_MDOW_REQUEST_DEBUGGING_DEBUGGER_DATA = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,

  // Only used for testing
  HCOM_MDOW_REQUEST_DEVELOPER_1             = 0xf0 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_DEVELOPER_2             = 0xf1 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_DEVELOPER_3             = 0xf2 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_DEVELOPER_4             = 0xf3 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  // Testing QSPI flash
  HCOM_MDOW_REQUEST_QSPI_FLASH_INIT         = 0xf4 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_QSPI_FLASH_WRITE        = 0xf5 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_QSPI_FLASH_READ         = 0xf6 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
};

// Messages sent from meadow to host
enum HcomHostRequestType
{
  HCOM_HOST_REQUEST_UNDEFINED_REQUEST       = 0x00 | HCOM_PROTOCOL_HEADER_UNDEFINED_TYPE,

  // Only header
  HCOM_HOST_REQUEST_UPLOAD_FILE_COMPLETED   = 0x01 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  
  // Simple with some text message
  HCOM_HOST_REQUEST_TEXT_REJECTED           = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_ACCEPTED           = 0x02 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_CONCLUDED          = 0x03 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_ERROR              = 0x04 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_INFORMATION        = 0x05 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_LIST_HEADER        = 0x06 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_LIST_MEMBER        = 0x07 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_CRC_MEMBER         = 0x08 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_MONO_STDOUT        = 0x09 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_DEVICE_INFO        = 0x0A | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_TRACE_MSG          = 0x0B | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_RECONNECT          = 0x0C | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_TEXT_MONO_STDERR        = 0x0D | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,

  HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY      = 0x0E | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL      = 0x0F | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,

  HCOM_HOST_REQUEST_INIT_UPLOAD_OKAY        = 0x10 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL        = 0x11 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,

  // Simple with mono debug data
  HCOM_HOST_REQUEST_DEBUGGING_MONO_DATA     = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
  HCOM_HOST_REQUEST_SEND_INITIAL_FILE_BYTES = 0x02 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
  HCOM_HOST_REQUEST_UPLOADING_FILE_DATA     = 0x03 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
};

#endif  // __INCLUDE_MEADOW_HCOM_PROTOCOL__H
