/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_lists_subdir.c
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

// This file contains the new file list scheme that includes displaying
// subdirectories. The previous version remains in hcom_file_list.c to support
// CLIv1 users. But, should be removed when CLIv1 is no longer supported.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <nuttx/config.h>
#include <dirent.h>
#include <sys/stat.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) hcom_file_lists_subdir.c"
#endif

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

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This single function handles file list with and without CRC checksum.
int hcom_file_lists_all_files_in_subdirectory(const HcomProtoHdrMsg_t *hdrMsg,
          hcom_dnld_shared_t *dnldShared, bool isCrcNeeded)
{
  int fileCount = 0;
  int dirCount = 0;
  DIR *dirp;
  struct dirent *direntry;
  off_t totalSizeOfFiles;
  uint32_t totalFlashSizeKB;

  char *fileInformation = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fileInformation == NULL)
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
    free(fileInformation);
    return -1;
  }

  // For file list, there's no file name just path so, 0 elements is the
  // default. Anything greater we should include in the returned list.
  bool useFullPath = false;   // For now don't add full path name to files
  totalSizeOfFiles = 0;
  totalFlashSizeKB = 0;

  // Show the starting point for this group of items 
  snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
              "Directory:%s", dnldShared->dnldOrigPathName);

  // Send to directory to CLI
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
              fileInformation, thisFile, __LINE__);

  hcom_logging_syslog(LOG_INFO, "%s@%d-%s\n",
              thisFile, __LINE__, dnldShared->dnldFullPathName);

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      fileCount++;

      // CRC or No CRC?
      if(isCrcNeeded)
      {
        char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
        if(completeNameBuf == NULL)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
          free(fileInformation);
          return -ENOMEM;
        }

        // Build full path and file name, for CRC call
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
          
          free(completeNameBuf);
          free(fileInformation);
          return detectError;
        }
      
        totalSizeOfFiles += fileSize;
        totalFlashSizeKB += blockSizeKB;

        // Send this file's information to CLI
        snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                  "%s%s [0x%08x] %d KB (%u bytes)",
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name,
                  crcChecksum, blockSizeKB, fileSize);

        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                      fileInformation, thisFile, __LINE__);

        hcom_logging_syslog(LOG_INFO, "%s@%d-%s%s checksum:0x%08x, %d KB (%u bytes)\n",
                  thisFile, __LINE__,
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name,
                  crcChecksum, blockSizeKB, fileSize);

        free(completeNameBuf);
      }
      else
      {
        // Get the file name and size but not CRC
        snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                  "%s%s",
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name);

        // Send to file to CLI
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                  fileInformation, thisFile, __LINE__);

        hcom_logging_syslog(LOG_INFO, "%s@%d-%s%s\n",
                  thisFile, __LINE__,
                  useFullPath ? dnldShared->dnldFullPathName : "",
                  direntry->d_name);
      }
    }
    else if(DIRENT_ISDIRECTORY(direntry->d_type) &&
              dnldShared->dnldRqstCat != pathnameOriginal)
    {
      // Ignore these directories, we only care about named directories
      if (strcmp(direntry->d_name, ".") == 0 || strcmp(direntry->d_name, "..") == 0)
        continue;

      dirCount++;

      snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                "/%s", direntry->d_name);

      // Send to directory to CLI
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                fileInformation, thisFile, __LINE__);

      hcom_logging_syslog(LOG_INFO, "%s@%d-/%s\n",
                thisFile, __LINE__, direntry->d_name);
    }
    else if(DIRENT_ISBLK(direntry->d_type) &&
              dnldShared->dnldRqstCat != pathnameOriginal)
    {
      // Show block devices too
      snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
                "/%s", direntry->d_name);

      // Send to directory to CLI
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                fileInformation, thisFile, __LINE__);

      hcom_logging_syslog(LOG_INFO, "%s@%d-/%s\n",
                thisFile, __LINE__, direntry->d_name);
    }
    // Ignore character devices and links
  }

  // This code doesn't counting block devices
  if(isCrcNeeded)
  {
    snprintf_chk(fileInformation, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
          "A total of %d file%s and %d director%s using %d KB (%u bytes)", fileCount,
          fileCount == 1 ? "" : "s", dirCount == 1 ? "y" : "ies",
          totalFlashSizeKB, totalSizeOfFiles);

    // Send the totals
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                  fileInformation, thisFile, __LINE__);

    hcom_logging_syslog(LOG_INFO, "%s@%d-A total of %d file%s and %d director%s using %d KB (%u bytes)\n",
              thisFile, __LINE__,
              fileCount, fileCount == 1 ? "" : "s",
              dirCount, dirCount == 1 ? "y" : "ies",
              totalFlashSizeKB, totalSizeOfFiles);
  }
  else
  {
      char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "A total of %d file%s and %d director%s found",
                fileCount, fileCount == 1 ? "" : "s",
                dirCount, dirCount == 1 ? "y" : "ies");
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                    hostMsg, thisFile, __LINE__);
  }

  closedir(dirp);

  free(fileInformation);
  return OK;
}
