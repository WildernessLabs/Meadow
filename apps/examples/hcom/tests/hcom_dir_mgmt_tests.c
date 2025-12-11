/****************************************************************************
 * /apps/examples/hcom/tests/hcom_dir_mgmt_tests.c
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

// This module contains the code needed to test Meadow_Issues
// #320 Add HCOM support for “current directory” 

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_dnld_shared.h>
#include <meadow/meadow_apps_core_share.h>
#include <dirent.h>

#if defined (CONFIG_DIR_MGMT_TESTS) || defined (CONFIG_ALL_MEADOW_TESTS)
#pragma message "(--) dir_mgmt_tests.c"

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/dirent.h>

#define HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS (0)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int hcom_dir_mgmt_tst_log_directories_files(const char *initialDir);

//===================================================================
// Debug code. If needed use this function with the following debug code
static off_t hcom_dir_mgmt_find_file_status(char *path)
{
  int ret;
  struct stat statBuf;
  ret = stat(path, &statBuf);
  if (ret < 0)
  {
    syslog(LOG_MTEST, "stat call error. ret:%d, errno:%d\n", ret, errno);
    return -1;
  }

#if HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS > 0
  syslog(LOG_MTEST, "Reported by stat:\n");

  if (S_ISREG(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : File\n");
  else if (S_ISDIR(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Directory\n");
  else if (S_ISCHR(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Character driver\n");
  else if (S_ISBLK(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Block driver\n");
  else if (S_ISMQ(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Message queue\n");
  else if (S_ISSEM(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Named semaphore\n");
  else if (S_ISSHM(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Shared memory\n");
  else if (S_ISSOCK(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Socket\n");
  else if (S_ISMTD(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Named MTD driver\n");
  else if (S_ISLNK(statBuf.st_mode))
    syslog(LOG_MTEST, "type        : Symbolic link\n");
  else
    syslog(LOG_MTEST, "type        : Unknown\n");

  syslog(LOG_MTEST, "file size   : %d (bytes)\n",  statBuf.st_size);
  syslog(LOG_MTEST, "block size  : %d (bytes)\n",  statBuf.st_blksize);
  syslog(LOG_MTEST, "size        : %d (blocks)\n", statBuf.st_blocks);
  syslog(LOG_MTEST, "access time : %d\n",          statBuf.st_atime);
  syslog(LOG_MTEST, "modify time : %d\n",          statBuf.st_mtime);
  syslog(LOG_MTEST, "change time : %d\n",          statBuf.st_ctime);
#endif

#if HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS > 0
  struct statfs buf;
  ret = statfs(path, &buf);
  if (ret < 0)
  {
    syslog(LOG_MTEST, "ERROR statfs(%s) failed with errno=%d\n", path, errno);
    return -1;
  }
  else
  {
    // LITTLEFS_SUPER_MAGIC = 0x0a732923
    syslog(LOG_MTEST, "Reported by statfs:\n");
    syslog(LOG_MTEST, "FS Type           : %0x\n", buf.f_type);
    syslog(LOG_MTEST, "Block size        : %d\n",  buf.f_bsize);
    syslog(LOG_MTEST, "Number of blocks  : %d\n",  buf.f_blocks);
    syslog(LOG_MTEST, "Free blocks       : %d\n",  buf.f_bfree);
    syslog(LOG_MTEST, "Free user blocks  : %d\n",  buf.f_bavail);
    syslog(LOG_MTEST, "Number file nodes : %d\n",  buf.f_files);
    syslog(LOG_MTEST, "Free file nodes   : %d\n",  buf.f_ffree);
    syslog(LOG_MTEST, "File name length  : %d\n",  buf.f_namelen);
  }
#endif

  return statBuf.st_size;
}

//===================================================================
// This function prints the path as a directory header and all the file found
// in that directory.
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
        // Skip a line and print this directory path as a header before the
        // first file is printed
        syslog(LOG_MTEST, "\n");
        syslog(LOG_MTEST, "Directory:%s\n", currentPath);
        filesFound = true;
      }

      // Print file name with file size
      // At this point currentPath doesn't contain the file name
      char * fullFileName = malloc(512);
      size_t cPathLen = strlen(currentPath);
      memcpy(fullFileName, currentPath, cPathLen);
      fullFileName[cPathLen] = '\0';
      strcat(fullFileName, "/");
      strcat(fullFileName, entry->d_name);
      off_t fsize = hcom_dir_mgmt_find_file_status(fullFileName);
      free(fullFileName);

      syslog(LOG_MTEST, "  %s [%ld bytes]\n", entry->d_name, fsize);
      fileCount++;
    }
  }

  // Line if we printed files
  if(filesFound)
  {
    syslog(LOG_MTEST, "----------%d Total File(s) (Level:%d)----------\n",
              fileCount, dirLevel);
  }
  else
  {
    syslog(LOG_MTEST, "No Files:%s\n", currentPath);
  }

  // Return to the start of directory and look for deeper directories
  rewinddir(dir[dirLevel]);
}

//===================================================================
// This function modifies the currentPath to follow the structure of the
// file system. Once begun this function only calls to print the directory
// and file information.
static int hcom_dir_mgmt_find_next_directory(const char *initialDir)
{
  int dirLevel = 0;
  int dirEndLevel;
  DIR *dir[HCOM_FILE_DNLD_MAX_NUMB_DIR_ELEMENTS];
  struct dirent *entry;
  char *currentPath = malloc(1024);  // malloc size is a guess
  bool showAll = false;

  // Single '/'? If so, show all the data and types.
  if(strlen(initialDir) == 1)
  {
    showAll = true;
  }

  dirEndLevel = dirLevel;
  strcpy(currentPath, initialDir);

  // Open the initial directory entry and place it in the array
  dir[dirLevel] = opendir(currentPath);
  if(dir[dirLevel] == NULL)
  {
    syslog(LOG_ERR, "%s@%d-opendir failed, errno:%d, Path:'%s', dir level:%d\n",
              thisFile, __LINE__, errno, currentPath, dirLevel);
    return -errno;
  }

  // Print files from starting directory
  hcom_dir_mgmt_print_directory_files(dir, dirLevel, currentPath);

  // Manage currentPath and dirlevel as it moves up and down within
  // the directory structure.
  while(true)
  {
    // Search for directories in currentPath, which may change multiple times
    // while this loop executes, but always deeper in to subdirectories. On
    // leaving this loop (no more directories on this branch) we will move the
    // directory up 1 level to it's original parent.
    while ((entry = readdir(dir[dirLevel])) != NULL)
    {
      if (DIRENT_ISDIRECTORY(entry->d_type))
      {
        // Ignore these directories, we only care about named directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        // Update the currentPath to include the just found child directory
        // If the only character is '/' we are at the root directory and don't
        // want to add another '/'.
        if(!(strlen(currentPath) == 1 && currentPath[0] == '/'))
        {
          strcat(currentPath, "/");
        }

        strcat(currentPath, entry->d_name);
        dirLevel++;

        // Open deeper child directory
        dir[dirLevel] = opendir(currentPath);
        if(dir[dirLevel] == NULL)
        {
          syslog(LOG_ERR, "%s@%d-opendir failed, errno:%d, Path:'%s', dir level:%d\n",
                    thisFile, __LINE__, errno, currentPath, dirLevel);
          return -errno;
        }

        // Reads the directory at currentPath/dirLevel and prints all the
        // files found there (if there are any).
        hcom_dir_mgmt_print_directory_files(dir, dirLevel, currentPath);
      }
      else if(showAll)
      {
        // All non-file types if starting at root directory
        char *entryType;
        if(! (DIRENT_ISFILE(entry->d_type)))
        {
          if(DIRENT_ISCHR(entry->d_type)) {entryType = "char";}
          else if(DIRENT_ISBLK(entry->d_type)) {entryType = "block";}
          else if(DIRENT_ISLINK(entry->d_type)) {entryType = "link";}
          else {entryType = "????";}
          syslog(LOG_MTEST, "%s/%s [%s]\n", currentPath, entry->d_name, entryType);
        }
      }
    }

    // We reached the bottom of this directory branch
    closedir(dir[dirLevel]);

    // Is the level equal to our starting level (i.e. are we finished?)
    if(dirEndLevel == dirLevel)
    {
      break;      // Break out of loop as we are done
    }

    // Remove child subdirectory from currentPath
    char *lastSlash = strrchr(currentPath, '/');
    *lastSlash = '\0';

    // Up a parent directory level and resume the search from there
    dirLevel--;
  }

  free(currentPath);
  return OK;
}

//===================================================================
// This is the entry point for the non-recursive version. It was used to
// verify that the subdirectory additions were correctly done.
int hcom_dir_mgmt_tst_log_directories_files(const char *initialDir)
{
  int ret;
  
  if(initialDir == NULL || strlen(initialDir) < 1 || initialDir[0] != '/')
  {
    syslog(LOG_ERR, "%s@%d-InitialDir was invalid\n", thisFile, __LINE__);
    return -EINVAL;
  }

  // Do the heavy lifting
  ret = hcom_dir_mgmt_find_next_directory(initialDir);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Listing files failed, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  return OK;
}

//--------------------------------------------------------------------------
// NOTE - Recursive function
static int hcom_dir_mgmt_tst_recurse_nested_directories(const char *rootDir,
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
              
      syslog(LOG_MTEST, "%*s%s/\n", indent, "", entry->d_name);

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];   // MAX_PATH
      snprintf_chk(path, sizeof(path), "%s/%s", rootDir, entry->d_name);

      // Recursion is here since directory
      hcom_dir_mgmt_tst_recurse_nested_directories(path, hostMsg, entry, indent + 2);
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

      syslog(LOG_MTEST, "%*s%s [%s]\n", indent, "", entry->d_name, entryType);

    }
#endif
  }
  // Blank line
  syslog(LOG_MTEST, "\n");

  closedir(dir);
  return OK;
}

//===========================================================================
// THIS RECURSIVE CODE IS FOR DIAGNOSTICS
// Used to confirm the above non recursive is correct
//===========================================================================
// Note: Using "/" as the rootDir would show all files and directories etc.
static int hcom_dir_mgmt_tst_recurse_nested_directories_start(const char *rootDir)
{
  // Keep these larger objects off the stack of the recursive function
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  struct dirent *entry = NULL;

  return hcom_dir_mgmt_tst_recurse_nested_directories(rootDir, hostMsg, entry, 0);
}

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// These tests are developer -p 13
void meadow_dir_mgmt_tests(uint32_t userData)
{
  int ret;
  bool isMounted = false;
  uint32_t totalBytes;
  uint32_t freeBytes;
  uint32_t usedBytes;

  syslog(LOG_MTEST, "Directory management received 'set developer -p 13 -v %lu'\n", userData);
  usleep(20 * 1000);

  // We assume sdcard needs to be mounted and if not, an error message, we'll
  // ignore.
  // mount(source, target, fstype, mountflags, data)
  // e.g. mount("/dev/mmcsd0", "/sdcard", "vfat", 0, NULL);
  ret = mount(MEADOW_SDCARD_BLOCK_NAME, MEADOW_SDCARD_MOUNT_POINT_NAME,
            MEADOW_SDCARD_FILE_SYS_TYPE, 0, NULL);
  if(ret < 0)
    syslog(LOG_MTEST, "SD-Card mount attempt failed. ret:%d, errno:%d\n", ret, errno);
  else
    isMounted = true;

  switch (userData)
  {
      // Test getting total available external flash size in bytes
    case 1:
      ret = meadow_read_file_total_free_flash_size(&totalBytes, &freeBytes);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Error: calling meadow_read_file_total_free_flash_size(), ret:%d, errno:%d\n",
                  ret, errno);
        return;
      }
      usedBytes = totalBytes - freeBytes;
      syslog(LOG_MTEST, "Total bytes:%lu (%luMb), Free bytes:%lu (%luMb), Used bytes (calculated):%lu (%luMb)\n",
                totalBytes, totalBytes/(1024*1024),
                freeBytes, freeBytes/(1024*1024),
                usedBytes, usedBytes/(1024*1024));
      break;

  case 2:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("/meadow0");
    break;

  case 3:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("/sdcard");
    break;

  case 4:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("/");
    break;

  case 5:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("");
    break;

  case 6:
    hcom_dir_mgmt_tst_log_directories_files("/meadow0");
    break;

  case 7:
    hcom_dir_mgmt_tst_log_directories_files("/mmcsd0");
    break;

  case 8:
    hcom_dir_mgmt_tst_log_directories_files("/sdcard");
    break;

  case 9:       // Illegal entry
    hcom_dir_mgmt_tst_log_directories_files("/");
    break;

  case 10:       // Illegal entry
    hcom_dir_mgmt_tst_log_directories_files("/dev");
    break;

  case 11:       // Illegal entry
    hcom_dir_mgmt_tst_log_directories_files("");

    break;
  case 12:       // Illegal entry
    hcom_dir_mgmt_tst_log_directories_files(NULL);
    break;

  default:
    break;
  }

  if(isMounted)
  {
    ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
    if(ret < 0)
      syslog(LOG_MTEST, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);

    syslog(LOG_MTEST, "===> umount successful\n");
  }
}

#endif      // #if defined (CONFIG_DIR_MGMT_TESTS)

