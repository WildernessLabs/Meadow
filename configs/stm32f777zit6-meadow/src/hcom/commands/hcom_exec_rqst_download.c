/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_exed_download.c
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
#include "../esp32/hcom_esp32_comms.h"

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _currentHcomDataPacketAction;
static bool _fileSystemOpenFailed;
static bool _fileDownloadFailedNoted;

static uint32_t _xferTargetMcuAddr;
static uint32_t _xferRecvFullFileCrc;
static uint32_t _xferRecvFullFileSize;
static uint32_t _xferMeadowCalcCrc = 0;  // This is over all the payload (original data)
static uint32_t _xferCalcFullFileSize = 0; // This is the size of the original
static uint32_t _xferCalcPacketCrc = 0;    // This is over all packets

static char _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH + 1];

static int _dbgNumbPacketsRecvd = 0;
static int _lastPercentSent;

#if HCOM_RECV_DEBUG_TIMING
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;

static uint64_t get_current_time64(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_rqst_download_file_rqst_setup()
{
    _fileDownloadFailedNoted = false;
    _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
  return OK;
}

//====================================================================
bool hcom_exec_rqst_download_is_download_active()
{
  return (_currentHcomDataPacketAction != CurrentHcomDataPacketActionNone);
}

//=======================================================================================
void hcom_exec_rqst_download_file_rqst_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
                                                uint32_t partitionId, uint16_t requestType)
{
  off_t msgOffset = 0;
  char *sendStartMsg;
  size_t fileNameLength;
  char *fileNameBuffer;
  int ret;
  
#ifndef CONFIG_MTD_PARTITION
  partitionId = 0;    // Ignore any other partition value
#endif

  _lastPercentSent = 0;
  _xferMeadowCalcCrc = 0; // Setup for checksum calculation of orig file
  _fileSystemOpenFailed = false;
  _fileDownloadFailedNoted = false;

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = get_current_time64();
#endif

  // File size
  _xferRecvFullFileSize = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                          (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Checksum
  _xferRecvFullFileCrc = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Destination address within the target MCU (only used by ESP32)
  _xferTargetMcuAddr = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Log some diagnostic information 
  switch(requestType)
  {
    case HCOM_MDOW_REQUEST_START_FILE_TRANSFER:
      // Meadow
      fileNameLength = recvPacketDataSize - (msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
      fileNameBuffer = malloc(fileNameLength + 1);
      memcpy(fileNameBuffer, recvPacketData + msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH,
              fileNameLength);
      fileNameBuffer[fileNameLength] = '\0';
      f7syslog(LOG_INFO, "Meadow download (Size:%d, Crc:0x%08x, Name:%s)\n",
              _xferRecvFullFileSize, _xferRecvFullFileCrc, fileNameBuffer);
      _currentHcomDataPacketAction = CurrentHcomDataPacketActionF7FileXfer;
      
      // Adding file to F7 file system
      ret = hcom_file_commands_open_active_file(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
      if (ret < 0)
      {
        _fileSystemOpenFailed = true;
        f7syslog(LOG_ERR, "%s@%d-Error:from call to open file in flash:%d\n", thisFile, __LINE__, ret);
      }
      free(fileNameBuffer);
      break;

    case HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER:
      // ESP32
      memcpy(_md5FileHash, recvPacketData + msgOffset, HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
      _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH] = '\0';

      f7syslog(LOG_INFO, "ESP32 download (Size:%d, Crc:0x%08x, MCUAddr:0x%08x, MD5Hash:%s)\n",
              _xferRecvFullFileSize, _xferRecvFullFileCrc, _xferTargetMcuAddr, _md5FileHash);
      _currentHcomDataPacketAction = CurrentHcomDataPacketActionEsp32FileXfer;

      // Adding file to ESP32-pico-d4 flash
      ret = hcom_esp32_exec_download_flash_start(_xferRecvFullFileSize, _xferTargetMcuAddr, _md5FileHash);
      if (ret < 0)
      {
        _fileSystemOpenFailed = true;
        _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
        f7syslog(LOG_ERR, "%s@%d-Error:from call for ESP32 start transfer:%d\n", thisFile, __LINE__, ret);
      }
      break;

      default:
        DEBUGASSERT(false); //Unknown file download request
        break;
  }

  // Send text message to host
  if (_fileSystemOpenFailed)
    sendStartMsg = "Failed to start file transfer";
  else
    sendStartMsg = "File transfer start begun";

  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendStartMsg,
          thisFile, __LINE__);
}

//============================================================================
// Process data packet based on currently active state
void hcom_exec_rqst_download_data_packet(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb)
{
  // TODO - verify that packets are numbered sequentially

  int ret;
  int msgOffset = sizeof(uint16_t); // size of sequence number

  if (_fileSystemOpenFailed)
  {
    if(!_fileDownloadFailedNoted)
    {
      // New feature - p-m This should send a message to host to stop sending
      f7syslog(LOG_ERR, "%s@%d-Error:Data packets ignored, previous error.\n", thisFile, __LINE__);
      _fileDownloadFailedNoted = true;
    }
    return;
  }

  _dbgNumbPacketsRecvd++;

  // Calculate the running checksum which includes the sequence number
  _xferCalcPacketCrc = crc32part(packet, packetSize, _xferCalcPacketCrc);

  const uint8_t *recvOrigData = packet + msgOffset;
  const size_t recvOrigDataSize = packetSize - msgOffset;

  if(seqNumb % 250 == 0)
    hcom_comms_dbg(LOG_DEBUG, "Sequence %d\n", seqNumb);

  // Compare _xferRecvFullFileSize with _xferCalcFullFileSize and send a message to host
  int percentDone = (_xferCalcFullFileSize  * 100) / _xferRecvFullFileSize;
  if(percentDone / 10 != _lastPercentSent)
  {
    // 10, 20 etc
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    _lastPercentSent = percentDone / 10;

    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "File %d%% downloaded", percentDone);

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
  }

  // Depending on what we're doing process this data packet
  switch (_currentHcomDataPacketAction)
  {
    case CurrentHcomDataPacketActionF7FileXfer:
      // Calculate CRC checksum of the payload without sequence number
      _xferMeadowCalcCrc = crc32part(recvOrigData, recvOrigDataSize, _xferMeadowCalcCrc);
      _xferCalcFullFileSize += recvOrigDataSize;

      ret = hcom_file_commands_write_to_active_file(recvOrigData, recvOrigDataSize);
      break;

    case CurrentHcomDataPacketActionEsp32FileXfer:
      ret = hcom_esp32_exec_add_flash_data(recvOrigData, recvOrigDataSize, seqNumb);
      break;

    default:
      ret = -1;
      f7syslog(LOG_ERR, "%s@%d-Error:Data Packet (SeqNumb=%d), unknown data packet action\n",
              thisFile, __LINE__, seqNumb);
      break;
  }

  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Data packet file write failed:%d seq:%d\n",
             thisFile, __LINE__, ret, seqNumb); usleep(10 * 1000);
  }
}

//=======================================================================================
// Process a end of file transfer message
void hcom_exec_rqst_download_file_rqst_end(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  char *sendMsgToHost;
  char *espCalculatedMd5;  
  int stringLen;
  uint16_t requestType;

  f7syslog(LOG_NOTICE, "End of %s transfer\n", _currentHcomDataPacketAction ? "Meadow" : "ESP32");
  switch(_currentHcomDataPacketAction)
  {
    case CurrentHcomDataPacketActionF7FileXfer:
      ret = hcom_file_commands_close_active_file();
      if (ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:File close:%d\n", thisFile, __LINE__, ret);
      }

      // Compare results and report to host
      if (_fileSystemOpenFailed)
      {
        sendMsgToHost = "File Start Failed.";
        stringLen = strlen(sendMsgToHost);
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
      else if (_xferMeadowCalcCrc == _xferRecvFullFileCrc && _xferCalcFullFileSize == _xferRecvFullFileSize)
      {
        stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Download success (checksums calc:0x%08X, expected:0x%08X)",
            _xferMeadowCalcCrc, _xferRecvFullFileCrc);
        sendMsgToHost = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
      }
      else
      {
        if (_xferMeadowCalcCrc != _xferRecvFullFileCrc)
        {
          stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Checksum error Calc=0x%08X, Recv=0x%08X",
                  _xferMeadowCalcCrc, _xferRecvFullFileCrc);
          sendMsgToHost = hostMsg;
          requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
        }
        else
        {
          DEBUGASSERT(_xferCalcFullFileSize != _xferRecvFullFileSize);
          stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "File size mismatch Calc=%d, Recv=%d",
                  _xferCalcFullFileSize, _xferRecvFullFileSize);
          sendMsgToHost = hostMsg;
          requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
        }
      }
      break;

    case CurrentHcomDataPacketActionEsp32FileXfer:
      ret = hcom_esp32_exec_add_flash_end(userData);
      if (ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:ESP32 File end error:%d\n", thisFile, __LINE__, ret);
      }

      // Compare the two MD5 hashs
      espCalculatedMd5 = hcom_esp32_exec_get_md5_file_hash();
      int cmpResult = strcmp(espCalculatedMd5, _md5FileHash);
      f7syslog(LOG_INFO, "Esp32 MD5 hash:'%s', CLI MD5 hash:'%s', %s\n", espCalculatedMd5, _md5FileHash,
              cmpResult == 0 ? "Success" : "Error");
      if(cmpResult == 0)
      {
        stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "File Sent Success MD5 calc='%s', CLI='%s')", espCalculatedMd5, _md5FileHash);
        sendMsgToHost = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
      }
      else
      {
          stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "MD5 hash compare error MD5 calc:%s, CLI:%s)", espCalculatedMd5, _md5FileHash);
          sendMsgToHost = hostMsg;
          requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
      break;

      default:
        f7syslog(LOG_ERR, "%s@%d-Error:unknown end data packet action:%d \n", thisFile, __LINE__, _currentHcomDataPacketAction);
        //DEBUGASSERT(false); //Unknown file download request
        break;
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(requestType, 0, sendMsgToHost, thisFile, __LINE__);

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionEndedAt = get_current_time64();
  hcom_comms_dbg(LOG_DEBUG, "File transfer %d packets, took %llu mSec, CalcPacketCRC:0x%08x CalcFileCRC:0x%08x\n",
           _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           _xferCalcPacketCrc, _xferMeadowCalcCrc);
#else
  hcom_comms_dbg(LOG_DEBUG, "Host has sent %d packets\n", _dbgNumbPacketsRecvd);
#endif

  _xferCalcPacketCrc = 0;
  _xferCalcFullFileSize = 0;
  _xferMeadowCalcCrc = 0; // Set to 0 for next message

  _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
}
