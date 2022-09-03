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

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

static int _fileDescriptor;
static uint32_t _activePartitionId;
static const char *_simpleFileName; // Memory in hcom_file_dnld_stm32f7.c
static char *_fullFileName;         // Memory never reclaimed

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
  _fullFileName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(_fullFileName == NULL)
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
    
  free(_fullFileName);
}

//==================================================================
// The active file is the file currently being downloaded to flash
int hcom_file_write_open_active_file(const uint32_t partitionId,
          const char *mountPoint, const char *simpleFileName)
{
  if (_shutting_down)
    return OK;

  // Only used if download fails
  _simpleFileName = simpleFileName;

  // Build the full path and file name string (e.g. /mnt0/FileName.ext)
#ifdef CONFIG_MTD_PARTITION
  snprintf_chk(_fullFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d/%s",
                                mountPoint, partitionId, simpleFileName);
#else
  snprintf_chk(_fullFileName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                                mountPoint, simpleFileName);
#endif

  if (!hcom_via_nx_is_mounted(partitionId))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-F/S not mounted %s\n",
             thisFile, __LINE__, _fullFileName);
    return -ENOENT; // No such file or directory
  }

  // Second (flags) parameter O_RDONLY, O_WRONLY, or O_RDWR ||
  // Third parameter 644 = owner has read and write permission, group has
  // read and others have read 777 everyone has read write and execute
  // permission.
  set_errno(0);
  _fileDescriptor = open(_fullFileName, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (_fileDescriptor == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno:%d\n",
              thisFile, __LINE__, _fullFileName, get_errno());
    return -get_errno();
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Opened '%s'\n", thisFile, __LINE__, _fullFileName);
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
             thisFile, __LINE__, _fullFileName, Errno);
    return nbytes;
  }

  if (nbytes < fileWriteSize)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-'%s' wrote %d of %d bytes\n",
             thisFile, __LINE__, _fullFileName, nbytes, fileWriteSize);
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Wrote %d bytes to %s\n", 
            thisFile, __LINE__, nbytes, _fullFileName);
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
             thisFile, __LINE__, _fullFileName, errno);
    ret = -errno;       // Continue even with error
  }

  _fileDescriptor = -1;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Closed %s\n", thisFile, __LINE__, _fullFileName);
#endif

  return ret;
}

//=============================================================================
// Watchdog timeout occurred while doing a download. This function will delete
// the file, return the state to inactive and send a request to the CLI to
// the file needs to be resent the file.
int hcom_file_write_stm32f7_cleanup_on_dnld_error()
{
  int ret = OK;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  // Close the file
  ret = hcom_file_write_close_active_file();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-unlink failed for '%s', errno %d\n",
             thisFile, __LINE__, _fullFileName, get_errno());
  }

  // Delete the file from the file system
  ret = unlink(_fullFileName);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-delete failed for '%s', errno %d\n",
             thisFile, __LINE__, _fullFileName, get_errno());
  }

#if HCOM_PROTOCOL_INCLUDE_POST_RC1_REQUEST_TYPES > 0
  // The next call will free the simple file name so build the CLI message before
  // setting the state to inactive.
  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "File '%s' download to Meadow failed", _simpleFileName);
#endif

  hcom_file_dnld_stm32f7_free_file_name_buf();
  hcom_file_dnld_stm32f7_set_to_inactive();

#if HCOM_PROTOCOL_INCLUDE_POST_RC1_REQUEST_TYPES > 0
  // Send message to CLI
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_DNLD_FAIL_RESEND, 0, hostMsg,
        thisFile, __LINE__);
#endif

  return ret;
}
