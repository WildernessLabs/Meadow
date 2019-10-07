/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_file_commands.c
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
// This file contains a number of functions used to work with the Nuttx file
// system.
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nuttx/drivers/ramdisk.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/fat.h>
#include <nuttx/fs/dirent.h>
#include <nuttx/userspace.h>

#define HCOM_INVALID_PARTITION_ID_VALUE 0xffffffff

// Note: This code is VERY SmartFS dependent
/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;

static int _fileDescriptor;
static char *_activePathFileName;
static uint32_t _activePartitionId;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/

int hcom_file_commands_setup()
{
  _shutting_down = false;

  _fileDescriptor = -1;
  _activePathFileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  _activePathFileName[0] = '\0';
  _activePartitionId = HCOM_INVALID_PARTITION_ID_VALUE;

  return OK;
}

//=======================================================================
// Called before hcom mgr closes _hcom_communications_fd which, forces a receive
// error which, causes the thread to return.
void hcom_file_commands_shutdown()
{
  _shutting_down = true;

  if (_fileDescriptor != -1)
    hcom_file_commands_close_active_file();
    
  free(_activePathFileName);
}

//=======================================================================
// returns true if there is an active file
bool hcom_file_commands_is_active_file()
{
  return _activePathFileName[0] != '\0';
}

//==================================================================
int hcom_file_commands_open_active_file(const uint32_t partitionId, const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;

  if (_shutting_down)
    return OK;

  if (_activePathFileName[0] != '\0')
  {
    f7syslog(LOG_ERR, "%s() ERROR: File system in use. The file '%s' is active.\n",
             __func__, _activePathFileName);
    return -EMFILE; // File already open
  }

  DEBUGASSERT(_activePartitionId == HCOM_INVALID_PARTITION_ID_VALUE);

#ifdef CONFIG_FS_SMARTFS
  // Nuttx NAME_MAX is set by CONFIG_NAME_MAX. CONFIG_SMARTFS_MAXNAMLEN is smartfs specific
  if(strlen(fileName) > NAME_MAX || strlen(fileName) > CONFIG_SMARTFS_MAXNAMLEN)
  {
    f7syslog(LOG_ERR, "%s() Error: file name '%s', %d is longer than NAME_MAX (%d) or CONFIG_SMARTFS_MAXNAMLEN (%d)\n",
             __func__, fileName, strlen(fileName), NAME_MAX, CONFIG_SMARTFS_MAXNAMLEN);
    return -ENAMETOOLONG;
  }
#endif

#ifdef CONFIG_MTD_PARTITION
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf(_activePathFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, fileName);
#else
  DEBUGASSERT(partitionId == 0);
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf(_activePathFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                                mountPoint, fileName);
#endif

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size.
  if (filePathAndNameLen < 0 || filePathAndNameLen >= HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH - 1)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Opening (%s) name buffer too small %d, length %d.\n",
             __func__, _activePathFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, filePathAndNameLen);

    _activePathFileName[0] = '\0';
    return -ENAMETOOLONG; // File name too long
  }

  if (!hcom_fs_helper_is_fs_mounted(partitionId))
  {
    f7syslog(LOG_ERR, "%s() Error: file system not mounted %s\n",
             __func__, _activePathFileName);
    _activePathFileName[0] = '\0';
    return -ENOENT; // No such file or directory
  }

  if (_fileDescriptor != -1)
  {
    f7syslog(LOG_ERR, "%s() Error: File Descriptor active. Seems file '%s' is active\n",
             __func__, _activePathFileName);
    _activePathFileName[0] = '\0';
    return -EMFILE; // Too many files open
  }

#ifdef CONFIG_FS_SMARTFS
  if (filePathAndNameLen > PATH_MAX)
  {
    f7syslog(LOG_ERR, "%s() Error: file path and name '%s' (*%d) is longer than PATH_MAX (%d)\n",
             __func__, _activePathFileName, filePathAndNameLen, PATH_MAX);
    _activePathFileName[0] = '\0';
    return -ENAMETOOLONG;
  }
#endif

  // Second (flags) parameter O_RDONLY, O_WRONLY, or O_RDWR ||
  // Third parameter 644 = owner has read and write permission, group has read and others have read
  // 777 everyone has read write and execute permission
  set_errno(0);
  _fileDescriptor = open(_activePathFileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (_fileDescriptor == -1)
  {
    int Errno = get_errno();

#ifdef CONFIG_FS_SMARTFS
    // FYI - #define ENAMETOOLONG 91 #define ENAMETOOLONG_STR "File name too long"
    if (Errno == ENAMETOOLONG)
      f7syslog(LOG_ERR, "%s() Error: failed to open '%s' for writing. File Name too long. Change CONFIG_SMARTFS_MAXNAMLEN.\n",
               __func__, _activePathFileName);
    else
#endif

      f7syslog(LOG_ERR, "%s() Error: failed to open '%s' for writing. errno: %d\n",
               __func__, _activePathFileName, Errno);
    _activePathFileName[0] = '\0';
    return _fileDescriptor;
  }

  f7syslog(LOG_DEBUG, "File System successfully opened %s\n", _activePathFileName);

  _activePartitionId = partitionId;
  return OK;
}

//==================================================================
int hcom_file_commands_write_to_active_file(const uint8_t *fileWriteData, const size_t fileWriteSize)
{
  if (_shutting_down)
    return OK;

  if (!hcom_fs_helper_is_fs_mounted(_activePartitionId))
    return -ENOENT; // No such file or directory

  if (_fileDescriptor < 0)
    return -EBADF; // Bad file number

  ssize_t nbytes = 0;
  nbytes = write(_fileDescriptor, fileWriteData, fileWriteSize);
  if (nbytes < 0)
  {
    int Errno = get_errno();
    f7syslog(LOG_ERR, "%s() ERROR: failed to write %s for writing: errno %d\n",
             __func__, _activePathFileName, Errno);
    return nbytes;
  }

  if (nbytes < fileWriteSize)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Failed to write all bytes to %s only wrote %d of %d\n",
             __func__, _activePathFileName, nbytes, fileWriteSize);
  }

  f7syslog(LOG_DEBUG, "File System successfully wrote %d bytes to file\n", nbytes);
  return OK;
}

//==================================================================
int hcom_file_commands_close_active_file()
{
  if (!hcom_fs_helper_is_fs_mounted(_activePartitionId))
    return -ENOENT; // No such file or directory

  if (_fileDescriptor < 0) // Okay to close file > once?
    return -EBADF;         // Bad file number

  int ret = close(_fileDescriptor);
  if (ret < 0)
  {
    int Errno = get_errno();
    f7syslog(LOG_ERR, "%s() ERROR: Failed to close %s for writing: errno %d\n",
             __func__, _activePathFileName, Errno);
    return ret;
  }

  f7syslog(LOG_DEBUG, "File System successfully closed %s file\n", _activePathFileName);

  _fileDescriptor = -1;
  _activePathFileName[0] = '\0';
  _activePartitionId = HCOM_INVALID_PARTITION_ID_VALUE;

  return OK;
}

//==================================================================
// Remove the file requested
int hcom_file_commands_delete_by_name(const uint32_t partitionId, const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;
  char *fullPathAndFileName = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);

  DEBUGASSERT(_activePartitionId == HCOM_INVALID_PARTITION_ID_VALUE);
  if (_activePathFileName[0] != '\0')
  {
    // Check if the file to remove is the active file
    if(strcmp(fileName, _activePathFileName) == 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Cannot delete '%s' because it is currently in use.\n",
              __func__, fileName);
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
  // is non-negative and less than buf_size.
  if (filePathAndNameLen < 0 || filePathAndNameLen >= HCOM_MAX_HOST_STRING_BUFF_LENGTH - 1)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Deleting (truncated file name '%s') failed name too long.\n",
             __func__, fullPathAndFileName);
    free(fullPathAndFileName);
    return -ENAMETOOLONG; // File name too long
  }

  int ret = unlink(fullPathAndFileName);
  if (ret < 0)
  {
    int Errno = get_errno();
    f7syslog(LOG_ERR, "%s() ERROR: Failed to unlink %s for writing: errno %d\n",
             __func__, fileName, Errno);
    free(fullPathAndFileName);
    return ret;
  }

  f7syslog(LOG_DEBUG, "File System successfully deleted the file '%s'\n", fileName);
  free(fullPathAndFileName);
  return OK;
}

//==================================================================
// This call will calculate the crc32 checksum for the active file
uint32_t hcom_file_commands_calc_crc_for_file(char *completeFilePath)
{
  uint32_t crc32Checksum = 0;
  uint8_t *crcReadBuff;
  struct stat fileStatus;
  int ret;
  int fd;

  f7syslog(LOG_DEBUG, "%s() - Entered \n", __func__);

  if (_shutting_down)
    return OK;

  // Existing file - open read only
  set_errno(0);
  fd = open(completeFilePath, O_RDONLY);
  if (fd == -1)
  {
    int Errno = get_errno();

#ifdef CONFIG_FS_SMARTFS
    // FYI - #define ENAMETOOLONG 91 #define ENAMETOOLONG_STR "File name too long"
    if (Errno == ENAMETOOLONG)
      f7syslog(LOG_ERR, "%s() Error: failed to open '%s' for writing. File Name too long. Change CONFIG_SMARTFS_MAXNAMLEN.\n",
               __func__, completeFilePath);
    else
#endif
      f7syslog(LOG_ERR, "%s() Error: failed to open '%s' for writing. errno: %d\n",
               __func__, completeFilePath, Errno);
    return -errno;
  }

  f7syslog(LOG_DEBUG, "File System successfully opened %s\n", completeFilePath);
  // struct stat
  // {
  //     _dev_t         st_dev;
  //     _ino_t         st_ino;
  //     unsigned short st_mode;
  //     short          st_nlink;
  //     short          st_uid;
  //     short          st_gid;
  //     _dev_t         st_rdev;
  //     _off_t         st_size;
  //     time_t         st_atime;
  //     time_t         st_mtime;
  //     time_t         st_ctime;
  // };

  ret = fstat(fd, &fileStatus);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() Error: fstat of %s failed: %s errno %d\n", __func__, completeFilePath, errno);
    return -errno;
  }

  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    f7syslog(LOG_ERR, "%s() Error: lseek failed: %s errno %d\n", __func__, completeFilePath, errno);
    return -errno;
  }

  f7syslog(LOG_DEBUG, "%s() - Reading all data for CRC calculation.\n", __func__);

  // Read all the data
  #define HCOM_FILE_READ_BUFF_SIZE_FOR_CRC 1024
  crcReadBuff = malloc(HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);

  ssize_t nbytes;
  do
  {
    nbytes = read(fd, crcReadBuff, HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);
    if (nbytes < 0)
    {
      f7syslog(LOG_ERR, "%s() Error: read failed: %s errno %d\n", __func__, completeFilePath, errno);
      free(crcReadBuff);
      return -errno;
    }

    if (nbytes > 0)
    {
      crc32Checksum = crc32part(crcReadBuff, nbytes, crc32Checksum);
    }
  } while (nbytes > 0);
  free(crcReadBuff);

  ret = close(fd);
  if (ret < 0)
  {
    int Errno = get_errno();
    f7syslog(LOG_ERR, "%s() ERROR: Failed to close %s for writing: errno %d\n",
             __func__, completeFilePath, Errno);
    return -Errno;
  }

  f7syslog(LOG_DEBUG, "%s() - Successfully calculated the checksum for '%s' as 0x%08x\n",
    __func__, completeFilePath, crc32Checksum);
  return crc32Checksum;
}
