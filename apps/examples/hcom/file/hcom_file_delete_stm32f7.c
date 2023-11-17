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
#include <meadow/hcom_dnld_shared.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/dirent.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) hcom_file_delete_stm32f7.c"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// This is directly called by CLI
int hcom_file_delete_stm32f7_file_by_name(hcom_dnld_shared_t *dnldShared)
{
  // Delete the file
  return hcom_file_delete_stm32f7_file_by_name_internal(dnldShared);
}

//====================================================================
// This is a internal function accessable to internal callers
int hcom_file_delete_stm32f7_file_by_name_internal(hcom_dnld_shared_t *dnldShared)
{
  int ret;

  // Memory for text message to host
  char *hostMsg = malloc(HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  if(hostMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Delete the specified file
  ret = unlink(dnldShared->dnldFullPathName);
  if (ret < 0)
  {
    char *errorCause = malloc(HCOM_TINY_HOST_STRING_BUFF_LENGTH);
    ret = -get_errno();
    switch(ret)
    {
      case -ENOENT: // No such file or directory
      strcpy(errorCause, "No such file");
      break;

      case -EEXIST: // File already open
      strcpy(errorCause, "Another file is being processed");
      break;
      
      case -ENAMETOOLONG: // File name too long
      strcpy(errorCause, "File name too long");
      break;

      case -EMFILE: // Too many files open
      strcpy(errorCause, "Too many files open");
      break;

      default:  // different error
      snprintf_chk(errorCause, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "Unexpected error:%d", ret);
      break;
    }

    hcom_logging_syslog(LOG_ERR, "%s@%d-Error-failed to delete:'%s' %s, errno:%d\n",
        thisFile, __LINE__, dnldShared->dnldFullPathName, errorCause, get_errno());

    // Message to host PC
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Meadow failed to delete file '%s'. %s",
          dnldShared->dnldFullPathName, errorCause);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, thisFile, __LINE__);

    free(errorCause);
    free(hostMsg);
    
    // Concluded message will be sent by caller
    return ret;
  }
#if defined (CONFIG_DIR_MGMT_TESTS)
  {
    syslog(2, "%s@%d-File:'%s' deleted, ret:%d, errno:%d\n",
          thisFile, __LINE__, dnldShared->dnldFullPathName, ret, errno);
  }
#endif

  // Once the file has been deleted we must delete any related empty
  // subdirectories.
  // An element count of 2 means there will be no subdirectories.
  if(dnldShared->dnldPathNameEleCount > 2)
  {
    uint32_t dirDepth = dnldShared->dnldPathNameEleCount - 2;
    char *pathNameTemp = malloc(strlen(dnldShared->dnldFullPathName) + 1);
    strcpy(pathNameTemp, dnldShared->dnldFullPathName);

    // We'll remove the deepest directory to the first.
    // Find the last '/' which should be the end of the string
    char *slashPtr = strrchr(pathNameTemp, '/');
    while(slashPtr != NULL && dirDepth > 0)
    {
      // Replace last '/' with NULL
      *slashPtr = '\0';

#if defined (CONFIG_DIR_MGMT_TESTS)
      syslog(2, "Attempting to delete:%s/n", pathNameTemp);
#endif

      ret = rmdir(pathNameTemp);
      if(ret < 0)
      {
        // Have we reached the end of empty directories?
        // Note: ENOTEMPTY in the NuttX errno.h file is 90 but rmdir returned
        //  39. I was only able to reconcile this by locating the littlefs
        //  header file, lfs.h which used 39 for not empty. I tried to
        //  reference this header at nuttx/fs/littlefs/lfs.h but the compiler
        //  couldn't find it because it's on the Nuttx side and this code is on
        //  the apps side.
        //  Realizing that the Nuttx Rebase may well fix the problem, I decided
        //  to define it here.
#define LITTLEFS_VERSION_OF_ENOTEMPTY (39)
        // If an attempt to delete a directory failed, because wasn't empty,
        // this is not an error. It means we are finished removing empty
        // directories.
        if(errno != LITTLEFS_VERSION_OF_ENOTEMPTY)
        {
          syslog(LOG_ERR, "%s@%d-ERROR deleting directory '%s', ret:%d, errno:%d\n",
                    thisFile, __LINE__, pathNameTemp, ret, errno);
          ret = -errno;
        }
#if 1
        else
        {
          syslog(2, "%s@%d-Directory '%s' is not empty, ret:%d, errno:%d\n",
                    thisFile, __LINE__, pathNameTemp, ret, errno);
        }
#endif
        break;    // Leave directory delete loop
      }
      else
      {
        dnldShared->dnldPathNameEleCount--;
      }

      // Prep for next loop
      slashPtr = strrchr(pathNameTemp, '/');
      dirDepth--;
    }

    free(pathNameTemp);
  }

  // Send text message to host.
  snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "File '%s' deleted", dnldShared->dnldFullPathName);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg, thisFile, __LINE__);
  free(hostMsg);

  // Concluded message will be sent by caller
  return ret;
}
