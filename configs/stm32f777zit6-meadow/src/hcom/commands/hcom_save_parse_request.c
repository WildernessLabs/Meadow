/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_save_parse_request.c
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

#include "../hcom_common.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_COMMS_DEBUG 0
#if HCOM_COMMS_DEBUG > 0
#define hcom_comms_dbg(...) f7syslog_x(__VA_ARGS__)
#else
#define hcom_comms_dbg(...)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

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
    f7syslog(LOG_ERR, "%s() ERROR: circular buffer allocation failed\n", __func__);
    return -1;
  }

  int result = hcom_cirbuf_init(_hcom_cbuf, HCOM_CIRCULAR_BUF_MEM_SIZE);
  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_cirbuf_init failed\n", __func__);
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
      hcom_comms_dbg(LOG_DEBUG, "%d bytes added to circular buffer\n", recvByteCnt);

      // In all valid cases pull all full packets and process them
      break;
    }
    else if (result == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this data
      f7syslog(LOG_WARNING, "%s() WARNING: No room in circular buffer, will pull and try again\n", __func__);
      result = hcom_comms_recvpull_all_packets_from_buffer();
      if (result == HCOM_CIR_BUF_GET_FOUND_MSG)
        continue;   // There should be room now for the failed add

      if (result == HCOM_CIR_BUF_GET_NONE_FOUND || result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        f7syslog(LOG_ERR, "%s() ERROR: Unexpected error from attempt to pull all packets from circular buffer %d\n",
                 __func__, result);
        return OK;    // Report and throw data away.
      }
    }
    else if (result == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
      // Bad argument
      f7syslog(LOG_ERR, "%s() ERROR: Bad argument passed to circular buffer\n", __func__);
      return OK; // Report and throw data away and keep going
    }
    else
    {
      f7syslog(LOG_ERR, "%s() ERROR: Unknown result %d from circular buffer add\n", __func__, result);
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

    if (result == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
    {
      // WARNING: THE SIZE OF THE CIRCULAR BUFFER SHOULD BE FIXED.
      // TOO MUCH EXPANSION WILL CAUSE SERIOUS PROBLEMS!
      // Packet size bigger than packet parsing buffer so allocate space
      f7syslog(LOG_WARNING, "%s() WARNING: Packet parsing buffer too small, will increase from %d to %d bytes\n",
               __func__, _max_packet_size, packetLength);

      // The buffer needs to be expanded
      _max_packet_size = packetLength;
      free(packet_dest_buf);
      free(decode_dest_buf);
      packet_dest_buf = (uint8_t *)malloc(_max_packet_size);
      decode_dest_buf = (uint8_t *)malloc(_max_packet_size);
      continue; // Try again
    }

    DEBUGASSERT(result == HCOM_CIR_BUF_GET_FOUND_MSG);

    // Decode the packet and drop trailing delimiter (0x00)
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
      f7syslog(LOG_ERR, "%s() ERROR: processing data failed: %d\n", __func__, result);
      return result;
      // If ever supported NEED TO SEND NAK TO HOST TO RESEND BAD DATA
    }
    else
    {
      f7syslog(LOG_ERR, "%s() ERROR: unknown value returned from processing data: %d\n", __func__, result);
      return result;
    }
  }
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

  hcom_comms_dbg(LOG_DEBUG, "  Processing Decoded Packet (seq numb:%d, length:%d bytes\n", seqNumb, packetSize); 
  hcom_utils_diag_print_buffer(packet, packetSize, LOG_DEBUG);

  if (seqNumb == HCOM_PROTOCOL_REQUEST_HEADER_SIMPLE_SEQ_NUMBER)
  {
    // Request packet
    hcom_execute_host_command_type(packet + msgOffset, packetSize - msgOffset);
  }
  else
  {
    // Data Packet (Sequence number > 0)
    hcom_exec_rqst_download_data_packet(packet, packetSize, seqNumb);
  }

  return OK;
}

//========================================================================
// Parse the manditory header
void hcom_execute_host_command_type(const uint8_t *recvOrigData, const size_t recvOrigDataSize)
{
  int ret;
  uint8_t msgOffset = 0;
  
  uint16_t protocolVersion = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  if(protocolVersion != (uint16_t)HCOM_PROTOCOL_CURRENT_VERSION_NUMBER)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
        "Received unsupported protocol version %04x, expected %04x",
        protocolVersion, (uint16_t)HCOM_PROTOCOL_CURRENT_VERSION_NUMBER);

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    f7syslog(LOG_ERR, "Error: %s\n", hostMsg);

    ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      
    return;
  }

  uint16_t protocolControl = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  uint16_t requestType = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8);
  msgOffset += sizeof(uint16_t);

  uint32_t userData = recvOrigData[msgOffset] + (recvOrigData[msgOffset + 1] << 8) +
                      (recvOrigData[msgOffset + 2] << 16) + (recvOrigData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  const uint8_t *recvPayload = recvOrigData + msgOffset;
  const size_t recvPayloadSize = recvOrigDataSize - msgOffset;

  // Todo - could use this switch to create smaller sub-switches
  char *headerType;
  switch(requestType & HCOM_PROTOCOL_HEADER_TYPE_MASK)
  {
    case HCOM_PROTOCOL_HEADER_TYPE_SIMPLE:
      headerType = "Simple";
      DEBUGASSERT(recvPayloadSize == 0);
      break;

    case HCOM_PROTOCOL_HEADER_TYPE_FILE:
      headerType = "Header";
      DEBUGASSERT(recvPayloadSize != 0);
      break;
      
    case HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY:
      headerType = "Simple Binary";
      DEBUGASSERT(recvPayloadSize != 0);
      break;
      
    default:
      f7syslog(LOG_ERR, "%s() Warning: Unknown header type in message 0x%04x\n", __func__, requestType);
  }

  hcom_comms_dbg(LOG_DEBUG, "Protocol version is %04x, control %04x, request type %04x (hdr type '%s'), user data %04x\n",
      protocolVersion, protocolControl, requestType, headerType, userData);

  switch (requestType)
  {
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_download_file_rqst_start(recvPayload, recvPayloadSize, userData);
      break;
      
    // Note: Start file transfer provides the 'Accepted' message and
    // end file transfer the 'Concluded' message
    case HCOM_MDOW_REQUEST_END_FILE_TRANSFER:
      hcom_exec_rqst_download_file_rqst_end(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_delete(recvPayload, recvPayloadSize, userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_flash_verify_erase(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

      // Partitions the entire flash chip with the number of partitions that
      // are defined by userData.
    case HCOM_MDOW_REQUEST_PARTITION_FLASH_FS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_partition(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

      // Mount the file system for testing.
    case HCOM_MDOW_REQUEST_MOUNT_FLASH_FS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_mount(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_FORMAT_FLASH_FILE_SYS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_format(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_INITIALIZE_FLASH_FS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_initialize(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_CREATE_ENTIRE_FLASH_FS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_create(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_change_trace_level(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_enable_disable_nsh(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_LIST_PARTITION_FILES:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_return_file_list(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_return_file_list_with_crc(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_MONO_RUN_STATE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_mono_run_state(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_get_device_info(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_flash_bulk_erase(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    // The following commands send the HCOM_HOST_REQUEST_TEXT_CONCLUDED message when Meadow restarts
    case HCOM_MDOW_REQUEST_RESET_PRIMARY_MCU:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_mcu_restart(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_flash_fs_part_renew_file_system(userData);   // Forces restart
      break;

// NOT IMPLEMENTED
    case HCOM_MDOW_REQUEST_ENTER_DFU_MODE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_enter_dfu_mode(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_MONO_DISABLE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_mono_disable(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_MONO_ENABLE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_mono_enable(userData);   // Forces restart
      break;

    case HCOM_MDOW_REQUEST_NO_DIAG_TO_HOST:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_no_diag_msg_to_host(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_SEND_SYSLOG_TO_HOST:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_misc_send_diag_to_host(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_HOST_REQUEST_DEBUGGER_MSG:
      // Accepted and concluded not needed here!
      hcom_remote_dbg_recv_host_send_to_mono(recvPayload, recvPayloadSize, userData);
      break;

#if defined(CONFIG_HCOM_MTD_STRESS_TEST)
    case HCOM_MDOW_REQUEST_DEVELOPER_1:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_developer_1(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_2:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_developer_2(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_3:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_developer_3(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_4:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_developer_4(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_INIT:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_flash_qspi_init(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_WRITE:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_flash_qspi_write(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;

    case HCOM_MDOW_REQUEST_S25FL_QSPI_READ:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      hcom_exec_rqst_testing_flash_qspi_read(userData);
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
      break;
#endif

    default:
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_REJECTED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

      f7syslog(LOG_ERR, "%s() ERROR: Received unsupported command type %04x\n", __func__, requestType);
      hcom_utils_diag_print_buffer(recvOrigData, recvOrigDataSize, LOG_ERR);
      
      ret = hcom_comms_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0);
      if (ret < 0)
        f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  }
}
