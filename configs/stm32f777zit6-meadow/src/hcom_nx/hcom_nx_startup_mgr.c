/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_startup_mgr.c
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

// This module is called during nuttx startup

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "hcom_nx_common.h"
#include <assert.h>

#if defined (CONFIG_FS_PROCFS)
#include "stm32f777zit6-meadow.h"
#endif

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
// Note: definition was added to stm32_boot.c
int hcom_nx_setup_mgr(FAR struct mtd_dev_s *mtd)
{
  int ret;

  if (mtd == NULL)
  {
    return ERROR;
  }

  ret = hcom_nx_utils_startup_handling_of_trace_level();
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to initialize syslog level:%d\n", ret);
    return ret;
  }

#if (defined (CONFIG_FS_PROCFS) && defined (CONFIG_SYSTEM_NSH))
  ret = mount(NULL, STM32_PROCFS_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0)
  {
    syslog(LOG_ERR, "ERROR: Failed to mount procfs at %s: %d\n",
            STM32_PROCFS_MOUNTPOINT, ret);
    return ret;
  }
#endif

  // Initialize hcom nuttx driver
  ret = hcom_nx_upd_initialize();
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup hcom nuttx upd:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Saves a copy of mtd
  ret = hcom_nx_exec_ex_flash_setup(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup misc %d\n", thisFile, __LINE__, ret);
    return ret;
  }

// Initialize the file system as needed
#if defined(CONFIG_HCOM_FILESYSTEM_INIT)    // defined in menuconfig
  ret = hcom_nx_create_fs_initialize(mtd);
  if (ret < 0)
  {
    syslog(LOG_CRIT, "%s@%d-setup F/S helper %d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  return OK;
}
