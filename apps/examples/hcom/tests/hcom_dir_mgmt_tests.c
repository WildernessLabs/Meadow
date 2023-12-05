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

void meadow_dir_mgmt_test_nested_directory(uint32_t userData);

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

  syslog(1, "Directory management received 'set developer -d 13 -v %lu'\n", userData);

  switch (userData)
  {
  case 1:
    meadow_dir_mgmt_test_nested_directory(userData);
    break;
  
      // Test getting total available external flash size in bytes
    case 2:
      ret = meadow_read_file_total_free_flash_size(&totalBytes, &freeBytes);
      if(ret < 0)
      {
        syslog(1, "Error: calling meadow_read_file_total_free_flash_size(), ret:%d, errno:%d\n",
                  ret, errno);
        return;
      }
      usedBytes = totalBytes - freeBytes;
      syslog(1, "--> Total bytes:%lu (%luMb), Free bytes:%lu (%luMb), Used bytes:%lu (%luMb)\n",
                totalBytes, totalBytes/(1024*1024),
                freeBytes, freeBytes/(1024*1024),
                usedBytes, usedBytes/(1024*1024));
      break;

    case 3:
      hcom_file_dir_nested_dev_dir_and_files_start();
      // show_directories("/meadow0", 1);
      break;


  default:
    break;
  }
}

//=======================================================================
void meadow_dir_mgmt_test_nested_directory(uint32_t userData)
{
  int ret;
  
  syslog(2, "Test - Nested Directory, Devices and Files\n");

  ret = hcom_file_dir_nested_dev_dir_and_files_start();
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:ret:%d, errno:%d\n", thisFile, __LINE__, ret, errno);
  }
}

#endif      // #if defined (CONFIG_DIR_MGMT_TESTS)

