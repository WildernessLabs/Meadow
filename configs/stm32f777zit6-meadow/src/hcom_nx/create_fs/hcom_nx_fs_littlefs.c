/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\create_fs\hcom_nx_fs_littlefs.c
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

// This module contains code to support LittleFS

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

#ifdef CONFIG_FS_LITTLEFS

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include "../../../fs/littlefs/lfs.h"   // There must be a better way!
#include <dirent.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

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
// It will register the littlefs driver.
int hcom_nx_create_littlefs_support_init_master(FAR struct mtd_dev_s *master_flash_mtd)
{
  int ret;

  syslog(LOG_DEBUG, "%s@%d-Register master mtd\n", thisFile, __LINE__);

  char *finalSourceName = malloc(HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
#ifdef CONFIG_MTD_PARTITION
  // Register the MTD driver so that it can be accessed from the VFS
  // master mtd becomes '/dev/little0'
  int stringLen = snprintf(finalSourceName, HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH,
            "%s0", HCOM_NX_FILE_MOUNT_POINT_SOURCE);
  DEBUGASSERT(stringLen < HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  // Since there are no partitions we register as '/dev/little'
  DEBUGASSERT(strlen(HCOM_NX_FILE_MOUNT_POINT_SOURCE) < HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
  strncpy(finalSourceName, HCOM_NX_FILE_MOUNT_POINT_SOURCE, HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
#endif

  ret = register_mtddriver(finalSourceName, master_flash_mtd, 0755, NULL);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-register_mtddriver() ret:%d, errno:%d\n",
            thisFile, __LINE__, ret, errno);
    free(finalSourceName);
    return ret;
  }
  
  free(finalSourceName);
  return OK;
}

#ifdef CONFIG_MTD_PARTITION
//=====================================================================================
// Each partition is initialized here. The individual partitions have already been created
int hcom_nx_create_littlefs_init_1_part(uint32_t partitionId, struct mtd_dev_s *partMtd)
{
  char *partName = malloc(HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
  int ret;
  int stringLen;

  syslog(LOG_DEBUG, "%s@%d-Registering part %d\n", thisFile, __LINE__, partitionId);

  if (partMtd == NULL)
  {
    syslog(LOG_ERR, "%s@%d-mtd is NULL, part %d",
             thisFile, __LINE__, partitionId);
    free(partName);
    return -1;
  }

  // result "/dev/little0p0", "/dev/little0p1", "/dev/little0p2"...
  stringLen = snprintf(partName, HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s0p%d",
          HCOM_NX_FILE_MOUNT_POINT_SOURCE, partitionId);
  DEBUGASSERT(stringLen < HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
  syslog(LOG_DEBUG, "%s@%d-Register part %d as '%s'. MTD:%p\n",
           thisFile, __LINE__, partitionId, partName, partMtd);

  // Register the MTD driver so that it can be accessed from the VFS
  ret = register_mtddriver(partName, partMtd, 0755, partMtd);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-register_mtddriver() ret:%d, errno:%d\n",
            thisFile, __LINE__, ret, errno);
    free(partName);
    return ret;
  }

  free(partName);
  return OK;
}
#endif

//=====================================================================================
// Mount each partition and format, if needed
int hcom_nx_create_littlefs_mount_format_1_part(uint32_t partitionId)
{
  int ret;

  syslog(LOG_DEBUG, "%s@%d-LittleFS mount part %d\n", thisFile, __LINE__, partitionId);

  // For LittleFS a mount failure with a specific error return indicates formatting is needed
  ret = hcom_nx_create_fs_mount(HCOM_NX_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE, partitionId, NULL);
  if(ret >= 0)
    return OK;   // Mount successful

  // Note: LFS_ERR_CORRUPT (-52) is unique to LittleFS. It is returned when LittleFS is attempting
  // to mount a partition and it detects that the partition is not formatted.
  if (ret != LFS_ERR_CORRUPT)
  {
    syslog(LOG_ERR, "%s@%d-LittleFS mount for '%s' type '%s' on Part %d err:%d\n",
              thisFile, __LINE__, HCOM_FILE_MOUNT_POINT_TARGET,
              HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
    return ret;
  }

  syslog(LOG_DEBUG, "Part %d format required\n", partitionId);

  // This call will format then mount
  // The last argument causes LittleFS to format and then mount.
  // The last parameter is ultimately passed to the lfs_vfs.c, the littlefs_bind() function. 
  ret = hcom_nx_create_fs_mount(HCOM_NX_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            HCOM_NX_FILE_MOUNT_FORCE_FORMAT);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Format and remount '%s' to '%s' for type '%s' on Part %d err:%d\n",
              thisFile, __LINE__, HCOM_NX_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
              HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
  }

  syslog(LOG_DEBUG, "Part %d formatted\n", partitionId);
  return ret;
}
#endif
