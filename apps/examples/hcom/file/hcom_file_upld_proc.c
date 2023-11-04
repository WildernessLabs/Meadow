/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_upld_proc.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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
static char *_activeFileName;
static int  _activeFd = -1;
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
void hcom_file_upld_proc_initial_bytes_in_file(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize, uint32_t partitionId)
{
  int ret;
  int fd;
  char *fileNameBuffer;
  char *fileName;
  
  HcomProtoTextMsg_t *textMsg = (HcomProtoTextMsg_t *)hdrMsg;

#ifndef CONFIG_MTD_PARTITION
  partitionId = 0;    // Ignore any other partition value if no partitioning
#endif

  // How long must the file name be
  size_t fileNameLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  // Last field in file info is the file name
  fileNameBuffer = malloc(fileNameLen + 1);
  if(fileNameBuffer == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }
  memset(fileNameBuffer, 0, fileNameLen + 1);
  memcpy(fileNameBuffer, textMsg->textData, fileNameLen);

  // Create the name of the mount point part of the file name
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fullMountPtName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    free(fileNameBuffer);
    return;
  }

#ifdef CONFIG_MTD_PARTITION
  snprintf_chk(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
#else
  strncpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#endif

  fileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
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
// This function receives a command from the HOST to upload a file
// First the host sends the file name. Then this functions sends the file to
// the host. Sending a file to the host requires 3 steps:
// 1. Send a file information message with the file's vitals
// 2. Wait for Host PC to respond that it is ready
// 3. Send a 1-n  data messages that contain the files contents
// 4. Send a file end message so the CLI can close the file and verify it
//=============================================================
void hcom_file_upld_proc_start_file_upload(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize, uint32_t partitionId)
{
  uint32_t crc32Checksum = 0;
  char *fileNameBuffer;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];   // 128 bytes
  HcomProtoTextMsg_t *recvdTextMsg = (HcomProtoTextMsg_t *)hdrMsg;

  _uploadAction = HcomUpldActionNone;

  // In case the next command never comes
  if(_activeFileName != NULL)
  {
    free(_activeFileName);
    _activeFileName = NULL;
  }

#ifndef CONFIG_MTD_PARTITION
  partitionId = 0;    // Ignore any other partition value if no partitioning
#endif

  // The caller has provided a file name, calculate it's length
  size_t fileNameLen = packetSize - HCOM_PROTOCOL_TEXT_MSG_START_OFF;

  fileNameBuffer = malloc(fileNameLen + 1);
  if(fileNameBuffer == NULL)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, hostMsg, thisFile, __LINE__);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
    return;
  }

  memset(fileNameBuffer, 0, fileNameLen + 1);
  memcpy(fileNameBuffer, recvdTextMsg->textData, fileNameLen);

  // Create the name of the mount point part of the file name
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fullMountPtName == NULL)
  {
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
    free(fileNameBuffer);
    return;
  }

#ifdef CONFIG_MTD_PARTITION
  snprintf_chk(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
#else
  strncpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#endif

  _activeFileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(_activeFileName == NULL)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    hcom_logging_syslog(LOG_ERR, hostMsg, thisFile, __LINE__);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
              
    free(fileNameBuffer);
    free(fullMountPtName);
    return;
  }

  // Finally we can create the complete file and path name
  snprintf_chk(_activeFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", 
                fullMountPtName, fileNameBuffer);
  // No longer needed
  free(fileNameBuffer);
  free(fullMountPtName);

  // ---------------------------------------------------------------
  // Open the file to upload
  set_errno(0);
  _activeFd = open(_activeFileName, O_RDONLY);
  if (_activeFd == -1)
  {
    if(errno == ENOENT)
    {
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
            "File '%s' could not be found in Meadow file system.",
            _activeFileName);
    }
    else
    {
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
            "File '%s' could not be opened, error:%d", _activeFileName, errno);
    }
    hcom_logging_syslog(LOG_ERR, "%s@%d-opening '%s', errno:%d\n",
                thisFile, __LINE__, _activeFileName, errno);

    // This message will notify the user
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR,
              0, hostMsg, thisFile, __LINE__);
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
    free(_activeFileName);
    _activeFileName = NULL;
    return;
  }

  int detectError;
  uint32_t blockSizeKB;   // Required for call
  off_t fileSize;

  // Calculate the CRC checksum
  crc32Checksum = hcom_file_misc_calc_crc_for_file_fd(_activeFd, _activeFileName,
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

    free(_activeFileName);
    _activeFileName = NULL;
    close(_activeFd);
    _activeFd = -1;
    return;
  }

  // Report to the host success and wait for it to respond
  size_t totalMsgLength;
  HcomProtoFileMsg_t *fileMsg;
  
  fileMsg = (HcomProtoFileMsg_t *)malloc(g_current_hcom_maximum_packet_size);
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

    free(_activeFileName);
    _activeFileName = NULL;
    close(_activeFd);
    _activeFd = -1;
    return;
  }

  fileMsg->fileInfo.fileSize = fileSize;
  fileMsg->fileInfo.fileCheckSum = crc32Checksum;
  fileMsg->stdHeader.userData = 0;
  fileMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_INIT_UPLOAD_OKAY;

  // Copy the file name to the end of the structure
  size_t activeFileNameLen = strlen(_activeFileName) - 1;
  memcpy(fileMsg->fileInfo.fileName, _activeFileName, activeFileNameLen);
  totalMsgLength = activeFileNameLen + HCOM_PROTOCOL_FILE_MSG_LENGTH;

  // syslog(2, "AP---> File CRC is:0x%08x, length:%d. Sending 'Init upload OK' to HOST\n",
  //           crc32Checksum, fileSize);
  
  // This message contains what the host needs to start receiving a file
  hcom_host_send_std_msg_data((HcomProtoHdrMsg_t *)fileMsg,
            totalMsgLength, thisFile, __LINE__);

  free(fileMsg);
  _uploadAction = HcomUpldActionInitialized;

  // Note: _activeFileName not freed yet
  return;
}

//==================================================================
// This function receives a command from the HOST to upload file data
// This function is called after the CLI has a chance to process an above
// success. This will upload all the files data and send the end message.
//==================================================================
void hcom_file_upld_proc_begin_file_uploading(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize, uint32_t partitionId)
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
    
    // This message will stop upload
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_UPLOAD_FAIL, 0,
              thisFile, __LINE__);
  }
  _uploadAction = HcomUpldActionUploading;

  // Use the information from start initialize and begin uploading
  ret = hcom_file_upld_proc_build_upload_packet(_activeFd, _activeFileName);
  if(ret < 0)
  {
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "File '%s' data upload failed", _activeFileName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
  }

  // Finished with file upload
  _uploadAction = HcomUpldActionNone;
  free(_activeFileName);
  _activeFileName = NULL;
  close(_activeFd);
  _activeFd = -1;
}

//==================================================================
// This function builds and uploads the files contents
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
  binMsg = (HcomProtoBinMsg_t *)malloc(g_current_hcom_maximum_packet_size);
  if(binMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
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

      // This call will build the standard message and send it to the host
      // Length must include header + data
      hcom_host_send_std_msg_data((HcomProtoHdrMsg_t *)binMsg,
                HCOM_PROTOCOL_HEADER_MSG_LENGTH + nbytes,
                thisFile, __LINE__);
      totalSent += nbytes;
    }
  } while (nbytes > 0);

  // syslog(2, "AP--->Data upload complete. Sent %d Msgs:, bytes:%d\n",
            // sentCount, totalSent);
  free(binMsg);

  // ---------------------------------------------------------------
  // Send the end message
  uint8_t msgBuf[HCOM_PROTOCOL_HEADER_MSG_LENGTH];
  HcomProtoHdrMsg_t *endHdrMsg = (HcomProtoHdrMsg_t *)msgBuf;
  endHdrMsg->stdHeader.rqstType = HCOM_HOST_REQUEST_UPLOAD_FILE_COMPLETED;
  endHdrMsg->stdHeader.userData = 0;

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
