/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_misc.c
 * 
 *   Copyright (C) 2021-2023 Wilderness Labs. All rights reserved.
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
#include <sys/statfs.h>

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
// This call will calculate the CRC32 checksum for the requested file name
// provided.  It will open and close the file before exiting.
uint32_t hcom_file_misc_calc_crc_for_file(char *completeFilePath,
          off_t *fileSize, uint32_t *blockSizeKB, int *detectError)
{
  int ret;
  int fd;
  uint32_t crc32Checksum;

  // Existing file - open read only
  set_errno(0);
  fd = open(completeFilePath, O_RDONLY);
  if (fd == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno: %d\n",
                thisFile, __LINE__, completeFilePath, errno);
    *detectError = -errno;
    return 0;
  }

  crc32Checksum = hcom_file_misc_calc_crc_for_file_fd(fd, completeFilePath, fileSize,
            blockSizeKB, detectError);
  
  ret = close(fd);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-close %s, errno:%d\n",
             thisFile, __LINE__, completeFilePath, errno);
    *detectError = -errno;
    return 0;
  }

  return crc32Checksum;
}

//==========================================================================
// This public function will determine the CRC for a file which has already
// been opened.
uint32_t hcom_file_misc_calc_crc_for_file_fd(int fd, char *completeFilePath,
          off_t *fileSize, uint32_t *blockSizeKB, int *detectError)
{
  int ret;
  uint8_t *crcReadBuff;
  struct stat fileStatus;
  uint32_t crc32Checksum = 0;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "Opened %s for CRC\n", completeFilePath);
#endif

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
    *detectError = -errno;
    return 0;
  }

  *fileSize = fileStatus.st_size;
  *blockSizeKB = (fileStatus.st_blksize * fileStatus.st_blocks) / 1024;

  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-lseek failed %s, errno:%d\n",
              thisFile, __LINE__, completeFilePath, errno);
    *detectError = -errno;
    return 0;
  }

  // Read all the data and calculate the checksum
  #define HCOM_FILE_READ_BUFF_SIZE_FOR_CRC 1024
  crcReadBuff = malloc(HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);
  if(crcReadBuff == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    *detectError = -ENOMEM;
    return 0;
  }

  ssize_t nbytes;
  do
  {
    nbytes = read(fd, crcReadBuff, HCOM_FILE_READ_BUFF_SIZE_FOR_CRC);
    if (nbytes < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-read %s, errno:%d\n",
                thisFile, __LINE__, completeFilePath, errno);
      free(crcReadBuff);
      *detectError = -errno;
      return 0;
    }

    if (nbytes > 0)
    {
      crc32Checksum = crc32part(crcReadBuff, nbytes, crc32Checksum);
    }
  } while (nbytes > 0);
  free(crcReadBuff);

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Checksum for '%s' 0x%08x\n",
            thisFile, __LINE__, completeFilePath, crc32Checksum);
#endif

  *detectError = OK;

  return crc32Checksum;
}

//==========================================================================
// This function returns the total and free space on the external flash
int meadow_read_file_total_free_flash_size(uint32_t *totalBytes,
          uint32_t *freeBytes)
{
  int ret;
  struct statfs statFs;

  ret = statfs("/meadow0", &statFs);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error: ret:%d, errno:%d\n",
              __FILE__, __LINE__, ret, errno);
    return ret;
  }

  *totalBytes = statFs.f_bsize * statFs.f_blocks;
  *freeBytes = statFs.f_bsize * statFs.f_bfree;

  return OK;
}
