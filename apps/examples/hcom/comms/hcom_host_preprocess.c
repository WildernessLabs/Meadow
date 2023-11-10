/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_preprocess.c
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

// This file does processing of all request. For file downloads it allocates
// and populates a struct that contains file specific information.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "../hcom_common.h"
#include <meadow/hcom_dnld_shared.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) hcom_host_preprocess.c"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

// The following are allocated at startup
static hcom_dnld_shared_t *_DnldShared;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_preprocess_packet(hcom_dnld_shared_t *dnldShared,
          const uint8_t *packet, const size_t packetSize);
static int hcom_host_process_run_loop(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Note: this thread was created by Nuttx and initially was used to do the
// Meadow initialization. Once all initialization is completed, it is used
// here to processing the HCOM messages. Therefore, it never returns.
int hcom_host_process_setup()
{
  int ret;
  _shutting_down = false;

  // These allocation continue throughout the life of Meadow
  _packet_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_packet_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Packet buffer allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }
  
  _decode_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Decoded allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  // One time allocation for the download shared structure
  _DnldShared = malloc(sizeof(hcom_dnld_shared_t));
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Download shared allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  memset(_DnldShared, 0, sizeof(hcom_dnld_shared_t));

  // The watchdog needs access to the download shared structure
  ret = hcom_host_watchdog_initialize(_DnldShared);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Watchdog init failed:%d\n",
              thisFile, __LINE__, ret);
    return -ENOEXEC;    // May be better error code....
  }

  // This threads priority and initial stack size are set via menuconfig
  // Priority: CONFIG_USERMAIN_PRIORITY with a default of 100. This priority
  // is adjusted in the following code.
  // Stacksize: CONFIG_USERMAIN_STACKSIZE with a default stack size of 2048.
  // The stack size was set to 65536, I've not changed this value (Peter).
  // RTOS Features->Stack and heap information->Main thread stack size
  //
  // See /nuttx/sched/init/nx_bringup.c for user main's creation
  struct sched_param sparam;
  sparam.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
  sched_setparam(0, &sparam);

  // This thread needs to respond to SIGALRM
  sigset_t set;
  (void)sigemptyset(&set);
  (void)sigaddset(&set, SIGALRM);
  ret = sigprocmask(SIG_UNBLOCK, &set, NULL);
  if (ret != OK)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-sigprocmask() failed:%d\n",
              thisFile, __LINE__, ret);
    return -EPERM;
  }

  // This thread now runs host processing of received messages. As such this
  // call will never return.
  ret = hcom_host_process_run_loop();
 
  // This return is only reached on shutddown
  return ret;
}

//===========================================================================
// Free any memory in struct hcom_dnld_shared_s.
// The intent is any function can call this and be assured that all the
// internally allocated memory is freed.
static int hcom_file_dir_mgmt_free_file_info(hcom_dnld_shared_t *dnldShared)
{
  // Free any string memory allocations
  if(dnldShared->dnldOrigPathName != NULL)
  {
    free(dnldShared->dnldOrigPathName);
    dnldShared->dnldOrigPathName = NULL;
  }

  if(dnldShared->dnldFullPathName != NULL)
  {
    free(dnldShared->dnldFullPathName);
    dnldShared->dnldFullPathName = NULL;
  }

  memset(dnldShared, 0, sizeof(hcom_dnld_shared_t));

  dnldShared->dnldCurrentState = HcomStm32F7DnldStateInvalid;
  return OK;
}

//====================================================================
// Shutdown
void hcom_host_process_shutdown()
{
  _shutting_down = true;

  free(_decode_dest_buf);
  free(_packet_dest_buf);

  hcom_file_dir_mgmt_free_file_info(_DnldShared);
}

//==========================================================================
// Are we involved in some external flash download activity? There are the
// following states: HcomStm32F7DnldStateNone, HcomStm32F7DnldStateStarting
// and HcomStm32F7DnldStateFileXfer
static bool hcom_host_process_is_stm32f7_dnld_active(hcom_dnld_shared_t *dnldShared)
{
  return((dnldShared->dnldCurrentState == HcomStm32F7DnldStateStarting) ||
         (dnldShared->dnldCurrentState == HcomStm32F7DnldStateFileXfer));
}

//==========================================================================
// This function will take the information from the list request and save it
// in the hcom_dnld_shared_t structure.
static int hcom_host_process_init_file_list(const HcomProtoHdrMsg_t *hdrMsg,
            const size_t packetSize, const uint32_t userData,
            const uint16_t requestType, hcom_dnld_shared_t *dnldShared)
{
// (new)
  int ret;
  char *pathName;

  // Clear the entire struct containing all information.
  memset(dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // Start populating the shared download fields
#ifdef CONFIG_MTD_PARTITION    // This is a nuttx configuration
  dnldShared->dnldFilePartId = userData;
#else
  dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
#endif

  HcomProtoTextMsg_t *textMsg = (HcomProtoTextMsg_t *)hdrMsg;
  size_t pathNameLength = packetSize - HCOM_PROTOCOL_TEXT_MSG_LENGTH;
  pathName = malloc(pathNameLength + 1);
  if (pathName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  memcpy(pathName, textMsg->textData, pathNameLength);
  pathName[pathNameLength] = '\0';  // Make into C string

  syslog(1, "-----> %s@%d-pathNameLength:%d, packetSize:%d, pathName '%s'\n",
            thisFile, __LINE__, pathNameLength, packetSize, pathName);
  usleep(20 * 1000);

  // Do work to test and/or construct the proper full file name
  // Note: This call may allocate memory, therefore, this must be considered
  // this memory after this point.
  ret = hcom_file_dir_mgmt_eval_build_pathname(dnldShared, pathName,
            pathNameLength, false);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Eval pathname, errno:%d, ret:%d\n",
              thisFile, __LINE__, errno, ret);
  }

  free(pathName);
  return ret;
}

//==========================================================================
// This function consolidates a lot of the needed processing for file download
// and file delete into a single function instead of being spread all over
// the code base.
static int hcom_host_process_init_write_or_del(const HcomProtoHdrMsg_t *hdrMsg,
            const size_t packetSize, const uint32_t userData,
            const uint16_t requestType, hcom_dnld_shared_t *dnldShared)
{
  int ret;
  char *pathName;

  // Verify that mono has been disabled, if not don't allow download
  if(hcom_mono_ctrl_is_mono_enabled())
  {
    char hostMsg[HCOM_TINY_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_TINY_HOST_STRING_BUFF_LENGTH,
            "Mono must be disabled for file download");

    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, hostMsg);

    // Let CLI user know the problem
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);

    // Notify CLI that download can't continue because of an error.
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL, 0, thisFile, __LINE__);
    return -EPERM;    // Operation not permitted
  }

  // Clear the entire struct containing all information.
  memset(dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // Start populating the shared download fields
#ifdef CONFIG_MTD_PARTITION    // This is a nuttx configuration
  dnldShared->dnldFilePartId = userData;
#else
  dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
#endif

  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
  size_t pathNameLength = packetSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
  pathName = malloc(pathNameLength + 1);
  if (pathName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }
  memcpy(pathName, fileMsg->fileInfo.fileName, pathNameLength);
  pathName[pathNameLength] = '\0';  // Make into C string

  syslog(1, "-----> %s@%d-pathNameLength:%d, packetSize:%d, pathName '%s'\n",
            thisFile, __LINE__, packetSize, pathName);

  // Do work to test and/or populate struct
  //
  // Note: This call may allocate memory, therefore, this must be considered
  // this memory after this point.
  ret = hcom_file_dir_mgmt_eval_build_pathname(dnldShared, pathName,
            pathNameLength, true);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Eval pathname, errno:%d, ret:%d\n",
              thisFile, __LINE__, errno, ret);

    // Messages to CLI already sent
    free(pathName);
    return ret;
  }

  // For the Meadow file system download start, need to do extra initialization
  if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
      requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME)
  {
    // If there are subdirectories, the number of elements will be > 2 since
    // this is a count of the number of elements
    if(dnldShared->dnldPathNameEleCount > 2)
    {
      ret = hcom_file_dir_mgmt_check_and_add_subdir(dnldShared);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Checking subdir errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        free(pathName);
        return ret;
      }
    }

    ret = hcom_host_watchdog_dnld_timer_initialize();
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer init errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);

      free(pathName);
      return ret;
    }

    // Start the timer to ensure we start and continue to receiving data
    // from CLI
    ret = hcom_host_watchdog_dnld_timer_set_delay(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);
    }
  }

  free(pathName);
  return ret;
}

//==========================================================================
// This thread processes all the messages the receive thread has written to
// the circular buffer in this loop
int hcom_host_process_run_loop()
{
  int ret;
  size_t packetLength;

  while (!_shutting_down)
  {
    // Dequeue the next packet received. Also, checks watchdog timer
    ret = hcom_host_enq_deq_dequeue_packet(_packet_dest_buf, &packetLength);
    if(ret < 0)
    {
      if(ret == -ETIME)
      {
        // Watchdog timeout waiting for data
        hcom_logging_syslog(LOG_ERR, "%s@%d-Watchdog timeout during download\n",
                  thisFile, __LINE__);

        // If any download/delete state information, delete it
        hcom_file_dir_mgmt_free_file_info(_DnldShared);
      }
      else
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Failed to dequeue message\n",
                  thisFile, __LINE__);
      }
      continue;
    }

    // We have a good packet. Drop trailing delimiter using --packetLength,
    // then decode the packet and route it for processing.
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf,
              --packetLength, _decode_dest_buf);
    if(decodedPacketSize == 0)
      continue;

    // Do initial processing of packet
    ret = hcom_host_preprocess_packet(_DnldShared, _decode_dest_buf,
              decodedPacketSize);
    if (ret < 0)
    {
      // Error message already logged for -EOWNERDEAD errors
      if(ret != -EOWNERDEAD)
      {
        // Let CLI user know the problem
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                "Error during download/delete command processing",
                thisFile, __LINE__);

        hcom_logging_syslog(LOG_ERR, "%s@%d-Processing data packet, ret:%d\n",
                  thisFile, __LINE__, ret);
      }

      // If any download/delete state information, delete it
      hcom_file_dir_mgmt_free_file_info(_DnldShared);
    }
  }

  // Thread is exiting, this is not good
  return OK;
}

//============================================================================
// Parse and process received decoded packets as sent by host (CLI).
// Grab the sequence number and use it to determine if data or command.
int hcom_host_preprocess_packet(hcom_dnld_shared_t *dnldShared,
          const uint8_t *decodedPacket, const size_t decodedSize)
{
  int ret;
  uint16_t requestType;
  uint32_t userData;

  // All messages contain a sequence number field. The sequence number
  // determines if this packet is a command or data.
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  // Test to determine if this is a data packet or a command
  if (hcomDataMsg->seqNumber != HCOM_PROTOCOL_COMMAND_TYPE_SEQUENCE_NUMBER)
  {
    // This must be a Data packet and not a command message
#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
      hcom_diag_decode_data_packet_type(decodedSize);
      usleep(100 * 1000);
#endif

    // Data Packet - test for stm32f7 download error for Start or Data
    if(dnldShared->dnldCurrentState == HcomStm32F7DnldStateInvalid)
    {
      // Let CLI user know the problem
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
              "Download Data received, but no active download", thisFile, __LINE__);

      hcom_logging_syslog(LOG_ERR, "%s@%d-Download Data received but no active download\n",
                thisFile, __LINE__);

      return -EOWNERDEAD;      // There must have been a previous error
    }

    // Must be a Data Packet (download) because sequence number != 0.
    // What is the current download state? What is active, external flash or
    // ESP32?
    if(hcom_host_process_is_stm32f7_dnld_active(dnldShared))
    {
      // Keep resetting the watchdog here on every packet
      ret = hcom_host_watchdog_dnld_timer_set_delay(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        return ret;
      }

      // Here's the data for download
      ret = hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, decodedSize,
                dnldShared);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Receving data errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        return ret;
      }
    }
    else if(hcom_file_dnld_esp32_is_active())
    {
      hcom_file_dnld_esp32_recvd_file_data(hcomDataMsg, decodedSize);
    }
    else
    {
      // CLI is confused or something else is wrong. We should never receive a
      // packet with a sequence number when no download is active.
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data received but no active download\n",
                thisFile, __LINE__);
    }

    // Finished with preprocessing data only packet
    return OK;
  }

  //----------------------------------------------------------------------
  // Since sequence number is 0, must be a command (non-data packet) message.
  // And these always have the full HCOM header.
  // Note: Adding the full header to data packets has been planned for a while.
  const HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) decodedPacket;

#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
  hcom_diag_decode_recvd_message_type(hdrMsg, decodedSize);
  usleep(100 * 1000);
#endif

  // Pull out important command related values
  userData = hdrMsg->stdHeader.userData;
  requestType = hdrMsg->stdHeader.rqstType;

  if(hdrMsg->stdHeader.version < HCOM_PROTOCOL_PREFERRED_VERSION_NUMBER)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "Meadow is expecting a different CLI Protocol version. Please update Meadow.CLI." \
          " (version received: %04x required: %04x).",
          hdrMsg->stdHeader.version, HCOM_PROTOCOL_PREFERRED_VERSION_NUMBER);

    hcom_logging_syslog(LOG_ERR, "%s\n", hostMsg);
    uint16_t level = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    if (hdrMsg->stdHeader.version < HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER)
    {
      level = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
    else
    {
      g_current_hcom_protocol_version = hdrMsg->stdHeader.version;
    }

    hcom_host_send_simple_string_msg(level, 0, hostMsg, thisFile, __LINE__);
    if (level == HCOM_HOST_REQUEST_TEXT_ERROR)
    {
      return -ENOTSUP;
    }

    // A wrong protocol version error won't send the Concluded message, from
    // these message types, must be done here.
    if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
       requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
       requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
      {
        // Caller expects a Concluded message for these messages, even for
        // a wrong CLI protocol version.
        hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0,
                  thisFile, __LINE__);
      }
  }

  // Most message need on additonal processing. However, those requiring file
  // location information do. We'll isolate these and forward the others
//   if()
//   {
// requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER
// requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END
// requestType == HCOM_MDOW_REQUEST_LIST_PARTITION_FILES
// requestType == HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC
// requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER
// requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME
// requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME

// HCOM_MDOW_REQUEST_UPLOAD_FILE_INIT
// HCOM_MDOW_REQUEST_UPLOAD_START_DATA_SEND
// HCOM_MDOW_REQUEST_UPLOAD_ABORT_DATA_SEND

//   }
  
  ret = hcom_host_route_request_by_cmd_type(hdrMsg, decodedSize, userData,
            requestType, dnldShared);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Request Type:%u errno:%d, ret:%d\n",
              thisFile, __LINE__, requestType, errno, ret);
  }
  return ret;


  // Remove any remaining memory unless this is the file download end message
  if(requestType != HCOM_MDOW_REQUEST_END_FILE_TRANSFER &&
     requestType != HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END)
  {
    hcom_file_dir_mgmt_free_file_info(dnldShared);
  }

  // This allows the file list processing to include a subdirectories
  if(requestType == HCOM_MDOW_REQUEST_LIST_PARTITION_FILES ||
     requestType == HCOM_MDOW_REQUEST_LIST_PART_FILES_AND_CRC)
  {
    // Need to get the information associated with these messages. It could
    // be empty or include 1 or more subdirectories, from which a file list
    // is to be generated.
    ret = hcom_host_process_init_file_list(hdrMsg, decodedSize, userData,
            requestType, dnldShared);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Init file list errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);

      hcom_file_dir_mgmt_free_file_info(dnldShared);
      return ret;   // On error exit
    }
  }

  // For downloading/deleting files we need more file related information
  // and we need this information persisted until the file has been received. 
  // This is done here so it doesn't needed to be done in multiple places.
  else if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
          requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
          requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
  {
    // Starting a file transfer or delete needs special pre-processing
    // before being routed to the various write and delete functions.
    ret = hcom_host_process_init_write_or_del(hdrMsg, decodedSize, userData,
            requestType, dnldShared);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Init write/delete errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);

      hcom_file_dir_mgmt_free_file_info(dnldShared);

      // These request types need a Concluded message. Why? Because in the
      // normal case the End message will do this. But, on an error the End
      // message cannot be expected.
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0, thisFile, __LINE__);

      return ret;   // On error exit
    }
  }

  // End of file download
  else if(requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER ||
          requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_FILE_END)
  {
    // Did an earlier error occur? Looks like CLI sent ending message anyway.
    if(dnldShared->dnldCurrentState == HcomStm32F7DnldStateInvalid)
    {
      // Let CLI know there's a problem
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
              "'End' command received, but no active download", thisFile, __LINE__);

      hcom_logging_syslog(LOG_ERR, "%s@%d-Download End request but no active download.\n",
                thisFile, __LINE__);

      // Caller expects a Concluded message for End message.
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_CONCLUDED, 0,
                thisFile, __LINE__);

      return -EOWNERDEAD;      // There must have been a previous error
    }

    // Since we're about to finish the data download, stop and delete
    // watchdog as it's no longer needed.
    ret = hcom_host_watchdog_dnld_timer_delete();
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);
    }
  }

  //-------------------------------------------------------------------
  // All command types are routed to processing code by this call.
  //-------------------------------------------------------------------
  // There are only a few command type handlers that report errors.
  // Specifically, those dealing with file download and delete.
  ret = hcom_host_route_request_by_cmd_type(hdrMsg, decodedSize, userData,
            requestType, dnldShared);
  if(ret < 0)
  {
    // These may have allocated memory, if they fail, we need to free the
    // memory.
    if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
       requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
       requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
    {
      hcom_file_dir_mgmt_free_file_info(dnldShared);
    }

    hcom_logging_syslog(LOG_ERR, "%s@%d-Request Type:%u errno:%d, ret:%d\n",
              thisFile, __LINE__, requestType, errno, ret);
  }

  return OK;
}
