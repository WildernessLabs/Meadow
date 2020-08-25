/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_lists.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
static uint32_t hcom_file_lists_calc_crc_for_file(char *completeFilePath, off_t *fileSize,
          uint32_t *blockSizeKB);
static int hcom_file_lists_all_dev_dir_and_files(const char *name, int indent, uint32_t userData);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_lists_files_in_partition(uint32_t partitionId)
{
  int fileCount = 0;

  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *singleFileFound = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  DIR *dirp;
  struct dirent *direntry;

  hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_LIST_HEADER, 0, thisFile, __LINE__);

  // Construct file name
#ifdef CONFIG_MTD_PARTITION
  int stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  DEBUGASSERT(strlen(HCOM_FILE_MOUNT_POINT_TARGET) < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  strncpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

#endif

  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-opendir(\"%s\") errno:%d\n",
              thisFile, __LINE__, fullMountPtName, errno);
    free(fullMountPtName);
    free(singleFileFound);
    return -1;
  }

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      fileCount++;

      // Get the next file name
#ifdef CONFIG_MTD_PARTITION
      hcom_logging_syslog(LOG_INFO, "%s@%d-Found file '%s' in part %d\n",
                thisFile, __LINE__, direntry->d_name, partitionId);
#else
      hcom_logging_syslog(LOG_INFO, "%s@%d-Found file '%s'\n", thisFile, __LINE__, direntry->d_name);
#endif
      int fileNameLen;
      fileNameLen = snprintf(singleFileFound, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", fullMountPtName, direntry->d_name);
      DEBUGASSERT(fileNameLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0, singleFileFound, thisFile, __LINE__);
    }
  }

  if(fileCount == 0)
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                  "No files found", thisFile, __LINE__);
  }
  else
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "A total of %d file%s found", fileCount, fileCount == 1 ? "" : "s");
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_LIST_MEMBER, 0,
                  hostMsg, thisFile, __LINE__);
  }

  closedir(dirp);

  free(fullMountPtName);
  free(singleFileFound);

  return OK;
}

//=====================================================================
int hcom_file_lists_files_and_crc_in_partition(uint32_t partitionId)
{
  int fileCount = 0;

  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *singleFileFound = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  int stringLen;
  DIR *dirp;
  struct dirent *direntry;

  hcom_host_send_header_msg(HCOM_HOST_REQUEST_TEXT_LIST_HEADER, 0, thisFile, __LINE__);

  // Construct file name
#ifdef CONFIG_MTD_PARTITION
  stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  strcpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET);
#endif

  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-opendir '%s' errno:%d\n",
            thisFile, __LINE__, fullMountPtName, errno);

    free(fullMountPtName);
    free(singleFileFound);
    free(completeNameBuf);
    return -1;
  }

  off_t totalSizeOfFiles = 0;
  uint32_t totalFlashSizeKB = 0;

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      fileCount++;
      stringLen = snprintf(completeNameBuf, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", 
                fullMountPtName, direntry->d_name);
      DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      
      // Find the CRC checksum
      off_t fileSize;
      uint32_t blockSizeKB;
      uint32_t crcChecksum = hcom_file_lists_calc_crc_for_file(completeNameBuf, &fileSize, &blockSizeKB);
      totalSizeOfFiles += fileSize;
      totalFlashSizeKB += blockSizeKB;

#ifdef CONFIG_MTD_PARTITION
      hcom_logging_syslog(LOG_INFO, "%s@%d-'%s' in part %d checksum 0x%08x\n",
                thisFile, __LINE__, direntry->d_name, partitionId, crcChecksum);
#else
      hcom_logging_syslog(LOG_INFO, "%s@%d-'%s' checksum 0x%08x\n",
                thisFile, __LINE__, direntry->d_name, crcChecksum);
#endif

      // Add this file to the csv list 
      int fileNameLen = 0;
      fileNameLen = snprintf(singleFileFound, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s [0x%08x] %d KB (%u bytes)",
            fullMountPtName, direntry->d_name, crcChecksum, blockSizeKB, fileSize);

      DEBUGASSERT(fileNameLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                    singleFileFound, thisFile, __LINE__);
    }
  }

  if(fileCount == 0)
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                  "No files found", thisFile, __LINE__);
  }
  else
  {
    // Need comma separators for file size? I tried %'d and this didn't work. Here are some DIY ideas:
    // https://stackoverflow.com/questions/1449805/how-to-format-a-number-from-1123456789-to-1-123-456-789-in-c/24795133#24795133
    snprintf(singleFileFound, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH,
          "A total of %d file%s using %d KB (%u bytes)", fileCount,
          fileCount == 1 ? "" : "s", totalFlashSizeKB, totalSizeOfFiles);

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_CRC_MEMBER, 0,
                  singleFileFound, thisFile, __LINE__);
  }

  closedir(dirp);

  free(fullMountPtName);
  free(singleFileFound);
  free(completeNameBuf);

  return OK;
}

// ==============================================================
// THIS IS AN UNDOCUMENTED FEATURE CALLABLE from Developer 4.
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

      snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s/\n", indent, "", entry->d_name);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      char path[256];
      snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
      
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
      snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
              "%*s%s [%s]\n",indent, "", entry->d_name, entryType);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION,
                0, hostMsg, thisFile, __LINE__);
    }
  }

  closedir(dir);
  return OK;
}

//==================================================================
// This call will calculate the crc32 checksum for the requested file
uint32_t hcom_file_lists_calc_crc_for_file(char *completeFilePath,
          off_t *fileSize, uint32_t *blockSizeKB)
{
  uint32_t crc32Checksum = 0;
  uint8_t *crcReadBuff;
  struct stat fileStatus;
  int ret;
  int fd;

  // Existing file - open read only
  set_errno(0);
  fd = open(completeFilePath, O_RDONLY);
  if (fd == -1)
  {
    int Errno = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno: %d\n",
                thisFile, __LINE__, completeFilePath, Errno);
    return -errno;
  }

  hcom_logging_syslog(LOG_DEBUG, "Opened %s for CRC\n", completeFilePath);

  // from nuttx stat.h
  // struct stat
  // {
  //   /* Required, standard fields */
  //   mode_t    st_mode;    /* File type, attributes, and access mode bits */
  //   off_t     st_size;    /* Size of file/directory, in bytes */
  //   blksize_t st_blksize; /* Block size used for filesystem I/O */
  //   blkcnt_t  st_blocks;  /* Number of blocks allocated */
  //   time_t    st_atime;   /* Time of last access */
  //   time_t    st_mtime;   /* Time of last modification */
  //   time_t    st_ctime;   /* Time of last status change */
  //   /* Internal fields.  These are part this specific implementation and
  //   * should not referenced by application code for portability reasons.
  //   */
  // #ifdef CONFIG_PSEUDOFS_SOFTLINKS
  //   uint8_t   st_count;   /* Used internally to limit traversal of links */
  // #endif
  // };

  ret = fstat(fd, &fileStatus);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-fstat of %s failed errno:%d\n",
           thisFile, __LINE__, completeFilePath, errno);
    return -errno;
  }

  *fileSize = fileStatus.st_size;
  *blockSizeKB = (fileStatus.st_blksize * fileStatus.st_blocks) / 1024;
   
  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-lseek failed %s, errno:%d\n",
              thisFile, __LINE__, completeFilePath, errno);
    return -errno;
  }

  // Read all the data
  #define HCOM_FILE_READ_BUFF_SIZE_FOR_CRC 1024
  crcReadBuff = malloc(HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);

  ssize_t nbytes;
  do
  {
    nbytes = read(fd, crcReadBuff, HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);
    if (nbytes < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-read %s, errno:%d\n",
                thisFile, __LINE__, completeFilePath, errno);
      free(crcReadBuff);
      return -errno;
    }

    if (nbytes > 0)
    {
      crc32Checksum = crc32part(crcReadBuff, nbytes, crc32Checksum);
    }
  } while (nbytes > 0);
  free(crcReadBuff);

  ret = close(fd);
  if (ret < 0)
  {
    int Errno = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-close %s, errno:%d\n",
             thisFile, __LINE__, completeFilePath, Errno);
    return -Errno;
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Checksum for '%s' 0x%08x\n",
            thisFile, __LINE__, completeFilePath, crc32Checksum);
  return crc32Checksum;
}
