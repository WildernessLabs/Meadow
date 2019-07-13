/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow_hcom_fs_helper.c
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
// This file contains a number of featureds that are specific to the Nuttx
// SmartFS file system.
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "meadow_hcom_common.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/smart.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct mtd_dev_s *_mtdPartArray[HCOM_FLASH_FILE_PARTITION_COUNT_MAX];
static uint32_t _mountedPartitionIdIs;
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

#ifndef CONFIG_MTD_PARTITION
#warning "CONFIG_MTD_PARTITION must be configured"
#endif

#ifndef CONFIG_MTD_SMART
#warning "CONFIG_MTD_SMART and CONFIG_MTD_SMART_SECTOR_SIZE are expected to be defined"
#endif

int hcom_fs_helper_setup()
{
  _mountedPartitionIdIs = HCOM_INVALID_PARTITION_ID_VALUE;
  _shutting_down = false;
  return OK;
}

void hcom_fs_helper_shutdown()
{
  _shutting_down = true;
}

//=====================================================================
// This function will create partitions, initialize the file system, mount
// the partititions and if necessary format the partition, for the entire
// external flash device
// This function may be called at startup and/or via hcom
// HOW TO MAKE THIS CONDITIONAL?
// HOW TO SAVE THE NUMBER OF PARTITIONS? So on next boot up we do the same thing.
int hcom_fs_helper_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *entire_flash_mtd,
                                                            uint32_t numbOfPartitions)
{
  int ret;
  uint32_t partCounter;

  f7syslog(LOG_INFO, "File System Creation Begun, will partition\n");

  // Create the number of partitions in the external flash
  ret = hcom_fs_helper_init_fs_partitions(entire_flash_mtd, numbOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: partitioning file system: %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_INFO, "Partitioning complete, will initialize\n");

  // Initialize the file system
  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_INFO, "Attempting to Initialize Smart file system partition %d ****\n",
             partCounter);

    ret = hcom_fs_helper_initialize_fs(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned '%d' while initializing partiton %d\n",
               __func__, ret, partCounter);
      return ret;
    }
  }

  f7syslog(LOG_INFO, "File system initialization complete, will mount\n");

  // Attempt to mount - if fails format and attempt to mount once more
  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_INFO, "Attempt to mount partition %d\n",
             partCounter);

    ret = hcom_fs_helper_mount_and_format(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: %d from mount / format attempt, partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
  }

  f7syslog(LOG_INFO, "File system mount complete. File system creation successfully completed\n");
  return OK;
}

//=====================================================================
// Attempts to mount. If this fails with an error of -ENODEV the the
// next step is to call mksmartfs (format).
int hcom_fs_helper_mount_and_format(uint32_t partitionId)
{
  int ret;

  ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId);
  if (ret < 0)
  {
    // Note: -ENODEV is may be unique to SmartFS. It is returned when smartfs is attempting
    // to mount an unformatted partition.
    if (ret != -ENODEV)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Initial mount failed '%s' to '%s' for type '%s' on PartitionID %d errno:%d\n",
               __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
               HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
      return ret;
    }

    f7syslog(LOG_INFO, "Mount attempt indicates formatting required for partition %d. Formatting begun.\n",
             partitionId);

    // Format the partition
    ret = hcom_fs_helper_format_smartfs(partitionId);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Format failed for partition %d\n", __func__, partitionId);
      return ret;
    }

    f7syslog(LOG_INFO, "Format successful for partition %d. Attempting second mount\n", partitionId);

    ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                              HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Second mount attempt failed '%s' to '%s' for type '%s' on PartitionID %d errno:%d\n",
               __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
               HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
      return ret;
    }
  }

  f7syslog(LOG_INFO, "Mount successful for partition %d.\n", partitionId);
  return OK;
}

//======================================================================
// Partitions the entire flash chip with the number of partitions specified.
// This sets the size of each partition
int hcom_fs_helper_init_fs_partitions(FAR struct mtd_dev_s *entire_flash_mtd, uint32_t numberOfPartitions)
{
  FAR struct mtd_geometry_s geo;
  off_t partitionOffset;

  if (numberOfPartitions > HCOM_FLASH_FILE_PARTITION_COUNT_MAX)
  {
    f7syslog(LOG_ERR, "%s() ERROR: The requested number of partitions %d exceeds the maximum of %d\n",
             __func__, numberOfPartitions, HCOM_FLASH_FILE_PARTITION_COUNT_MAX);
    return -1;
  }

  // Get geometry of QSPI Flash
  int ret = entire_flash_mtd->ioctl(entire_flash_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Reading MTD geometry failed: %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_DEBUG, "===> MTD Geo info - neraseblocks %u, erasesize %u programmable blocksize %u\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t blocksPerErase = geo.erasesize / geo.blocksize;
  off_t nblocks = (geo.neraseblocks / numberOfPartitions) * blocksPerErase;
  size_t partsize = nblocks * geo.blocksize;

  off_t offset = 0;
  for (partitionOffset = 0; partitionOffset < numberOfPartitions; partitionOffset++)
  {
    _mtdPartArray[partitionOffset] = mtd_partition(entire_flash_mtd, offset, nblocks);
    offset += nblocks;
    if (!_mtdPartArray[partitionOffset])
    {
      f7syslog(LOG_ERR, "%s() ERROR: mtd_partition failed. offset=%lu nblocks=%lu\n",
               __func__, (unsigned long)offset, (unsigned long)nblocks);
    }

    f7syslog(LOG_INFO, "===> Partition %d created at offset %d with size = %d bytes\n",
             partitionOffset, offset, partsize);
  }
  return OK;
}

//=====================================================================================
// This function will call smartfs_initialize
int hcom_fs_helper_initialize_fs(uint32_t partitionOffset)
{
  int ret;
  char partName[64];

  snprintf(partName, 64, "p%d", partitionOffset);
  f7syslog(LOG_INFO, "Calling smart_initialize with part name '%s' for number = %d, Part mtd = %p\n",
           partName, partitionOffset, _mtdPartArray[partitionOffset]);

  if (_mtdPartArray[partitionOffset] == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: The mtd is not in _mtdPartArray for partition %d",
             __func__, partitionOffset);
    return -1;
  }

  // result "/dev/smart0p0", "/dev/smart0p1", "/dev/smart0p2"...
  ret = smart_initialize(0, _mtdPartArray[partitionOffset], partName);
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
// Inspiritation and code from apps/fsutils/mksmartfs/makesmartfs.c
int hcom_fs_helper_format_smartfs(uint32_t partitionId)
{
  struct smart_format_s fmt;
  int ret;
  int fd;
  int x;
  uint8_t type;
  struct smart_read_write_s request;
  char finalTargetName[64];

  /* Find the inode of the block driver indentified by 'source' */

  // e.g. /dev/smart0
  snprintf(finalTargetName, 64, "%s0p%d", HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);

  f7syslog(LOG_INFO, "Formatting smartfs using '%s' for partition = %d. This can take a long time.\n",
           finalTargetName, partitionId);
  fd = open(finalTargetName, O_RDWR);
  if (fd < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to open %s returned file descriptor %d\n", __func__, finalTargetName, fd);
    return fd;
  }

  /* Perform a low-level SMART format */

#ifndef CONFIG_MTD_SMART_SECTOR_SIZE
#  define CONFIG_MTD_SMART_SECTOR_SIZE 1024
#endif

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

  f7syslog(LOG_INFO, "Formatted smartfs successful.\n");
  return OK;
}

//==============================================================================
// This method is called to mount one file system partition
int hcom_fs_helper_mount_partitioned_fs(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId)
{
  int ret;
  char finalSourceName[64];
  char finalTargetName[64];

  if (_shutting_down)
    return OK;

  // e.g. /dev/smart0
  snprintf(finalSourceName, 64, "%s0p%d", sourceDevice, partitionId);
  // e.g. /meadow0
  snprintf(finalTargetName, 64, "%s%d", targetDevice, partitionId);

  f7syslog(LOG_INFO, "Attempting to mount '%s' to '%s' for type '%s' for partition %d\n",
           finalSourceName, finalTargetName, fileSystemType, partitionId);

  // e.g. mount("/dev/ram0", "/mnt", "vfat", 0, NULL);  // Needs backing block device
  // e.g. mount(NULL, "/mnt", "nxffs", 0, NULL);        // When no backing block device
  ret = mount(finalSourceName, finalTargetName, fileSystemType, 0, NULL);
  if (ret < 0)
  {
    // Ths mount function puts the error code into errno
    int errnumb = get_errno();
    return -errnumb;
  }

  f7syslog(LOG_INFO, "Successfully mounted '%s' to '%s' for type '%s'\n",
        finalSourceName, finalTargetName, fileSystemType);

  _mountedPartitionIdIs = partitionId;
  return OK;
}

//======================================================================
// Verify that the entire chip contains 0xff
int hcom_fs_helper_verify_erased_flash(FAR struct mtd_dev_s *entire_flash_mtd)
{
// because I know blocksize = 256 and 4096 is required multiple
#define HCOM_MULTIPLER_TO_REDUCE_OVERHEAD (16)

  FAR struct mtd_geometry_s geo;
  int blockCounter;
  int errorBlocks;
  int ret;

  // Get geometry of QSPI Flash
  ret = entire_flash_mtd->ioctl(entire_flash_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Read geometry for MTD failed: %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_INFO, "Verifying if entire MTD is erased, neraseblocks %d, erasesize %d blocksize %d\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  syslog(0, "In %s() allocating %d bytes and %d bytes total=%d\n", __func__,
    geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD,
    geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD,
    geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD + geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD);
  usleep(15 * 1000);

  uint8_t *readBuffer = (uint8_t *)malloc(geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD);
  uint8_t *baseReference = (uint8_t *)malloc(geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD);
  memset(baseReference, 0xff, geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD); // base line for erased flash

  errorBlocks = 0;

  for (blockCounter = 0; blockCounter < geo.neraseblocks; blockCounter++)
  {
    // nread   = MTD_BREAD(dev->mtd, startblock, nblocks, readBuffer);
    size_t blocksRead = MTD_BREAD(entire_flash_mtd, blockCounter * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD,
                                  HCOM_MULTIPLER_TO_REDUCE_OVERHEAD, readBuffer);
    if (blocksRead == HCOM_MULTIPLER_TO_REDUCE_OVERHEAD)
    {
      ret = memcmp(baseReference, readBuffer, geo.blocksize * HCOM_MULTIPLER_TO_REDUCE_OVERHEAD);
      if (ret != 0)
      {
        f7syslog(LOG_WARNING, "%s() WARNING: Block %04d not erased\n", __func__, blockCounter);
        errorBlocks++;
      }
      continue;
    }

    if (blocksRead == 0)
      break; // End of data

    f7syslog(LOG_ERR, "%s() ERROR: Expected to read %d blocks but got %d blocks\n",
             __func__, HCOM_MULTIPLER_TO_REDUCE_OVERHEAD, blocksRead);
    break;
  }

  f7syslog(LOG_INFO, "Verified %04d bytes (%d of %d blocks)\n",
           blockCounter * geo.erasesize, blockCounter, geo.neraseblocks);

  free(baseReference);
  free(readBuffer);

  return errorBlocks;
}

//=====================================================================
//
bool hcom_fs_helper_is_fs_mounted(uint32_t partitionId)
{
  return (_mountedPartitionIdIs != HCOM_INVALID_PARTITION_ID_VALUE);
}
