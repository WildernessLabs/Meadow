/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dir_cmds.c
 * 
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

// This module contains the code needed to resolve Meadow_Issues
// #320 Add HCOM support for “current directory”


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

// These are the supported CLI/HCOM commmands:
// Change working directory (cd)
// Make directory (mkdir)
// Return working directory (pwd)
// Remove directory (rmdir)
// List directories-subset (ls)

#define HCOM_FILE_DIR_OUTPUT_TO_SYSLOG (1)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

// Use "/" to show all files and directories
static char workingDir[] = "/meadow0";

/****************************************************************************
 * Private Functions
 ****************************************************************************/

int hcom_file_dir_nested_dev_dir_and_files(const char *name,
          char *hostMsg, struct dirent *entry, int indent);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
int hcom_file_dir_nested_dev_dir_and_files_start()
{
  // Keep these larger objects off the stack of the recursive function
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  struct dirent *entry;

  return hcom_file_dir_nested_dev_dir_and_files(workingDir, hostMsg, entry, 0);
}

//--------------------------------------------------------------------------
// NOTE - Recursive function
int hcom_file_dir_nested_dev_dir_and_files(const char *name,
            char *hostMsg, struct dirent *entry, int indent)
{
  DIR *dir;

  if (!(dir = opendir(name)))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Could not open:%s as a directory\n",
              thisFile, __LINE__, name);
    return -1;
  }

  while ((entry = readdir(dir)) != NULL)
  {
    if (DIRENT_ISDIRECTORY(entry->d_type))
    {
      if(strcmp(entry->d_name, "proc") == 0)
        return OK; // ignore procfs information

      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s/\n", indent, "", entry->d_name);
              
#if HCOM_FILE_DIR_OUTPUT_TO_SYSLOG > 0
      syslog(2, hostMsg);
#else
     hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);
#endif
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];
      snprintf_chk(path, sizeof(path), "%s/%s", name, entry->d_name);

      // Recursion is here
      hcom_file_dir_nested_dev_dir_and_files(path, hostMsg, entry, indent + 1);
    }
//     else
//     {
//       // All non-directory types
//       char *entryType;
//       if(DIRENT_ISFILE(entry->d_type)) {entryType = "file";}
//       else if(DIRENT_ISCHR(entry->d_type)) {entryType = "char";}
//       else if(DIRENT_ISBLK(entry->d_type)) {entryType = "block";}
//       else if(DIRENT_ISLINK(entry->d_type)) {entryType = "link";}
//       else {entryType = "????";}
//       snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
//               "%*s%s [%s]\n", indent, "", entry->d_name, entryType);
// #if HCOM_FILE_DIR_OUTPUT_TO_SYSLOG > 0
//       syslog(2, hostMsg);
// #else
//       hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
//                 0, hostMsg, thisFile, __LINE__);
// #endif
//     }
  }

  closedir(dir);
  return OK;
}
