/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_lists.c
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
// 

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <nuttx/config.h>
#include <dirent.h>
#include <sys/stat.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int hcom_file_lists_all_dev_dir_and_files(const char *name, int indent, uint32_t userData);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This single function handles file list with and without CRC checksum.
int hcom_file_lists_all_files_in_directory(const HcomProtoHdrMsg_t *hdrMsg,
          hcom_dnld_shared_t *dnldShared, bool isCrcNeeded)
{
  int fileCount = 0;
  DIR *dirp;
  struct dirent *direntry;
  off_t totalSizeOfFiles;
  uint32_t totalFlashSizeKB;

  char *fileFoundName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fileFoundName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Tell CLI to output a header for the file list
  hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_LIST_HEADER, 0, thisFile, __LINE__);

  // Open the directory
  dirp = opendir(dnldShared->dnldFullPathName);
  if ( !dirp )
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-opendir(\"%s\") errno:%d\n",
              thisFile, __LINE__, dnldShared->dnldFullPathName, errno);
    return -1;
  }

  // For file list, there's no file name just path so, 0 elements is the
  // default. Anything greater we should include in the returned list.
  bool useFullPath = true;
  if(dnldShared->dnldPathNameEleCount == 0)
    useFullPath = false;

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      fileCount++;

      // CRC or No CRC?
      if(isCrcNeeded)
      {
        totalSizeOfFiles = 0;
        totalFlashSizeKB = 0;

        if(DIRENT_ISFILE(direntry->d_type))
        {
          char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
          if(completeNameBuf == NULL)
          {
            hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
            return -ENOMEM;
          }

          // Build full path and file, for CRC call
          snprintf_chk(completeNameBuf, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s",
                    dnldShared->dnldFullPathName, direntry->d_name);

          // Find the CRC checksum
          off_t fileSize;
          uint32_t blockSizeKB;
          int detectError;
          uint32_t crcChecksum = hcom_file_misc_calc_crc_for_file(completeNameBuf,
                    &fileSize, &blockSizeKB, &detectError);
          if(detectError < 0)
          {
            hcom_logging_syslog(LOG_ERR, "%s@%d-Error in Checksum calculation,err:%d\n",
                      thisFile, __LINE__, detectError);
            return detectError;
          }
        
          totalSizeOfFiles += fileSize;
          totalFlashSizeKB += blockSizeKB;

          // Send this file's information to CLI
          snprintf_chk(fileFoundName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                    "%s%s [0x%08x] %d KB (%u bytes)",
                    useFullPath ? dnldShared->dnldFullPathName : "",
                    direntry->d_name,
                    crcChecksum, blockSizeKB, fileSize);

          hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                        fileFoundName, thisFile, __LINE__);

          hcom_logging_syslog(LOG_INFO, "%s@%d-%s%s checksum:0x%08x, %d KB (%u bytes)\n",
                    thisFile, __LINE__,
                  useFullPath ? dnldShared->dnldFullPathName : "",
                    direntry->d_name,
                    crcChecksum, blockSizeKB, fileSize);

          free(completeNameBuf);
        }
      }
      else
      {
        // Get the file name and size
        snprintf_chk(fileFoundName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                  "%s%s",
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name);

        // Send to file to CLI
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                  fileFoundName, thisFile, __LINE__);

        hcom_logging_syslog(LOG_INFO, "%s@%d-%s%s\n",
                  thisFile, __LINE__,
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name);
      }
    }
  }

  if(isCrcNeeded)
  {
    if(fileCount == 0)
    {
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                    "No files found", thisFile, __LINE__);
    }
    else
    {
      snprintf_chk(fileFoundName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
            "A total of %d file%s using %d KB (%u bytes)", fileCount,
            fileCount == 1 ? "" : "s", totalFlashSizeKB, totalSizeOfFiles);

      // Send the totals
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                    fileFoundName, thisFile, __LINE__);
                    
      hcom_logging_syslog(LOG_INFO, "%s@%d-A total of %d file%s using %d KB (%u bytes)\n",
                thisFile, __LINE__,
                fileCount, fileCount == 1 ? "" : "s", totalFlashSizeKB, totalSizeOfFiles);
    }
  }
  else
  {
    if(fileCount == 0)
    {
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                    "No files found", thisFile, __LINE__);
    }
    else
    {
      char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "A total of %d file%s found", fileCount, fileCount == 1 ? "" : "s");
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                    hostMsg, thisFile, __LINE__);
    }
  }

  closedir(dirp);

  free(fileFoundName);
  return OK;
}

// ==============================================================
// THIS IS AN UNDOCUMENTED FEATURE
// The above statement is no longer true. This feature is being used by some
// part of CLI, unsure of the usage
int hcom_file_lists_all_dev_dir_and_files_start(uint32_t userData)
{
  // Changing "/" to "meadow0" will only show meadow files
  return hcom_file_lists_all_dev_dir_and_files("/", 0, userData);
}

//----------------------------------------------
// NOTE - Recursive function
int hcom_file_lists_all_dev_dir_and_files(const char *name, int indent, uint32_t userData)
{
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  DIR *dir;
  struct dirent *entry;

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
      // Only show procfs information if userData == 1234
      if(userData != 1234 && strcmp(entry->d_name, "proc") == 0)
        return OK; // ignore procfs information

      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s/\n", indent, "", entry->d_name);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];
      snprintf_chk(path, sizeof(path), "%s/%s", name, entry->d_name);
      
      // Recursion is here
      hcom_file_lists_all_dev_dir_and_files(path, indent + 1, userData);
    }
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
              "%*s%s [%s]\n",indent, "", entry->d_name, entryType);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);
    }
  }

  closedir(dir);
  return OK;
}
