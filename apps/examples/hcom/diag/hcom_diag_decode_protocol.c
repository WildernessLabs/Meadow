/****************************************************************************
 * apps\examples\hcom\diag\hcom_diag_decode_protocol.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_protocol.h>

#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
char *hcom_diag_find_meadow_request_type(uint16_t meadowRqstType);
char *hcom_diag_find_host_request_type(uint16_t hostRqstType);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Takes a hcom message and outputs a string containing the header information
void hcom_diag_decode_recvd_message_type(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize)
{
  uint16_t rqstType = hdrMsg->stdHeader.rqstType;
  char *requestStr = hcom_diag_find_meadow_request_type(rqstType);
  syslog(2, "------------- Meadow Received ---------------\n");
  syslog(2, "Received '%s' (0x%04x) %u bytes\n", requestStr,
            rqstType, packetSize);
  hcom_diag_print_buffer((const uint8_t *)hdrMsg, packetSize, 1);
}

char *hcom_diag_find_meadow_request_type(uint16_t rqstType)
{
  switch(rqstType)
  {
    case HCOM_MDOW_REQUEST_UNDEFINED_REQUEST:       return "UNDEFINED_REQUEST";
    case HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL:      return "CHANGE_TRACE_LEVEL";
    case HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS:   return "FORMAT_FLASH_FILE_SYS";
    case HCOM_MDOW_REQUEST_END_FILE_TRANSFER:       return "END_FILE_TRANSFER";
    case HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU:     return "RESTART_PRIMARY_MCU";
    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:     return "VERIFY_ERASED_FLASH";
    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:        return "BULK_FLASH_ERASE";
    case HCOM_MDOW_REQUEST_ENTER_DFU_MODE:          return "ENTER_DFU_MODE";
    case HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH:      return "ENABLE_DISABLE_NSH";
    case HCOM_MDOW_REQUEST_LIST_PARTITION_FILES:    return "LIST_PARTITION_FILES";
    case HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC: return "LIST_PART_FILES_AND_CRC";
    case HCOM_MDOW_REQUEST_MONO_DISABLE:            return "MONO_DISABLE";
    case HCOM_MDOW_REQUEST_MONO_ENABLE:             return "MONO_ENABLE";
    case HCOM_MDOW_REQUEST_MONO_RUN_STATE:          return "MONO_RUN_STATE";
    case HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION:  return "GET_DEVICE_INFORMATION";
    case HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS:     return "PART_RENEW_FILE_SYS";
    case HCOM_MDOW_REQUEST_NO_TRACE_TO_HOST:        return "NO_TRACE_TO_HOST";
    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_HOST:      return "SEND_TRACE_TO_HOST";
    case HCOM_MDOW_REQUEST_END_ESP_FILE_TRANSFER:   return "END_ESP_FILE_TRANSFER";
    case HCOM_MDOW_REQUEST_READ_ESP_MAC_ADDRESS:    return "READ_ESP_MAC_ADDRESS";
    case HCOM_MDOW_REQUEST_RESTART_ESP32:           return "RESTART_ESP32";
    case HCOM_MDOW_REQUEST_MONO_FLASH:              return "MONO_FLASH";
    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_UART:      return "SEND_TRACE_TO_UART";
    case HCOM_MDOW_REQUEST_NO_TRACE_TO_UART:        return "NO_TRACE_TO_UART";
    case HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME:     return "MONO_UPDATE_RUNTIME";
    case HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END:    return "MONO_UPDATE_FILE_END";
    case HCOM_MDOW_REQUEST_MONO_START_DBG_SESSION:  return "MONO_START_DBG_SESSION";
    case HCOM_MDOW_REQUEST_GET_DEVICE_NAME:         return "GET_DEVICE_NAME";
    case HCOM_MDOW_REQUEST_GET_INITIAL_FILE_BYTES:  return "GET_INITIAL_FILE_BYTES";
    case HCOM_MDOW_REQUEST_UPLOAD_ABORT_DATA_SEND:  return "ABORT_DATA_SEND";
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:     return "START_FILE_TRANSFER";
    case HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME:     return "DELETE_FILE_BY_NAME";
    case HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER: return "START_ESP_FILE_TRANSFER";
    case HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND:  return "START_SENDING_DATA";
    case HCOM_MDOW_REQUEST_UPLOAD_FILE_INIT:        return "UPLOAD_FILE_INIT";
    case HCOM_MDOW_REQUEST_RTC_SET_TIME_CMD:        return "RTC_SET_TIME";
    case HCOM_MDOW_REQUEST_RTC_READ_TIME_CMD:       return "RTC_READ_TIME";
    case HCOM_MDOW_REQUEST_RTC_WAKEUP_TIME_CMD: return "RTC_SET_WAKEUP_TIME";
    case HCOM_MDOW_REQUEST_DEBUGGING_DEBUGGER_DATA: return "DEBUGGING_DEBUGGER_DATA";
    case HCOM_MDOW_REQUEST_DEVELOPER_1:             return "DEVELOPER_1";
    case HCOM_MDOW_REQUEST_DEVELOPER_2:             return "DEVELOPER_2";
    case HCOM_MDOW_REQUEST_DEVELOPER_3:             return "DEVELOPER_3";
    case HCOM_MDOW_REQUEST_DEVELOPER_4:             return "DEVELOPER_4";
    case HCOM_MDOW_REQUEST_QSPI_FLASH_INIT:         return "QSPI_FLASH_INIT";
    case HCOM_MDOW_REQUEST_QSPI_FLASH_WRITE:        return "QSPI_FLASH_WRITE";
    case HCOM_MDOW_REQUEST_QSPI_FLASH_READ:         return "QSPI_FLASH_READ";
    default:
      return "Meadow Request Type not found";
  }
}

//======================================================================
void hcom_diag_decode_sending_message_type(const uint8_t *hostRawMsg,
          const uint16_t hostRqstType, const size_t packetSize)
{
  char *requestStr = hcom_diag_find_host_request_type(hostRqstType);

  syslog(2, "->Sending '%s' (0x%04x) %u bytes\n",
        requestStr, hostRqstType, packetSize);
  hcom_diag_print_buffer(hostRawMsg, packetSize, 1);
}

char *hcom_diag_find_host_request_type(uint16_t hostRqstType)
{
  switch(hostRqstType)
  {
    case HCOM_HOST_REQUEST_UNDEFINED_REQUEST:      return "UNDEFINED_REQUEST";
    case HCOM_HOST_REQUEST_TEXT_REJECTED:          return "TEXT_REJECTED";
    case HCOM_HOST_REQUEST_TEXT_ACCEPTED:          return "TEXT_ACCEPTED";
    case HCOM_HOST_REQUEST_TEXT_CONCLUDED:         return "TEXT_CONCLUDED";
    case HCOM_HOST_REQUEST_TEXT_ERROR:             return "TEXT_ERROR";
    case HCOM_HOST_REQUEST_TEXT_INFORMATION:       return "TEXT_INFORMATION";
    case HCOM_HOST_REQUEST_TEXT_LIST_HEADER:       return "TEXT_LIST_HEADER";
    case HCOM_HOST_REQUEST_TEXT_LIST_MEMBER:       return "TEXT_LIST_MEMBER";
    case HCOM_HOST_REQUEST_TEXT_CRC_MEMBER:        return "TEXT_CRC_MEMBER";
    case HCOM_HOST_REQUEST_TEXT_MONO_STDOUT:       return "TEXT_MONO_STDOUT";
    case HCOM_HOST_REQUEST_TEXT_DEVICE_INFO:       return "TEXT_DEVICE_INFO";
    case HCOM_HOST_REQUEST_TEXT_TRACE_MSG:         return "TEXT_TRACE_MSG";
    case HCOM_HOST_REQUEST_TEXT_RECONNECT:         return "TEXT_RECONNECT";
    case HCOM_HOST_REQUEST_TEXT_MONO_STDERR:       return "TEXT_MONO_STDERR";
    case HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY:     return "FILE_START_OKAY";
    case HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL:     return "FILE_START_FAIL";
    case HCOM_HOST_REQUEST_INIT_UPLOAD_OKAY:       return "INIT_UPLOAD_OKAY";
    case HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL:       return "INIT_UPLOAD_FAIL";
    case HCOM_HOST_REQUEST_DEBUGGING_MONO_DATA:    return "DEBUGGING_MONO_DATA";
    case HCOM_HOST_REQUEST_UPLOADING_FILE_DATA:    return "UPLOADING_FILE_DATA";
    default:
      return "Host Request Type not found";
  }
}
#endif
