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

#if defined (CONFIG_DIR_MGMT_TESTS)
#pragma message "(--) dir_mgmt_tests.c"

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
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
#define HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS (0)

static int hcom_dir_mgmt_tst_log_directories_files(const char *initialDir);

#if HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS > 0
//===================================================================
// Debug code. If needed use this function with the following debug code
static void hcom_dir_mgmt_find_file_status(char *path)
{
  int ret;
  struct stat statBuf;

  ret = stat(path, &statBuf);
  if (ret < 0)
  {
    syslog(2, "stat call error. ret:%d, errno:%d\n", ret, errno);
    return;
  }
  
  syslog(2, "Reported by stat:\n");

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

  syslog(2, "file size   : %d (bytes)\n",  statBuf.st_size);
  syslog(2, "block size  : %d (bytes)\n",  statBuf.st_blksize);
  syslog(2, "size        : %d (blocks)\n", statBuf.st_blocks);
  syslog(2, "access time : %d\n",          statBuf.st_atime);
  syslog(2, "modify time : %d\n",          statBuf.st_mtime);
  syslog(2, "change time : %d\n",          statBuf.st_ctime);

  struct statfs buf;
  ret = statfs(path, &buf);
  if (ret == 0)
  {
    // LITTLEFS_SUPER_MAGIC = 0x0a732923
    syslog(2, "Reported by statfs:\n");
    syslog(2, "FS Type           : %0x\n", buf.f_type);
    syslog(2, "Block size        : %d\n",  buf.f_bsize);
    syslog(2, "Number of blocks  : %d\n",  buf.f_blocks);
    syslog(2, "Free blocks       : %d\n",  buf.f_bfree);
    syslog(2, "Free user blocks  : %d\n",  buf.f_bavail);
    syslog(2, "Number file nodes : %d\n",  buf.f_files);
    syslog(2, "Free file nodes   : %d\n",  buf.f_ffree);
    syslog(2, "File name length  : %d\n",  buf.f_namelen);
  }
  else
  {
    syslog(2, "show_statfs: ERROR statfs(%s) failed with errno=%d\n",
            path, errno);
  }
}
#endif

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
        // Skip a line and print this directory path as a header to the files
        syslog(2, "\n");
        syslog(2, "Directory:%s\n", currentPath);
        filesFound = true;
      }

      // Print file information
      syslog(2, "  %s\n", entry->d_name);
#if HCOM_DIR_MGMT_TST_SHOW_FULL_FILE_STATUS > 0
      hcom_dir_mgmt_find_file_status(currentPath);
#endif
      fileCount++;
    }
  }

  // Line if we printed files
  if(filesFound)
  {
    syslog(2, "----------%d Total File(s) (Level:%d)----------\n",
              fileCount, dirLevel);
  }
#if 1
  else
  {
    syslog(2, "Empty directory:%s\n", currentPath);
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

        // Open deeper child directory
        dir[dirLevel] = opendir(currentPath);
        if(dir[dirLevel] == NULL)
        {
          syslog(LOG_ERR, "%s@%d-opendir failed, errno:%d, Path:'%s'\n",
                    thisFile, __LINE__, errno, currentPath);
          return -errno;
        }

        // Opens the directory at currentPath and prints all the files found
        // there (if there are any).
        hcom_dir_mgmt_print_directory_files(dir, dirLevel, currentPath);
      }
#if 0
      else
      {
        char *entryType;
        if(DIRENT_ISFILE(entry->d_type)) {entryType = "file";}
        else if(DIRENT_ISCHR(entry->d_type)) {entryType = "char";}
        else if(DIRENT_ISBLK(entry->d_type)) {entryType = "block";}
        else if(DIRENT_ISLINK(entry->d_type)) {entryType = "link";}
        else {entryType = "????";}
        syslog(2, "%s@%d-At:'%s', %s type found, named:%s\n",
                  thisFile, __LINE__, currentPath, entryType, entry->d_name);
      }
#endif
    }

    // We reached the bottom of this directory branch
    closedir(dir[dirLevel]);

    // Is the level equal to our starting level (i.e. are we finished?)
    if(dirStartLvl == dirLevel)
    {
      break;      // Break out of loop as we are done
    }

    // Remove child subdirectory from currentPath
    char *lastShash = strrchr(currentPath, '/');
    *lastShash = '\0';

    // Up a parent directory level and resume the search from there
    dirLevel--;
  }

  return OK;
}

//===================================================================
// Public - This is only diagnostic in nature, used to verify that the
// subdirectory modifications were correct.
int hcom_dir_mgmt_tst_log_directories_files(const char *initialDir)
{
  int ret;
  int dirLevel = 0;
  DIR *dir[HCOM_FILE_DNLD_MAX_NUMB_ELEMENTS];
  
  if(initialDir == NULL || strlen(initialDir) < 1 || initialDir[0] != '/')
  {
    syslog(LOG_ERR, "%s@%d-InitialDir was invalid\n", thisFile, __LINE__);
    return -EINVAL;
  }

  // Room for nesting of directories. This memory is used by this function to
  // manage the current path as it changes up or down within the directory
  // structure.
  char *currentPath = malloc(512);
  strcpy(currentPath, initialDir);

  ret = hcom_dir_mgmt_find_next_directory(dir, dirLevel, currentPath);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Listing files failed, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  free(currentPath);

  return OK;
}

//--------------------------------------------------------------------------
// NOTE - Recursive function, is not public
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
              
      syslog(2, "%*s%s/\n", indent, "", entry->d_name);

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

      syslog(2, "%*s%s [%s]\n", indent, "", entry->d_name, entryType);

    }
#endif
  }
  // Blank line
  syslog(2, "\n");

  closedir(dir);
  return OK;
}

//===========================================================================
// THIS RECURSIVE CODE IS FOR DIAGNOSTICS, KEEPING SINCE IT WORKS.
// Call using 'meadow set developer -d 13 -v n'
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
// These tests are developer -d 13
void meadow_dir_mgmt_tests(uint32_t userData)
{
  int ret;
  uint32_t totalBytes;
  uint32_t freeBytes;
  uint32_t usedBytes;

  syslog(2, "Directory management received 'set developer -d 13 -v %lu'\n", userData);
  usleep(20 * 1000);

  switch (userData)
  {
      // Test getting total available external flash size in bytes
    case 1:
      ret = meadow_read_file_total_free_flash_size(&totalBytes, &freeBytes);
      if(ret < 0)
      {
        syslog(2, "Error: calling meadow_read_file_total_free_flash_size(), ret:%d, errno:%d\n",
                  ret, errno);
        return;
      }
      usedBytes = totalBytes - freeBytes;
      syslog(2, "--> Total bytes:%lu (%luMb), Free bytes:%lu (%luMb), Used bytes (calculated):%lu (%luMb)\n",
                totalBytes, totalBytes/(1024*1024),
                freeBytes, freeBytes/(1024*1024),
                usedBytes, usedBytes/(1024*1024));
      break;

  case 2:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("/meadow0");
    break;

  case 3:
    hcom_dir_mgmt_tst_recurse_nested_directories_start("/mmcsd0");
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
    hcom_dir_mgmt_tst_log_directories_files("/");
    break;

  case 9:       // Illegal
    hcom_dir_mgmt_tst_log_directories_files("");
    break;

  case 10:       // Illegal
    hcom_dir_mgmt_tst_log_directories_files(NULL);
    break;

  default:
    break;
  }
}

#endif      // #if defined (CONFIG_DIR_MGMT_TESTS)

