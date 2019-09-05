/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_smartfs_support.c
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

#ifdef CONFIG_FS_SMARTFS

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/smart.h>
#include <dirent.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Implementation
 ****************************************************************************/

#ifndef CONFIG_MTD_SMART_SECTOR_SIZE
#  define CONFIG_MTD_SMART_SECTOR_SIZE 1024
#endif

//=====================================================================================
int hcom_smartfs_support_setup()
{
  _shutting_down = false;
  return OK;
}

//=====================================================================================
void hcom_smartfs_support_shutdown()
{
  _shutting_down = true;
}

//=====================================================================================
// This function will call smartfs_initialize
int hcom_smartfs_support_initialize_fs(uint32_t partitionOffset, struct mtd_dev_s *partMtd)
{
  int ret;
  char partName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];

  snprintf(partName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "p%d", partitionOffset);
  f7syslog(LOG_INFO, "Calling smart_initialize with part name '%s' for number = %d, Part mtd = %p\n",
           partName, partitionOffset, partMtd);

  if (partMtd == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: The mtd is NULL for partition %d",
             __func__, partitionOffset);
    return -1;
  }

  // result "/dev/smart0p0", "/dev/smart0p1", "/dev/smart0p2"...
  ret = smart_initialize(0, partMtd, partName);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Smart Initialize for partition %d, ret = %d\n",
             __func__, partitionOffset, ret);
    return ret;
  }

  f7syslog(LOG_INFO, "SUCCESS Smart Initialization - Part number = %d\n",
           partitionOffset);

  return OK;
}

//=====================================================================================
int hcom_smartfs_support_mount_format(uint32_t partitionId)
{
  int ret;

  // For smartfs a mount failure with a specific error return indicates formatting is needed
  ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId);
  if (ret < 0)
  {
    // Note: -ENODEV may be unique to SmartFS. It is returned when smartfs is attempting
    // to mount a partition and it detects that the partion is not formatted.
    if (ret != -ENODEV)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Initial mount failed '%s' to '%s' for type '%s' on PartitionID %d Error %d\n",
                __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
      return ret;
    }

    f7syslog(LOG_INFO, "fs->Mount attempt indicates formatting required for partition %d. Formatting begun.\n",
              partitionId);

    // Format the partition
    ret = hcom_smartfs_support_format(partitionId);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Format SmartFS failed for partition %d\n", __func__, partitionId);
      return ret;
    }

    f7syslog(LOG_INFO, "fs->Format successful for partition %d. Attempting second mount\n", partitionId);

    // Attempt to mount again
    ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                              HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Second mount attempt failed '%s' to '%s' for type '%s' on PartitionId %d Error %d\n",
                __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
      return ret;
    }
  }

  return ret;
}

//======================================================================
// Format for smartfs here
int hcom_smartfs_support_format(int partitionId)
{
  // Inspiritation and code originally from apps/fsutils/mksmartfs/makesmartfs.c
  struct smart_format_s fmt;
  int ret;
  int fd;
  int x;
  uint8_t type;
  struct smart_read_write_s request;
  char fullMountPtName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];

  /* Find the inode of the block driver indentified by 'source' */

  // e.g. /dev/smart0
  ret = snprintf(fullMountPtName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s0p%d",
      HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size.
  if (ret < 0 || ret >= HCOM_MAX_FILE_PATH_BUFF_LENGTH - 1)
  {
    f7syslog(LOG_ERR, "%s() ERROR: buffer to build %s with partition name %d, too small\n",
        __func__, HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);
    return -E2BIG;
  }
  
  f7syslog(LOG_WARNING, "Formatting smartfs partition %d. This may take up to 25 minutes.\n",
           partitionId);

  // Communications isn't setup yet
  // char *formatMessage;
  // formatMessage = "The file system indicates that formatting is required. This will take about 15 minutes.";
  // ret = hcom_host_msg_bldr_send_text(formatMessage, strlen(formatMessage));
  // if (ret < 0)
  // {
  //   f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  // }

  fd = open(fullMountPtName, O_RDWR);
  if (fd < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to open %s returned file descriptor %d\n", __func__, fullMountPtName, fd);
    return fd;
  }

  /* Perform a low-level SMART format */

  ret = ioctl(fd, BIOC_LLFORMAT, CONFIG_MTD_SMART_SECTOR_SIZE << 16);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to ioctl BIOC_LLFORMAT returned error %d\n", __func__, ret);
    return ret;
  }

  /* Get the format information so we know how big the sectors are */

  ret = ioctl(fd, BIOC_GETFORMAT, (unsigned long)&fmt);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to ioctl BIOC_GETFORMAT returned error %d\n", __func__, ret);
    return ret;
  }

  /* Now Write the filesystem to media.  Loop for each root dir entry and
   * allocate the reserved Root Dir Enty, then write a blank root dir for it.
   */

  type = SMARTFS_SECTOR_TYPE_DIR;
  request.offset = 0;
  request.count = 1;
  request.buffer = &type;
  x = 0;
  ret = ioctl(fd, BIOC_ALLOCSECT, SMARTFS_ROOT_DIR_SECTOR + x);
  if (ret != SMARTFS_ROOT_DIR_SECTOR + x)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to ioctl BIOC_ALLOCSECT returned error %d\n", __func__, ret);
    return -EIO;
  }

  /* Mark this block as a directory entry */

  request.logsector = SMARTFS_ROOT_DIR_SECTOR + x;

  /* Issue a write to the sector, single byte */

  ret = ioctl(fd, BIOC_WRITESECT, (unsigned long)&request);
  if (ret != 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to ioctl BIOC_WRITESECT returned error %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_WARNING, "Formatted smartfs successful.\n");
  return OK;
}
#endif