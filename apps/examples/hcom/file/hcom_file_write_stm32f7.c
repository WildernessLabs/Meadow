/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_write_stm32f7.c
 * 
 *   Copyright (C) 2019 - 2023 Wilderness Labs. All rights reserved.
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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/dirent.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--)hcom_file_write_stm32f7 .c"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

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

  return OK;
}

//=======================================================================
// Called before hcom mgr closes _hcom_communications_fd which, forces a receive
// error which, causes the thread to return.
void hcom_file_write_shutdown()
{
  _shutting_down = true;
}

//==================================================================
// The active file is the file currently being downloaded to flash
int hcom_file_write_open_active_file(hcom_dnld_shared_t *dnldShared)
{
  if (_shutting_down)
    return OK;

  // Only test if meadow file system. We've already attempted to mount the
  // SDCard so if this open is related to SDCard no need to retest here.
  if(dnldShared->dnldRqstCat == pathnameMeadow)
  {
    if (!hcom_via_nx_is_mounted(dnldShared->dnldFilePartId))
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-F/S not mounted %s\n",
              thisFile, __LINE__, dnldShared->dnldFullPathName);
      return -ENOENT; // No such file or directory
    }
  }

  // Second (flags) parameter O_RDONLY, O_WRONLY, or O_RDWR ||
  // Third parameter 644 = owner has read and write permission, group has
  // read and others have read 777 everyone has read write and execute
  // permission.
  set_errno(0);

  dnldShared->dnldFileFD = open(dnldShared->dnldFullPathName, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (dnldShared->dnldFileFD == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno:%d\n",
              thisFile, __LINE__, dnldShared->dnldFullPathName, get_errno());
    return -get_errno();
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Opened '%s'\n", thisFile, __LINE__, dnldShared->dnldFullPathName);
#endif

  return OK;
}

//==================================================================
// When data to be added to a file is received, it arrives here for writing.
int hcom_file_write_to_active_file(hcom_dnld_shared_t *dnldShared,
          const uint8_t *fileWriteData, const size_t fileWriteSize)
{
  uint8_t *writeDataBuff = NULL;

  if (_shutting_down)
    return OK;

  if (dnldShared->dnldFileFD < 0)
    return -EBADF; // Bad file number

  // Only test if Meadow file system We've already attempted to mount the
  // SDCard so if this open is related to SDCard no need to retest here.
  if(dnldShared->dnldRqstCat == pathnameMeadow)
  {
    if (!hcom_via_nx_is_mounted(dnldShared->dnldFilePartId))
      return -ENOENT; // No such file or directory
    
    writeDataBuff = (uint8_t *)fileWriteData;
  }
  else if(dnldShared->dnldRqstCat == pathnameSdcard)
  {
    // Found that the SD-Card write must be properly aligned or it writes data
    // to the file that is outside of the specified buffers beginning address.
    // malloc will allocate memory that this 4-byte aligned.
    writeDataBuff = malloc(fileWriteSize);
    if(writeDataBuff == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    memcpy(writeDataBuff, fileWriteData, fileWriteSize);
  }

#if defined (CONFIG_DIR_MGMT_TESTS)
  #if (HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0)
  syslog(1, "------- %s@%d Show first 16 of file write ------\n",
            __FILE__, __LINE__);
  hcom_diag_print_buffer(writeDataBuff, 16, 1);
  #endif
#endif

  ssize_t nbytes = 0;
  nbytes = write(dnldShared->dnldFileFD, writeDataBuff, fileWriteSize);
  if (nbytes < 0)
  {
    int Errno = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-failed to write %s, errno %d\n",
             thisFile, __LINE__, dnldShared->dnldFullPathName, Errno);

    if(dnldShared->dnldRqstCat == pathnameSdcard)
      free(writeDataBuff);
      
    return nbytes;
  }

  // Only free if allocated (i.e. sdcard)
  if(dnldShared->dnldRqstCat == pathnameSdcard)
  {
    free(writeDataBuff);
  }

  if (nbytes < fileWriteSize)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-'%s' wrote %d of %d bytes\n",
             thisFile, __LINE__, dnldShared->dnldFullPathName, nbytes, fileWriteSize);
  }

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Wrote %d bytes to %s\n", 
            thisFile, __LINE__, nbytes, dnldShared->dnldFullPathName);
#endif

  return OK;
}

//==================================================================
// When downloading to a file and the end of file message is received
// this function is called to close the file and clean up.
// We ask for the file name to be returned so that it's available to use in
// requesting CLI to resend on error.
int hcom_file_write_close_active_file(hcom_dnld_shared_t *dnldShared)
{
  int ret = OK;

  // Only test if meadow file system. We've already attempted to mount the
  // SDCard so if this open is related to SDCard no need to retest here.
  if(dnldShared->dnldRqstCat == pathnameMeadow)
  {
    if (!hcom_via_nx_is_mounted(dnldShared->dnldFilePartId))
      return -ENOENT;         // No such file or directory
  }

  if (dnldShared->dnldFileFD < 0)  // Okay to close file multiple times in nuttx?
    return -EBADF;          // Bad file number

  ret = close(dnldShared->dnldFileFD);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Close of %s, errno %d\n",
             thisFile, __LINE__, dnldShared->dnldFullPathName, errno);
    ret = -errno;       // Continue even with error
  }

  dnldShared->dnldFileFD = -1;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Closed %s\n", thisFile, __LINE__, dnldShared->dnldFullPathName);
#endif

  return ret;
}
