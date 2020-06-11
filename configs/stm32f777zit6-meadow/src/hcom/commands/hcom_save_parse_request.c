/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_save_parse_request.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#if defined (CONFIG_HCOM_ESP32_COMMS)
#include "../esp32/hcom_esp32_comms.h"
#endif
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static struct host_com_cir_buffer_s *_hcom_cbuf;
static size_t _max_packet_size = HCOM_SAFE_PACKET_BUF_SIZE;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_execute_host_command_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize);
static int hcom_parse_request_and_process(const uint8_t *packet, const size_t packetSize);
static int hcom_comms_recvpull_all_packets_from_buffer(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_save_parse_request_setup()
{
  _shutting_down = false;

  _hcom_cbuf = (struct host_com_cir_buffer_s *)malloc(sizeof(struct host_com_cir_buffer_s));
  if (_hcom_cbuf == NULL)
  {
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-cir buf alloc\n", thisFile, __LINE__);
    return -1;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE,
          HCOM_PROTOCOL_PACKET_DELIMITER_VALUE);
  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-hcom_cirbuf_init\n", thisFile, __LINE__);
    return -1;
  }

  return OK;
}

//====================================================================
void hcom_save_parse_request_shutdown()
{
  _shutting_down = true;

  free(_hcom_cbuf);
}

//=======================================================================
// Add the received data to the circular buffer. It can be added byte by byte
// or several messages at once.
int hcom_comms_recv_process_raw_data(uint8_t recvBuff[], const ssize_t recvByteCnt)
{
  int result;

  if (recvByteCnt == 0)
    return OK;

  // This loop is used to add raw data to the buffer until no more will fit
  for (;;)
  {
    result = hcom_cirbuf_add_bytes(_hcom_cbuf, recvBuff, recvByteCnt);
    if(result == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      hcom_comms_dbg(LOG_DEBUG, "%s@%d-%d bytes added to cir buf\n", thisFile, __LINE__, recvByteCnt);

      // In all valid cases pull all full packets and process them
      break;
    }
    else if (result == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this data
      hcom_utils_f7syslog(LOG_WARNING, "%s@%d-No room in cir buf, pull and retry\n",
              thisFile, __LINE__);
      result = hcom_comms_recvpull_all_packets_from_buffer();
      if (result == HCOM_CIR_BUF_GET_FOUND_MSG)
        continue;   // There should be room now for the failed add

      if (result == HCOM_CIR_BUF_GET_NONE_FOUND || result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        hcom_utils_f7syslog(LOG_ERR, "%s@%d-pull packets from cir buf %d\n",
                 thisFile, __LINE__, result);
        return OK;    // Report and throw data away.
      }
    }
    else if (result == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // Bad argument
      hcom_utils_f7syslog(LOG_ERR, "%s@%d-Bad argument to cir buf\n", thisFile, __LINE__);
      return OK; // Report and throw data away and keep going
    }
    else
    {
      hcom_utils_f7syslog(LOG_ERR, "%s@%d-Unknown cir buf add err:%d\n", thisFile, __LINE__, result);
      return OK; // Report and throw data away and keep going
    }
  }

  // This could be on a separate thread
  result = hcom_comms_recvpull_all_packets_from_buffer();
  return result;
}

//====================================================================
// Pull and process all the complete packets from the circular buffer
int hcom_comms_recvpull_all_packets_from_buffer()
{
  int result;
  // Todo - the memory allocated for this is never freed.
  static uint8_t *packet_dest_buf = NULL;
  static uint8_t *decode_dest_buf = NULL;

  if (packet_dest_buf == NULL)
  {
    packet_dest_buf = (uint8_t *)malloc(_max_packet_size);
    decode_dest_buf = (uint8_t *)malloc(_max_packet_size);
  }

  for (;;)
  {
    size_t packetLength;
    // If buffer too small packetLength will contain the desired size
    result = hcom_cirbuf_get_next_packet(_hcom_cbuf, packet_dest_buf, _max_packet_size, &packetLength);

    if (result == HCOM_CIR_BUF_GET_NONE_FOUND)
      return OK; // Return to receive more data

    DEBUGASSERT(result != HCOM_CIR_BUF_GET_DEST_NO_ROOM);
    DEBUGASSERT(result == HCOM_CIR_BUF_GET_FOUND_MSG);

    // Drop trailing delimiter of 0x00 then decode the packet
    size_t decodedPacketSize = hcom_comms_cobs_decoder(packet_dest_buf, --packetLength, decode_dest_buf);

    if(decodedPacketSize == 0)
      continue;
      
    // Process the received data
    result = hcom_parse_request_and_process(decode_dest_buf, decodedPacketSize);
    if (result == OK)
    {
      continue; // pull next packet
    }
    else if (result < 0)
    {
      hcom_utils_f7syslog(LOG_ERR, "%s@%d-processing data:%d\n", thisFile, __LINE__, result);
      return result;
      // If ever supported NEED TO SEND NAK TO HOST TO RESEND BAD DATA
    }
    else
    {
      hcom_utils_f7syslog(LOG_ERR, "%s@%d-unknown value %d\n",
              thisFile, __LINE__, result);
      return result;
    }
  }
}

//====================================================================
// Parse and process received packet as sent by host
// 1) Grab the sequence number
// 2) Remove sequence number and process as needed
int hcom_parse_request_and_process(const uint8_t *packet, const size_t packetSize)
{
  int msgOffset = 0;

  // Recover sequence number and "remove" from packet
  uint16_t seqNumb = packet[msgOffset] + (packet[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  // The sequence number determines packet type
  if (seqNumb == HCOM_PROTOCOL_REQUEST_HEADER_SIMPLE_SEQ_NUMBER)
  {
    // A non-data packet
    hcom_comms_dbg(LOG_DEBUG, "%s@%d-Non-data seq:%d, len:%d\n", thisFile, __LINE__, seqNumb, packetSize); 
#if HCOM_COMMS_DEBUG > 0
    hcom_utils_diag_print_buffer(packet, packetSize, LOG_DEBUG);
#endif
    hcom_execute_host_command_type(packet + msgOffset, packetSize - msgOffset);
  }
  else
  {
    // Data Packet (sequence number > 0) 
    hcom_comms_dbg(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n", thisFile, __LINE__, seqNumb, packetSize); 
    hcom_exec_rqst_data_packet_recvd(packet, packetSize, seqNumb);
  }

  return OK;
}

//========================================================================
// Parse the manditory header
void hcom_execute_host_command_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize)
{
  int msgOffset = 0;
  
  uint16_t protocolVersion = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  if(protocolVersion != (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
        "Received unsupported protocol version:%04x, expected:%04x",
        protocolVersion, (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER);

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_utils_f7syslog(LOG_ERR, "%s\n", hostMsg);
    hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
    return;
  }

  uint16_t requestType = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

//uint16_t extraData = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  uint32_t userData = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8) +
                      (recvOrigData[msgOffset + 2] << 16) + (recvOrigData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  const uint8_t *recvPayload = recvOrigData + msgOffset;
  const size_t recvPayloadSize = recvOrigDataSize - msgOffset;

  switch (requestType)
  {
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_download_file_rqst_start(recvPayload, recvPayloadSize, userData, requestType);
      break;
      
    // Note: Start file transfer provides the 'Accepted' message and
    // end file transfer the 'Concluded' message
    case HCOM_MDOW_REQUEST_END_FILE_TRANSFER:
      hcom_exec_rqst_download_file_rqst_end(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_delete(recvPayload, recvPayloadSize, userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    //-------------------------------------------------
    // ESP32 follow
    case HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_download_file_rqst_start(recvPayload, recvPayloadSize, userData, requestType);
      break;

    // Note: Start file transfer provides the 'Accepted' message and
    // end file transfer the 'Concluded' message
    case HCOM_MDOW_REQUEST_END_ESP_FILE_TRANSFER:
      hcom_exec_rqst_download_file_rqst_end(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
    
#if defined (CONFIG_HCOM_ESP32_COMMS)
    case HCOM_MDOW_REQUEST_READ_ESP_MAC_ADDRESS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_esp32_exec_read_esp32_mac(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_RESTART_ESP32:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_esp32_exec_restart_esp32(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
#endif

    //---------------------------------------------------
    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_flash_verify_erase(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

      // Partitions the entire flash chip with the number of partitions that
      // are defined by userData.
    case HCOM_MDOW_REQUEST_PARTITION_FLASH_FS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_partition(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

      // Mount the file system for testing.
    case HCOM_MDOW_REQUEST_MOUNT_FLASH_FS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_mount(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_format(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_initialize(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_create(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);

    case HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_change_trace_level(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_enable_disable_nsh(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_LIST_PARTITION_FILES:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_return_file_list(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_return_file_list_with_crc(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_MONO_RUN_STATE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_mono_run_state(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_get_device_info(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_flash_bulk_erase(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    // The following commands send the HCOM_HOST_REQUEST_TEXT_CONCLUDED message when Meadow restarts
    case HCOM_MDOW_REQUEST_RESET_PRIMARY_MCU:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_mcu_restart(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_flash_fs_part_renew_file_system(userData);   // Forces restart
      break;

// NOT IMPLEMENTED
    case HCOM_MDOW_REQUEST_ENTER_DFU_MODE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_enter_dfu_mode(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_MONO_DISABLE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_mono_disable(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_MONO_ENABLE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_mono_enable(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_MONO_FLASH:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_misc_mono_flash(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_NO_TRACE_TO_HOST:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_trace_do_not_send_trace_to_host(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_HOST:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_trace_send_trace_to_host(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_NO_TRACE_TO_UART:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_trace_do_not_send_trace_to_uart1(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_UART:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_trace_send_trace_to_uart1(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
    case HCOM_HOST_REQUEST_MONO_DEBUGGER_MSG:
      // Accepted and concluded not needed here!
      hcom_remote_dbg_recv_host_send_to_mono(recvPayload, recvPayloadSize, userData);
      break;

#if defined(CONFIG_HCOM_MTD_STRESS_TEST)
    case HCOM_MDOW_REQUEST_DEVELOPER_1:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_developer_1(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_2:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_developer_2(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_3:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_developer_3(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_4:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_developer_4(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_INIT:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_flash_qspi_init(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_WRITE:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_flash_qspi_write(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_READ:
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_exec_rqst_testing_flash_qspi_read(userData);
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
#endif

    default:
    {
      char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
      int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Unknown cmd:0x%04x received", requestType);

      DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
      hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, hostMsg,
              thisFile, __LINE__);

      hcom_utils_f7syslog(LOG_ERR, "%s@%d-Received unsupported cmd:0x%04x\n",
             thisFile, __LINE__, requestType);
      hcom_utils_diag_print_buffer(recvOrigData, recvOrigDataSize, LOG_ERR);
      
      hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
    }
  }
}
