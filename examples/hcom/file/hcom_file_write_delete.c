/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_write_delete.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
// This file contains a number of functions used to work with the Nuttx file
// system.
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

static bool _shutting_down;

static int _fileDescriptor;
static uint32_t _activePartitionId;
static char *_hcomActiveFileName;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int hcom_file_write_del_remove_file_by_name(const uint32_t partitionId, const char *mountPoint, const char *fileName);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
int hcom_file_write_del_setup()
{
  _shutting_down = false;

  _fileDescriptor = -1;
  _hcomActiveFileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  _activePartitionId = HCOM_INVALID_PARTITION_ID_VALUE;

  _hcomActiveFileName[0] = '\0';

  return OK;
}

//=======================================================================
// Called before hcom mgr closes _hcom_communications_fd which, forces a receive
// error which, causes the thread to return.
void hcom_file_write_del_shutdown()
{
  _shutting_down = true;

  if (_fileDescriptor != -1)
    hcom_file_write_del_close_active_file();
    
  free(_hcomActiveFileName);
}

//==================================================================
// This is the activefile is the file currently being downloaded
int hcom_file_write_del_open_active_file(const uint32_t partitionId,
          const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;

  if (_shutting_down)
    return OK;

  if (_hcomActiveFileName[0] != '\0')
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File '%s' in use\n",
             thisFile, __LINE__, _hcomActiveFileName);
    return -EMFILE; // File already open
  }

  DEBUGASSERT(_activePartitionId == HCOM_INVALID_PARTITION_ID_VALUE);

#ifdef CONFIG_MTD_PARTITION
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf(_hcomActiveFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, fileName);
#else
  DEBUGASSERT(partitionId == 0);
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf(_hcomActiveFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                                mountPoint, fileName);
#endif

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size. Otherwise, the string may be truncated.
  if (filePathAndNameLen < 0 || filePathAndNameLen >= HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH - 1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Open '%s', buffer too small (%d), need %d\n",
             thisFile, __LINE__, _hcomActiveFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, filePathAndNameLen);

    _hcomActiveFileName[0] = '\0';
    return -ENAMETOOLONG; // File name too long
  }

  if (!hcom_via_nx_is_mounted(partitionId))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-F/S not mounted %s\n",
             thisFile, __LINE__, _hcomActiveFileName);
    _hcomActiveFileName[0] = '\0';
    return -ENOENT; // No such file or directory
  }

  if (_fileDescriptor != -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-File Descriptor in use, by '%s'\n",
             thisFile, __LINE__, _hcomActiveFileName);
    _hcomActiveFileName[0] = '\0';
    return -EMFILE; // Too many files open
  }

  // Second (flags) parameter O_RDONLY, O_WRONLY, or O_RDWR ||
  // Third parameter 644 = owner has read and write permission, group has read and others have read
  // 777 everyone has read write and execute permission
  set_errno(0);
  _fileDescriptor = open(_hcomActiveFileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (_fileDescriptor == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno:%d\n",
              thisFile, __LINE__, _hcomActiveFileName, get_errno());
    _hcomActiveFileName[0] = '\0';
    return _fileDescriptor;
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Opened '%s'\n", thisFile, __LINE__, _hcomActiveFileName);

  _activePartitionId = partitionId;
  return OK;
}

//==================================================================
// When data to be added to a file is received it arrives here.
int hcom_file_write_del_add_to_active_file(const uint8_t *fileWriteData, const size_t fileWriteSize)
{
  if (_shutting_down)
    return OK;

  if (!hcom_via_nx_is_mounted(_activePartitionId))
    return -ENOENT; // No such file or directory

  if (_fileDescriptor < 0)
    return -EBADF; // Bad file number

  ssize_t nbytes = 0;
  nbytes = write(_fileDescriptor, fileWriteData, fileWriteSize);
  if (nbytes < 0)
  {
    int Errno = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-failed to write %s, errno %d\n",
             thisFile, __LINE__, _hcomActiveFileName, Errno);
    return nbytes;
  }

  if (nbytes < fileWriteSize)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-'%s' wrote %d of %d bytes\n",
             thisFile, __LINE__, _hcomActiveFileName, nbytes, fileWriteSize);
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Wrote %d bytes to %s\n", 
            thisFile, __LINE__, nbytes, _hcomActiveFileName);
  return OK;
}

//==================================================================
// When downloading to a file and the end of file message is received
// this function is called to close the file and clean up.
int hcom_file_write_del_close_active_file()
{
  if (!hcom_via_nx_is_mounted(_activePartitionId))
    return -ENOENT; // No such file or directory

  if (_fileDescriptor < 0) // Okay to close file > once?
    return -EBADF;         // Bad file number

  int ret = close(_fileDescriptor);
  if (ret < 0)
  {
    int Errno = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-Close of %s, errno %d\n",
             thisFile, __LINE__, _hcomActiveFileName, Errno);
    return ret;
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Closed %s\n", thisFile, __LINE__, _hcomActiveFileName);

  _fileDescriptor = -1;
  _hcomActiveFileName[0] = '\0';
  _activePartitionId = HCOM_INVALID_PARTITION_ID_VALUE;

  return OK;
}

//=====================================================================
// When a request to delete a file by name arrives it first is processed
// in this function
void hcom_file_write_del_remove_file_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
    uint32_t partitionId)
{
  int ret;
  char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);

  size_t fileNameLength = recvPacketDataSize - HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET;
  char *fileNameBuffer = malloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET, fileNameLength);

  ret = hcom_file_write_del_remove_file_by_name(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error from call to delete:%d\n",
        thisFile, __LINE__, ret);
    hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, thisFile, __LINE__);
  }

  // Send text message to host
  int stringLen = snprintf(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
        "Meadow successfully deleted '%s'", fileNameBuffer);

  DEBUGASSERT(stringLen < HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  free(fileNameBuffer);
  free(hostMsg);
}

//=====================================================================
// Remove the file specified
int hcom_file_write_del_remove_file_by_name(const uint32_t partitionId, const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;
  char *fullPathAndFileName = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);

  DEBUGASSERT(_activePartitionId == HCOM_INVALID_PARTITION_ID_VALUE);
  if (_hcomActiveFileName[0] != '\0')
  {
    // Check if the file to remove is the active file
    if(strcmp(fileName, _hcomActiveFileName) == 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Cannot delete '%s', in use\n",
              thisFile, __LINE__, fileName);
      free(fullPathAndFileName);
      return -EMFILE;    // Too many files open (1 is too many)
    }
  }

#ifdef CONFIG_MTD_PARTITION
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf(fullPathAndFileName, HCOM_MAX_HOST_STRING_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, fileName);
#else
  filePathAndNameLen = snprintf(fullPathAndFileName, HCOM_MAX_HOST_STRING_BUFF_LENGTH, "%s/%s",
                                mountPoint, fileName);
#endif

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size. Otherwise, the string may be truncated.
  if (filePathAndNameLen < 0 || filePathAndNameLen >= HCOM_MAX_HOST_STRING_BUFF_LENGTH - 1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Delete '%s' (truncated) but file name too long.\n",
             thisFile, __LINE__, fullPathAndFileName);
    free(fullPathAndFileName);
    return -ENAMETOOLONG; // File name too long
  }

  int ret = unlink(fullPathAndFileName);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-unlink %s, errno %d\n",
             thisFile, __LINE__, fullPathAndFileName, get_errno());
    free(fullPathAndFileName);
    return ret;
  }

  hcom_logging_syslog(LOG_DEBUG, "Deleted '%s'\n", fileName);
  free(fullPathAndFileName);
  return OK;
}
