/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dnld_stm32f7.c
 * 
 *   Copyright (C) 2019-2023 Wilderness Labs. All rights reserved.
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
#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_dnld_shared.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) hcom_file_dnld_stm32f7.c"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

// Display time spent downloading via syslog
#define HCOM_FILE_DNLD_F7_DEBUG_TIMING       (0)

// For no cache behavior, set the 2 following to '0'
#define HCOM_FILE_DNLD_CREATE_MEMORY_CACHE   (0)   // Cache file then write
#define HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS  (0)   // No file write
#define HCOM_FILE_DNLD_MAX_CACHE_FILE_SIZE   (8 * 1024 * 1024)  // 8MB limit

// This combination is disallowed
#if(HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 0 &&\
  HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS == 1)
#pragma GCC error "Illegal download configuration\n"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _stateErrShown;

#if (HCOM_FILE_DNLD_F7_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
static int _dbgNumbPacketsRecvd = 0;        // Only used in LOG_INFO & LOG_DEBUG messages
#endif

#if HCOM_FILE_DNLD_F7_DEBUG_TIMING > 0
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;
#endif

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
void *_dnldCacheMemory;
off_t _dnldCacheOffset;
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_dnld_stm32f7_setup()
{
  _stateErrShown = false;   // In case of data before begin
  return OK;
}

//==========================================================================
static void hcom_file_dnld_cleanup_cache_memory(void)
{
#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
    if (_dnldCacheMemory)
  {
    free(_dnldCacheMemory);
    _dnldCacheMemory = NULL;
    _dnldCacheOffset = 0;
  }
#endif
}

//==========================================================================
// Beginning of a file download into the flash file system.
// Called from hcom_host_route.c. The incomplete file name has been supplied.
int hcom_file_dnld_stm32f7_file_begin(const HcomProtoHdrMsg_t *hdrMsg,
          hcom_dnld_shared_t *dnldShared)
{
#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
  _dnldCacheOffset = 0;
  _dnldCacheMemory = NULL;
#endif

  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;

#if (HCOM_FILE_DNLD_F7_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd = 0;
#endif

  dnldShared->dnldCurrentState = HcomStm32F7DnldStateStarting;

  // Prep for download
  _stateErrShown = false;

#if HCOM_FILE_DNLD_F7_DEBUG_TIMING > 0
  _dbgReceptionBeganAt = hcom_utils_get_current_time64_ns();
#endif

  // Save checksum & name length
  dnldShared->dnldTotalFileSize = fileMsg->fileInfo.fileSize;
  dnldShared->dnldInitFileCrc = fileMsg->fileInfo.fileCheckSum;

  // Log some diagnostic information
  hcom_logging_syslog(LOG_INFO, "%s@%d-Meadow download begin (FileLen:%d, Crc:0x%08x, Name:%s)\n",
          thisFile, __LINE__, dnldShared->dnldTotalFileSize,
          dnldShared->dnldInitFileCrc, dnldShared->dnldOrigPathName);

#if (HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS == 0)
  // We need a file to store the downloaded data

  //----------------------------------------------------------------------
  // Delete was added here to address Meadow Issue #855. Also, O_TRUNC was
  // removed from the open call. After this change, even after several hours
  // of continuous download testing, no assertion was seen. Did this fix
  // the problem completely? Don't know. It at least reduced it's occurrence.
  // Plus, it seems like a cleaner way to handle an existing file, delete
  // then recreate. Also, since LittleFS is designed to not allow a partially 
  // written files, this change will save LFS some work.
  int ret = hcom_file_misc_delete_existing(dnldShared, true);
  if (ret < 0)
  {
    // Because the call passed true, the ENOENT error will return OK. Meaning
    // that the file doesn't exist.
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return ret;
  }

  // Create the file in F7 file system. With Issue #855 the above delete was
  // added meaning that this call will always create a new file.
  ret = hcom_file_write_open_active_file(dnldShared);
  if (ret < 0)
  {
    char *hostMsg;

    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
        thisFile, __LINE__);
      dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
      return -ENOMEM;
    }

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
      snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH, "Unexpected error:%d", ret);
      errorCause = hostMsg;
      break;
    }

    // Notify CLI that something when wrong with opening the file
    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
          "File '%s' Init for download failed because %s",
          dnldShared->dnldOrigPathName, errorCause);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL,
          0, hostMsg, thisFile, __LINE__);

    free(hostMsg);

    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return ret;    // File open failed return
  }
#endif

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)

  if (dnldShared->dnldTotalFileSize > HCOM_FILE_DNLD_MAX_CACHE_FILE_SIZE)
  {
      hcom_logging_syslog(LOG_ERR, "%s@%d-%d bytes is too large for cache\n",
                        thisFile, __LINE__, dnldShared->dnldTotalFileSize);
      dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
      return -EFBIG;    // File too large
  }

  // Allocate memory for cache
  _dnldCacheMemory = (void *) malloc(dnldShared->dnldTotalFileSize);
  if(_dnldCacheMemory == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return -ENOMEM;
  }
#endif

  // Set current action
  dnldShared->dnldCurrentState = HcomStm32F7DnldStateFileXfer;

  // Notify CLI that it's okay to send the file's data now
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY,
            0, "", thisFile, __LINE__);
  return OK;
}

//============================================================================
// Process a file data packet
int hcom_file_dnld_stm32f7_recvd_file_data(const HcomProtoDataMsg_t *hcomDataMsg,
          const size_t packetSize, hcom_dnld_shared_t *dnldShared)
{
  char* hostMsg = NULL;

  // Ignore download if it's not expected. Either not begin or an error
  if(dnldShared->dnldCurrentState != HcomStm32F7DnldStateFileXfer)
  {
    // Show problem, but only once
    if(! _stateErrShown)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Unexpected data packet received\n",
              thisFile, __LINE__);
      _stateErrShown = true;
    }

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
    hcom_file_dnld_cleanup_cache_memory();
#endif
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return -ENOTRECOVERABLE;      // State not recoverable
  }

#if (HCOM_FILE_DNLD_F7_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd++;
#endif

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  if(seqNumb % 250 == 0)
    hcom_logging_syslog(LOG_DEBUG, "Sequence %d\n", hcomDataMsg->seqNumber);
#endif

  // char seqNumMsg[16];
  // snprintf_chk(seqNumMsg, 16, "Sequence:%u\r", hcomDataMsg->seqNumber);
  // hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
  //           0, seqNumMsg, thisFile, __LINE__);

  // Compare _xferRecvFullFileSize with _xferCalcFullFileSize and send a message to host
  int percentDone = (dnldShared->dnldRecvdFileSize  * 100) / dnldShared->dnldTotalFileSize;
  if(percentDone / 10 != dnldShared->dnldPercentSent)
  {
    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
      hcom_file_dnld_cleanup_cache_memory();
#endif
      dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
      return -ENOMEM;
    }

    // 10, 20 etc
    dnldShared->dnldPercentSent = percentDone / 10;

    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
              "File %d%% downloaded", percentDone);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
              0, hostMsg, thisFile, __LINE__);

    // syslog(1, "%s\n", hostMsg);

    free(hostMsg);
  }

  size_t binDataLen = packetSize - HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF;
  if(binDataLen < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-binDataLen was out of range:%ld\n",
             thisFile, __LINE__, binDataLen);
#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
      hcom_file_dnld_cleanup_cache_memory();
#endif
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return -EFAULT;   // Bad address
  }

  // As received, calculate CRC checksum (without sequence number)
  dnldShared->dnldCalcFileCrc = crc32part(hcomDataMsg->binData, binDataLen,
            dnldShared->dnldCalcFileCrc);

#if defined (CONFIG_DIR_MGMT_TESTS)
  #if (HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0)
  syslog(2, "------- %s@%d (Showing first 16 of %lu packet) ------\n", __FILE__, __LINE__, packetSize);
  hcom_diag_print_buffer((uint8_t *)hcomDataMsg, 16, 1);
  #endif
#endif

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)

  if (_dnldCacheOffset + binDataLen > dnldShared->dnldTotalFileSize)
  {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Cache overflow detected\n", thisFile, __LINE__);
      hcom_file_dnld_cleanup_cache_memory();
      dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
      return -EOVERFLOW;
  }
  // Copy file data to cache memory
  memcpy((_dnldCacheMemory + _dnldCacheOffset), hcomDataMsg->binData, binDataLen);
  _dnldCacheOffset += binDataLen;

 #if (HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS == 1)
  // Since only writing to cache must manage file size before exiting
  dnldShared->dnldRecvdFileSize += binDataLen;
  return OK;
 #endif

#else //  (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
  // Write this file fragment to the file system
  int ret = hcom_file_write_to_active_file(dnldShared, hcomDataMsg->binData,
            binDataLen);
  if (ret < 0)
  {
    // Error
    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
      return -ENOMEM;
    }
    uint16_t seqNumb = hcomDataMsg->seqNumber;

    hcom_logging_syslog(LOG_ERR, "%s@%d-Write of %s failed:%d seq:%d\n",
             thisFile, __LINE__, dnldShared->dnldOrigPathName, ret, seqNumb);

    // Notify host
    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
              "Write of '%s', seq %d failed", dnldShared->dnldOrigPathName, seqNumb);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    free(hostMsg);
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return ret;
  }
#endif//  (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)

  dnldShared->dnldRecvdFileSize += binDataLen;

  // Ready for next download packet
  return OK;
}

//=======================================================================================
// Process the end of transfer message from CLI.
int hcom_file_dnld_stm32f7_file_end(hcom_dnld_shared_t *dnldShared)
{
  int ret;
  char* hostMsg = NULL;
  char *msgToSend;
  uint16_t requestType;

  // hcom_logging_syslog(LOG_NOTICE, "EOF received from CLI\n");

  if(dnldShared->dnldCurrentState != HcomStm32F7DnldStateFileXfer)
  {
    hcom_logging_syslog(LOG_WARNING, "%s@%d-Dnld end, unexpected state:%d\n",
              thisFile, __LINE__, dnldShared->dnldCurrentState);
#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
    hcom_file_dnld_cleanup_cache_memory();
#endif
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return -ENOTRECOVERABLE; // State not recoverable
  }

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1 &&\
     HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS == 0)
  // If we have cached the entire file. Write it to the file system in a
  // single operation.
  ret = hcom_file_write_to_active_file(dnldShared, _dnldCacheMemory,
            dnldShared->dnldTotalFileSize);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s write failed:%d\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName, ret);
    hcom_file_dnld_cleanup_cache_memory();
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return ret;
  }
#endif

  // Calculate the CRC checksum
  uint32_t actualFileCrc;
  int detectError = OK; // Required by crc file function

#if (HCOM_FILE_DNLD_CACHE_NO_FILE_ACCESS == 1)
  // Calculate CRC32 from cache since no file to check
  actualFileCrc = crc32(_dnldCacheMemory, dnldShared->dnldTotalFileSize);
#else
  // Close the file if there is one open
  ret = hcom_file_write_close_active_file(dnldShared);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s close failed:%d\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName, ret);
    // Finish cleaning up even if error
  }
  // Calculate the CRC32
  off_t fileSize;       // Required by function call but not used
  uint32_t blockSizeKB; // Required by function call but not used

  // Note this call will open the file.
  actualFileCrc = hcom_file_misc_calc_crc_for_file(dnldShared->dnldFullPathName,
                &fileSize, &blockSizeKB, &detectError);
  // Finished with cache memory
#endif

#if (HCOM_FILE_DNLD_CREATE_MEMORY_CACHE == 1)
    hcom_file_dnld_cleanup_cache_memory();
#endif

  // Report results to host
  hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
  if(hostMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;
    return -ENOMEM;
  }

  if(detectError < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error in Checksum calculation err:%d\n",
              thisFile, __LINE__, detectError);

    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
            "Download of '%s' state unknown due to checksum calculation fault:%d",
            dnldShared->dnldOrigPathName, detectError);
    msgToSend = hostMsg;
    requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
  }
  else
  {
    // Compare results and report to host
    if (dnldShared->dnldCalcFileCrc == dnldShared->dnldInitFileCrc &&
              dnldShared->dnldCalcFileCrc == actualFileCrc &&
              dnldShared->dnldRecvdFileSize == dnldShared->dnldTotalFileSize)
    {
      snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
          "Download of '%s' succeeded (checksum:0x%08X)",
          dnldShared->dnldOrigPathName, dnldShared->dnldCalcFileCrc);

      msgToSend = hostMsg;
      requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    }
    else
    {
      if (dnldShared->dnldCalcFileCrc != dnldShared->dnldInitFileCrc ||
          dnldShared->dnldCalcFileCrc != actualFileCrc)
      {
        snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
                "Download of '%s' failed due to checksum mismatch, f/s read:0x%08X, dnld calc:0x%08X, sender:0x%08X",
                dnldShared->dnldOrigPathName, actualFileCrc,
                dnldShared->dnldCalcFileCrc, dnldShared->dnldInitFileCrc);
        msgToSend = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
      else
      {
        snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
                "Download of '%s' failed due to file size mismatch calculated:%d, sender:%d",
                dnldShared->dnldOrigPathName, dnldShared->dnldRecvdFileSize,
                dnldShared->dnldTotalFileSize);
        msgToSend = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
    }
  }

  if(requestType == HCOM_HOST_REQUEST_TEXT_ERROR)
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, msgToSend);

  // Send text message to host
  hcom_host_send_simple_string_msg(requestType, 0, msgToSend, thisFile, __LINE__);
  free(hostMsg);

#if HCOM_FILE_DNLD_F7_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64_ns();
  hcom_logging_syslog(LOG_INFO, "%s@%d-File transfer %d packets, took %llu mSec, CalcFileCRC:0x%08x\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           dnldShared->dnldCalcFileCrc);
#endif
  dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  return OK;
}
