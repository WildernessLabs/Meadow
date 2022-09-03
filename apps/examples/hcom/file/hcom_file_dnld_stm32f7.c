/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dnld_stm32f7.c
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

// The functions in the file setup for a file write, write the file and
// end the download process while verifying file integrity.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 0          // Enables the display of time spent

#warning "(--) Peter working here"

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _currentF7DnldState;
static char *_simpleFileName;
static uint32_t _xferRecvFullFileCrc;
static uint32_t _xferRecvFullFileSize;    // File size based on received data
static uint32_t _xferCalcFullFileSize;    // This is the size of the original
static uint32_t _xferMeadowCalcCrc;       // This is over all the payload (original data)
static uint32_t _partitionId;
static int _lastPercentSent;
static bool _stateErrShown;

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
static int _dbgNumbPacketsRecvd = 0;        // Only used in LOG_INFO & LOG_DEBUG messages
#endif

#if HCOM_RECV_DEBUG_TIMING > 0
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;
#endif

// How long before the watchdog wakes up if there is not download activity?
#define HCOM_FILE_DNLD_STM32F7_WDOG_TIME (3)

//--------------------------------------------------------------------
// This enum defines the current processing state of the download code for a
// specific download.
// The protocol could be modified so that each data packet contains this
// information. This would allow more than one operation to be processed
// at a time.
// To do this the protocol would need to carry an additional field to identify
// the "session" a particular data packet belonged to. Each data packet's
// session would be unuque and the sequence numbers 1-n would be unique for
// each session.
enum hcom_download_stm32f7_packet_state
{
  HcomStm32F7DnldStateNone = 0,
  HcomStm32F7DnldStateStarting = 1,
  HcomStm32F7DnldStateFileXfer = 2,
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_dnld_stm32f7_setup()
{
    _stateErrShown = false;   // In case of data before begin
    _currentF7DnldState = HcomStm32F7DnldStateNone;
  return OK;
}

//==========================================================================
// Are we involved in some download activity?
bool hcom_file_dnld_stm32f7_is_active()
{
  return (_currentF7DnldState != HcomStm32F7DnldStateNone);
}

//==========================================================================
void hcom_file_dnld_stm32f7_free_file_name_buf(void)
{
  // Cleanup resources and state
  if(_simpleFileName != NULL)
  {
    free(_simpleFileName);
    _simpleFileName = NULL;
  }
}

//==========================================================================
// The free memory and return to action inactive state
void hcom_file_dnld_stm32f7_set_to_inactive()
{
  _currentF7DnldState = HcomStm32F7DnldStateNone;
}

//==========================================================================
// Beginning of a file download into the flash file system.
// Called from hcom_host_route.c
void hcom_file_dnld_stm32f7_file_begin(const HcomProtoHdrMsg_t *hdrMsg,
      const size_t packetSize, uint32_t partitionId, uint16_t requestType)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd = 0;
#endif

  _currentF7DnldState = HcomStm32F7DnldStateStarting;

#ifdef CONFIG_MTD_PARTITION
  _partitionId = partitionId;
#else
  _partitionId = 0;    // Ignore any other partition value
#endif

  // Prep for download
  _stateErrShown = false;
  _lastPercentSent = 0;
  _xferCalcFullFileSize = 0;
  _xferMeadowCalcCrc = 0; // Setup for checksum calculation of orig file
  _simpleFileName = NULL;
  

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionBeganAt = hcom_utils_get_current_time64_ns();
#endif

  // File size, checksum & name length
  // File size, checksum & name length
  size_t fileNameLength = packetSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;

  _xferRecvFullFileSize = fileMsg->fileInfo.fileSize;
  _xferRecvFullFileCrc = fileMsg->fileInfo.fileCheckSum;
  _simpleFileName = malloc(fileNameLength + 1);
  if(_simpleFileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }

  memcpy(_simpleFileName, fileMsg->fileInfo.fileName, fileNameLength);
  _simpleFileName[fileNameLength] = '\0';

  // Log some diagnostic information
  hcom_logging_syslog(LOG_INFO, "%s@%d-Meadow downloading file (FileLen:%d, Crc:0x%08x, Name:%s)\n",
          thisFile, __LINE__, _xferRecvFullFileSize, _xferRecvFullFileCrc,
          _simpleFileName);

  // Adding file to F7 file system
  ret = hcom_file_write_open_active_file(_partitionId, HCOM_FILE_MOUNT_POINT_TARGET,
            _simpleFileName);
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
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Unexpected error:%d");
      errorCause = hostMsg;
      break;
    }

    // Notify CLI that something when wrong with opening the file
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "File '%s' download to Meadow failed because %s", _simpleFileName, errorCause);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL, 0, hostMsg,
          thisFile, __LINE__);

    // Cleanup after failure
    hcom_file_dnld_stm32f7_free_file_name_buf();
    hcom_file_dnld_stm32f7_set_to_inactive();
  }
  else
  {
    // Initialize and Start watchdog timer
    ret = hcom_file_misc_timer_init(_simpleFileName);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer init errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
    }

    ret = hcom_file_misc_timer_set(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
    }

    // Set current action
    _currentF7DnldState = HcomStm32F7DnldStateFileXfer;

    // Notify CLI that it's okay to send the file's data now
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY, 0, thisFile, __LINE__);
  }
}

//============================================================================
// Process a data packet
void hcom_file_dnld_stm32f7_recvd_file_data(const HcomProtoDataMsg_t *hcomDataMsg,
          const size_t packetSize)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  // Reset the watchdog
  ret = hcom_file_misc_timer_set(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  // Ignore download if it's not expected
  if(_currentF7DnldState != HcomStm32F7DnldStateFileXfer)
  {
    // Show problem, but only once
    if(! _stateErrShown)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Unexpected data packet received\n",
              thisFile, __LINE__, ret);
      _stateErrShown = true;
    }

    // Don't do any processing
    return;
  }

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

  // Calculate CRC checksum of the payload without sequence number
  _xferMeadowCalcCrc = crc32part(hcomDataMsg->binData, binDataLen,
            _xferMeadowCalcCrc);

  // Actually write the data
  ret = hcom_file_write_to_active_file(hcomDataMsg->binData,
            binDataLen);

  _xferCalcFullFileSize += binDataLen;

  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Data packet for %s failed:%d seq:%d\n",
             thisFile, __LINE__, _simpleFileName, ret, seqNumb);

    // Notify host
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "At packet %d of '%s' failed",
              seqNumb, _simpleFileName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    hcom_file_dnld_stm32f7_free_file_name_buf();
    hcom_file_dnld_stm32f7_set_to_inactive();
  }
}

//=======================================================================================
// Process the end of transfer message from CLI.
void hcom_file_dnld_stm32f7_file_end(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  char *sendMsgToHost;
  uint16_t requestType;

  hcom_logging_syslog(LOG_NOTICE, "End of file transfer\n");

  // Stop and delete watchdog
  ret = hcom_file_misc_timer_delete();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n", thisFile, __LINE__, errno, ret);
  }

  // Allocate memory
  char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(completeNameBuf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }

  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fullMountPtName == NULL)
  {
    free(completeNameBuf);
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }

  if(_currentF7DnldState != HcomStm32F7DnldStateFileXfer)
  {
    hcom_logging_syslog(LOG_WARNING, "%s@%d-Dnld end, unexpected state\n",
              thisFile, __LINE__);
    // Continue even with error
  }

  ret = hcom_file_write_close_active_file();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s close failed:%d\n",
              thisFile, __LINE__, _simpleFileName, ret);
    // Continue even with error
  }

  // Construct file name
#ifdef CONFIG_MTD_PARTITION
  snprintf_chk(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, _partitionId);
#else
  strcpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET);
#endif

  snprintf_chk(completeNameBuf, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", 
            fullMountPtName, _simpleFileName);

  off_t fileSize;       // Required by function call but not used
  uint32_t blockSizeKB; // Required by function call but not used
  int detectError = OK;

  uint32_t actualFileCrc = hcom_file_misc_calc_crc_for_file(completeNameBuf,
                &fileSize, &blockSizeKB, &detectError);

  free(completeNameBuf);
  free(fullMountPtName);

  // Report to host
  if(detectError < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error in Checksum calculation err:%d\n",
              thisFile, __LINE__, detectError);

    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Download of '%s' state unknown due to checksum calulation fault:%d",
            _simpleFileName, detectError);
    sendMsgToHost = hostMsg;
    requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
  }
  else
  {
    // Compare results and report to host
    if (_xferMeadowCalcCrc == _xferRecvFullFileCrc && _xferMeadowCalcCrc == actualFileCrc
                && _xferCalcFullFileSize == _xferRecvFullFileSize)
    {
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Download of '%s' success (checksums calculated:0x%08X, expected:0x%08X)",
          _simpleFileName, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
      sendMsgToHost = hostMsg;
      requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    }
    else
    {
      if (_xferMeadowCalcCrc != _xferRecvFullFileCrc || _xferMeadowCalcCrc != actualFileCrc)
      {
        snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "Download of '%s' failed due to checksum mismatch, f/s read:0x%08X, dnld calc:0x%08X, sender:0x%08X",
                _simpleFileName, actualFileCrc, _xferMeadowCalcCrc, _xferRecvFullFileCrc);
        sendMsgToHost = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
      else
      {
        snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "Download of '%s' failed due to file size mismatch calculated:%d, sender:%d",
                _simpleFileName, _xferCalcFullFileSize, _xferRecvFullFileSize);
        sendMsgToHost = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
    }
  }

  if(requestType == HCOM_HOST_REQUEST_TEXT_ERROR)
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, sendMsgToHost);

  // Send text message to host
  hcom_host_send_simple_string_msg(requestType, 0, sendMsgToHost, thisFile, __LINE__);

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64_ns();
  hcom_logging_syslog(LOG_INFO, "%s@%d-File transfer %d packets, took %llu mSec, CalcFileCRC:0x%08x\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           _xferMeadowCalcCrc);
#endif

  hcom_file_dnld_stm32f7_free_file_name_buf();
  hcom_file_dnld_stm32f7_set_to_inactive();
}
