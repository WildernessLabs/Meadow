/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_route.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <nuttx/config.h>


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

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_host_route_setup()
{
  _shutting_down = false;
  return OK;
}

//====================================================================
void hcom_host_route_shutdown()
{
  _shutting_down = true;
}

//========================================================================
// This function routes the message to the proper processing functions
void hcom_host_route_request_by_cmd_type(const HcomProtoHdrMsg_t *hdrMsg,
            const size_t packetSize)
{
#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
  hcom_diag_decode_recvd_message_type(hdrMsg, packetSize);
  usleep(100 * 1000);
#endif

  if(hdrMsg->stdHeader.version != (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "Meadow is expecting a newer CLI Protocol version. Please update Meadow.CLI on your connecting computer." \
          " (version received::%04x required:%04x).",
          hdrMsg->stdHeader.version, (uint16_t)HCOM_PROTOCOL_HCOM_VERSION_NUMBER);

    hcom_logging_syslog(LOG_ERR, "%s\n", hostMsg);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
    return;
  }

  const uint16_t requestType = hdrMsg->stdHeader.rqstType;
  const uint32_t userData = hdrMsg->stdHeader.userData;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "-->Received Meadow command of RqstType:0x%04x\n",
            requestType);
#endif

 switch (requestType)
  {
    // Start file transfer handles both Meadow and ESP32 file starts
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_dnld_proc_flash_file_sys_begin(hdrMsg, packetSize,
                userData, requestType);
      break;
      
    // End file transfer handles both Meadow and ESP32 file starts
    // Note: Start file transfer provided the 'Accepted' message and
    // end file transfer provides the 'Concluded' message
    case HCOM_MDOW_REQUEST_END_FILE_TRANSFER:
      hcom_file_dnld_proc_flash_file_sys_end(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_write_del_remove_file_start(hdrMsg, packetSize, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    //-------------------------------------------------
#if defined (CONFIG_HCOM_ESP32_COMMS)
    // ESP32 follow
    // Note: Start file transfer provides the 'Accepted' message and
    // end file transfer the 'Concluded' message
    case HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_dnld_proc_esp32_flash_begin(hdrMsg);
      break;

    // Note: Start file transfer provides the 'Accepted' message and
    // end file transfer the 'Concluded' message
    case HCOM_MDOW_REQUEST_END_ESP_FILE_TRANSFER:
      hcom_file_dnld_proc_esp32_flash_end(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_READ_ESP_MAC_ADDRESS:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_esp32_util_read_esp32_mac(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_RESTART_ESP32:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_esp32_util_restart_esp32(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
#endif

    //---------------------------------------------------
    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_CHANGE_TRACE_LEVEL:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_diag_logging_change_trace_level(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_LIST_PARTITION_FILES:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_lists_files_in_partition(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_lists_files_and_crc_in_partition(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_GET_DEVICE_INFORMATION:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_misc_rqst_get_device_info(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_GET_DEVICE_NAME:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_misc_rqst_get_device_name(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    //-------------------------------------------------------------------------
    // The following restart Meadow. The HCOM_HOST_REQUEST_TEXT_CONCLUDED message
    // is sent by the Meadow restart code, after Meadow has restarted.
    case HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          thisFile, __LINE__);
      hcom_via_nx_host_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          thisFile, __LINE__);
      hcom_via_nx_host_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          thisFile, __LINE__);
      hcom_via_nx_host_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_MONO_DISABLE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_mono_ctrl_disable_mono(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          thisFile, __LINE__);
      hcom_via_nx_host_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_MONO_ENABLE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_mono_ctrl_enable_mono(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          thisFile, __LINE__);
      hcom_via_nx_host_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_ENTER_DFU_MODE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_misc_rqst_enter_dfu_mode(userData);
      hcom_via_nx_only_restart_meadow();
      break;

    case HCOM_MDOW_REQUEST_MONO_FLASH:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    // -------------------------------------------------------
    // To the CLI user there are 3 steps that appear as a single command, much
    // like file download. But, the CLI actually sends these 2 commands one
    // before the file data is downloaded and the other after the data is
    // downloaded. This is like the file downloading for system files
    // 1. CLI sends this first
    case HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_dnld_proc_flash_file_sys_begin(hdrMsg, packetSize,
                userData, requestType);
      break;
      
      // 2. CLI sends data.....
      // 3. CLI sends the file end
    case HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END:
      hcom_file_dnld_proc_flash_file_sys_end(userData);
      // Next copy the file to flash area, this must be done on the nuttx
      // side. This will take several seconds because it first erases the
      // 2 MB flash area and then copies the 2 MB file.
      // Note: a different requestType is used than sent
      hcom_via_nx_forward_cli_cmd_to_nx(HCOM_MDOW_REQUEST_MONO_FLASH, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
    // -------------------------------------------------------

    case HCOM_MDOW_REQUEST_MONO_RUN_STATE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_mono_ctrl_report_mono_enabled_state(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_HOST:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      // Both k-land and userland need this command.
      // The nuttx side must start first
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_trace_to_cli_enable_command(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_NO_TRACE_TO_HOST:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      // Both k-land and userland need this command.
      // The nuttx side must stop first so the pthread it holds so it can terminate
      hcom_trace_to_cli_disable_command(userData);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_trace_to_cli_disable_cleanup(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_NO_TRACE_TO_UART:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_SEND_TRACE_TO_UART:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_GET_INITIAL_FILE_BYTES:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_upld_proc_initial_bytes_in_file(hdrMsg, packetSize, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_UPLOAD_FILE_INIT:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_upld_proc_start_file_upload(hdrMsg, packetSize, userData);
      // After data sent the HCOM_HOST_REQUEST_TEXT_CONCLUDED message will be sent
      // by HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND case.
      break;

    case HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND:
      hcom_file_upld_proc_begin_file_uploading(hdrMsg, packetSize, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_UPLOAD_ABORT_DATA_SEND:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_file_upld_proc_abort_file_upload(hdrMsg, packetSize, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_RTC_SET_TIME_CMD:
      syslog(1, "rtc-at HCOM_MDOW_REQUEST_RTC_SET_TIME_CMD\n");
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_execute_rtc_set_clock(hdrMsg, packetSize);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_RTC_READ_TIME_CMD:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
    case HCOM_MDOW_REQUEST_MONO_START_DBG_SESSION:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_mono_remote_dbg_enable(userData);

      // This will restart meadow and send the concluded message on restart
      hcom_via_nx_host_restart_meadow();
      break;
      
      // Debugging data received from VS via CLI
    case HCOM_MDOW_REQUEST_DEBUGGING_DEBUGGER_DATA:
      // Accepted and concluded not needed here! This is debugging data
      hcom_mono_remote_dbg_recv_host_sending_to_mono(hdrMsg, packetSize, userData);
      break;
#endif

    case HCOM_MDOW_REQUEST_DEVELOPER_1:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_developer_tests_developer_1(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_2:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_developer_tests_developer_2(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    // Developer 3 is now used to execute tests in kernelland
    case HCOM_MDOW_REQUEST_DEVELOPER_3:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_DEVELOPER_4:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_developer_tests_developer_4(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

#if HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD > 0
    case HCOM_MDOW_REQUEST_QSPI_FLASH_INIT:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_QSPI_FLASH_WRITE:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    case HCOM_MDOW_REQUEST_QSPI_FLASH_READ:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_forward_cli_cmd_to_nx(requestType, userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;
#endif

    case HCOM_MDOW_REQUEST_EXEC_DIAG_APP_CMD:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_via_nx_exec_diag_app_cmd(hdrMsg, packetSize);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    //------------------------------------------------------
    // The following currently does nothing
    case HCOM_MDOW_REQUEST_ENABLE_DISABLE_NSH:
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ACCEPTED, 0, thisFile, __LINE__);
      hcom_diag_misc_launch_nsh(userData);
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
      break;

    default:
    {
      char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "Meadow received unknown/unavailable CLI request:0x%04x received",
                requestType);

      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, hostMsg,
              thisFile, __LINE__);

      hcom_logging_syslog(LOG_ERR, "%s@%d-Received unsupported request type:0x%04x\n",
             thisFile, __LINE__, requestType);
      hcom_diag_print_buffer((const uint8_t*)hdrMsg, packetSize, LOG_ERR);
      
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);
    }
  }
}
