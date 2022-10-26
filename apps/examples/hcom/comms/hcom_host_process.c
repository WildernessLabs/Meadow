/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_process.c
 * 
 *   Copyright (C) 2019 - 2022 Wilderness Labs. All rights reserved.
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
#warning "Peter working here (--)"
#include "../hcom_common.h"
#include <meadow/hcom_dnld_shared.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

static hcom_dnld_shared_t *_dnldShared;

// static timer_t _processWdogTimerId;
// static bool _hcom_host_process_wdog_timedout;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_process_route_packet(const uint8_t *packet, const size_t packetSize);
static int hcom_host_process_run(void);
static int hcom_host_process_init_dnld_share(uint32_t partitionId);
static bool hcom_host_process_is_stm32f7_dnld_active(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_host_process_setup()
{
  int ret;
  _shutting_down = false;

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

  // Allocate for the download shared structure
  _dnldShared = malloc(sizeof(hcom_dnld_shared_t));
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Download shared allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  // The watchdog needs access to the download shared structure
  ret = hcom_host_watchdog_initialize(_dnldShared);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Watchdog init failed:%d\n",
              thisFile, __LINE__, ret);
    return -ENOEXEC;    // May be better error code....
  }

  struct sched_param sparam;
  sparam.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
  sched_setparam(0, &sparam);
  // (--) HOW TO SET STACKSIZE? MAY BE DONE VIA CONFIG OPTION?
  // HCOM_THREAD_STACKSIZE_HCOM_PROCESS

  // hcom_host_process_run(NULL);
  ret = hcom_host_process_run();
 
  // This return is only reached on shutddown
  return ret;
}

//====================================================================
void hcom_host_process_shutdown()
{
  _shutting_down = true;
  free(_decode_dest_buf);
  free(_packet_dest_buf);
}

//==========================================================================
// Are we involved in some download activity?
bool hcom_host_process_is_stm32f7_dnld_active()
{
  return (_dnldShared->dnldCurrentState != HcomStm32F7DnldStateNone);
}

//=================================================================
// This thread processes all the messages the receive thread has written to
// the circular buffer.
// FAR void *hcom_host_process_run(FAR void *arg)
int hcom_host_process_run()
{
  int ret;
  size_t packetLength;
  
  while (!_shutting_down)
  {
    // Get the next received packet
    ret = hcom_host_enq_deq_dequeue_packet(_packet_dest_buf, &packetLength);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Failed to dequeue message\n", thisFile, __LINE__);
      continue;
    }

    // We have a good packet. Drop trailing delimiter using --packetLength,
    // then decode the packet and route it.
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);
    if(decodedPacketSize == 0)
      continue;

    ret = hcom_host_process_route_packet(_decode_dest_buf, decodedPacketSize);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n",
                thisFile, __LINE__, ret);
    }
  }

  // Thread is exiting
  return OK;
}

//====================================================================
// Parse and process received decoded packets as sent by host (CLI).
// Grab the sequence number, using it to determine if data or command.
int hcom_host_process_route_packet(const uint8_t *decodedPacket, const size_t decodedSize)
{
  uint16_t requestType;
  uint32_t userData;

  // All messages contain a sequence number field. The sequence number
  // determines if this packet is a command or data.
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  if (hcomDataMsg->seqNumber == HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // Only commands (non-data packets) have the full HCOM header
    const HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) decodedPacket;

#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
    hcom_diag_decode_recvd_message_type(hdrMsg, decodedSize);
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
      return -ENOTSUP;
    }

    // Pull out important values
    requestType = hdrMsg->stdHeader.rqstType;
    userData = hdrMsg->stdHeader.userData;

    // For downloading or deleting files need more information and require
    // HCOM to keep this activity state alive while downloading. These
    // commands are those that need the file's name and may need to establish
    // a temporary state while being processed.
    if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
      requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
      requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
    {
      // Initialize the struct containing all download/delete state information
      hcom_host_process_init_dnld_share(userData);
    
      // We'll do a little work here so it doesn't need to be done in multiple
      // places.
      size_t fileNameLength = decodedSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
      _dnldShared->dnldOrigFileName = malloc(fileNameLength + 1);
      if(_dnldShared->dnldOrigFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }

      // File name
      HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
      memcpy(_dnldShared->dnldOrigFileName, fileMsg->fileInfo.fileName, fileNameLength);
      _dnldShared->dnldOrigFileName[fileNameLength] = '\0';

      // Build the full path plus file name string (e.g. /mnt0/FileName.ext)
      size_t fullFileNameLen = strlen(_dnldShared->dnldOrigFileName) + \
                strlen(HCOM_FILE_MOUNT_POINT_TARGET) + 3; // Room for '/', partition Id, NULL

      _dnldShared->dnldFullFileName = malloc(fullFileNameLen + 1);
      if(_dnldShared->dnldFullFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }

#ifdef CONFIG_MTD_PARTITION
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s%d/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldFilePartId,
                                _dnldShared->dnldOrigFileName);
#else
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldOrigFileName);
#endif

      // For file download start need to initialize a watchdog timer
      if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER)
      {
        int ret;

        ret = hcom_host_watchdog_dnld_timer_initialize();
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-Timer init errno:%d, ret:%d\n",
                    thisFile, __LINE__, errno, ret);
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
    }
    else if(requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER)
    {
      int ret;

      // Stop and delete watchdog
      ret = hcom_host_watchdog_dnld_timer_delete();
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        hcom_host_process_free_dnld_share_mem();
      }
    }

    // Route the command message
    hcom_host_route_request_by_cmd_type(hdrMsg, decodedSize, userData,
              requestType, _dnldShared);
  }
  else
  {
    // Must be a Data Packet because sequence number != 0. Is it for external
    // flash or ESP32?
    if(hcom_host_process_is_stm32f7_dnld_active())
    {
      int ret;

      // Reset the watchdog
      ret = hcom_host_watchdog_dnld_timer_set_delay(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        hcom_host_process_free_dnld_share_mem();
      }

      // Here's the data
      hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, decodedSize, _dnldShared);
    }
    else if(hcom_file_dnld_esp32_is_active())
    {
      hcom_file_dnld_esp32_recvd_file_data(hcomDataMsg, decodedSize);
    }
    else
    {
      // CLI must be confused, sending sequence number when no dowload is active
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data received but no active download\n",
                thisFile, __LINE__);
    }
  }

  return OK;
}

//============================================================
// Free any memory that needs freeing in struct hcom_dnld_shared_s.
// The intent is any function can call this and be assured that all the
// internally allocated memory is freed.
int hcom_host_process_free_dnld_share_mem()
{
  // Free any strings etc.
  if(_dnldShared->dnldOrigFileName != NULL)
  {
    free(_dnldShared->dnldOrigFileName);
    _dnldShared->dnldOrigFileName = NULL;
  }

  if(_dnldShared->dnldFullFileName != NULL)
  {
    free(_dnldShared->dnldFullFileName);
    _dnldShared->dnldFullFileName = NULL;
  }

  return OK;
}

//============================================================
// Basic initialization
int hcom_host_process_init_dnld_share(uint32_t partitionId)
{
  memset(_dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // This is a nuttx configuration about partitioning 
#ifdef CONFIG_MTD_PARTITION
  _dnldShared->dnldFilePartId = partitionId;
#else
  _dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
#endif

  // Reset to None
  _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  return OK;
}
