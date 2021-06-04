/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dnld_proc.c
 * 
 *   Copyright (C) 2019-2020 Wilderness Labs. All rights reserved.
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

#define HCOM_RECV_DEBUG_TIMING 0          // Enables the display of time spent

// This needs to allow 30 seconds. The receiving code while downloading
// normally waits HCOM_RECV_TIMEOUT_ACTIVE_SECONDS seconds.
#define HCOM_DNLD_PROC_ESP_START_CNT    (30/HCOM_RECV_TIMEOUT_ACTIVE_SECONDS)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _currentHcomDataPacketAction;
static char *_fileNameBuffer;               // F7 Flash file system
static uint32_t _xferRecvFullFileCrc;       // F7 Flash file system
static uint32_t _xferRecvFullFileSize;      // Both
static uint32_t _xferMeadowCalcCrc = 0;     // This is over all the payload (original data)
static uint32_t _xferCalcFullFileSize = 0;  // This is the size of the original
static uint32_t _xferCalcPacketCrc = 0;     // This is over all packets
static uint32_t _partitionId = 0;
static int _dbgNumbPacketsRecvd = 0;

static int _lastPercentSent;
static int _esp32WaitCount;

#if defined (CONFIG_HCOM_ESP32_COMMS)
static uint32_t _xferTargetMcuAddr;         // ESP32
static char _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH + 1];
#endif

#if HCOM_RECV_DEBUG_TIMING > 0
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_dnld_proc_setup()
{
    _currentHcomDataPacketAction = HcomDnldActionNone;
  return OK;
}

//==========================================================================
// Are we actively receiving a file?
bool hcom_file_dnld_proc_is_active()
{
  return (_currentHcomDataPacketAction != HcomDnldActionNone);
}

//==========================================================================
// Are we starting an ESP32 download? If the receive thread will ignore
// it's timeout for a bit longer.
bool hcom_file_dnld_proc_wait_for_esp32_starting()
{
  if(_currentHcomDataPacketAction != HcomDnldActionEsp32Starting)
    return false;

  _esp32WaitCount++;
  if(_esp32WaitCount > HCOM_DNLD_PROC_ESP_START_CNT)
    return false;
    
  return true;
}

//==========================================================================
// The state needs to be restored to action none state.
void hcom_file_dnld_restore_to_inactive_state()
{
  _currentHcomDataPacketAction = HcomDnldActionNone;
}

//==========================================================================
// Beginning of a file download into the flash file system
// Note: This function is shared by all download types that store in the
// flash file system.
void hcom_file_dnld_proc_flash_file_sys_begin(const uint8_t *recvPayloadData,
          const size_t recvPayloadSize, uint32_t partitionId, uint16_t requestType)
{
  int ret;
  off_t msgOffset = 0;
  size_t fileNameLength;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen = 0;
  _dbgNumbPacketsRecvd = 0;
  _currentHcomDataPacketAction = HcomDnldActionMeadowStarting;

#ifdef CONFIG_MTD_PARTITION
  _partitionId = partitionId;
#else
  _partitionId = 0;    // Ignore any other partition value
#endif

  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;
  _xferMeadowCalcCrc = 0; // Setup for checksum calculation of orig file
  _fileNameBuffer = NULL;
  

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionBeganAt = hcom_utils_get_current_time64();
#endif

  // TODO:This should be based on a struct
  // File size
  _xferRecvFullFileSize = recvPayloadData[msgOffset] + (recvPayloadData[msgOffset + 1] << 8) +
                          (recvPayloadData[msgOffset + 2] << 16) + (recvPayloadData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Checksum
  _xferRecvFullFileCrc = recvPayloadData[msgOffset] + (recvPayloadData[msgOffset + 1] << 8) +
                         (recvPayloadData[msgOffset + 2] << 16) + (recvPayloadData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Account for MCU address field
  msgOffset += sizeof(uint32_t);

  // Skip past the MD5 field (32 bytes)
  fileNameLength = recvPayloadSize - (msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
  _fileNameBuffer = malloc(fileNameLength + 1);

  memcpy(_fileNameBuffer, recvPayloadData + msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH,
          fileNameLength);
  _fileNameBuffer[fileNameLength] = '\0';

  // Log some diagnostic information 
  hcom_logging_syslog(LOG_INFO, "%s@%d-Meadow downloading file (Size:%d, Crc:0x%08x, Name:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferRecvFullFileCrc, _fileNameBuffer);

  // Adding file to F7 file system
  ret = hcom_file_write_del_open_active_file(_partitionId, HCOM_FILE_MOUNT_POINT_TARGET, _fileNameBuffer);

  if (ret < 0)
  {
    char *errorCause;
    switch(ret)
    {
      case -EEXIST: // File already open
      errorCause = "Another file is being processed";
      break;
      
      case -ENAMETOOLONG: // File name too long
      errorCause = "File name too long";
      break;
      
      case -ENOENT: // No such directory
      errorCause = "No such directory";
      break;
      
      case -EMFILE: // Too many files open
      errorCause = "Too many files open";
      break;

      default:  // different error
      snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Unexpected error:%d");
      errorCause = hostMsg;
      break;
    }

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File '%s' download to Meadow failed because %s", _fileNameBuffer, errorCause);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);

    hcom_file_dnld_restore_to_inactive_state();
    free(_fileNameBuffer);

    // Notify CLI that something when wrong with opening the file
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_FILE_START_FAIL, 0, thisFile, __LINE__);
    return;
  }

  // Best to change the current action before telling CLI ok to send
  _currentHcomDataPacketAction = HcomDnldActionMeadowFileXfer;

  // Notify CLI that it's okay to send the file data
  hcom_host_send_header_msg(HCOM_HOST_REQUEST_FILE_START_OKAY, 0, thisFile, __LINE__);
}

//============================================================================
void hcom_file_dnld_proc_esp32_flash_begin(const uint8_t *recvPayloadData,
          const size_t recvPayloadSize, uint32_t partitionId, uint16_t requestType)
{
#if defined (CONFIG_HCOM_ESP32_COMMS)
  int ret;
  off_t msgOffset = 0;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;

  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;

  DEBUGASSERT(_fileNameBuffer == NULL);
  DEBUGASSERT(requestType == HCOM_MDOW_REQUEST_START_ESP_FILE_TRANSFER);

  // Verify that mono has been disabled
  if(hcom_mono_ctrl_is_mono_enabled())
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Mono must be disabled for ESP32 file download");
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, hostMsg);
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);

    // Notify CLI that download can't start because mono is enabled
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_FILE_START_FAIL, 0, thisFile, __LINE__);

    hcom_file_dnld_restore_to_inactive_state();
    return;
  }

  // Best to change the current action before telling CLI ok to send
  _esp32WaitCount = 0;
  _currentHcomDataPacketAction = HcomDnldActionEsp32Starting;
  _dbgReceptionBeganAt = hcom_utils_get_current_time64();
#endif

  // TODO:This should be based on a struct not inline addition
  // File size
  _xferRecvFullFileSize = recvPayloadData[msgOffset] + (recvPayloadData[msgOffset + 1] << 8) +
                          (recvPayloadData[msgOffset + 2] << 16) + (recvPayloadData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Account for checksum which is not used by ESP32. ESP32 uses MD5 calculation instead
  msgOffset += sizeof(uint32_t);

  // Destination address within the target MCU (only used by ESP32)
  _xferTargetMcuAddr = recvPayloadData[msgOffset] + (recvPayloadData[msgOffset + 1] << 8) +
                         (recvPayloadData[msgOffset + 2] << 16) + (recvPayloadData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  memcpy(_md5FileHash, recvPayloadData + msgOffset, HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
  _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH] = '\0';

  // Log some diagnostic information 
  hcom_logging_syslog(LOG_INFO, "%s@%d-Start ESP32 download (Size:%d, MCUAddr:0x%08x, MD5Hash:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferTargetMcuAddr, _md5FileHash);

  // Adding file to ESP32-pico-d4 flash
  ret = hcom_esp32_exec_download_flash_start(_xferRecvFullFileSize, _xferTargetMcuAddr);
  if (ret < 0)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File download to ESP32 flash at '0x%08x' was unable to begin", _xferTargetMcuAddr);
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
            
    hcom_file_dnld_restore_to_inactive_state();

    // Notify CLI that something when wrong with start
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_FILE_START_FAIL, 0, thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, "%s@%d-download ESP32 start transfer:%d\n", thisFile, __LINE__, ret);
    return;
  }

  _currentHcomDataPacketAction = HcomDnldActionEsp32FileXfer;

  // Notify CLI that it's okay to send data
  hcom_host_send_header_msg(HCOM_HOST_REQUEST_FILE_START_OKAY, 0, thisFile, __LINE__);
#endif
}

//============================================================================
// Process data packet based on currently active state
// Note:This function is shared by all download types
void hcom_file_dnld_proc_recvd_file_data(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb)
{
  int ret;
  int msgOffset = sizeof(uint16_t); // size of sequence number

  _dbgNumbPacketsRecvd++;

  // Calculate the running checksum which includes the sequence number
  _xferCalcPacketCrc = crc32part(packet, packetSize, _xferCalcPacketCrc);

  const uint8_t *recvOrigData = packet + msgOffset;
  const size_t recvOrigDataSize = packetSize - msgOffset;

  if(seqNumb % 250 == 0)
    hcom_logging_syslog(LOG_DEBUG, "Sequence %d\n", seqNumb);

  // Compare _xferRecvFullFileSize with _xferCalcFullFileSize and send a message to host
  int percentDone = (_xferCalcFullFileSize  * 100) / _xferRecvFullFileSize;
  if(percentDone / 10 != _lastPercentSent)
  {
    // 10, 20 etc
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    _lastPercentSent = percentDone / 10;

    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "File %d%% downloaded", percentDone);
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
  }

  // Depending on what we're doing, process this data packet
  switch (_currentHcomDataPacketAction)
  {
    case HcomDnldActionMeadowFileXfer:
      // Calculate CRC checksum of the payload without sequence number
      _xferMeadowCalcCrc = crc32part(recvOrigData, recvOrigDataSize, _xferMeadowCalcCrc);

      ret = hcom_file_write_del_add_to_active_file(recvOrigData, recvOrigDataSize);
      break;

    case HcomDnldActionEsp32FileXfer:
#if defined (CONFIG_HCOM_ESP32_COMMS)
      ret = hcom_esp32_exec_add_flash_data(recvOrigData, recvOrigDataSize, seqNumb);
#endif
      break;
      
    default:
      ret = -1;
      hcom_logging_syslog(LOG_ERR, "%s@%d-Data Packet (SeqNumb=%d), unexpected action:%d\n",
              thisFile, __LINE__, seqNumb, _currentHcomDataPacketAction);
      break;
  }
  
  _xferCalcFullFileSize += recvOrigDataSize;

  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Data packet file write failed:%d seq:%d\n",
             thisFile, __LINE__, ret, seqNumb);
  }
}

//=======================================================================================
// Process a end of file transfer message for all but ESP32
// Note: userData is 0 unless it's last file to be sent by the CLI command.
// For F7 this value is not used but the ESP32 needs this defined.
void hcom_file_dnld_proc_flash_file_sys_end(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  char *sendMsgToHost;
  int stringLen = 0;
  uint16_t requestType;

  hcom_logging_syslog(LOG_NOTICE, "End of file transfer\n");

  DEBUGASSERT(_currentHcomDataPacketAction == HcomDnldActionMeadowFileXfer);

  ret = hcom_file_write_del_close_active_file();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s close:%d\n", thisFile, __LINE__, _fileNameBuffer, ret);
  }

  // Construct file name
  char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

#ifdef CONFIG_MTD_PARTITION
  stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, _partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  strcpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET);
#endif

  stringLen = snprintf(completeNameBuf, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", 
            fullMountPtName, _fileNameBuffer);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

  off_t fileSize;       // Not used
  uint32_t blockSizeKB; // Not used
  uint32_t actualFileCrc = hcom_file_lists_calc_crc_for_file(completeNameBuf,
                &fileSize, &blockSizeKB);
  free(completeNameBuf);
  free(fullMountPtName);
  
  // Compare results and report to host
  if (_xferMeadowCalcCrc == _xferRecvFullFileCrc && _xferMeadowCalcCrc == actualFileCrc
              && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "Download of '%s' success (checksums calc:0x%08X, expected:0x%08X)",
        _fileNameBuffer, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
    sendMsgToHost = hostMsg;
    requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
  }
  else
  {
    if (_xferMeadowCalcCrc != _xferRecvFullFileCrc || _xferMeadowCalcCrc != actualFileCrc)
    {
      stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "Download of '%s' failed due to checksum mismatch, file:0x%08X, download:0x%08X, received from CLI:0x%08X",
              _fileNameBuffer, actualFileCrc, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
      sendMsgToHost = hostMsg;
      requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
    else
    {
      stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "Download of '%s' failed due to file size mismatch Meadow calculated:%d, received from CLI:%d",
              _fileNameBuffer, _xferCalcFullFileSize, _xferRecvFullFileSize);
      sendMsgToHost = hostMsg;
      requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(requestType, 0, sendMsgToHost, thisFile, __LINE__);

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64();
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-File transfer %d packets, took %llu mSec, CalcPacketCRC:0x%08x CalcFileCRC:0x%08x\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           _xferCalcPacketCrc, _xferMeadowCalcCrc);
#else
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Host has sent %d packets\n", thisFile, __LINE__, _dbgNumbPacketsRecvd);
#endif

  if(_fileNameBuffer != NULL)
  {
    free(_fileNameBuffer);
    _fileNameBuffer = NULL;
  }

  _xferCalcPacketCrc = 0;
  _xferCalcFullFileSize = 0;
  _xferMeadowCalcCrc = 0; // Set to 0 for next message

  hcom_file_dnld_restore_to_inactive_state();
}

//=======================================================================================
// Process a end of ESP32 file transfer message
// Note: The the userData is 0 unless it's last file to be sent by the CLI command.
// For F7 this value is not used but the ESP32 uses this to reboot the ESP32.
// Only after a reboot will the ESP32 attempt to execute the program.
void hcom_file_dnld_proc_esp32_flash_end(uint32_t userData)
{
#if defined (CONFIG_HCOM_ESP32_COMMS)

  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  char *espCalculatedMd5;
  bool lastFile = userData == 1 ? true : false;
  int stringLen = 0;
  uint16_t requestType;

  hcom_logging_syslog(LOG_NOTICE, "End of ESP32 transfer\n");

  DEBUGASSERT(_currentHcomDataPacketAction == HcomDnldActionEsp32FileXfer);

  // Compare the two MD5 hashs
  espCalculatedMd5 = hcom_esp32_exec_get_md5_file_hash();
  int md5CmpResult = strcmp(espCalculatedMd5, _md5FileHash);

  hcom_logging_syslog(LOG_INFO,
          "%s@%d-File end-Esp32 calculated MD5:'%s', received from CLI MD5:'%s', %s\n",
          thisFile, __LINE__, espCalculatedMd5, _md5FileHash,
          md5CmpResult == 0 ? "Success" : "Error");
  
  if(md5CmpResult == 0 && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "File Sent Success MD5 ESP32 Calulated:'%s', received from CLI:'%s')",
            espCalculatedMd5, _md5FileHash);
    requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
  }
  else
  {
    if(md5CmpResult != 0)
    {
      stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "MD5 hash compare error MD5 ESP32 Calculated:%s, received from CLI:%s)",
                espCalculatedMd5, _md5FileHash);
      requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
    else
    {
      stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
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
      hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 File end error:%d\n", thisFile, __LINE__, ret);
    }
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(requestType, 0, hostMsg, thisFile, __LINE__);

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64();
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-File transfer %d packets, took %llu mSec\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000));
#else
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Host has sent %d packets\n", thisFile, __LINE__, _dbgNumbPacketsRecvd);
#endif

  _xferCalcFullFileSize = 0;
  hcom_file_dnld_restore_to_inactive_state();

  // Give time for CLI to receive all the messages
  usleep(500 * 1000);

  // Shutdown all of ESP32 comms
  hcom_esp32_stop_and_prep_for_restart();

#endif
}