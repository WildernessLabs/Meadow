/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_delete_stm32f7.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/dirent.h>

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int hcom_file_delete_file_by_name(const uint32_t partitionId,
          const char *mountPoint, const char *fileName);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/

//=====================================================================
// When a request to delete a file by name arrives it first is processed
// in this function to get it's file system name.
void hcom_file_delete_stm32f7_file_begin(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize, uint32_t partitionId)
{
  int ret;
  uint16_t hostMsgType;
  HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;

  uint32_t fileNameLength = packetSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;

  // For delete, only the file name field is populated, no other fields
  char *deleteFileName = malloc(fileNameLength + 1);
  if(deleteFileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }

  memcpy(deleteFileName, fileMsg->fileInfo.fileName, fileNameLength);
  deleteFileName[fileNameLength] = '\0';

  // Memory for text message to host
  char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  if(hostMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return;
  }

  ret = hcom_file_delete_file_by_name(partitionId,
            HCOM_FILE_MOUNT_POINT_TARGET, deleteFileName);
  if (ret < 0)
  {
    char *errorCause;
    switch(ret)
    {
      case -EEXIST: // File already open
      errorCause = "Another file is being processed delete file";
      break;
      
      case -ENAMETOOLONG: // File name too long
      errorCause = "File name too long";
      break;
      
      case -ENOENT: // No such file or directory
      errorCause = "No such file";
      break;
      
      case -EMFILE: // Too many files open
      errorCause = "Too many files open";
      break;

      default:  // different error
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "Unexpected error:%d", ret);
      errorCause = hostMsg;
      break;
    }
    hostMsgType = HCOM_HOST_REQUEST_TEXT_ERROR;

    hcom_logging_syslog(LOG_ERR, "%s@%d-Error %d (%s) failed to delete:'%s'\n",
        thisFile, __LINE__, ret, errorCause, deleteFileName);

    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
          "Meadow failed to delete '%s' - %s", deleteFileName, errorCause);
  }
  else
  {
    hostMsgType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
          "Meadow successfully deleted '%s'", deleteFileName);
  }

  // Send text message to host
  hcom_host_send_simple_string_msg(hostMsgType, 0, hostMsg, thisFile, __LINE__);

  free(deleteFileName);
  free(hostMsg);
}

//=====================================================================
// Remove the file specified by name
int hcom_file_delete_file_by_name(const uint32_t partitionId,
          const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;
  char *fullPathAndFileName = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);
  if(fullPathAndFileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

#ifdef CONFIG_MTD_PARTITION
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf_chk(fullPathAndFileName, HCOM_MAX_HOST_STRING_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, fileName);
#else
  filePathAndNameLen = snprintf_chk(fullPathAndFileName, HCOM_MAX_HOST_STRING_BUFF_LENGTH, "%s/%s",
                                mountPoint, fileName);
#endif

  // Error? Overflow already handled by snprintf_chk
  if (filePathAndNameLen < 0)
  {
    free(fullPathAndFileName);
    return filePathAndNameLen;    // Return error
  }

  int ret = unlink(fullPathAndFileName);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-unlink %s, errno %d\n",
             thisFile, __LINE__, fullPathAndFileName, get_errno());
    free(fullPathAndFileName);
    return -get_errno();
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "Deleted '%s'\n", fileName);
#endif

  free(fullPathAndFileName);
  return OK;
}
