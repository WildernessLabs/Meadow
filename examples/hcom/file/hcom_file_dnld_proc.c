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

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _currentHcomDataPacketAction;
static bool _initFileSysError;
static bool _fileDownloadFailedNoted;

static char *_fileNameBuffer;               // F7 Flash file system
static uint32_t _xferRecvFullFileCrc;       // F7 Flash file system
static uint32_t _xferRecvFullFileSize;      // Both
static uint32_t _xferMeadowCalcCrc = 0;     // This is over all the payload (original data)
static uint32_t _xferCalcFullFileSize = 0;  // This is the size of the original
static uint32_t _xferCalcPacketCrc = 0;     // This is over all packets
static int _dbgNumbPacketsRecvd = 0;
static int _lastPercentSent;


#if defined (CONFIG_HCOM_ESP32_COMMS)
static uint32_t _xferTargetMcuAddr;         // ESP32
static char _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH + 1];
#endif

#if HCOM_RECV_DEBUG_TIMING
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
    _fileDownloadFailedNoted = false;
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
// The state needs to be restored to action none state.
void hcom_file_dnld_restore_to_inactive_state()
{
  _currentHcomDataPacketAction = HcomDnldActionNone;
}

//==========================================================================
// Beginning of a file download into the flash file system
// Note: This function is shared by all download types that store in the
// flash file system.
void hcom_file_dnld_proc_flash_file_sys_begin(const uint8_t *recvPacketData,
          const size_t recvPacketDataSize, uint32_t partitionId, uint16_t requestType)
{
  int ret;
  off_t msgOffset = 0;
  size_t fileNameLength;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen = 0;

#ifndef CONFIG_MTD_PARTITION
  partitionId = 0;    // Ignore any other partition value
#endif

  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;
  _xferMeadowCalcCrc = 0; // Setup for checksum calculation of orig file
  _initFileSysError = false;
  _fileDownloadFailedNoted = false;
  _fileNameBuffer = NULL;
  
  _currentHcomDataPacketAction = HcomDnldActionMeadowFileXfer;

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = hcom_utils_get_current_time64();
#endif

  // File size
  _xferRecvFullFileSize = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                          (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Checksum
  _xferRecvFullFileCrc = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Account for MCU address field
  msgOffset += sizeof(uint32_t);

  // Skip past the MD5 field (32 bytes)
  fileNameLength = recvPacketDataSize - (msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
  _fileNameBuffer = malloc(fileNameLength + 1);

  memcpy(_fileNameBuffer, recvPacketData + msgOffset + HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH,
          fileNameLength);
  _fileNameBuffer[fileNameLength] = '\0';

  // Log some diagnostic information 
  hcom_logging_syslog(LOG_INFO, "%s@%d-Meadow downloading file (Size:%d, Crc:0x%08x, Name:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferRecvFullFileCrc, _fileNameBuffer);

  // Adding file to F7 file system
  ret = hcom_file_write_del_open_active_file(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, _fileNameBuffer);
  if (ret < 0)
  {
    _initFileSysError = true;
    _currentHcomDataPacketAction = HcomDnldActionNone;
    hcom_logging_syslog(LOG_ERR, "%s@%d-from call to open file in flash:%d\n", thisFile, __LINE__, ret);
  }

  if (_initFileSysError)
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File download to Meadow failed to start for '%s'", _fileNameBuffer);
  else
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Meadow file download of '%s' has begun", _fileNameBuffer);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);
}

//============================================================================
void hcom_file_dnld_proc_esp32_flash_begin(const uint8_t *recvPacketData,
          const size_t recvPacketDataSize, uint32_t partitionId, uint16_t requestType)
{
#if defined (CONFIG_HCOM_ESP32_COMMS)
  int ret;
  off_t msgOffset = 0;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen = 0;

  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;
  _initFileSysError = false;
  _fileDownloadFailedNoted = false;
  _fileNameBuffer = NULL;

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
            
    _initFileSysError = true;
    return;
  }

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = hcom_utils_get_current_time64();
#endif

  // File size
  _xferRecvFullFileSize = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                          (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Account for checksum which is not used by ESP32. ESP32 uses MD5 calculation instead
  msgOffset += sizeof(uint32_t);

  // Destination address within the target MCU (only used by ESP32)
  _xferTargetMcuAddr = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  memcpy(_md5FileHash, recvPacketData + msgOffset, HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH);
  _md5FileHash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH] = '\0';

  _currentHcomDataPacketAction = HcomDnldActionEsp32FileXfer;

  // Log some diagnostic information 
  hcom_logging_syslog(LOG_INFO, "%s@%d-Start ESP32 download (Size:%d, MCUAddr:0x%08x, MD5Hash:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferTargetMcuAddr, _md5FileHash);

  // Adding file to ESP32-pico-d4 flash
  ret = hcom_esp32_exec_download_flash_start(_xferRecvFullFileSize, _xferTargetMcuAddr);
  if (ret < 0)
  {
    _initFileSysError = true;
    _currentHcomDataPacketAction = HcomDnldActionNone;
    hcom_logging_syslog(LOG_ERR, "%s@%d-download ESP32 start transfer:%d\n", thisFile, __LINE__, ret);
  }

  if (_initFileSysError)
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File download to ESP32 flash at '0x%08x' did not begin", _xferTargetMcuAddr);
  else
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File download to ESP32 flash at '0x%08x' has begun", _xferTargetMcuAddr);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);
#endif
}

//============================================================================
// Process data packet based on currently active state
// Note:This function is shared by all download types
void hcom_file_dnld_proc_recvd_file_data(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb)
{
  int ret;
  int msgOffset = sizeof(uint16_t); // size of sequence number

  if (_initFileSysError)
  {
    if(!_fileDownloadFailedNoted)
    {
      // Someday, when nothing else to do, modify the protocol so that the
      // host knows to not send data that's going to be trashed sending.
      hcom_logging_syslog(LOG_ERR, "%s@%d-Data packets will be ignored.\n", thisFile, __LINE__);
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
      hcom_logging_syslog(LOG_ERR, "%s@%d-Data Packet (SeqNumb=%d), unknown data packet action\n",
              thisFile, __LINE__, seqNumb);
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

  // At present the CLI isn't smart enough to know that continuing to send data
  // when the file could not be opened is a waste of time.
  if (_initFileSysError)
  {
    if(_fileNameBuffer != NULL)
    {
      free(_fileNameBuffer);
      _fileNameBuffer = NULL;
    }
    return;
  }

  hcom_logging_syslog(LOG_NOTICE, "End of file transfer\n");

  DEBUGASSERT(_currentHcomDataPacketAction == HcomDnldActionMeadowFileXfer);

  ret = hcom_file_write_del_close_active_file();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s close:%d\n", thisFile, __LINE__, _fileNameBuffer, ret);
  }

  // Compare results and report to host
  if (_xferMeadowCalcCrc == _xferRecvFullFileCrc && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "Download of '%s' success (checksums calc:0x%08X, expected:0x%08X)",
        _fileNameBuffer, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
    sendMsgToHost = hostMsg;
    requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
  }
  else
  {
    if (_xferMeadowCalcCrc != _xferRecvFullFileCrc)
    {
      stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "Download of '%s' failed due to checksum mismatch, Meadow calculated:0x%08X, received from CLI:0x%08X",
              _fileNameBuffer, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
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

#if HCOM_RECV_DEBUG_TIMING
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

  _currentHcomDataPacketAction = HcomDnldActionNone;
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

  // At present the CLI isn't smart enough to know that continuing to send data
  // when the file could not be opened is a waste of time.
  if(_initFileSysError)
  {
    return;
  }

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

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionEndedAt = hcom_utils_get_current_time64();
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-File transfer %d packets, took %llu mSec\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000));
#else
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Host has sent %d packets\n", thisFile, __LINE__, _dbgNumbPacketsRecvd);
#endif

  _xferCalcFullFileSize = 0;
  _currentHcomDataPacketAction = HcomDnldActionNone;

  // Give time for CLI to receive all the messages
  usleep(500 * 1000);

  // Shutdown all of ESP32 comms
  hcom_esp32_stop_and_prep_for_restart();

#endif
}