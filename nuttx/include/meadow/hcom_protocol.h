/****************************************************************************
 * \include\meadow\hcom_protocol.h
 * 
 *   Copyright (C) 2019 - 2023 Wilderness Labs. All rights reserved.
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

// Protocol versions 6 and below would fail if the protocol version numbers
// did not match so for all versions prior to 7 we will fail if the version
// numbers do not match.
//
// From version 7 and above it will be the responsibility of the method
// being invoked to check the protocol version number and act accordingly.
#define HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER     ((uint16_t) 0x0006)
#define HCOM_PROTOCOL_PREFERRED_VERSION_NUMBER    ((uint16_t) 0x0007)

// Hold the current protocol version number.  This can be used to allow
// communication between older versions of CLI and the OS.
extern uint16_t g_current_hcom_protocol_version;

// COBS needs a specific delimiter. Zero seems to be traditional.
#define HCOM_PROTOCOL_COBS_ENCODING_DELIMITER_VALUE (0x00)

// What sequence number is used to identify a non-data, command message?
#define HCOM_PROTOCOL_COMMAND_TYPE_SEQUENCE_NUMBER (0)

// Note: while the MD5 hash is 128-bits (16-bytes), it is 32 character
// hex string from ESP32
#define HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH (32)

// Note: because 'sizeof' and 'offsetof' are process by the C preprocessor
// and not the compiler, these defines cannot be used in #if statements.

// Define the absolute maximum packet sizes for sent and receive. The length
// on the wire will be a bit longer because it's encoded.
#define HCOM_PROTOCOL_CURRENT_PACKET_MAX_SIZE             8192
#define HCOM_PROTOCOL_MINIMUM_VERSION_PACKET_MAX_SIZE     512

// Allow the protocol to dynamically change the maximum packet size.
extern uint16_t g_current_hcom_maximum_packet_size;

//--------------------------------------------------------------------
// The following structs define the HCOM Protocol Data Messages
//--------------------------------------------------------------------
// Deprecated - This structure should be removed. But, this will take a
// significant breaking change to the Protocol and to CLI. All messages
// should use the standard header defined as HcomProtoFileInfoHdr_t, and this
// structure should never be used.
// FYI: This message type wasn't used to send file data to the host PC only
// to send download data (binary file data) to Meadow.
struct HcomProtoDataMsg_s
{
  // This is the only header
  uint16_t seqNumber;

  // Body just binary data
  uint8_t binData[0];

} __attribute__((packed));

typedef struct HcomProtoDataMsg_s HcomProtoDataMsg_t;

#define HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF (offsetof(HcomProtoDataMsg_t, binData))

//--------------------------------------------------------------------
// The following are used to define HCOM Messages that can be sent/received
//--------------------------------------------------------------------
// This struct defines a standard protocol message header. This type of header
// is used for all message types, except legacy/deprecated HcomProtoDataMsg_t.
struct HcomProtoStdHdr_s
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

typedef struct HcomProtoStdHdr_s HcomProtoStdHdr_t;

#define HCOM_PROTOCOL_STD_HDR_SIZE (sizeof(HcomProtoStdHdr_t))

//--------------------------------------------------------------------
// This structure defines the additional information needed to initiate a file
// download, delete and other file related messages. Many of the following
// fields are not required nor needed for every message type.
// Note: This struct should be broken into 2, 1 for ESP32 and 1 for Nuttx. But
// this would be a breaking change.
struct HcomProtoFileInfoHdr_s
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

typedef struct HcomProtoFileInfoHdr_s HcomProtoFileInfoHdr_t;

#define HCOM_PROTOCOL_FILE_INFO_HDR_SIZE (sizeof(HcomProtoFileInfoHdr_t))
#define HCOM_PROTOCOL_FILE_INFO_NAME_HDR_OFF (offsetof(HcomProtoFileInfoHdr_t, fileName))

//--------------------------------------------------------------------
// Header only message. This is a very popular message type.
struct HcomProtoHdrMsg_s
{
  // This is the only thing in a header only message
  HcomProtoStdHdr_t stdHeader;

} __attribute__((packed));

typedef struct HcomProtoHdrMsg_s HcomProtoHdrMsg_t;
#define HCOM_PROTOCOL_HEADER_MSG_LENGTH (sizeof(HcomProtoHdrMsg_t))

//--------------------------------------------------------------------
// Header plus File Info message
struct HcomProtoFileMsg_s
{
  HcomProtoStdHdr_t stdHeader;

  // Additional information relate to file downloads/uploads
  HcomProtoFileInfoHdr_t fileInfo;

} __attribute__((packed));

typedef struct HcomProtoFileMsg_s HcomProtoFileMsg_t;
#define HCOM_PROTOCOL_FILE_MSG_LENGTH (sizeof(HcomProtoFileMsg_t))

//--------------------------------------------------------------------
// Header plus Text Info
struct HcomProtoTextMsg_s
{
  // This is the only thing in a header only message
  HcomProtoStdHdr_t stdHeader;

  // Some 'simple' messages containing string information.
  char textData[0];   // This can be path name

} __attribute__((packed));

typedef struct HcomProtoTextMsg_s HcomProtoTextMsg_t;

#define HCOM_PROTOCOL_TEXT_MSG_LENGTH (sizeof(HcomProtoTextMsg_t))
#define HCOM_PROTOCOL_TEXT_MSG_START_OFF (offsetof(HcomProtoTextMsg_t, textData))

//--------------------------------------------------------------------
// This diagnostic command message was originally created to allow HCOM to use
// code designed to be used with NSH. Some apps side code is not be able to be
// built/used because Meadow is using the Nuttx protected build. This is
// initially being done to support the 'ping' command.
// Note: The name HcomProtoDiagCmdMsg_s is poor. What's diagnostic about it?
struct HcomProtoDiagCmdMsg_s
{
  HcomProtoStdHdr_t stdHeader;

  // By convention the argument list is comma separated and the first entry
  // is the name of the application to execute. (e.g. ping, wildernesslabs.co)

  // Argument list length. Note: this is the only difference between this
  // struct and struct HcomProtoTextMsg_s. Therefore, they could/should be
  // combined. This would reduce the clutter.
  uint16_t argListLen;

  // Argument list field
  char argListText[0];

} __attribute__((packed));

typedef struct HcomProtoDiagCmdMsg_s HcomProtoDiagCmdMsg_t;

#define HCOM_PROTOCOL_DIAG_CMD_ARG_LIST_LEN_OFFSET (offsetof(HcomProtoDiagCmdMsg_t, argListLen))
#define HCOM_PROTOCOL_DIAG_CMD_ARG_LIST_TEXT_OFFSET (offsetof(HcomProtoDiagCmdMsg_t, argListText))

//--------------------------------------------------------------------
// Header plus Binary Info
struct HcomProtoBinMsg_s
{
  // This is the only thing in a header only message
  HcomProtoStdHdr_t stdHeader;

  // Additional binary information for debugging or file data
  uint8_t binData[0];

} __attribute__((packed));

typedef struct HcomProtoBinMsg_s HcomProtoBinMsg_t;
#define HCOM_PROTOCOL_BIN_MSG_LENGTH (sizeof(HcomProtoBinMsg_t))
#define HCOM_PROTOCOL_BIN_DATA_OFFSET (offsetof(HcomProtoBinMsg_t, binData))

//--------------------------------------------------------------------
// What is the amount of space available in a message with only a header?
#define HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN (g_current_hcom_maximum_packet_size - \
          (HCOM_PROTOCOL_HEADER_MSG_LENGTH))

// This is the maximum length of a message that can fit in a single packet
#define HCOM_LARGE_HOST_STRING_BUFF_LENGTH  HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN

// Based on the encoding scheme (COTS), after encoding there will usually be
// 2-3 bytes added for a short message. One that prepends the message and the
// delimiter of '0'. For messages longer than 254 bytes, another byte may be
// added every 254 bytes. What would be a safe size for the receive buffer
// that can hold an encoded message? The COBS encoding can add 2 bytes every
// 254 bytes. Add a fudge factor of 8 for safety.
// Note: The COBS encoded size varies depending on the data type. A file
// containing all null values (assuming the delimiter is null) will need 3
// additional bytes, no matter what the file size. A text file will need to
// insert the protocol delimiter every 254 bytes plus the 3 bytes.
#define HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE (g_current_hcom_maximum_packet_size + \
          (g_current_hcom_maximum_packet_size / 254) + 8)

//--------------------------------------------------------------------------
// HCOM Protocol message type definitions
//--------------------------------------------------------------------------
enum HcomProtoMsgMajorTypes
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
  // ToDo: HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME is miscategorized should be
  // HCOM_PROTOCOL_HEADER_FILE_START_TYPE like HCOM_MDOW_REQUEST_START_FILE_TRANSFER.
  HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME     = 0x1c | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END    = 0x1d | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_MONO_START_DBG_SESSION  = 0x1e | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_GET_DEVICE_NAME         = 0x1f | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // >>> Breaking protocol change.
  // ToDo: HCOM_MDOW_REQUEST_GET_INITIAL_FILE_BYTES is miscategorized should be
  // HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE since it is a header followed by text
  HCOM_MDOW_REQUEST_GET_INITIAL_FILE_BYTES  = 0x20 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND  = 0x21 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_UPLOAD_ABORT_DATA_SEND  = 0x22 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // The file types have the optional data field defined for sending file information
  HCOM_MDOW_REQUEST_START_FILE_TRANSFER     = 0x01 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,
  HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME     = 0x02 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,
  HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER = 0x03 | HCOM_PROTOCOL_HEADER_FILE_START_TYPE,

  // These message are a header followed by text
  HCOM_MDOW_REQUEST_UPLOAD_FILE_INIT        = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_EXEC_DIAG_APP_CMD       = 0x02 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_RTC_SET_TIME_CMD        = 0x03 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_RTC_READ_TIME_CMD       = 0x04 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_RTC_WAKEUP_TIME_CMD     = 0x05 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_LIST_FILES_SUBDIR       = 0x06 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_MDOW_REQUEST_LIST_FILES_SUBDIR_CRC   = 0x07 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,

  // This is a simple type with binary data
  HCOM_MDOW_REQUEST_DEBUGGING_DEBUGGER_DATA = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,

  // >>> Breaking protocol change.
  // This should be move our of the 0xfx range since it has nothing to do with
  // diagnostics
  // Old set developer 4 now used to get file and directory listing. At this
  // time (Oct23) CLIv1 only uses it to find nested temporary files in a fixed
  // set of directories
  HCOM_MDOW_REQUEST_GET_FILES_AND_FOLDERS   = 0xf3 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // Testing QSPI flash
  HCOM_MDOW_REQUEST_QSPI_FLASH_INIT         = 0xf4 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_QSPI_FLASH_WRITE        = 0xf5 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_QSPI_FLASH_READ         = 0xf6 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
  HCOM_MDOW_REQUEST_OTA_REGISTER_DEVICE     = 0xf7 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,

  // Replacing the old set developer level with new request format.
  HCOM_MDOW_REQUEST_DEVELOPER               = 0xf8 | HCOM_PROTOCOL_HEADER_ONLY_TYPE,
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
  
  // The Meadow file name is enclosed in single quotes 'filename' and CLI will
  // need to workout what file was being downloaded and start the download over
  HCOM_HOST_REQUEST_DNLD_FAIL_RESEND        = 0x12 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,
  HCOM_HOST_REQUEST_DEVICE_PUBLIC_KEY       = 0x13 | HCOM_PROTOCOL_HEADER_SIMPLE_TEXT_TYPE,

  // Simple with mono debug data
  HCOM_HOST_REQUEST_DEBUGGING_MONO_DATA     = 0x01 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
  HCOM_HOST_REQUEST_SEND_INITIAL_FILE_BYTES = 0x02 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
  HCOM_HOST_REQUEST_UPLOADING_FILE_DATA     = 0x03 | HCOM_PROTOCOL_HEADER_SIMPLE_BINARY_TYPE,
};

#endif  // __INCLUDE_MEADOW_HCOM_PROTOCOL__H
