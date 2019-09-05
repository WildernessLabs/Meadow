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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hcom_common.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <dirent.h>

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

//==================================================================
int hcom_fs_helper_setup(FAR struct mtd_dev_s *mtd)
{
  _mountedPartitionIdIs = HCOM_INVALID_PARTITION_ID_VALUE;
  _shutting_down = false;

#if 1
  int ret = initialize_file_system_on_bootup(mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: file system initialization returned error '%d' \n",
              __func__, ret);
    return ret;
  }
#endif

#ifdef CONFIG_FS_SMARTFS
  hcom_smartfs_support_setup();
#endif
  return OK;
}

//==================================================================
void hcom_fs_helper_shutdown()
{
  _shutting_down = true;

#ifdef CONFIG_FS_SMARTFS
  hcom_smartfs_support_shutdown();  
#endif
}

//==================================================================
// The file system must be initialized with the right number
// of partitions at startup
int initialize_file_system_on_bootup(FAR struct mtd_dev_s *master_flash_mtd)
{
  int ret;

  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(master_flash_mtd, HCOM_NUMBER_OF_FS_PARTITIONS);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Creation of file system returned error '%d'\n", __func__, ret);
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
    f7syslog(LOG_DEBUG, "Attempting to Initialize file system partition %d\n",
             partCounter);

#ifdef CONFIG_FS_SMARTFS
    ret = hcom_smartfs_support_initialize_fs(partCounter, _mtdPartArray[partCounter]);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
#endif
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
// Attempt to mount. Each file system type may do this differently.
int hcom_fs_helper_mount_and_format(uint32_t partitionId)
{
  int ret;

#ifdef CONFIG_FS_SMARTFS
  ret = hcom_smartfs_support_mount_format(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Initial mount failed '%s' to '%s' for type '%s' on PartitionID %d Error %d\n",
          __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
          HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
    return ret;
  }
#endif

  f7syslog(LOG_INFO, "fs->Mount successful for partition %d. Format not required.\n", partitionId);
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

//==============================================================================
// This method is called to mount one file system partition
int hcom_fs_helper_mount_partitioned_fs(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId)
{
  int ret;
  char finalSourceName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  char fullMountPtName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];

  if (_shutting_down)
    return OK;

  // e.g. /dev/smart0 or dev/little0
  snprintf(finalSourceName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s0p%d", sourceDevice, partitionId);
  // e.g. /meadow0
  snprintf(fullMountPtName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s%d", targetDevice, partitionId);

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
  char fullMountPtName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  char fileListBuff[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

  snprintf(fullMountPtName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
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
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s/%s", fullMountPtName, direntry->d_name);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_FILE_PATH_BUFF_LENGTH, ",%s/%s", fullMountPtName, direntry->d_name);
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
  char fullMountPtName[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  char fileListBuff[HCOM_MAX_FILE_PATH_BUFF_LENGTH];
  char completeNameBuf[128];
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

  snprintf(fullMountPtName, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);

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
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_FILE_PATH_BUFF_LENGTH, "%s/%s [0x%08x]",
            fullMountPtName, direntry->d_name, crcChecksum);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_FILE_PATH_BUFF_LENGTH, ",%s/%s [0x%08x]",
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

//=====================================================================================
int hcom_fs_helper_fs_initialize_proxy(uint32_t partitionOffset)
{
#ifdef CONFIG_FS_SMARTFS
  int ret = hcom_smartfs_support_initialize_fs(partitionOffset, _mtdPartArray[partitionOffset]);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing partition %d\n",
              __func__, ret, partitionOffset);
    return ret;
  }
#endif
  return OK;
}

//=====================================================================================
int hcom_fs_helper_format_fs_proxy(uint32_t partitionOffset)
{
#ifdef CONFIG_FS_SMARTFS
  int ret = hcom_smartfs_support_format(partitionOffset);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format SmartFS failed for partition %d\n", __func__, partitionOffset);
    return ret;
  }
#endif
  return OK;
}
