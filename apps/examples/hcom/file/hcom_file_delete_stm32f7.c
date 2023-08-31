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
// This is called by CLI
void hcom_file_delete_stm32f7_file_by_name(hcom_dnld_shared_t *dnldShared)
{
  hcom_file_delete_stm32f7_file_by_name_internal(dnldShared);
  
  // This will remove all data from dnld shared struct
  hcom_host_process_free_dnld_share_mem();
}

//====================================================================
// This is a internal function accessable to internal callers
void hcom_file_delete_stm32f7_file_by_name_internal(hcom_dnld_shared_t *dnldShared)
{
  int ret;
  uint16_t hostMsgType;

  // Memory for text message to host
  char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  if(hostMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    hcom_host_process_free_dnld_share_mem();
    return;
  }

  ret = unlink(dnldShared->dnldFullFileName);
  if (ret < 0)
  {
    char *errorCause;

    hcom_logging_syslog(LOG_ERR, "%s@%d-unlink %s, errno %d\n",
             thisFile, __LINE__, dnldShared->dnldFullFileName,
             get_errno());

    switch(get_errno())
    {
      case ENOENT: // No such file or directory
      errorCause = "No such file";
      break;

      case EEXIST: // File already open
      errorCause = "Another file is being processed";
      break;
      
      case ENAMETOOLONG: // File name too long
      errorCause = "File name too long";
      break;

      case EMFILE: // Too many files open
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
        thisFile, __LINE__, get_errno(), errorCause, dnldShared->dnldFullFileName);

    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
          "Meadow failed to delete '%s' - %s",
          dnldShared->dnldFullFileName, errorCause);
  }
  else
  {
    hostMsgType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
          "Meadow successfully deleted '%s'",
          dnldShared->dnldFullFileName);
  }

  // Send text message to host
  hcom_host_send_simple_string_msg(hostMsgType, 0, hostMsg, thisFile, __LINE__);

  free(hostMsg);
}
