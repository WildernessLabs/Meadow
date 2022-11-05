/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dnld_esp32.c
 * 
 *   Copyright (C) 2019-2022 Wilderness Labs. All rights reserved.
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

// The functions in the file setup to write a file, write the file and
// end the download process while verifying file integrity.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#if defined (CONFIG_HCOM_ESP32_COMMS)
#include "../esp32/hcom_esp32_comms.h"
#endif

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
#if defined (CONFIG_HCOM_ESP32_COMMS)

#define HCOM_RECV_DEBUG_TIMING 0          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _currentESP32DnldState;
static uint32_t _xferRecvFullFileSize;    // File size based on received data
static uint32_t _xferCalcFullFileSize;    // File size from start message
static int _lastPercentSent;
static int _esp32WaitCount;
static uint32_t _xferTargetMcuAddr;
static char _md5FileHash[HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH + 1];

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
static int _dbgNumbPacketsRecvd = 0;        // Only used in LOG_INFO & LOG_DEBUG messages
#endif

#if HCOM_RECV_DEBUG_TIMING > 0
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;
#endif

//--------------------------------------------------------------------
// This enum defines the current processing state of the download code for a
// specific download.
// The protocol could be modified so that each data packet contains this
// information. This would allow more than one operation to be processed
// at a time.
// To do this the protocol would need to be enhanced so that start download
// command carried an additional field to identify the "series" a particular
// data packet belonged to. Each data packet's series would be unuque and the
// sequence numbers 1-n would be unique for each series.
enum hcom_download_esp32_packet_state
{
  HcomESP32DnldStateNone = 0,
  HcomEsp32DnldStateStarting = 1,
  HcomESP32DnldStateEsp32FileXfer = 1,
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_dnld_esp32_setup()
{
    _currentESP32DnldState = HcomESP32DnldStateNone;
  return OK;
}

//==========================================================================
// Are we involved in some download activity?
bool hcom_file_dnld_esp32_is_active()
{
  return (_currentESP32DnldState != HcomESP32DnldStateNone);
}

//==========================================================================
// The state needs to be restored to action none state.
void hcom_file_dnld_esp32_set_to_inactive()
{
  _currentESP32DnldState = HcomESP32DnldStateNone;
}

//============================================================================
// Called from hcom_host_route()
void hcom_file_dnld_esp32_file_begin(const HcomProtoHdrMsg_t *hdrMsg)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;

  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;

  // Verify that mono has been disabled
  if(hcom_mono_ctrl_is_mono_enabled())
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Mono must be disabled for ESP32 file download");
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, hostMsg);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);

    // Notify CLI that download can't start because mono is enabled
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL, 0, thisFile, __LINE__);

    hcom_file_dnld_esp32_set_to_inactive();
    return;
  }

  // Best to change the current action before telling CLI ok to send
  _esp32WaitCount = 0;
  _currentESP32DnldState = HcomEsp32DnldStateStarting;

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = hcom_utils_get_current_time64_ns();
#endif

  _xferRecvFullFileSize = fileMsg->fileInfo.fileSize;
  _xferTargetMcuAddr = fileMsg->fileInfo.fileFlashAddr;
  memcpy(_md5FileHash, fileMsg->fileInfo.fileMD5Hash, HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH);
  _md5FileHash[HCOM_PROTOCOL_COMMAND_MD5_HASH_LENGTH] = '\0';

  // Log some diagnostic information 
  hcom_logging_syslog(LOG_INFO, "%s@%d-Start ESP32 download (Size:%d, MCUAddr:0x%08x, MD5Hash:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferTargetMcuAddr, _md5FileHash);

  // Adding file to ESP32-pico-d4 flash
  ret = hcom_esp32_exec_download_flash_start(_xferRecvFullFileSize, _xferTargetMcuAddr);
  if (ret < 0)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File download to ESP32 flash at '0x%08x' was unable to begin", _xferTargetMcuAddr);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
            
    hcom_file_dnld_esp32_set_to_inactive();

    // Notify CLI that something when wrong with start
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL, 0, thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, "%s@%d-download ESP32 start transfer:%d\n", thisFile, __LINE__, ret);
    return;
  }

  _currentESP32DnldState = HcomESP32DnldStateEsp32FileXfer;

  // Notify CLI that it's okay to send data
  hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY, 0, thisFile, __LINE__);
}

//============================================================================
// Process a data packet based on currently active state
// Note:This function is used by both download types STM32F7 and ESP32
void hcom_file_dnld_esp32_recvd_file_data(const HcomProtoDataMsg_t *hcomDataMsg,
          const size_t packetSize)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd++;
#endif

  uint32_t seqNumb = hcomDataMsg->seqNumber;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  if(seqNumb % 250 == 0)
    hcom_logging_syslog(LOG_DEBUG, "Sequence %d\n", seqNumb);
#endif

  // Compare _xferRecvFullFileSize with _xferCalcFullFileSize and send a message to host
  int percentDone = (_xferCalcFullFileSize  * 100) / _xferRecvFullFileSize;
  if(percentDone / 10 != _lastPercentSent)
  {
    // 10, 20 etc
    _lastPercentSent = percentDone / 10;

    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "File %d%% downloaded", percentDone);

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
  }

  size_t binDataLen = packetSize - (HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF);

  ret = hcom_esp32_exec_add_flash_data(hcomDataMsg->binData, binDataLen, seqNumb);
  _xferCalcFullFileSize += binDataLen;

  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Data packet file write failed:%d seq:%d\n",
             thisFile, __LINE__, ret, seqNumb);
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Data packet %d file write failed", seqNumb);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "Abort file transfer",
            thisFile, __LINE__);
  }
}

//=======================================================================================
// Process a end of ESP32 file transfer message
// Note: The the userData is 0 unless it's last file to be sent by the CLI command.
// For F7 this value is not used but the ESP32 uses this to reboot the ESP32.
// Only after a reboot will the ESP32 attempt to execute the program.
void hcom_file_dnld_esp32_file_end(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH];
  char *espCalculatedMd5;
  bool lastFile = userData == 1 ? true : false;
  uint16_t requestType;

  hcom_logging_syslog(LOG_NOTICE, "End of ESP32 transfer\n");

  if(_currentESP32DnldState != HcomESP32DnldStateEsp32FileXfer)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 dnld end, unexpected state\n",
              thisFile, __LINE__);
    return;
  }

  // Compare the two MD5 hashs
  espCalculatedMd5 = hcom_esp32_exec_get_md5_file_hash();
  int md5CmpResult = strcmp(espCalculatedMd5, _md5FileHash);

  hcom_logging_syslog(LOG_INFO,
          "%s@%d-File end-Esp32 calculated MD5:'%s', received from CLI MD5:'%s', %s\n",
          thisFile, __LINE__, espCalculatedMd5, _md5FileHash,
          md5CmpResult == 0 ? "Success" : "Error");
  
  if(md5CmpResult == 0 && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "File received successfully MD5 ESP32 calculated:'%s', received from CLI:'%s')",
            espCalculatedMd5, _md5FileHash);
    requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
  }
  else
  {
    if(md5CmpResult != 0)
    {
      snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
                "MD5 hash compare error MD5 ESP32 calculated:%s, received from CLI:%s)",
                espCalculatedMd5, _md5FileHash);
      requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
    else
    {
      snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
              "Download failed due to file size mismatch Meadow calculated:%d, received from CLI:%d",
              _xferCalcFullFileSize, _xferRecvFullFileSize);
      requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
  }

  if(lastFile)
  {
    // Tell ESP32 download complete and restart ESP32 since all files have been flashed
    ret = hcom_esp32_exec_add_flash_end();
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 File end error:%d\n",
                thisFile, __LINE__, ret);
    }
  }

  // Send text message to host
  hcom_host_send_simple_string_msg(requestType, 0, hostMsg, thisFile, __LINE__);

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64_ns();
  hcom_logging_syslog(LOG_INFO, "%s@%d-File transfer %d packets, took %llu mSec\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd,
           ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000));
#else
 #if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Host has sent %d packets\n",
            thisFile, __LINE__, _dbgNumbPacketsRecvd);
 #endif
#endif

  _xferCalcFullFileSize = 0;
  hcom_file_dnld_esp32_set_to_inactive();

  // Give time for CLI to receive all the messages
  usleep(500 * 1000);

  // Shutdown all of ESP32 comms
  hcom_esp32_stop_and_prep_for_restart();

}
#endif
