/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dir_mgmt_utils.c
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
#include <syslog.h>

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) dir_mgmt_tests.c"
#endif

#pragma GCC optimize("O0")    // Prevent compiler from changing the code

#define HCOM_FILE_DIR_OUTPUT_TO_SYSLOG (1)

// The following deal with subdirectory support
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR   ("/meadow0/")
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN   (9)
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR   ("/mmcsd0/")
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN   (8)

enum hcom_file_dir_mgmt_type_e
{
  fnameInvalid = 100,     // Illegal format provided
  fnameOriginal = 101,    // No '/' found
  fnameMeadowFull = 102,  // Starts '/meadow0/'
  fnameMmcsdFull = 103    // Starts '/mmcsd0/'
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static hcom_dnld_shared_t *_dnldShared;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_file_dir_mgmt_read_nested_directories(const char *rootDir,
          char *hostMsg, struct dirent *entry, int indent);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//============================================================================
// This function will return the depth of subdirectories in the file name.
static uint32_t find_filename_subdir_depth(const char *fileName, size_t strLen)
{
  // Allocate a modifiable version of the string for tokenizing
  char *fileNameTemp = malloc(strLen + 1);
  strcpy(fileNameTemp, fileName);

  // Count the number of '/' characters to give an indication of the subdir
  // depth
  uint32_t tokenCount = 0;
  char *savePtr;
  char *token = strtok_r(fileNameTemp, "/", &savePtr);

  while (token != NULL)
  {
    tokenCount++;
    // syslog(1, "==>> token: '%s', tokenCount:\n", token, tokenCount);
    token = strtok_r(NULL, "/", &savePtr);
  }

  free(fileNameTemp);

  // The number of elements is 1 more than the number of '/' characters
  return tokenCount + 1;
}

//============================================================================
// This function will check the received filename and categorize it so the
// remaining steps will know what they are dealing with
static int hcom_file_dir_mgmt_categorize_filename(const char *fileName,
          size_t strLen, uint32_t *fileNameElements)
{
  *fileNameElements = 0;
  
  // Valid and invalid file names
  // 'filename', '/meadow0/filename', '/meadow0/dir1/dir2/filename'
  // These are illegal formats:
  // '/filename' - has leading '/'
  // /dirname/filename/ - missing leading '/meadow0'

  // Is this a bare filename (i.e. no '/')
  if(memchr(fileName, '/', strLen) == NULL)
  {
    // No '/' in file name, this is like original naming scheme
    return fnameOriginal;    // 101
  }
  else if (strLen < MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN)
  {
    // 100
    return fnameInvalid;          // Too short 
  }
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR,
              fileName, MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN) == 0)
  {
    // '/meadow0/' found, but can't end in '/', must have file name
    if(fileName[strLen-1] == '/')
        return fnameInvalid;
    
    *fileNameElements = find_filename_subdir_depth(fileName, strLen);

    return fnameMeadowFull;    // 102
  }
  // (--) MUST TEST IF SD-Card ENABLED
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR,
              fileName, MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN) == 0)
  {
    // '/mmcsd0/' found, but can't end in '/', must have file name
    if(fileName[strLen-1] == '/')
        return fnameInvalid;

    *fileNameElements = find_filename_subdir_depth(fileName, strLen);

    return fnameMmcsdFull;    // 103
  }
  else
  {
    return fnameInvalid;    // 100
  }
}

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// File name processing
int hcom_file_dir_mgmt_build_pathname_save(hcom_dnld_shared_t *dnldShared,
          HcomProtoFileMsg_t *fileMsg, size_t fileNameLength)
{
  int catType;
  uint32_t fileNameElements;
  size_t dnldFileAndPathLen;
  
  _dnldShared = dnldShared;

    // Allocated + space for '/0'
  dnldShared->dnldOrigFileName = malloc(fileNameLength + 1);
  if(dnldShared->dnldOrigFileName == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Continue to populate shared download struct with file name information
  memcpy(dnldShared->dnldOrigFileName, fileMsg->fileInfo.fileName,
            fileNameLength);
  // Make into C string
  dnldShared->dnldOrigFileName[fileNameLength] = '\0';

  // There are 3 valid file name formats.
  // 1. A simple file name, with just a file name and nothing else.
  // 2. A file beginning with '/meadow0/'
  // 3. A file beginning with '/mmcsd0/'
  // This call will catergorize as one of the above or error. In the case of
  // a file within subdirectories, it provides the number of subdirectories.
  // This is used to further catergorize the request. 
  catType = hcom_file_dir_mgmt_categorize_filename(dnldShared->dnldOrigFileName,
            fileNameLength, &fileNameElements);
  dnldShared->dnldFNameEleCount = fileNameElements;
  
// (--) SOME DIAGNOSTIC CODE
  char diagFNameType[32];
  switch (catType)
  {
  case fnameInvalid:
    strcpy(diagFNameType, "fnameInvalid - bad filename");
    break;
  case fnameOriginal:
    dnldShared->dnldIsRootMeadow0 = true;
    strcpy(diagFNameType, "fnameOriginal-no '/'");
    break;
  case fnameMeadowFull:
    dnldShared->dnldIsRootMeadow0 = true;
    strcpy(diagFNameType, "fnameMeadowFull ('/meadow0/')");
    break;
  case fnameMmcsdFull:
    dnldShared->dnldIsRootMeadow0 = false;
    strcpy(diagFNameType, "fnameMmcsdFull ('/mmcsd0/')");
    break;
  default:
    strcpy(diagFNameType, "default?");
    break;
  }

  syslog(1, "===> Valid format, file '%s'. It is categorized as %d (%s), fileNameElements:%lu\n",
            dnldShared->dnldOrigFileName, catType, diagFNameType, fileNameElements);
// (--) SOME DIAGNOSTIC CODE

  if(catType == fnameInvalid)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-file name '%s' is invalid\n",
              thisFile, __LINE__, dnldShared->dnldOrigFileName);

    // (--) Need to send a host message here
    return -EINVAL;   // Bad argument
  }

  // Save sub-directory depth for whoever may want it
  dnldShared->dnldFNameEleCount = fileNameElements;

  // A file name based on the original naming convention needs
  // to have '/meadow0/' prepended to the filename so it can be used.
  if(catType == fnameOriginal)
  {
    // Build the full path plus file name string (e.g. /meadow0/FileName.ext)
    dnldFileAndPathLen = strlen(dnldShared->dnldOrigFileName) + \
              strlen(HCOM_FILE_MOUNT_POINT_TARGET) + 3; // Room for '/', partition Id, NULL

    dnldShared->dnldFileAndPathName = malloc(dnldFileAndPathLen + 1);
    if(dnldShared->dnldFileAndPathName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

#ifdef CONFIG_MTD_PARTITION
    snprintf_chk(dnldShared->dnldFileAndPathName, dnldFileAndPathLen, "%s%d/%s",
                              HCOM_FILE_MOUNT_POINT_TARGET,
                              dnldShared->dnldFilePartId,
                              dnldShared->dnldOrigFileName);
#else
    snprintf_chk(dnldShared->dnldFileAndPathName, dnldFileAndPathLen, "%s/%s",
                              HCOM_FILE_MOUNT_POINT_TARGET,
                              dnldShared->dnldOrigFileName);
#endif
  }
  else
  {
    // (--) CHANCE TO REFACTOR??? SINCE THE SAME NAME IS IN 2 PLACES IN STRUCT
    // Since the entire path must be provide by the host message, we'll
    // allocate the same size buffer as originally provided for the full name.
    // This should take care of both '/meadow0/' and '/mmcsd0/' files.
    dnldShared->dnldFileAndPathName = malloc(fileNameLength + 1);
    if(dnldShared->dnldFileAndPathName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    // Just copy the name, null and all.
    strcpy(dnldShared->dnldFileAndPathName, dnldShared->dnldOrigFileName);
  }

  syslog(1, "===> %s@%d-Full download file name:'%s' with %lu subdirectories\n",
            __FILE__, __LINE__, dnldShared->dnldFileAndPathName, fileNameElements);
  usleep(20 * 1000);

  return OK;
}

//===========================================================================
// This function will add any missing directories needed to write the file
// being added
int hcom_file_dir_mgmt_check_and_add_subdir(hcom_dnld_shared_t *dnldShared)
{
  int ret;
  struct stat statBuf;
  struct stat *pStatBuf;
  char *savePtr;
  char *token;
  char *delimiterOffset;
  uint32_t tokenCount;
  int dirLevel;
  int dirOffset[dnldShared->dnldFNameEleCount];

  pStatBuf = &statBuf;

  // Allocate a modifiable version of the string
  char *fullFileNamePath = malloc(strlen(dnldShared->dnldFileAndPathName));
  strcpy(fullFileNamePath, dnldShared->dnldFileAndPathName);

  // syslog(1, "-->> %s@%d-Starting path:'%s', total depth:%lu\n",
  //           thisFile, __LINE__, fullFileNamePath, dnldShared->dnldFNameEleCount);
  // usleep(50 * 1000);

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

  // This loop will modify fullFileNamePath, adding 1 element on each pass
  for(dirLevel = 0; dirLevel < dnldShared->dnldFNameEleCount - 1; dirLevel++)
  {
    delimiterOffset = fullFileNamePath + dirOffset[dirLevel];

    if(dirLevel != 0)
    {
      // Restore the token ()'/') for next element
      *(delimiterOffset - 1) = '/';

      // syslog(1, "-->> %s@%d-Full Name:'%s', count:%d, delimiterOffset:'%s'\n",
      //     thisFile, __LINE__, fullFileNamePath, dirLevel, delimiterOffset - 1);
      // usleep(50 * 1000);
    }
    // else
    // {
    //   // At the start there's no delimiter to restore. If this level doesn't exist
    //   // we are in trouble.
    //   syslog(LOG_ERR, "-->> %s@%d-ERROR: Beginning path doesn't exist! (Full Name:'%s', count:%d, delimiterOffset:'%s')\n",
    //       thisFile, __LINE__, fullFileNamePath, dirLevel, delimiterOffset);
    //   usleep(50 * 1000);
    // }

    ret = stat(fullFileNamePath, pStatBuf);
    if(ret < 0)
    {
      if(errno == ENOENT)
      {
        // syslog(1, "-->> %s@%d-File doesn't exist, (errno:%d), so, we'll create '%s\n",
        //           thisFile, __LINE__, errno, fullFileNamePath);

        // Make the missing directory
        ret = mkdir(fullFileNamePath, 0777);
        if(ret < 0)
        {
          syslog(LOG_ERR, "%s@%d-mkdir of '%s' failed with, ret:%d, errno:%d\n",
                    thisFile, __LINE__, fullFileNamePath, ret, errno);
          return ret;
        }
        syslog(1, "%s@%d-Directory created\n", thisFile, __LINE__);

        // Don't move to next directory, evaluate this new one, the move on.
        dirLevel--;
        continue;   // Try again now that the directory exists
      }
      else
      {
        syslog(1, "%s@%d-Some error, errno:%d\n", thisFile, __LINE__, errno);
      }
      return ret;
    }

    syslog(1, "-->> %s@%d-stat() returned NO error.\n", thisFile, __LINE__);
    usleep(20 * 1000);

    if (S_ISREG(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : File\n");
      }
    else if (S_ISDIR(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Directory\n");
      }
    else if (S_ISCHR(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Character driver\n");
      }
    else if (S_ISBLK(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Block driver\n");
      }
    else if (S_ISMQ(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Message queue\n");
      }
    else if (S_ISSEM(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Named semaphore\n");
      }
    else if (S_ISSHM(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Shared memory\n");
      }
    else if (S_ISSOCK(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Socket\n");
      }
    else if (S_ISMTD(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Named MTD driver\n");
      }
    else if (S_ISLNK(pStatBuf->st_mode))
      {
        syslog(1, "\ttype        : Symbolic link\n");
      }
    else
      {
        syslog(1, "\ttype        : Unknown\n");
      }

    syslog(1, "\tsize        : %d (bytes)\n",  pStatBuf->st_size);
    syslog(1, "\tblock size  : %d (bytes)\n",  pStatBuf->st_blksize);
    syslog(1, "\tsize        : %d (blocks)\n", pStatBuf->st_blocks);
    syslog(1, "\taccess time : %d\n", pStatBuf->st_atime);
    syslog(1, "\tmodify time : %d\n", pStatBuf->st_mtime);
    syslog(1, "\tchange time : %d\n", pStatBuf->st_ctime);
  }

  free(fullFileNamePath);

  syslog(1, "--=>> ---TESTING FINISHED---\n");
  return OK;
}

//===========================================================================
// (--) REPLACE RECURSIVE WITH NON_RECURSIVE VERSION!!
// (--) KEEPING TILL NEW VERSION AVAILABLE SINCE THIS ONE WORKS
//===========================================================================
// Note: Use "/" as the rootDir to show all files and directories etc.
int hcom_file_dir_mgmt_read_nested_directories_start(const char *rootDir)
{
  // Keep these larger objects off the stack of the recursive function
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  struct dirent *entry = NULL;

  return hcom_file_dir_mgmt_read_nested_directories(rootDir, hostMsg, entry, 0);
}

//--------------------------------------------------------------------------
// NOTE - Recursive function, is not public
int hcom_file_dir_mgmt_read_nested_directories(const char *rootDir,
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

      snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s/\n", indent, "", entry->d_name);
              
// #if HCOM_FILE_DIR_OUTPUT_TO_SYSLOG > 0
      syslog(2, hostMsg);
// #else
//      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
//                 0, hostMsg, thisFile, __LINE__);
// #endif
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];   // MAX_PATH
      snprintf_chk(path, sizeof(path), "%s/%s", rootDir, entry->d_name);

      // Recursion is here since directory
      hcom_file_dir_mgmt_read_nested_directories(path, hostMsg, entry, indent + 1);
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
// #if HCOM_FILE_DIR_OUTPUT_TO_SYSLOG > 0
      syslog(2, hostMsg);
// #else
//       hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
//                 0, hostMsg, thisFile, __LINE__);
// #endif
    }
#endif

  }

  closedir(dir);
  return OK;
}

