/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\sdcard_tests.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx/hcom_nx_common.h"

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/stat.h>

#include <meadow/hcom_shared_common.h>

#if defined (CONFIG_SD_CARD_TESTS) || defined (CONFIG_ALL_MEADOW_TESTS)
#pragma message "(--) sdcard_tests.c"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_EX_SDCARD_TEST_FILE_NAME  "/sdcard/testfile.txt"

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;
static int _activeFd;

// 46 total characters
static char textForTesting[] ="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz\n";

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_nx_sdcard_file_stat_test(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_exec_test_sdcard_setup()
{
  _activeFd = -1;
  return OK;
}

//===================================================================
// 101
static int hcom_nx_sdcard_mount_card(void)
{
  int ret;

  // e.g. mount("/dev/mmcsd0", "/sdcard", "vfat", 0, NULL);
  ret = mount(MEADOW_SDCARD_BLOCK_NAME, MEADOW_SDCARD_MOUNT_POINT_NAME,
            MEADOW_SDCARD_FILE_SYS_TYPE, 0, NULL);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Mount failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }

  syslog(LOG_MTEST, "Mount successful\n");
  return ret;
}

//===================================================================
// 102
static int hcom_nx_sdcard_open_test_file(void)
{
  int ret;

  _activeFd = open(HCOM_EX_SDCARD_TEST_FILE_NAME, O_RDWR|O_CREAT|O_APPEND, 0644);
  if(_activeFd < 0)
  {
    _activeFd = -1;

    syslog(LOG_MTEST, "%s@%d-ERROR: open failed. ret(fd):%d, errno:%d\n",
      thisFile, __LINE__, _activeFd, errno);
    ret = _activeFd;
    return ret;
  }

  syslog(LOG_MTEST, "Open successful\n");
  return OK;
}

//===================================================================
// 103
static int hcom_nx_sdcard_write_test_file(void)
{
  if(_activeFd == -1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Test app believes file closed.\n",
      thisFile, __LINE__);
    return -EBADFD;
  }

  ssize_t nbytes = write(_activeFd, textForTesting, sizeof(textForTesting));
  if(nbytes < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: write failed. ret(nbytes):%d, errno:%d\n",
      thisFile, __LINE__, nbytes, errno);
    return nbytes;
  }

  if(nbytes != sizeof(textForTesting))
  {
    syslog(LOG_MTEST, "%s@%d-Warning: write only wrote:%d out of:%d\n",
      thisFile, __LINE__, nbytes, sizeof(textForTesting));
    return OK;
  }

  syslog(LOG_MTEST, "Write successful (%d written)\n", nbytes);
  return OK;
}

//===================================================================
// 104
static int hcom_nx_sdcard_read_test_file(void)
{
  uint8_t *buffer;

  if(_activeFd == -1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Test app believes file closed.\n",
      thisFile, __LINE__);
    return -EBADFD;
  }

  // Seek to beginning
  off_t offset = lseek(_activeFd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: lseek failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, offset, errno);
    return -errno;
  }

  // Read up to first 1024 bytes
  buffer = malloc(1024);
  ssize_t nbytes = read(_activeFd, buffer, 1024);
  if(nbytes < 0)
  {
    free(buffer);
    syslog(LOG_MTEST, "%s@%d-ERROR: reading failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, nbytes, errno);
    return nbytes;
  }

  syslog(LOG_MTEST, "---- Read %d bytes from test file ----\n", nbytes);
  
#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
    hcom_nx_diag_print_buffer(buffer, nbytes, 1);
#else
    syslog(LOG_MTEST, "Print buffer not included in build\n");
#endif

  free(buffer);
  return OK;
}

//===================================================================
// 105
static int hcom_nx_sdcard_close_test_file(void)
{
  int ret;

  if(_activeFd == -1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Test app believes file closed.\n",
              thisFile, __LINE__);
    return -EBADFD;
  }

  ret = close(_activeFd);
  if(ret < 0)
  {
    // I think close should flush internally without the fsync step???
    if(errno == EIO)
      syslog(LOG_MTEST, "%s@%d-May need to fsync\n", thisFile, __LINE__);
      
    syslog(LOG_MTEST, "%s@%d-ERROR: close failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }

  _activeFd = -1;  

  syslog(LOG_MTEST, "Close successful\n");
  return ret;
}

//===================================================================
// 106
static int hcom_nx_sdcard_unmount_card(void)
{
  int ret;

  ret = umount(MEADOW_SDCARD_MOUNT_POINT_NAME);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n", thisFile,
      __LINE__, ret, errno);
    return ret;
  }
  
  syslog(LOG_MTEST, "umount successful\n");
  return ret;
}

//===================================================================
// 107
static int hcom_nx_sdcard_delete_test_file(void)
{
  int ret;

  ret = unlink(HCOM_EX_SDCARD_TEST_FILE_NAME);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: delete failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  syslog(LOG_MTEST, "Delete successful\n");
  return ret;
}

//===================================================================
// 108 file status
static int hcom_nx_sdcard_file_stat_test(void)
{
  int ret;
  struct stat fileStatus;

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

  if(_activeFd == -1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Test app believes file closed.\n",
      thisFile, __LINE__);
    return -EBADFD;
  }

  ret = fstat(_activeFd, &fileStatus);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: fstat failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }

  off_t fileSize = fileStatus.st_size;

  syslog(LOG_MTEST, "fstat successful. file size:%d bytes\n", fileSize);
  return fileSize;
}

//===================================================================
// 109 fsync flushs to disk. It also closes the file. If the file is closed
// 
static int hcom_nx_sdcard_fsync_file(void)
{
  int ret;

  if(_activeFd == -1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Test app believes file closed.\n",
      thisFile, __LINE__);
    return -EBADFD;
  }

  ret = fsync(_activeFd);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: fsync failed. ret:%d, errno:%d\n",
      thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  syslog(LOG_MTEST, "fsync successful\n");
  return ret;
}

//===================================================================
// 'set developer -p 7' for these tests
// Route the test to the correct destination
int meadow_kt_sd_card_tests(uint32_t userData)
{
  switch(userData)
  {
    case 100:  // format as fat32
    // Future. For now use NSH or PC to format FAT32
    syslog(LOG_MTEST,
      "sdcard tests received %u this is NOT a supported option\n",
      userData);
    break;

    case 101: // mount
    syslog(LOG_MTEST, "sdcard tests received %u to execute mount test\n",
      userData);
    return hcom_nx_sdcard_mount_card();
    break;

    case 102: // Open test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute open testfile.txt\n",
      userData);
    return hcom_nx_sdcard_open_test_file();
    break;

    case 103: // Write test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute write test\n",
      userData);
    return hcom_nx_sdcard_write_test_file();
    break;

    case 104: // Read test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute read test\n",
      userData);
    return hcom_nx_sdcard_read_test_file();
    break;

    case 105: // Close test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute close test\n",
      userData);
    return hcom_nx_sdcard_close_test_file();
    break;

    case 106: // umount
    syslog(LOG_MTEST, "sdcard tests received %u to execute umount test\n",
      userData);
    return hcom_nx_sdcard_unmount_card();
    break;

    case 107: // Delete test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute delete test\n",
      userData);
    return hcom_nx_sdcard_delete_test_file();
    break;

    case 108: // File Stat test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute fstat test\n",
      userData);
    return hcom_nx_sdcard_file_stat_test();
    break;

    case 109: // fsync test file
    syslog(LOG_MTEST, "sdcard tests received %u to execute fsync test\n",
      userData);
    return hcom_nx_sdcard_fsync_file();
    break;

    default:
    syslog(LOG_MTEST, "Unknown value %u passed to hcom_nx_exec_sdcard_tests()\n",
      userData);
    break;
  }

  return OK;
}

#endif      // #if defined (CONFIG_SD_CARD_TESTS)
