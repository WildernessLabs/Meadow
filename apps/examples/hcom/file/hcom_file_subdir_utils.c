/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_subdir_utils.c
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

// This module contains utilities to support adding subdirectories to Meadow.OS
// This will resolve Meadow_Issues #320 Add HCOM support for “current directory”

// The existing situation is that all files are placed in the /meadow0/
// directory and the CLI just sends the bare file name and it is assumed that
// the file is to be written to the /meadow0/ device.
//
// The following rules will be implemented.
// 1. All reads or writes sent with just a bare file name will default to using meadow0/ device.
//    The intent is to support the existing CLI behavior, without changes.
// 2. Files prepended with a single '/' (e.g. /filename) will considered an error.
// 3. Files prepended with /dir are an error, must prepend with /meadow0/dir/file name.
// 4. For writing files, if the directory or directory tree does not exist, it will be created.
// 5. Reads from a non-existing directory will return an error.
// 6. There should be an nesting limit for directories. The initial limit is 6.
// 7. When HCOM deletes a file if the directory is now empty, it will be automatically deleted.
// 8. Al file writes or reads for the SD-Card must begin with '/mmcsd0/filename'.
//    No legacy support as #1.
// 9. Relative directories and the like are not supported.

// ctacke
// 1. Agreed, no leading '/' would be "legacy" and just mean /meadow0/ is pre-pended
// 2. this feels a bit confusing.  Why not force the client to always use an
//    absolute path (e.g. /meadow0/foo or /mmcsd0/bar)? That would keep things
//    clean for other attached devices or partitions in the future
// 3. See #2.  Put the work on the client to keep it straight
// 4. :+1:
// 5. :+1:
// 6. With the store name, that is really only 3, which seems light - can we do 6?
// 7. You mean only when it becomes empty through an HCOM delete, yes?  If so :+1:
// 8. See #2. if we require the absolute name it addresses this
// 9. :+1:
// 10. We will need a way to query directories - right now we can only get file lists
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

#define HCOM_FILE_DIR_OUTPUT_TO_SYSLOG (1)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;


/****************************************************************************
 * Private Functions
 ****************************************************************************/

int hcom_file_subdir_read_nested_directories(const char *rootDir,
          char *hostMsg, struct dirent *entry, int indent);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/

// Use rootDir as a single "/" to show all files and directories
int hcom_file_subdir_read_nested_directories_start(const char *rootDir)
{
  // Keep these larger objects off the stack of the recursive function
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  struct dirent *entry;

  return hcom_file_subdir_read_nested_directories(rootDir, hostMsg, entry, 0);
}

//--------------------------------------------------------------------------
// NOTE - Recursive function, is not public
int hcom_file_subdir_read_nested_directories(const char *rootDir,
            char *hostMsg, struct dirent *entry, int indent)
{
  DIR *dir;

  if (!(dir = opendir(rootDir)))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Could not open:%s as root directory\n",
              thisFile, __LINE__, rootDir);
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

      char path[256];   // MAX_PATH
      snprintf_chk(path, sizeof(path), "%s/%s", rootDir, entry->d_name);

      // Recursion is here since directory
      hcom_file_subdir_read_nested_directories(path, hostMsg, entry, indent + 1);
    }
#if 1     // Show everything not just directories
    else
    {
      // All non-directory types
      char *entryType;
      if(DIRENT_ISFILE(entry->d_type)) {entryType = "file";}
      else if(DIRENT_ISCHR(entry->d_type)) {entryType = "char";}
      else if(DIRENT_ISBLK(entry->d_type)) {entryType = "block";}
      else if(DIRENT_ISLINK(entry->d_type)) {entryType = "link";}
      else {entryType = "????";}
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s [%s]\n", indent, "", entry->d_name, entryType);
#if HCOM_FILE_DIR_OUTPUT_TO_SYSLOG > 0
      syslog(2, hostMsg);
#else
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);
#endif
    }
#endif

  }

  closedir(dir);
  return OK;
}

