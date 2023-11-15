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
// The following rules will be followed
// 1. All reads or writes sent with just a bare file name will default to using /meadow0/.
//    The intent is to support the existing CLIv1 behavior, without changes.
// 2. Files prepended with a single '/' (e.g. /filename) will considered an error.
// 3. Files downloaded to subdirectory must be in this format '/meadow0/dir/filename'.
// 4. For writing files, if the directory or directories don't exist, they will be created.
// 5. Reads from a non-existing directory will return an error.
// 6. There is a nesting limit for directories of 6, not counting /meadow0.
// 7. When a file is delete all lower, empty directories will be deleted.
// 8. All file writes or reads for the SD-Card must begin with /mmcsd0/.

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

// 8 elements in path will allow up to 6 subdirectories
#define HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS (8)

// The following deal with testing subdirectory support
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR   ("/meadow0/")
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN   (9)
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR   ("/mmcsd0/")
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN   (8)

// This enum is used to classify CLI requests
enum hcom_dir_mgmt_msg_type_e
{
  pathnameInvalid     = 0,    // Illegal format provided
  pathnameOriginal    = 1,    // No '/' found
  pathnameFullMeadow  = 2,    // Starts '/meadow0/'
  pathnameFullMmcsd   = 3     // Starts '/mmcsd0/'
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static hcom_dnld_shared_t *_dnldShared;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_dir_mgmt_read_nested_directories(const char *rootDir,
          char *hostMsg, struct dirent *entry, int indent);

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
// what they are dealing with. Valid and invalid file names are:
// 'filename', '/meadow0/filename', '/meadow0/dir1/dir2/filename'
// These are illegal formats:
// '/filename' - has leading '/'
// /dirname/filename/ - missing leading '/meadow0'
// endExpectFileName: true = pathname no ending '/', false = ending '/' needed
static int hcom_dir_mgmt_categorize_pathname(const char *pathName,
          size_t strLen, uint32_t *pathNameElements,
          bool endExpectFileName)
{
  *pathNameElements = 0;

  // Is this a bare filename (i.e. no '/')
  if(memchr(pathName, '/', strLen) == NULL)
  {
    // No '/' in file name, this is like original file naming scheme for
    // download
    if(endExpectFileName)
      *pathNameElements = 2;  // Includes /meadow0 to be added soon
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
          return pathnameInvalid;
    }
    else
    {
      if(pathName[strLen-1] != '/')
          return pathnameInvalid;
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
      // '/mmcsd0/' found, but can't end in '/', must have file name, unless this
      // being called for a file list, in which case it must end in '/'.
      if(endExpectFileName)
      {
        if(pathName[strLen-1] == '/')
            return pathnameInvalid;
      }
      else
      {
        if(pathName[strLen-1] != '/')
            return pathnameInvalid;
      }

      *pathNameElements = find_pathname_element_count(pathName, strLen);
      return pathnameFullMmcsd;
    }
  }
#endif
  return pathnameInvalid;
}

//=====================================================================
// File name processing. This is the entry point for all downloads, deletes
// etc.
static int hcom_dir_mgmt_eval_build_pathname(hcom_dnld_shared_t *dnldShared,
          char *pathNameStr, size_t fileNameLength,
          bool endExpectFileName)
{
  int catType;
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

  // There are 3 valid file name formats.
  // 1. A simple file name, with just a file name and nothing else.
  // 2. A file beginning with '/meadow0/'
  // 3. A file beginning with '/mmcsd0/'
  // This call will catergorize as one of the above or error. In the case of
  // a file within subdirectories, it will be considered a subdirectory as
  // there's no way via text to tell the difference. It must be assumed that
  // the final '/' signifies the start of the file name.
  // This is used to further catergorize the request.
  catType = hcom_dir_mgmt_categorize_pathname(dnldShared->dnldOrigPathName,
            fileNameLength, &pathNameElements, endExpectFileName);
  if(catType == pathnameInvalid)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-pathname '%s' is invalid\n",
              thisFile, __LINE__, dnldShared->dnldOrigPathName);

    char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "Path name '%s' is invalid\n", dnldShared->dnldOrigPathName);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);
    free(hostMsg);

    return -EINVAL;   // Bad argument
  }

  // The number of elements includes all levels.
  if(pathNameElements > HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-subdirectories: max is %lu, found %lu\n",
              thisFile, __LINE__, HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS, pathNameElements - 2);

    char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
    snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
            "Path Names are limited to a maximum:%lu elements, requested:%lu\n",
            HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS, pathNameElements);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);
    free(hostMsg);

    return -EINVAL;
  }

  // Save number of elements in pathname
  dnldShared->dnldPathNameEleCount = pathNameElements;

  // Establish the full path name if original format provided.
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
    // Since the entire path must be provide by the host message, we'll
    // allocate the same size buffer as the originally string provided for the
    // full path name.
    dnldShared->dnldFullPathName = malloc(fileNameLength + 1);
    if(dnldShared->dnldFullPathName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    // Just copy the name, null and all.
    strcpy(dnldShared->dnldFullPathName, dnldShared->dnldOrigPathName);
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// This public function will take the information from the request and save it
// in the hcom_dnld_shared_t structure.
// The isFileMsgType determines if the received messages is based on using
// HcomProtoFileMsg_t or HcomProtoTextMsg_t.
// The endExpectFileName is if the CLI provided pathname information end with a
//   'dir/filename' or '.../directory/'
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
    hcom_logging_syslog(LOG_ERR, "%s@%d-Eval pathname, errno:%d, ret:%d\n",
              thisFile, __LINE__, errno, ret);
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
  strcpy(fullFileNamePath, dnldShared->dnldFullPathName);

  token = strtok_r(fullFileNamePath, "/", &savePtr);
  dirOffset[0] = 0;
  tokenCount = 0;

  // This loop will tokenize the directories and filename. We don't really
  // care about the tokens, we want the side-effect of this operation which
  // removes the tokens from the original pathname and replaces them with
  // nulls. This allows us to find the offsets of theses "gaps" and then
  // rebuild the directory path one element at a time.
  while (token != NULL)
  {
    tokenCount++;
    token = strtok_r(NULL, "/", &savePtr);
    dirOffset[tokenCount] = (token - fullFileNamePath);
  }

  // fullFileNamePath now contains all the subdirectory elements as C strings.
  // This loop will add 1 '/' element on each pass to eventually reconstruct
  // the entire path. As each element is added, stat() will determine if this
  // element exist. if not it will be created.
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
          syslog(LOG_ERR, "%s@%d-mkdir of '%s' failed with, ret:%d, errno:%d\n",
                    thisFile, __LINE__, fullFileNamePath, ret, errno);

          char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
          snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
                  "Create directory '%s' failed, ret:%d, errno:%d\n",
                  fullFileNamePath, ret, errno);
          hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                  hostMsg, thisFile, __LINE__);
          free(hostMsg);
          free(fullFileNamePath);

          return ret;
        }
   
        // Don't move to next directory, re-evaluate this new one, then move
        // forward.
        dirLevel--;
        continue;
      }
      else
      {
        syslog(LOG_ERR, "%s@%d-Error from stat() call, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      }

      if (! S_ISDIR(pStatBuf->st_mode))
      {
        // All entries must be a directory, if not, it's an error
        char *hostMsg = malloc(HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH);
        snprintf_chk(hostMsg, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
                "Pathname '%s' is invalid, last element not a directory\n",
                 fullFileNamePath);
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                hostMsg, thisFile, __LINE__);
        free(hostMsg);
        free(fullFileNamePath);
        return -ENOTDIR;
      }

      return ret;
    }
  }

  free(fullFileNamePath);

  return OK;
}

//===================================================================
// This function prints the path as a header and all the file found in it
static void hcom_dir_mgmt_print_directory_files(DIR *dir[],
          int dirLevel, char *currentPath)
{
  struct dirent *entry;
  bool filesFound = false;
  int fileCount = 0;
  
  while ((entry = readdir(dir[dirLevel])) != NULL)
  {
    if (DIRENT_ISFILE(entry->d_type))
    {
      // Found a file
      if(!filesFound)
      {
        // Print the directory path as a header
        syslog(2, "\n");
        syslog(2, "Directory:%s\n", currentPath);
        filesFound = true;
      }

      // Print file information
      syslog(2, "  %s\n", entry->d_name);
      fileCount++;
    }
  }

  // Line if we printed files
  if(filesFound)
  {
    syslog(2, "----------%d File(s) (L:%d)----------\n",
              fileCount, dirLevel);
  }
#if 1
  else
  {
    syslog(2, "Directory:%s [Empty]\n", currentPath);
  }
#endif

  // Return to the start of directory and look for deeper directories
  rewinddir(dir[dirLevel]);
}

//===================================================================
// This function modifies the currentPath to follow the structure of the
// file system. Once begun this function only call code to print the directory
// and file information
static int hcom_dir_mgmt_find_next_directory(DIR *dir[],
          int dirLevel, char *currentPath)
{
  struct dirent *entry;
  int dirStartLvl = dirLevel;

  // Initialize the following loop
  dir[dirLevel] = opendir(currentPath);
  if(dir[dirLevel] == NULL)
  {
    syslog(LOG_ERR, "%s@%d-opendir failed, errno:%d, Path:'%s'\n",
              thisFile, __LINE__, errno, currentPath);
    usleep(20 * 1000);
    return -errno;
  }

  // Print files from starting directory
  hcom_dir_mgmt_print_directory_files(dir, dirLevel, currentPath);

  while(true)
  {
    // Search for directories in currentPath, which may change multiple times
    // while this loop executes, but always deeper in to subdirectories. On
    // leaving this loop (no more directories) we will move the directory up
    // 1 level.
    while ((entry = readdir(dir[dirLevel])) != NULL)
    {
      if (DIRENT_ISDIRECTORY(entry->d_type))
      {
        // Ignore these directories, we only care about named directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        // Update the currentPath to include the just found child directory
        strcat(currentPath, "/");
        strcat(currentPath, entry->d_name);

        dirLevel++;
        if(dirLevel > HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS - 1)
        {
          syslog(LOG_ERR, "%s@%d-Dir level too deep, Path:'%s', Level:%d\n",
                    thisFile, __LINE__, currentPath, dirLevel);
          return -ETOOMANYREFS;
        }

        // Open deeper child directory
        dir[dirLevel] = opendir(currentPath);
        if(dir[dirLevel] == NULL)
        {
          syslog(LOG_ERR, "%s@%d-opendir failed, errno:%d, Path:'%s'\n",
                    thisFile, __LINE__, errno, currentPath);
          usleep(20 * 1000);
          return -errno;
        }

        // Opens the directory at currentPath and prints all the files found
        // there (if there are any)
        hcom_dir_mgmt_print_directory_files(dir, dirLevel, currentPath);
      }
    }

    // We reached the bottom of this directory branch
    closedir(dir[dirLevel]);

    // Remove child subdirectory from currentPath
    char *lastShash = strrchr(currentPath, '/');
    *lastShash = '\0';

    dirLevel--;      // Back up a parent directory level

    // Is the level now below our starting level (finished?)
    if((dirStartLvl - 1) == dirLevel)
    {
      break;      // Break out of loop as we are done
    }
  }

  return OK;
}

//===================================================================
// Public
int hcom_dir_mgmt_print_files_and_directories(const char *initialDir)
{
  int ret;
  int dirLevel = 0;
  DIR *dir[HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS];
  
  if(initialDir == NULL || strlen(initialDir) < 1 || initialDir[0] != '/')
  {
    syslog(LOG_ERR, "%s@%d-InitialDir was invalid\n", thisFile, __LINE__);
    return -EINVAL;
  }

  // Room for nesting of directories. This memory is used by called
  // functions which add to and remove from the path.
  char *currentPath = malloc(512);
  strcpy(currentPath, initialDir);

  ret = hcom_dir_mgmt_find_next_directory(dir, dirLevel, currentPath);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Listing files failed, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    usleep(20 * 1000);
    return -errno;
  }

  free(currentPath);

  return OK;
}

//===========================================================================
// THIS RECURSIVE CODE IS ONLY FOR DIAGNOSTICS! KEEPING SINCE IT WORKS.
// Call using 'meadow set developer -d 13 -v n'
//===========================================================================
// Note: Using "/" as the rootDir would show all files and directories etc.
int hcom_dir_mgmt_read_nested_directories_start(const char *rootDir)
{
  // Keep these larger objects off the stack of the recursive function
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  struct dirent *entry = NULL;

  return hcom_dir_mgmt_read_nested_directories(rootDir, hostMsg, entry, 0);
}

//--------------------------------------------------------------------------
// NOTE - Recursive function, is not public
int hcom_dir_mgmt_read_nested_directories(const char *rootDir,
            char *hostMsg, struct dirent *entry, int indent)
{
  DIR *dir;

  if (!(dir = opendir(rootDir)))
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Could not open:%s as root directory, errno:%d\n",
              thisFile, __LINE__, rootDir, errno);
    return -errno;
  }

  while ((entry = readdir(dir)) != NULL)
  {
    if (DIRENT_ISDIRECTORY(entry->d_type))
    {
      if(strcmp(entry->d_name, "proc") == 0)
        return OK; // ignore procfs information
              
      syslog(2, "%*s%s/\n", indent, "", entry->d_name);

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];   // MAX_PATH
      snprintf_chk(path, sizeof(path), "%s/%s", rootDir, entry->d_name);

      // Recursion is here since directory
      hcom_dir_mgmt_read_nested_directories(path, hostMsg, entry, indent + 1);
    }
#if 1     // Show everything not just directories
    else
    {
      // All non-directory types
      char *entryType;
      if(DIRENT_ISFILE(entry->d_type)) {entryType = "file";}
      // else if(DIRENT_ISCHR(entry->d_type)) {entryType = "char";}
      // else if(DIRENT_ISBLK(entry->d_type)) {entryType = "block";}
      // else if(DIRENT_ISLINK(entry->d_type)) {entryType = "link";}
      // else {entryType = "????";}

      syslog(2, "%*s%s [%s]\n", indent, "", entry->d_name, entryType);

    }
#endif

  }
  syslog(2, "\n");

  closedir(dir);
  return OK;
}

