/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_upld_proc.c
 * 
 *   Copyright (C) 2021-2023 Wilderness Labs. All rights reserved.
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

// The functions in the file setup to upload a file or part of a file to the
// host PC.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <nuttx/arch.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) hcom_file_upld_proc.c"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
enum hcom_upload_data_packet_action
{
  HcomUpldActionNone = 0,
  HcomUpldActionInitialized = 1,
  HcomUpldActionUploading = 2
};

static char *thisFile = __FILE__;
static int _uploadAction;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_file_upld_proc_build_upload_packet(int fd, char *fileName);
/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_upld_proc_setup()
{
  return OK;
}

//==========================================================================
// This function receives a command from CLI and builds a single message to
// send back to the CLI. This message contains the first part of a file's
// data. However, the maximum number of bytes is fixed by
// HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN, which is defined in hcom_protocol.h.
// This file may no longer be necessary
//==========================================================================

// (--) I'm pretty sure, this function is never used
void hcom_file_upld_proc_initial_bytes_in_file(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize)
{
  int ret;
  int fd;
  char *fileNameBuffer;
  char *fileName;
  
  HcomProtoTextMsg_t *textMsg = (HcomProtoTextMsg_t *)hdrMsg;

  // How long must the file name be
  size_t fileNameLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  // Last field in file info is the file name
  fileNameBuffer = malloc(fileNameLen + 1);
  if(fileNameBuffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
              thisFile, __LINE__);
    return;
  }
  memset(fileNameBuffer, 0, fileNameLen + 1);
  memcpy(fileNameBuffer, textMsg->textData, fileNameLen);

  // Create the name of the mount point part of the file name
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fullMountPtName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", 
              thisFile, __LINE__);
    free(fileNameBuffer);
    return;
  }

  strncpy(fullMountPtName, HCOM_MEADOW0_PATH_NAME_PREFIX,
            HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

  fileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
              thisFile, __LINE__);
    free(fileNameBuffer);
    free(fullMountPtName);
    return;
  }
  
  snprintf_chk(fileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                fullMountPtName, fileNameBuffer);
  free(fileNameBuffer);
  free(fullMountPtName);

  // Open the file
  fd = open(fileName, O_RDONLY);
  if (fd == -1)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "File '%s' cannot be opened", fileName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno:%d\n",
                thisFile, __LINE__, fileName, errno);                
    free(fileName);
    return;
  }

  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "The file '%s' encountered a lseek error", fileName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    hcom_logging_syslog(LOG_ERR, "%s@%d-lseek failed %s, errno:%d\n",
              thisFile, __LINE__, fileName, errno);
    free(fileName);
    return;
  }

  // Read first data bytes
  uint8_t *returnBinData = malloc(HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
  if(returnBinData == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    free(fileName);
    return;
  }
  
  ssize_t nbytes;
  int bufOff = 0;
  do
  {
    nbytes = read(fd, returnBinData + bufOff, HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN - bufOff);
    if (nbytes < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-read %s, errno:%d\n",
                thisFile, __LINE__, fileName, errno); usleep(20 * 1000);
      free(fileName);
      free(returnBinData);
      return;
    }
    bufOff += nbytes;
  } while (nbytes > 0);

  ret = close(fd);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-close %s, errno:%d\n",
             thisFile, __LINE__, fileName, errno);
    free(fileName);
    free(returnBinData);
    return;
  }

  // Send the command to host
  hcom_host_send_binary_data_msg(HCOM_HOST_REQUEST_SEND_INITIAL_FILE_BYTES,
            0, returnBinData, bufOff, __FILE__, __LINE__);

  free(fileName);
  free(returnBinData);
}

//=============================================================
// This function receives a command from the HOST to upload a file.
// First the CLI sends the file name. Then this functions sends the file's
// information back to the CLI.
// Sending a file to the host requires 4 steps:
// 1. Send a file information message with the file's vitals (here)
// 2. Wait for CLI on the Host PC to respond that it is ready
// 3. Send a 1-n data messages that contain the files contents
// 4. Send a file end message so the CLI can close the file and verify it
//=============================================================
int hcom_file_upld_proc_start_file_upload(hcom_dnld_shared_t *dnldShared)
{
  int detectError;
  uint32_t blockSizeKB;   // Required for call but not used
  uint32_t crc32Checksum = 0;
  off_t fileSize;
  size_t totalMsgLength;
  size_t activeFileNameLen;  
  HcomProtoFileMsg_t *fileMsg;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];   // 128 bytes

  _uploadAction = HcomUpldActionNone;

  // Open the file to upload
  set_errno(0);

  dnldShared->dnldFileFD = open(dnldShared->dnldFullPathName, O_RDONLY);
  if (dnldShared->dnldFileFD == -1)
  {
    if(errno == ENOENT)
    {
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
            "File '%s' could not be found in Meadow file system.",
            dnldShared->dnldFullPathName);
    }
    else
    {
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
            "File '%s' could not be opened, error:%d",
            dnldShared->dnldFullPathName, errno);
    }

    hcom_logging_syslog(LOG_ERR, "%s@%d-opening '%s', errno:%d\n",
                thisFile, __LINE__, dnldShared->dnldFullPathName, errno);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);

    hcom_dir_mgmt_free_file_info(dnldShared);

    return -errno;
  }

  // Calculate the CRC checksum
  crc32Checksum = hcom_file_misc_calc_crc_for_file_fd(dnldShared->dnldFileFD,
          dnldShared->dnldFullPathName,
          &fileSize, &blockSizeKB, &detectError);
  if(detectError < 0)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "%s@%d-CRC calculation error:%d\n", thisFile, __LINE__, detectError);
    hcom_logging_syslog(LOG_ERR, "%s@%d-CRC calculation error:%d\n",
              thisFile, __LINE__, detectError);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);

    hcom_dir_mgmt_free_file_info(dnldShared);

    return -errno;
  }

  // Report to the host upload success thus far and wait for a response
  activeFileNameLen = strlen(dnldShared->dnldFullPathName) + 1;
  totalMsgLength = activeFileNameLen + HCOM_PROTOCOL_FILE_MSG_LENGTH;

  // syslog(LOG_MDIAG,
  //   "%s@%d-start_file_upload, from FS fileSize:%ld, totalMsgLength:%lu, fileNameLen:%lu\n",
  //   thisFile, __LINE__, fileSize, totalMsgLength, activeFileNameLen); usleep(20 * 1000);
  
  fileMsg = (HcomProtoFileMsg_t *)malloc(totalMsgLength);
  if(fileMsg == NULL)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, hostMsg);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);

    hcom_dir_mgmt_free_file_info(dnldShared);

    return -errno;
  }

  fileMsg->fileInfo.fileSize = fileSize;
  fileMsg->fileInfo.fileCheckSum = crc32Checksum;
  fileMsg->stdHeader.extraData = 0;
  fileMsg->stdHeader.userData = 0;
  fileMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_INIT_UPLOAD_OKAY;

  // Copy the file name to the end of the message structure
  memcpy(fileMsg->fileInfo.fileName, dnldShared->dnldFullPathName,
            activeFileNameLen);

  // syslog(LOG_MDIAG,
  //   "Up---> File CRC is:0x%08x, filesize:%lu, filename:'%s'. Sending 'Init upload OK' to HOST\n",
  //   crc32Checksum, fileSize, fileMsg->fileInfo.fileName);

  // This message contains what the host needs to start receiving a file
  hcom_host_send_std_msg_data((HcomProtoHdrMsg_t *)fileMsg,
            totalMsgLength, thisFile, __LINE__);

  free(fileMsg);
  _uploadAction = HcomUpldActionInitialized;

  return OK;
}

//==================================================================
// This function receives a command from the HOST CLI to upload file data
// This function is called after the CLI has a chance to process an above
// success. This will upload all the files data and send the end message.
//==================================================================
int hcom_file_upld_proc_begin_file_uploading(hcom_dnld_shared_t *dnldShared)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];   // 128 bytes

  if(_uploadAction != HcomUpldActionInitialized)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%s@%d-Must initialize upload before each upload.\n",
              thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, hostMsg, thisFile, __LINE__);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop CLI uploading
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
    return -ENODATA;    // No data available
  }
  _uploadAction = HcomUpldActionUploading;

  // Use the information from start initialize and begin uploading
  ret = hcom_file_upld_proc_build_upload_packet(dnldShared->dnldFileFD,
            dnldShared->dnldFullPathName);
  if(ret < 0)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "File '%s' data upload failed", dnldShared->dnldFullPathName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    _uploadAction = HcomUpldActionNone;

    hcom_dir_mgmt_free_file_info(dnldShared);
    return ret;
  }

  // Finished with file upload, cleanup
  _uploadAction = HcomUpldActionNone;
  hcom_dir_mgmt_free_file_info(dnldShared);

  return OK;
}

//==================================================================
// This function builds and uploads the files contents.
int hcom_file_upld_proc_build_upload_packet(int fd, char *fileName)
{
  uint16_t sequenceNumb = 1;
  HcomProtoBinMsg_t *binMsg;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];   // 128 bytes

  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "The file '%s' encountered a lseek error", fileName);

    hcom_logging_syslog(LOG_ERR, "%s@%d-lseek failed %s, errno:%d\n",
              thisFile, __LINE__, fileName, errno);

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    return -errno;
  }

  // Buffer to hold header + data
  binMsg = (HcomProtoBinMsg_t *)malloc(HCOM_PROTOCOL_CURRENT_PACKET_MAX_SIZE);
  if(binMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  // Send all the file data
  ssize_t nbytes;
  ssize_t totalSent = 0;    // diag
  int sentCount = 0;        // diag

  do
  {
    // Read bin data into the buffer after the header
    nbytes = read(fd, binMsg->binData, HCOM_PROTOCOL_COMMAND_MAX_PAYLOAD_LEN);
    if (nbytes < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-read %s, errno:%d\n",
                thisFile, __LINE__, fileName, errno);
      free(binMsg);
      return -errno;
    }

    binMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_UPLOADING_FILE_DATA;
    if(nbytes > 0)
    {
      // Send the data to the host
      sentCount++;
      binMsg->stdHeader.userData = sequenceNumb++;
      binMsg->stdHeader.extraData = 0;

      // This call will build the standard message and send it to the host
      // Length must include header + data
      hcom_host_send_std_msg_data((HcomProtoHdrMsg_t *)binMsg,
                HCOM_PROTOCOL_HEADER_MSG_LENGTH + nbytes,
                thisFile, __LINE__);
      totalSent += nbytes;
    }
  } while (nbytes > 0);

  // syslog(LOG_MDIAG, "UP--->%s@%d-Data upload complete. Sent %d Msgs:, bytes:%d\n",
  //           thisFile, __LINE__, sentCount, totalSent);
  free(binMsg);

  // ---------------------------------------------------------------
  // Send the end message
  uint8_t msgBuf[HCOM_PROTOCOL_HEADER_MSG_LENGTH];
  HcomProtoHdrMsg_t *endHdrMsg = (HcomProtoHdrMsg_t *)msgBuf;
  endHdrMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_UPLOAD_FILE_COMPLETED;
  endHdrMsg->stdHeader.userData = 0;
  endHdrMsg->stdHeader.extraData = 0;

  // Report to hosts that the entire file has been sent
  hcom_host_send_std_msg_data(endHdrMsg, HCOM_PROTOCOL_HEADER_MSG_LENGTH,
            thisFile, __LINE__);

  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
        "File '%s' uploaded successfully", fileName);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
            0, hostMsg, thisFile, __LINE__);

  return OK;
}

//==========================================================================
// NOT IMPLEMENTED
void hcom_file_upld_proc_abort_file_upload(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize, uint32_t partitionId)
{
  
}
