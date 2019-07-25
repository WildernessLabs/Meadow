/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_processcmd.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"

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

static void hcom_execute_host_command_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//====================================================================
int hcom_parse_request_setup()
{
  return OK;
}

//====================================================================
// Parse and process received packet as sent by host
// 1) Check sequence number - return on error
// 2) Remove sequence number and process as needed
int hcom_parse_request_and_process(const uint8_t *packet, const size_t packetSize)
{
  uint8_t msgOffset = 0;

  // Recover sequence number and "remove" from packet
  uint16_t seqNumb = packet[msgOffset] + (packet[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  f7syslog(LOG_INFO, "\n  ------- Processing Decoded Packet (seq numb:%d, length:%d bytes  -------\n", seqNumb, packetSize); 
  hcom_diag_print_buffer(packet, packetSize, LOG_DEBUG);

  if (seqNumb == HCOM_PROTOCOL_REQUEST_HDR_SEQ_NUMBER)
  {
    // Request packet
    hcom_execute_host_command_type(packet + msgOffset, packetSize - msgOffset);
  }
  else
  {
    // Data Packet (Sequence number > 0)
    hcom_exec_download_data_packet(packet, packetSize, seqNumb);
  }

  return OK;
}

//========================================================================
// Process a communications command message (aka header)
void hcom_execute_host_command_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize)
{
  uint8_t msgOffset = 0;
  uint16_t requestType = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  uint32_t userData = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8) +
                      (recvOrigData[msgOffset + 2] << 16) + (recvOrigData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  const uint8_t *recvPayload = recvOrigData + msgOffset;
  const size_t recvPayloadSize = recvOrigDataSize - msgOffset;

  switch(requestType & HCOM_PROTOCOL_HEADER_TYPE_MASK)
  {
    case HCOM_PROTOCOL_HEADER_TYPE_SIMPLE:
      syslog(LOG_DEBUG, "Header is Simple type\n");
      DEBUGASSERT(recvPayloadSize == 0);
      break;
    case HCOM_PROTOCOL_HEADER_TYPE_FILE:
      DEBUGASSERT(recvPayloadSize != 0);
      syslog(LOG_DEBUG, "Header is File type\n");
      break;
      
    default:
      f7syslog(LOG_ERR, "%s() ERROR: Unknown header type in message 0x%04x\n", __func__, requestType);
  }
  
  switch (requestType)
  {
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:
      hcom_exec_download_file_rqst_start(recvPayload, recvPayloadSize, userData);
      break;

    case HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME:
      hcom_exec_flash_fs_delete(recvPayload, recvPayloadSize, userData);
      break;

    case HCOM_MDOW_REQUEST_END_FILE_TRANSFER:
      hcom_exec_download_file_rqst_end(userData);
      break;

    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:
      hcom_exec_utility_request_flash_bulk_erase(userData);
      break;

    case HCOM_MDOW_REQUEST_RESET_PRIMARY_MCU:
      hcom_exec_utility_request_mcu_restart(userData);
      break;

    case HCOM_MDOW_REQUEST_ENTER_DFU_MODE:
      hcom_exec_utility_request_enter_dfu_mode(userData);
      break;

    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:
      hcom_exec_utility_request_flash_verify_erase(userData);
      break;

      // Partitions the entire flash chip with the number of partitions that
      // are defined by userData.
    case HCOM_MDOW_REQUEST_PARTITION_FLASH_FS:
      hcom_exec_flash_fs_partition(userData);
      break;

      // Mount the file system for testing.
    case HCOM_MDOW_REQUEST_MOUNT_FLASH_FS:
      hcom_exec_flash_fs_mount(userData);
      break;

    case HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS:
      hcom_exec_flash_fs_format(userData);
      break;

    case HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS:
      hcom_exec_flash_fs_initialize(userData);
      break;

    case HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS:
      hcom_exec_flash_fs_create(userData);
      break;

    case HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL:
      hcom_exec_utility_request_change_trace_level(userData);
      break;

    case HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH:
      hcom_exec_utility_request_enable_disable_nsh(userData);
      break;

    case HCOM_MDOW_REQUEST_LIST_PARTITION_FILES:
      hcom_exec_flash_fs_return_file_list(userData);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_1:
      hcom_exec_utility_developer_1(userData);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_2:
      hcom_exec_utility_developer_2(userData);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_3:
      hcom_exec_utility_developer_3(userData);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_4:
      hcom_exec_utility_developer_4(userData);
      break;

    default:
      f7syslog(LOG_ERR, "%s() ERROR: Received unsupported command type %04x\n", __func__, requestType);
      hcom_diag_print_buffer(recvOrigData, recvOrigDataSize, LOG_ERR);
  }
}
