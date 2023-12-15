/****************************************************************************
 * \apps\examples\hcom\file\hcom_dir_mgmt_utils.c
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
// The following rules will be followed:
// 1. All reads or writes sent with just a bare file name will default to using
//     /meadow0/. The intent is to support the existing CLIv1 behavior, without
//     changes to it.
// 2. All file writes or reads for the SD-Card must begin with /sdcard/.
// 3. Files prepended with a single '/' (e.g. '/filename' or './filename') will
//     considered an error.
// 4. Files downloaded to subdirectory must be in this format
//      '/meadow0/dir/filename' or '/sdcard/dir/filename'
// 5. There is a limit of 6 nested subdirectories, not counting /meadow0.
// 6. For writing files, if the directory or directories don't exist, they will
//      automatically be created.
// 7. When a file is delete all lower, empty directories will automatically be
//      deleted.
// 8. Reads from a non-existing directory will return an error.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_dnld_shared.h>
#include <meadow/meadow_os.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/dirent.h>
#include <syslog.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) dir_mgmt_utils.c"
#endif

#pragma GCC optimize("O0")    // Prevent compiler from changing the code

#define HCOM_FILE_DIR_OUTPUT_TO_SYSLOG (1)

// The following deal with testing subdirectory support
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR   ("/meadow0/")
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN   (9)
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR   ("/sdcard/")
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN   (8)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static hcom_dnld_shared_t *_dnldShared;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This function will return the depth of subdirectories in the file name.
static uint32_t find_pathname_element_count(const char *pathName, size_t strLen)
{
  // Allocate a modifiable version of the string for tokenizing
  char *pathNameTemp = malloc(strLen + 1);
  strcpy(pathNameTemp, pathName);

  // Count the number of '/' characters to give an indication of the subdir
  // depth
  uint32_t elementCount = 0;
  char *savePtr;
  char *token = strtok_r(pathNameTemp, "/", &savePtr);

  while (token != NULL)
  {
    elementCount++;
    token = strtok_r(NULL, "/", &savePtr);
  }

  free(pathNameTemp);

  return elementCount;
}

//============================================================================
// This function will check the received path/file name and categorize it and
// determine if the format is correct. This way the remaining steps will know
// what they are dealing with. Valid file names are:
// 'filename', '/meadow0/filename', '/meadow0/dir1/dir2/filename', '/'
//
// These are illegal formats:
// '/filename' - has leading '/'
// /dirname/filename/ - missing leading '/meadow0'
// endExpectFileName: true = pathname no ending '/', false = ending '/' needed
static enum hcom_file_msg_cat_e hcom_dir_mgmt_categorize_pathname(
          const char *pathName, size_t strLen,
          uint32_t *pathNameElements, bool endExpectFileName)
{
  *pathNameElements = 0;

  // Is this a bare filename (i.e. no '/')
  if(memchr(pathName, '/', strLen) == NULL)
  {

    // No '/' in file name, this is like original file naming scheme for
    // download
    if(endExpectFileName)
      *pathNameElements = 2;  // 2 includes '/meadow0' and file name
    else
      *pathNameElements = 0;  // For file list there's no file name
    
    return pathnameOriginal;
  }
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR,
              pathName, MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN) == 0)
  {
    // '/meadow0/' found. If this is for file download it cannot end in '/',
    // it must end with a file name. However, if this being called for a file
    // list it must end in '/'.
    if(endExpectFileName)
    {
      if(pathName[strLen-1] == '/')
          return pathnameInvalidSlash;
    }
    else
    {
      if(pathName[strLen-1] != '/')
          return pathnameInvalidNoSlash;
    }

    *pathNameElements = find_pathname_element_count(pathName, strLen);
    return pathnameFullMeadow;
  }
#if defined (CONFIG_STM32F7_SDMMC2)
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR,
              pathName, MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN) == 0)
  {
    meadow_configuration_t *config = meadow_os_deep_copy_config();
    if (config->sd_storage_supported)
    {
      // '/sdcard/' found, but can't end in '/', must have file name, unless this
      // being called for a file list, in which case it must end in '/'.
      if(endExpectFileName)
      {
        if(pathName[strLen-1] == '/')
            return pathnameInvalidSlash;
      }
      else
      {
        if(pathName[strLen-1] != '/')
            return pathnameInvalidNoSlash;
      }

      *pathNameElements = find_pathname_element_count(pathName, strLen);
      return pathnameFullSdcard;
    }
  }
  else if(strLen == 1 && pathName[0] == '/')
  {
    // Found a single '/'
    if(endExpectFileName)
    {
      return pathnameInvalidNoSlash;
    }
    else
    {
      *pathNameElements = 1;      // One is kind of correct
      return pathnameSingleSlash;
    }
  }
  else if(pathName[0] == '/' && pathName[strLen -1] == '/')
  {
    // Found '/pathname/'
    if(endExpectFileName)
    {
      return pathnameInvalidNoSlash;
    }
    else
    {
      *pathNameElements = find_pathname_element_count(pathName, strLen);
      return pathnameSlashSlash;
    }
  }

#endif
  // Lots of reasons, upper/lower case, spelling.... Wish I knew them all
  // The above is only looking for the positive reasons to allow progress.
  return pathnameInvalid;
}

//=====================================================================
// File name processing. This is the entry point for all downloads, deletes
// etc.
static int hcom_dir_mgmt_eval_build_pathname(hcom_dnld_shared_t *dnldShared,
          char *pathNameStr, size_t fileNameLength,
          bool endExpectFileName)
{
  enum hcom_file_msg_cat_e catType;
  uint32_t pathNameElements;
  size_t dnldFileAndPathLen;
  
  // Make sure shared version available
  _dnldShared = dnldShared;

  // Allocated + space for string terminating NULL
  dnldShared->dnldOrigPathName = malloc(fileNameLength + 1);
  if(dnldShared->dnldOrigPathName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Continue to populate shared download struct with file name information
  strcpy(dnldShared->dnldOrigPathName, pathNameStr);

  // There are 3 valid path name formats.
  // 1. A simple file name, with just a file name, nothing else.
  // 2. A path name beginning with '/meadow0/'
  // 3. A path name beginning with '/sdcard/'
  // 4. A path name being only '/'
  // This call will catergorize as one of the above or error. It is assumes
  // that the last '/' signifies the start of the file name, unless it is
  // a file list command.
  catType = hcom_dir_mgmt_categorize_pathname(dnldShared->dnldOrigPathName,
            fileNameLength, &pathNameElements, endExpectFileName);
  
  // Save
  dnldShared->dnldRqstCat = catType;

  if(catType == pathnameInvalid )
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-pathname '%s' is invalid\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName);

    char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
                thisFile, __LINE__);
      return -ENOMEM;   // No Memory
    }
    
    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "Path name '%s' is invalid\n", dnldShared->dnldOrigPathName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);

    free(hostMsg);
    return -EINVAL;   // Bad argument
  }

  if(catType == pathnameInvalidNoSlash || catType == pathnameInvalidSlash)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-pathname '%s' invalid, %s ending '/'\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName,
              catType==pathnameInvalidSlash ? "has" : "missing");

    char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
                thisFile, __LINE__);
      return -ENOMEM;   // No Memory
    }
    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "Path name '%s' is invalid, %s ending '/'\n",
            dnldShared->dnldOrigPathName,
            catType==pathnameInvalidSlash ? "has" : "missing");

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);

    free(hostMsg);
    return -EINVAL;   // Bad argument
  }

  // The number of elements includes all levels.
  if(pathNameElements > HCOM_FILE_DNLD_MAX_NUMB_DIR_ELEMENTS)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-subdirectories limited to %lu, found %lu\n",
              thisFile, __LINE__,
              HCOM_FILE_DNLD_MAX_NUMB_USER_SUBDIRS,
              pathNameElements - HCOM_FILE_DNLD_MANDATORY_DIR_ELEMENTS);

    char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
    if(hostMsg == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
                thisFile, __LINE__);
      return -ENOMEM;   // No Memory
    }

    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "Subdirectories limited to %lu, request contained %lu\n",
            HCOM_FILE_DNLD_MAX_NUMB_USER_SUBDIRS,
            pathNameElements - HCOM_FILE_DNLD_MANDATORY_DIR_ELEMENTS);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);
    free(hostMsg);
    return -EINVAL;
  }

  // Save number of elements in pathname
  dnldShared->dnldPathNameEleCount = pathNameElements;

  // Establish the full path name from the format provided
  if(catType == pathnameOriginal)
  {
    // A file name based on the original naming convention needs
    // to have '/meadow0/' prepended to the filename
    // (e.g. /meadow0/filename.ext).
    dnldFileAndPathLen = strlen(dnldShared->dnldOrigPathName) + \
              strlen(HCOM_MEADOW0_PATH_NAME_PREFIX) + 2; // Room for '/' + NULL

    dnldShared->dnldFullPathName = malloc(dnldFileAndPathLen + 1);
    if(dnldShared->dnldFullPathName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    snprintf_chk(dnldShared->dnldFullPathName, dnldFileAndPathLen, "%s/%s",
                              HCOM_MEADOW0_PATH_NAME_PREFIX,
                              dnldShared->dnldOrigPathName);
  }
  else
  {
    // Since the entire path must have been provide by the host message, we'll
    // allocate the same size buffer as the originally path name. That is one
    // of the following was found: pathnameFullMeadow, pathnameFullSdcard or
    // pathnameSingleSlash.
    dnldShared->dnldFullPathName = malloc(fileNameLength + 1);
    if(dnldShared->dnldFullPathName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    // Copy the name, null and all.
    strcpy(dnldShared->dnldFullPathName, dnldShared->dnldOrigPathName);
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// This public function will take the information from the request and save it
// in the hcom_dnld_shared_t structure.
//
// The 'isFileMsgType' bool determines if the received messages used the
// HcomProtoFileMsg_t or HcomProtoTextMsg_t struct to send the path name.
// The 'endExpectFileName' bool determines if the CLI provided pathname
// information ending in 'dir/filename' or '.../directory/'. Both are legal.
// It depents on the type of request. A list request has no file name, and
// it's last character must be '/'. Where a file download must end in a file
// name, so doesn't end with '/'.
int hcom_host_process_init_hcom_dnld_share(hcom_dnld_shared_t *dnldShared,
          const HcomProtoHdrMsg_t *hdrMsg, const size_t packetSize,
          bool isFileMsgType, bool endExpectFileName)
{
  int ret;
  char *pathName;
  size_t pathNameLength;

  // Clear the entire struct containing all information.
  memset(dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // Start populating the shared download fields
  dnldShared->dnldFilePartId = 0;    // It's always 0

  if(isFileMsgType)
  {
    pathNameLength = packetSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
  }
  else
  {
    pathNameLength = packetSize - HCOM_PROTOCOL_TEXT_MSG_LENGTH;
  }
  
  pathName = malloc(pathNameLength + 1);
  if (pathName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-allocation failed\n",
              thisFile, __LINE__);

    return -ENOMEM;
  }

  if(isFileMsgType)
  {
    HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
    memcpy(pathName, fileMsg->fileInfo.fileName, pathNameLength);
  }
  else
  {
    HcomProtoTextMsg_t *textMsg = (HcomProtoTextMsg_t *)hdrMsg;
    memcpy(pathName, textMsg->textData, pathNameLength);
  }

  pathName[pathNameLength] = '\0';  // Make into C string

  // Construct the proper full file name for 
  // Note: This call should allocate memory, therefore, this must be considered
  // this memory after this point.
  // Note: isFileMsgType is only true based for a few request types. There are only
  // 3 for stm32f7 and for 1 ESP32 ();
  ret = hcom_dir_mgmt_eval_build_pathname(dnldShared, pathName,
            pathNameLength, endExpectFileName);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Eval pathname, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }

  free(pathName);
  return ret;
}


//===========================================================================
// This function will add any missing directories needed to write the file
// being added
int hcom_dir_mgmt_check_and_add_subdir(hcom_dnld_shared_t *dnldShared)
{
  int ret;
  struct stat statBuf;
  struct stat *pStatBuf;
  char *savePtr;
  char *token;
  char *delimiterOffset;
  uint32_t tokenCount;
  int dirLevel;
  int dirOffset[dnldShared->dnldPathNameEleCount];

  pStatBuf = &statBuf;

  // Allocate a modifiable version of the string
  char *fullFileNamePath = malloc(strlen(dnldShared->dnldFullPathName));
  if(fullFileNamePath == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  if(dnldShared->dnldRqstCat == pathnameFullSdcard)
  {
    ret = mount(MEADOW_SDCARD_BLOCK_NAME, MEADOW_SDCARD_MOUNT_POINT_NAME,
              MEADOW_SDCARD_FILE_SYS_TYPE, 0, NULL);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-ERROR: Mount failed. ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return ret;
    }
#if defined (CONFIG_DIR_MGMT_TESTS)
    syslog(2, "%s@%d-mount successful\n",  __FILE__, __LINE__);
#endif
  }

  strcpy(fullFileNamePath, dnldShared->dnldFullPathName);

  token = strtok_r(fullFileNamePath, "/", &savePtr);
  dirOffset[0] = 0;
  tokenCount = 0;

  // This loop will tokenize the directories and filename. We don't really
  // care about the tokens, we want the side-effect of this operation which
  // removes the tokens from the original pathname and replaces them with
  // nulls. This allows us to find the offsets of theses "gaps" and then
  // rebuild the directory path one element at a time in the next step.
  while (token != NULL)
  {
    tokenCount++;
    token = strtok_r(NULL, "/", &savePtr);
    dirOffset[tokenCount] = (token - fullFileNamePath);
  }

  // fullFileNamePath now contains all the subdirectory elements as NULL
  // separated C strings.
  // This loop will replace 1 NULL with a '/' on each pass to eventually
  // reconstruct the entire path. As each element is added, stat() will
  // determine if this element exist. if not it will be created.
  // The number of directories is 1 less than the number of elements, so we -1.
  for(dirLevel = 0; dirLevel < dnldShared->dnldPathNameEleCount - 1; dirLevel++)
  {
    delimiterOffset = fullFileNamePath + dirOffset[dirLevel];

    if(dirLevel != 0)
    {
      // Restore the token ()'/') for next element
      *(delimiterOffset - 1) = '/';
    }

    ret = stat(fullFileNamePath, pStatBuf);
    if(ret < 0)
    {
      // No Entry?
      if(errno == ENOENT)
      {
        // Make the missing directory
        // 0644 owner has read and write permission, group and others read
        // 0777 everyone has read write and execute permission.
        ret = mkdir(fullFileNamePath, 0777);
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-mkdir of '%s' failed with, ret:%d, errno:%d\n",
                    thisFile, __LINE__, fullFileNamePath, ret, errno);

          char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
          if(hostMsg == NULL)
          {
            hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
                      thisFile, __LINE__);
            free(fullFileNamePath);
            return -ENOMEM;
          }

          snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
                  "Create directory '%s' failed, ret:%d, errno:%d\n",
                  fullFileNamePath, ret, errno);
          hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                  hostMsg, thisFile, __LINE__);
          free(hostMsg);
          free(fullFileNamePath);
          if(dnldShared->dnldRqstCat == pathnameFullSdcard)
          {
            ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
            if(ret < 0)
            {
              hcom_logging_syslog(LOG_ERR, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n",
                        thisFile, __LINE__, ret, errno);
              return ret;
            }
#if defined (CONFIG_DIR_MGMT_TESTS)
            syslog(2, "%s@%d-umount successful\n",  __FILE__, __LINE__);
#endif
          }

          return ret;
        }
   
        // Don't move to next directory, re-evaluate this new one, then move
        // forward.
        dirLevel--;
        continue;
      }
      else
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Error from stat() call, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      }

      if (! S_ISDIR(pStatBuf->st_mode))
      {
        // All entries must be a directory, if not, it's an error
        char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
        if(hostMsg == NULL)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n",
                    thisFile, __LINE__);
          free(hostMsg);
          free(fullFileNamePath);
          if(dnldShared->dnldRqstCat == pathnameFullSdcard)
          {
            ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
            if(ret < 0)
            {
              hcom_logging_syslog(LOG_ERR, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n",
                        thisFile, __LINE__, ret, errno);
              return ret;
            }
#if defined (CONFIG_DIR_MGMT_TESTS)
            syslog(2, "%s@%d-umount successful\n",  __FILE__, __LINE__);
#endif
          }
          return -ENOMEM;
        }

        snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
                "Pathname '%s' is invalid, last element not a directory\n",
                 fullFileNamePath);
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                hostMsg, thisFile, __LINE__);
        free(hostMsg);
        free(fullFileNamePath);
        return -ENOTDIR;
      }

      if(dnldShared->dnldRqstCat == pathnameFullSdcard)
      {
        ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
          return ret;
        }

#if defined (CONFIG_DIR_MGMT_TESTS)
        syslog(2, "%s@%d-umount successful\n",  __FILE__, __LINE__);
#endif
      }

      free(fullFileNamePath);
      return ret;
    }
#if defined (CONFIG_DIR_MGMT_TESTS)
    else
    {
      syslog(2, "stat() found at '%s':\n", fullFileNamePath);
      if (S_ISREG(statBuf.st_mode))
        syslog(2, "type        : File\n");
      else if (S_ISDIR(statBuf.st_mode))
        syslog(2, "type        : Directory\n");
      else if (S_ISCHR(statBuf.st_mode))
        syslog(2, "type        : Character driver\n");
      else if (S_ISBLK(statBuf.st_mode))
        syslog(2, "type        : Block driver\n");
      else if (S_ISMQ(statBuf.st_mode))
        syslog(2, "type        : Message queue\n");
      else if (S_ISSEM(statBuf.st_mode))
        syslog(2, "type        : Named semaphore\n");
      else if (S_ISSHM(statBuf.st_mode))
        syslog(2, "type        : Shared memory\n");
      else if (S_ISSOCK(statBuf.st_mode))
        syslog(2, "type        : Socket\n");
      else if (S_ISMTD(statBuf.st_mode))
        syslog(2, "type        : Named MTD driver\n");
      else if (S_ISLNK(statBuf.st_mode))
        syslog(2, "type        : Symbolic link\n");
      else
        syslog(2, "type        : Unknown\n");
    }
#endif
  }

  if(dnldShared->dnldRqstCat == pathnameFullSdcard)
  {
    ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      free(fullFileNamePath);
      return ret;
    }
#if defined (CONFIG_DIR_MGMT_TESTS)
    syslog(2, "%s@%d-umount successful\n",  __FILE__, __LINE__);
#endif
  }

  free(fullFileNamePath);
  return OK;
}
