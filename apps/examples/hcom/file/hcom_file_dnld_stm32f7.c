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
#include <sys/mount.h>
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

#define HCOM_RECV_DEBUG_TIMING 0          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _stateErrShown;

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
static int _dbgNumbPacketsRecvd = 0;        // Only used in LOG_INFO & LOG_DEBUG messages
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
int hcom_file_dnld_stm32f7_setup()
{
    _stateErrShown = false;   // In case of data before begin
  return OK;
}

//==========================================================================
// Beginning of a file download into the flash file system.
// Called from hcom_host_route.c. The incomplete file name has been supplied.
int hcom_file_dnld_stm32f7_file_begin(const HcomProtoHdrMsg_t *hdrMsg,
          hcom_dnld_shared_t *dnldShared)
{
  int ret = OK;

  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd = 0;
#endif

  dnldShared->dnldCurrentState = HcomStm32F7DnldStateStarting;

  // Prep for download
  _stateErrShown = false;

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionBeganAt = hcom_utils_get_current_time64_ns();
#endif

  // Save checksum & name length
  dnldShared->dnldInitFileSize = fileMsg->fileInfo.fileSize;
  dnldShared->dnldInitFileCrc = fileMsg->fileInfo.fileCheckSum;

  // Log some diagnostic information
  hcom_logging_syslog(LOG_INFO, "%s@%d-Meadow downloading file (FileLen:%d, Crc:0x%08x, Name:%s)\n",
          thisFile, __LINE__, dnldShared->dnldInitFileSize,
          dnldShared->dnldInitFileCrc, dnldShared->dnldOrigPathName);

  // Some file types (e.g. SD-Card) must be mounted before begin accessed
  if(dnldShared->dnldRqstCat == pathnameFullMmcsd)
  {
    // mount(source, target, fstype, mountflags, data)
    // e.g. mount("/dev/mmcsd0", "/sdcard", "vfat", 0, NULL);
    ret = mount(MEADOW_SDCARD_BLOCK_NAME, MEADOW_SDCARD_MOUNT_POINT_NAME,
              MEADOW_SDCARD_FILE_SYS_TYPE, 0, NULL);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ERROR: Mount failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
      return ret;
    }
#if defined (CONFIG_DIR_MGMT_TESTS)
    syslog(2, "Mount successful\n");
#endif
  }

  // Open the file in F7 file system
  ret = hcom_file_write_open_active_file(dnldShared);
  if (ret < 0)
  {
    char *hostMsg;

    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
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
      snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH, "Unexpected error:%d");
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
    return ret;
  }
  else
  {
    // Set current action
    dnldShared->dnldCurrentState = HcomStm32F7DnldStateFileXfer;

    // Notify CLI that it's okay to send the file's data now
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_OKAY,
              0, thisFile, __LINE__);
    ret = OK;
  }

  return ret;
}

//============================================================================
// Process a data packet
int hcom_file_dnld_stm32f7_recvd_file_data(const HcomProtoDataMsg_t *hcomDataMsg,
          const size_t packetSize, hcom_dnld_shared_t *dnldShared)
{
  int ret;
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

    return -ENOTRECOVERABLE;      // State not recoverable
  }

#if (HCOM_RECV_DEBUG_TIMING) > 0 || (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  _dbgNumbPacketsRecvd++;
#endif

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  if(seqNumb % 250 == 0)
    hcom_logging_syslog(LOG_DEBUG, "Sequence %d\n", hcomDataMsg->seqNumber);
#endif

  // Compare _xferRecvFullFileSize with _xferCalcFullFileSize and send a message to host
  int percentDone = (dnldShared->dnldCalcFileSize  * 100) / dnldShared->dnldInitFileSize;
  if(percentDone / 10 != dnldShared->dnldPercentSent)
  {
    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    // 10, 20 etc
    dnldShared->dnldPercentSent = percentDone / 10;

    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
              "File %d%% downloaded", percentDone);

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
              0, hostMsg, thisFile, __LINE__);
    free(hostMsg);
  }

  size_t binDataLen = packetSize - (HCOM_PROTOCOL_DATA_MSG_DATA_INFO_OFF);

  // Calculate CRC checksum of the payload without sequence number
  dnldShared->dnldCalcFileCrc = crc32part(hcomDataMsg->binData, binDataLen,
            dnldShared->dnldCalcFileCrc);

#if defined (CONFIG_DIR_MGMT_TESTS)
  syslog(1, "------- %s@%d (Showing 16 of %lu packet) ------\n", __FILE__, __LINE__, packetSize);
  hcom_diag_print_buffer(hcomDataMsg, 16, 1);
#endif

  // Write the data to the file system
  ret = hcom_file_write_to_active_file(dnldShared, hcomDataMsg->binData,
            binDataLen);
  if (ret < 0)
  {
    // Error
    hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    uint32_t seqNumb = hcomDataMsg->seqNumber;

    hcom_logging_syslog(LOG_ERR, "%s@%d-Write of %s failed:%d seq:%d\n",
             thisFile, __LINE__, dnldShared->dnldOrigPathName, ret, seqNumb);

    // Notify host
    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
              "Write of '%s', seq %d failed", dnldShared->dnldOrigPathName, seqNumb);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    free(hostMsg);
    return ret;
  }

  dnldShared->dnldCalcFileSize += binDataLen;
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

  hcom_logging_syslog(LOG_NOTICE, "End of file write received\n");

  if(dnldShared->dnldCurrentState != HcomStm32F7DnldStateFileXfer)
  {
    hcom_logging_syslog(LOG_WARNING, "%s@%d-Dnld end, unexpected state:%d\n",
              thisFile, __LINE__, dnldShared->dnldCurrentState);
    // Continue even with error
  }

  ret = hcom_file_write_close_active_file(dnldShared);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File %s close failed:%d\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName, ret);
    // Continue even with error
  }

  off_t fileSize;       // Required by function call but not used
  uint32_t blockSizeKB; // Required by function call but not used
  int detectError = OK;

  uint32_t actualFileCrc = hcom_file_misc_calc_crc_for_file(dnldShared->dnldFullPathName,
                &fileSize, &blockSizeKB, &detectError);

  // Some file types (e.g. SD-Card) must be unmounted too
  if(dnldShared->dnldRqstCat == pathnameFullMmcsd)
  {
    ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
      return ret;
    }
    
#if defined (CONFIG_DIR_MGMT_TESTS)
    syslog(2, "umount successful\n");
#endif
  }

  // Report to host
  hostMsg = malloc(HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH);
  if(hostMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  if(detectError < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error in Checksum calculation err:%d\n",
              thisFile, __LINE__, detectError);

    snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
            "Download of '%s' state unknown due to checksum calulation fault:%d",
            dnldShared->dnldOrigPathName, detectError);
    msgToSend = hostMsg;
    requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
  }
  else
  {
    // Compare results and report to host
    if (dnldShared->dnldCalcFileCrc == dnldShared->dnldInitFileCrc &&
              dnldShared->dnldCalcFileCrc == actualFileCrc &&
              dnldShared->dnldCalcFileSize == dnldShared->dnldInitFileSize)
    {
      snprintf_chk(hostMsg, HCOM_MED_LONG_HOST_STRING_BUFF_LENGTH,
          "Download of '%s' success (checksums calculated:0x%08X, expected:0x%08X)",
          dnldShared->dnldOrigPathName, dnldShared->dnldCalcFileCrc,
          dnldShared->dnldInitFileCrc);

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
                dnldShared->dnldOrigPathName, dnldShared->dnldCalcFileSize,
                dnldShared->dnldInitFileSize);
        msgToSend = hostMsg;
        requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
      }
    }
  }

  if(requestType == HCOM_HOST_REQUEST_TEXT_ERROR)
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, msgToSend);

  // Send text message to host
  hcom_host_send_simple_string_msg(requestType, 0, msgToSend, thisFile, __LINE__);

  if(hostMsg != NULL)
    free(hostMsg);

#if HCOM_RECV_DEBUG_TIMING > 0
  _dbgReceptionEndedAt = hcom_utils_get_current_time64_ns();
  hcom_logging_syslog(LOG_INFO, "%s@%d-File transfer %d packets, took %llu mSec, CalcFileCRC:0x%08x\n",
           thisFile, __LINE__, _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           dnldShared->dnldCalcFileCrc);
#endif
  dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  return OK;
}
