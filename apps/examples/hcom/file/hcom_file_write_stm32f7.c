/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_write_delete.c
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

// Used for writing
#define HCOM_INVALID_PARTITION_ID_VALUE 0xffffffff

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

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
int hcom_file_write_setup()
{
  _shutting_down = false;

  _fileDescriptor = -1;
  _hcomActiveFileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(_hcomActiveFileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  return OK;
}

//=======================================================================
// Called before hcom mgr closes _hcom_communications_fd which, forces a receive
// error which, causes the thread to return.
void hcom_file_write_shutdown()
{
  _shutting_down = true;

  if (_fileDescriptor != -1)
    hcom_file_write_close_active_file();
    
  free(_hcomActiveFileName);
}

//==================================================================
// The active file is the file currently being downloaded to flash
int hcom_file_write_open_active_file(const uint32_t partitionId,
          const char *mountPoint, const char *fileName)
{
  int filePathAndNameLen;

  if (_shutting_down)
    return OK;

  // Build the full path and file name string
#ifdef CONFIG_MTD_PARTITION
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf_chk(_hcomActiveFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, fileName);
#else
  // e.g. /mnt0/FileName.ext
  filePathAndNameLen = snprintf_chk(_hcomActiveFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                                mountPoint, fileName);
#endif

  if (!hcom_via_nx_is_mounted(partitionId))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-F/S not mounted %s\n",
             thisFile, __LINE__, _hcomActiveFileName);
    return -ENOENT; // No such file or directory
  }

  // Second (flags) parameter O_RDONLY, O_WRONLY, or O_RDWR ||
  // Third parameter 644 = owner has read and write permission, group has
  // read and others have read 777 everyone has read write and execute
  // permission.
  set_errno(0);
  _fileDescriptor = open(_hcomActiveFileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (_fileDescriptor == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno:%d\n",
              thisFile, __LINE__, _hcomActiveFileName, get_errno());
    return -get_errno();
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Opened '%s'\n", thisFile, __LINE__, _hcomActiveFileName);
#endif

  _activePartitionId = partitionId;
  return OK;
}

//==================================================================
// When data, to be added to a file is received, it arrives here.
int hcom_file_write_to_active_file(const uint8_t *fileWriteData, const size_t fileWriteSize)
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

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Wrote %d bytes to %s\n", 
            thisFile, __LINE__, nbytes, _hcomActiveFileName);
#endif

  return OK;
}

//==================================================================
// When downloading to a file and the end of file message is received
// this function is called to close the file and clean up.
int hcom_file_write_close_active_file()
{
  int ret = OK;

  if (!hcom_via_nx_is_mounted(_activePartitionId))
    return -ENOENT;         // No such file or directory

  if (_fileDescriptor < 0)  // Okay to close file multiple times in nuttx?
    return -EBADF;          // Bad file number

  ret = close(_fileDescriptor);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Close of %s, errno %d\n",
             thisFile, __LINE__, _hcomActiveFileName, errno);
    ret = -errno;       // Continue even with error
  }

  _fileDescriptor = -1;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Closed %s\n", thisFile, __LINE__, _hcomActiveFileName);
#endif

  return ret;
}

