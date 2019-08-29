/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_fs_helper.c
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

#include "hcom_common.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/smart.h>
#include <dirent.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define HCOM_TEMP_FILE_NAME_BUFFER_LEN 64

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

static int hcom_fs_helper_mount_and_format(uint32_t partitionId);
static int initialize_file_system_on_bootup(FAR struct mtd_dev_s *master_flash_mtd);

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

int hcom_fs_helper_setup(FAR struct mtd_dev_s *mtd)
{
  _mountedPartitionIdIs = HCOM_INVALID_PARTITION_ID_VALUE;
  _shutting_down = false;

#if 1
  int ret = initialize_file_system_on_bootup(mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: SmartFS file system initialization returned error '%d' \n",
              __func__, ret);
    return ret;
  }
#endif

  return OK;
}

void hcom_fs_helper_shutdown()
{
  _shutting_down = true;
}

//==================================================================
// The SmartFS file system must be initialized with the right number
// of partitions at startup
int initialize_file_system_on_bootup(FAR struct mtd_dev_s *master_flash_mtd)
{
  //uint32_t partCounter;
  int ret;

  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(master_flash_mtd, HCOM_NUMBER_OF_FS_PARTITIONS);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Creation of SmartFS returned error '%d'\n", __func__, ret);
    return ret;
  }

  return OK;
}

//=====================================================================
// This function will create partitions, initialize the file system, mount
// the partititions and if necessary format the partition, for the entire
// external flash device
// This function may be called at startup and/or via hcom
// HOW TO MAKE THIS CONDITIONAL?
// HOW TO SAVE THE NUMBER OF PARTITIONS? So on next boot up we do the same thing.
int hcom_fs_helper_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *master_flash_mtd,
                                                            uint32_t numbOfPartitions)
{
  int ret;
  uint32_t partCounter;

  f7syslog(LOG_INFO, "fs->File System Creation, step 1: create %d partitions\n", numbOfPartitions);

  // Create the number of partitions in the external flash
  ret = hcom_fs_helper_init_fs_partitions(master_flash_mtd, numbOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: partitioning file system: %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_INFO, "fs->Partitioning complete, step 2: initialize all partitions\n");

  // Initialize the file system
  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_DEBUG, "Attempting to Initialize Smart file system partition %d\n",
             partCounter);

    ret = hcom_fs_helper_initialize_fs(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
  }

  f7syslog(LOG_INFO, "fs->File system initialization complete, step 3: mount and format, if needed\n");

  // Attempt to mount - if fails format and attempt to mount once more
  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_INFO, "fs->Attempt to mount partition %d\n",
             partCounter);

    ret = hcom_fs_helper_mount_and_format(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: %d from mount / format attempt, partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
  }

  f7syslog(LOG_INFO, "fs->File system mount complete. File system creation successfully completed\n");
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
    ret = hcom_fs_helper_format_smartfs(partitionId);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Format failed for partition %d\n", __func__, partitionId);
      return ret;
    }

    f7syslog(LOG_INFO, "fs->Format successful for partition %d. Attempting second mount\n", partitionId);

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

  f7syslog(LOG_INFO, "fs->Mount successful for partition %d.\n", partitionId);
  return OK;
}

//======================================================================
// Partitions the entire flash chip with the number of partitions specified.
// This sets the size of each partition
int hcom_fs_helper_init_fs_partitions(FAR struct mtd_dev_s *master_flash_mtd, uint32_t numberOfPartitions)
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
  int ret = master_flash_mtd->ioctl(master_flash_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Reading MTD geometry failed: %d\n", __func__, ret);
    return ret;
  }

  f7syslog(LOG_DEBUG, "MTD Geo info - neraseblocks %u, erasesize %u programmable blocksize %u\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t blocksPerErase = geo.erasesize / geo.blocksize;
  off_t nblocks = (geo.neraseblocks / numberOfPartitions) * blocksPerErase;
  size_t partsize = nblocks * geo.blocksize;

  off_t offset = 0;
  for (partitionOffset = 0; partitionOffset < numberOfPartitions; partitionOffset++)
  {
    _mtdPartArray[partitionOffset] = mtd_partition(master_flash_mtd, offset, nblocks);
    offset += nblocks;
    if (!_mtdPartArray[partitionOffset])
    {
      f7syslog(LOG_ERR, "%s() ERROR: mtd_partition failed. offset=%lu nblocks=%lu\n",
               __func__, (unsigned long)offset, (unsigned long)nblocks);
    }

    f7syslog(LOG_INFO, "fs->Partition %d created at offset %d with size = %d bytes\n",
             partitionOffset, offset, partsize);
  }
  return OK;
}

//=====================================================================================
// This function will call smartfs_initialize
int hcom_fs_helper_initialize_fs(uint32_t partitionOffset)
{
  int ret;
  char partName[HCOM_TEMP_FILE_NAME_BUFFER_LEN];

  snprintf(partName, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "p%d", partitionOffset);
  f7syslog(LOG_INFO, "fs->Calling smart_initialize with part name '%s' for number = %d, Part mtd = %p\n",
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

  f7syslog(LOG_INFO, "fs->SUCCESS Smart Initialization - Part number = %d\n",
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
  char fullMountPtName[HCOM_MAX_FILE_PATH_NAME_LENGTH];

  /* Find the inode of the block driver indentified by 'source' */

  // e.g. /dev/smart0
  ret = snprintf(fullMountPtName, HCOM_MAX_FILE_PATH_NAME_LENGTH, "%s0p%d", HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);
  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size.  
  if (ret < 0 || ret >= HCOM_MAX_FILE_PATH_NAME_LENGTH - 1)
  {
    f7syslog(LOG_ERR, "%s() ERROR: buffer to build %s with partition name %d, too small\n",
        __func__, HCOM_FILE_MOUNT_POINT_SOURCE, partitionId);
    return -E2BIG;
  }
  
  f7syslog(LOG_WARNING, "fs->Formatting smartfs using '%s' for partition %d. This will take several minutes.\n",
           fullMountPtName, partitionId);

  char *formatMessage;
  formatMessage = "The file system indicates that formatting is required. The will take several minutes.";
  ret = hcom_host_msg_bldr_send_text(formatMessage, strlen(formatMessage));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }

  fd = open(fullMountPtName, O_RDWR);
  if (fd < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: call to open %s returned file descriptor %d\n", __func__, fullMountPtName, fd);
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

  f7syslog(LOG_WARNING, "fs->Formatted smartfs successful.\n");
  return OK;
}

//==============================================================================
// This method is called to mount one file system partition
int hcom_fs_helper_mount_partitioned_fs(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId)
{
  int ret;
  char finalSourceName[HCOM_TEMP_FILE_NAME_BUFFER_LEN];
  char fullMountPtName[HCOM_TEMP_FILE_NAME_BUFFER_LEN];

  if (_shutting_down)
    return OK;

  // e.g. /dev/smart0
  snprintf(finalSourceName, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s0p%d", sourceDevice, partitionId);
  // e.g. /meadow0
  snprintf(fullMountPtName, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s%d", targetDevice, partitionId);

  f7syslog(LOG_INFO, "fs->Attempting to mount '%s' to '%s' for type '%s' for partition %d\n",
           finalSourceName, fullMountPtName, fileSystemType, partitionId);

  // e.g. mount("/dev/ram0", "/mnt", "vfat", 0, NULL);  // Needs backing block device
  // e.g. mount(NULL, "/mnt", "nxffs", 0, NULL);        // When no backing block device
  ret = mount(finalSourceName, fullMountPtName, fileSystemType, 0, NULL);
  if (ret < 0)
  {
    // Ths mount function puts the error code into errno
    int errnumb = get_errno();
    return -errnumb;
  }

  f7syslog(LOG_INFO, "fs->Successfully mounted '%s' to '%s' for type '%s'\n",
        finalSourceName, fullMountPtName, fileSystemType);

  _mountedPartitionIdIs = partitionId;
  return OK;
}

//=====================================================================
//
bool hcom_fs_helper_is_fs_mounted(uint32_t partitionId)
{
  return (_mountedPartitionIdIs != HCOM_INVALID_PARTITION_ID_VALUE);
}

//=====================================================================
int hcom_fs_helper_get_list_files_in_partition(uint32_t partitionId, char *csvList, int csvListLen)
{
  int csvBufferOff = 0;
  char fullMountPtName[HCOM_TEMP_FILE_NAME_BUFFER_LEN];
  char fileListBuff[HCOM_TEMP_FILE_NAME_BUFFER_LEN];
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

  snprintf(fullMountPtName, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    f7syslog(LOG_ERR, "ERROR: opendir(\"%s\") failed with errno=%d\n", fullMountPtName, errno);
    return -1;
  }

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      // Get the next file name
      f7syslog(LOG_INFO, "fs->Found file '%s' in partition %d\n", direntry->d_name, partitionId);

      int fileNameLen;
      if(firstFile)
      {
        fileNameLen = snprintf(fileListBuff, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s/%s", fullMountPtName, direntry->d_name);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_TEMP_FILE_NAME_BUFFER_LEN, ",%s/%s", fullMountPtName, direntry->d_name);
      }

      if(csvBufferOff + fileNameLen > csvListLen - 1)
      {
        f7syslog(LOG_ERR, "ERROR: while building file name list, ran out of buffer space.\n");
        closedir(dirp);
        return -1;
      }

      // Add this file name to the list
      strcpy(csvList + csvBufferOff, fileListBuff);
      csvBufferOff += fileNameLen;
    }
  }

  csvList[csvBufferOff] = '\0';
  closedir(dirp);

  return OK;
}

//=====================================================================
int hcom_fs_helper_get_list_files_in_partition_and_crc(uint32_t partitionId, char *csvList, int csvListLen)
{
  int csvBufferOff = 0;
  char fullMountPtName[HCOM_TEMP_FILE_NAME_BUFFER_LEN];
  char fileListBuff[HCOM_TEMP_FILE_NAME_BUFFER_LEN];
  char completeNameBuf[128];
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

  snprintf(fullMountPtName, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);

  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    f7syslog(LOG_ERR, "ERROR: opendir '%s' failed with errno=%d\n", fullMountPtName, errno);
    return -1;
  }

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      snprintf(completeNameBuf, 128, "%s/%s", fullMountPtName, direntry->d_name);
      
      // Find the checksum
      uint32_t crcChecksum = hcom_file_commands_calc_crc_for_file(completeNameBuf);

      f7syslog(LOG_INFO, "fs->Found file '%s' in partition %d with checksum 0x%08x\n", direntry->d_name, partitionId, crcChecksum);

      // Add this file to the csv list 
      int fileNameLen = 0;

      if(firstFile)
      {
        fileNameLen = snprintf(fileListBuff, HCOM_TEMP_FILE_NAME_BUFFER_LEN, "%s/%s [0x%08x]",
            fullMountPtName, direntry->d_name, crcChecksum);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_TEMP_FILE_NAME_BUFFER_LEN, ",%s/%s [0x%08x]",
            fullMountPtName, direntry->d_name, crcChecksum);
      }

      if(csvBufferOff + fileNameLen > csvListLen - 1)
      {
        f7syslog(LOG_ERR, "ERROR: while building file name list, ran out of buffer space.\n");
        closedir(dirp);
        return -1;
      }

      // Add this file name to the list
      strcpy(csvList + csvBufferOff, fileListBuff);
      csvBufferOff += fileNameLen;
    }
  }

  csvList[csvBufferOff] = '\0';
  closedir(dirp);

  return OK;
}
