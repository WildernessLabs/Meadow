/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_littlefs_support.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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

#include "hcom_common.h"

#ifdef CONFIG_FS_LITTLEFS

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
//#include <nuttxfs/littlefs/lfs.h>
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

static bool _shutting_down;
static bool _first_init_master_fs;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Implementation
 ****************************************************************************/

//=====================================================================================
int hcom_littlefs_support_setup()
{
  _shutting_down = false;
  _first_init_master_fs = true;

  return OK;
}

//=====================================================================================
void hcom_littlefs_support_shutdown()
{
  _shutting_down = true;
}

//=====================================================================================
// This function will setup the parent mtd. The partitions will be created shortly.
int hcom_little_support_init_master_fs(FAR struct mtd_dev_s *master_flash_mtd)
{
  int ret;
  char finalSourceName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];

  f7syslog(LOG_DEBUG, "%s() - Will register master mtd as parent\n", __func__);

  // This check is needed because the host can call here and once is enough.
  if(_first_init_master_fs)
  {
    _first_init_master_fs = false;

    // Register the MTD driver so that it can be accessed from the VFS
    // master mtd becomes '/dev/little0'
    snprintf(finalSourceName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s0", HCOM_FILE_MOUNT_POINT_SOURCE);
    ret = register_mtddriver(finalSourceName, master_flash_mtd, 0755, NULL);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: register_mtddriver() failed with ret = %d, errno = %d\n",
              __func__, ret, errno);
      return ret;
    }
  }

  return OK;
}

//=====================================================================================
// This function will call littlefs_initialize for each partition.
// The individual partitions have already been created
int hcom_littlefs_support_init_part_fs(uint32_t partitionId, struct mtd_dev_s *partMtd)
{
  char partName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  int ret;

  f7syslog(LOG_DEBUG, "%s() - Registering partition %d\n", __func__, partitionId);

  if (partMtd == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: The mtd is NULL for partition %d",
             __func__, partitionId);
    return -1;
  }

  // result "/dev/little0p0", "/dev/little0p1", "/dev/little0p2"...
  snprintf(partName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s0p%d", HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);
  f7syslog(LOG_DEBUG, "Will register partition %d as '%s'. Part mtd = %p\n",
           partitionId, partName, partMtd);

  // Register the MTD driver so that it can be accessed from the VFS
  ret = register_mtddriver(partName, partMtd, 0755, partMtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: register_mtddriver() failed with ret = %d, errno = %d\n",
            __func__, ret, errno);
    return ret;
  }

  return OK;
}

//=====================================================================================
// Mount each individual partiton
int hcom_littlefs_support_mount_format(uint32_t partitionId)
{
  int ret;

  f7syslog(LOG_DEBUG, "%s() - Mount partition %d for LittleFS\n", __func__, partitionId);

  // For LittleFS a mount failure with a specific error return indicates formatting is needed
  ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, NULL);
  if(ret >= 0)
    return OK;   // Mount successful
    
  // Note: LFS_ERR_CORRUPT (-52) is unique to LittleFS. It is returned when LittleFS is attempting
  // to mount a partition and it detects that the partition is not formatted.
  if (ret != LFS_ERR_CORRUPT)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS initial mount failed for '%s' type '%s' on PartitionID %d Error %d\n",
              __func__, HCOM_FILE_MOUNT_POINT_TARGET,
              HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
    return ret;
  }

  f7syslog(LOG_WARNING, "fs->Mount attempt indicates partition %d formatting required\n",
            partitionId);

  // This call will format then mount
  ret = hcom_littlefs_support_format_and_mount(partitionId);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format failed for '%s' type '%s' on PartitionID %d Error %d\n",
              __func__, HCOM_FILE_MOUNT_POINT_TARGET,
              HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
  }

  f7syslog(LOG_WARNING, "fs->Partition %d formatting completed\n", partitionId);

  return ret;
}

//======================================================================
// Format
int hcom_littlefs_support_format_and_mount(int partitionId)
{
  int ret;

  // The last argument here will cause LittleFS to format and then mount.
  // The last parameter is passed to the lfs_vfs.c, the littlefs_bind() function. 
  ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            HCOM_FILE_MOUNT_FORCE_FORMAT);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format and second mount attempt failed '%s' to '%s' for type '%s' on PartitionId %d Error %d\n",
              __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
              HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
  }
  return ret;
}

#endif