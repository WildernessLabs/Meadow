/****************************************************************************
 * configs\stm32f777zit6-meadow\src\hcom_nx\tests\hcom_nx_sdcard_tests.c
 * 
 *   Copyright (C) 2019 - 2021 Wilderness Labs. All rights reserved.
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

// Available tests based on provided user data
// QspiWrite -1:  Bulk erases and writes data pattern to entire flash and reads to verify
// QspiWrite -2:  Bulk erases and writes data pattern to entire flash and stops
// QspiWrite -3:  Bulk erases and writes data pattern to one page and then verifies that all
//                 pages are correct, before and after the page written, repeats for each page.
//                 Note: This test will take about 26.4 years to complete for 512 MBit flash.
// QspiWrite 0-n: Fill buffer with test pattern. Page data determined by developerValue 0-n
//
// QspiInit -1:   Bulk entire flash
// QspiInit -2:   Finds any non-erased sectors and erases them (faster than bulk erase)
// QspiInit 0-n:  Erases one 4k sector as determined by developerValue 0-n
//
// QspiRead -1:   Displays via syslog the data in any non-erased pages
// QspiRead -2:   Displays via syslog the data in erased pages (not very useful)
// QspiRead 0-n:  Displays the data in the page determined by developerValue 0-n

// This code was put here from develop branch of github 27Apr2021 by PeterM.
// It was found in commit e1e4319ad4be4daed12479167b5918a2bfb131a4 April 2, 2020.
// 'nuttx/configs/stm32f777zit6-meadow/src/hcom/commands/hcom_exec_rqst_testing.c'
// This file was ignored in the hcom move to /apps, until now.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

// #include <meadow/hcom_upd_shared.h>
#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>

#if HCOM_INCLUDE_SD_CARD_TESTS_IN_BUILD > 0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_EX_SDCARD_FILE_SYS_TYPE  "vfat"
#define HCOM_EX_SDCARD_BLOCK_NAME   "/dev/mmcsd0"
#define HCOM_EX_SDCARD_MOUNT_POINT  "/sdcard"
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


/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_exec_test_sdcard_setup()
{
  _activeFd = -1;
  return OK;
}

//===================================================================
static int hcom_nx_sdcard_mount_card(void)
{
  int ret;

  // mount(source, target, fstype, mountflags, data)
  // e.g. mount("/dev/mmcsd0", "/mnt", "vfat", 0, NULL);
  ret = mount(HCOM_EX_SDCARD_BLOCK_NAME, HCOM_EX_SDCARD_MOUNT_POINT,
            HCOM_EX_SDCARD_FILE_SYS_TYPE, 0, NULL);
  if(ret < 0)
  {
    syslog(1, "%s@%d-ERROR: Mount failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  syslog(1, "Mount successful\n");
  return ret;
}

//===================================================================
static int hcom_nx_sdcard_unmount_card(void)
{
  int ret;

  ret = umount(HCOM_EX_SDCARD_MOUNT_POINT);
  if(ret < 0)
  {
    syslog(1, "%s@%d-ERROR: umount failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  syslog(1, "umount successful\n");
  return ret;
}

//===================================================================
static int hcom_nx_sdcard_open_test_file(void)
{
  int ret;
  _activeFd = open(HCOM_EX_SDCARD_TEST_FILE_NAME, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if(_activeFd < 0)
  {
    syslog(1, "%s@%d-ERROR: open failed. ret(fd):%d, errno:%d\n", thisFile, __LINE__, _activeFd, errno);
    ret = _activeFd;
    _activeFd = -1;
    return ret;
  }

  syslog(1, "open successful\n");
  return OK;
}

//===================================================================
static int hcom_nx_sdcard_close_test_file(void)
{
  int ret;
  ret = close(_activeFd);
  _activeFd = -1;  
  if(ret < 0)
  {
    syslog(1, "%s@%d-ERROR: close failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  syslog(1, "close successful\n");
  return ret;
}

//===================================================================
static int hcom_nx_sdcard_create_test_file(void)
{
  if(_activeFd == -1)
  {
    syslog(1, "%s@%d-ERROR: File is not open.\n",
              thisFile, __LINE__);
    return -EBADFD;
  }

  ssize_t nbytes = write(_activeFd, textForTesting, sizeof(textForTesting));
  if(nbytes < 0)
  {
    syslog(1, "%s@%d-ERROR: write failed. ret(nbytes):%d, errno:%d\n",
              thisFile, __LINE__, nbytes, errno);
    return nbytes;
  }
  
  if(nbytes != sizeof(textForTesting))
  {
    syslog(1, "%s@%d-Warning: write only wrote:%d out of:%d\n",
              thisFile, __LINE__, nbytes, sizeof(textForTesting));
    return OK;
  }
  
  syslog(1, "write successful\n");
  return OK;
}

//===================================================================
static int hcom_nx_sdcard_read_test_file(void)
{
  uint8_t buffer[1024];

  ssize_t nbytes = read(_activeFd, buffer, 1024);
  if(nbytes < 0)
  {
    syslog(1, "%s@%d-ERROR: reading failed. ret:%d, errno:%d\n", thisFile, __LINE__, nbytes, errno);
    return nbytes;
  }

  syslog(1, "---- Read %d bytes from test file ----\n", nbytes);
  hcom_nx_diag_print_buffer(buffer, nbytes, 1);
  return OK;
}

//===================================================================
static int hcom_nx_sdcard_delete_test_file(void)
{
  int ret;

  ret = unlink(HCOM_EX_SDCARD_TEST_FILE_NAME);
  _activeFd = -1;  
  if(ret < 0)
  {
    syslog(1, "%s@%d-ERROR: delete failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  syslog(1, "delete successful\n");
  return ret;
}

//===================================================================
// fsync should flush to disk
static int hcom_nx_sdcard_fsync_file(void)
{
  int ret;

  if(_activeFd == -1)
  {
    syslog(1, "%s@%d-ERROR: File is not open.\n",
              thisFile, __LINE__);
    return -EBADFD;
  }

  ret = fsync(_activeFd);
  _activeFd = -1;  
  if(ret < 0)
  {
    syslog(1, "%s@%d-ERROR: fsync failed. ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  syslog(1, "fsync successful\n");
  return ret;
}

//===================================================================
// Route the test to the correct destination
int hcom_nx_exec_sdcard_tests(struct hcom_nx_cmd_data *cmdData)
{
  uint32_t userData = cmdData->userData;
  syslog(1, "sdcard tests received %u to execute tests\n", userData);

  switch(userData)
  {
    case 100:  // format as fat32
    // Future if needed. Use NSH or PC to format FAT32
    break;

    case 101: // mount
    return hcom_nx_sdcard_mount_card();
    break;

    case 102:
    return hcom_nx_sdcard_open_test_file();
    break;

    case 103: // create test file
    return hcom_nx_sdcard_create_test_file();
    break;

    case 104:
    return hcom_nx_sdcard_close_test_file();
    break;

    case 105: // umount
    return hcom_nx_sdcard_unmount_card();
    break;

    case 106:
    return hcom_nx_sdcard_fsync_file();
    break;

    case 107:
    return hcom_nx_sdcard_read_test_file();
    break;

    case 108:
    return hcom_nx_sdcard_delete_test_file();
    break;

    default:
    syslog(1, "Unknown value %u passed to hcom_nx_exec_sdcard_tests()\n", userData);
    break;
  }

  return OK;
}

#endif  // #if HCOM_INCLUDE_SD_CARD_TESTS_IN_BUILD > 0

