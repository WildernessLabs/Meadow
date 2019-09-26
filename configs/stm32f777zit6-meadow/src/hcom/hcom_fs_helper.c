/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_fs_helper.c
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

#if ((!defined CONFIG_FS_SMARTFS) && (!defined CONFIG_FS_LITTLEFS))
#warning "Neither SmartFS nor LittleFS are configured. No file system exists."
#endif

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <dirent.h>

#ifdef CONFIG_FS_SMARTFS
#if CONFIG_SMARTFS_MAXNAMLEN < HCOM_MIN_EXPECTED_CONFIG_SMARTFS_MAXNAMLEN
#warning "Maximum SmartFS file name length less than 32. Change CONFIG_SMARTFS_MAXNAMLEN"
#endif
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct mtd_dev_s *_master_mtd;
static FAR struct mtd_dev_s *_mtdPartArray[HCOM_FLASH_FILE_PARTITION_COUNT_MAX];
static bool _mountedPartitionId[HCOM_FLASH_FILE_PARTITION_COUNT_MAX];
static bool _shutting_down;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_fs_helper_init_fs_on_boot(FAR struct mtd_dev_s *master_flash_mtd);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Implementation
 ****************************************************************************/

//==================================================================
int hcom_fs_helper_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  for(int i = 0; i < HCOM_FLASH_FILE_PARTITION_COUNT_MAX; i++)
  {
    _mtdPartArray[i] = NULL;
    _mountedPartitionId[i] = false;
  }

  _shutting_down = false;

#ifdef CONFIG_FS_SMARTFS
  hcom_smartfs_support_setup();
#endif

#ifdef CONFIG_FS_LITTLEFS
  hcom_littlefs_support_setup();
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

#ifdef CONFIG_FS_LITTLEFS
  hcom_littlefs_support_shutdown();  
#endif
}

//==================================================================
// todo - Currently called before the hcom thread is created
int hcom_fs_helper_init_file_system()
{
  int ret;

#ifdef CONFIG_FS_LITTLEFS
  ret = hcom_little_support_init_master_fs(_master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS file system initialization returned error '%d' \n",
              __func__, ret);
    return ret;
  }
#endif

  ret = hcom_fs_helper_init_fs_on_boot(_master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: file system initialization returned error '%d' \n",
              __func__, ret);
    return ret;
  }
  return OK;
}

//==================================================================
// This call not only initializes the file system with the right number
// of partitions it will also mount and if needed format each partition
int hcom_fs_helper_init_fs_on_boot(FAR struct mtd_dev_s *master_flash_mtd)
{
#if 1   // Disable file system initialization for testing
  int ret;
  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(master_flash_mtd,
        HCOM_NUMBER_OF_FS_PARTITIONS);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Creation of file system returned error '%d'\n", __func__, ret);
    return ret;
  }
#endif
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

  //------------------------------------------------------------------------
  f7syslog(LOG_INFO, "fs->File System Creation, step 1: create %d partitions\n", numbOfPartitions);

  // Create the number of partitions in the external flash
  ret = hcom_fs_helper_init_fs_partitions(master_flash_mtd, numbOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: partitioning file system: %d\n", __func__, ret);
    return ret;
  }

  //------------------------------------------------------------------------
  f7syslog(LOG_INFO, "fs->Partitioning complete, step 2: initialize all partitions\n");

  // Initialize the file system
  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_DEBUG, "Attempting to Initialize file system partition %d\n",
             partCounter);

#ifdef CONFIG_FS_SMARTFS
    ret = hcom_smartfs_support_init_part_fs(partCounter, _mtdPartArray[partCounter]);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
#endif

#if (defined CONFIG_FS_LITTLEFS && defined CONFIG_MTD_PARTITION)
    ret = hcom_littlefs_support_init_part_fs(partCounter, _mtdPartArray[partCounter]);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: LittleFS returned error '%d' while initializing partition %d\n",
               __func__, ret, partCounter);
      return ret;
    }
#endif
  }

  //------------------------------------------------------------------------
  f7syslog(LOG_INFO, "fs->File system initialization complete, step 3: mount and format as needed\n");

  for (partCounter = 0; partCounter < numbOfPartitions; partCounter++)
  {
    f7syslog(LOG_DEBUG, "fs->Attempt to mount partition %d\n", partCounter);

    // Attempt to mount - if fails format and attempt to mount again
#ifdef CONFIG_FS_SMARTFS
    ret = hcom_smartfs_support_mount_format(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Initial mount failed '%s' to '%s' for type '%s' on PartitionID %d Error %d\n",
            __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
      return ret;
    }
#endif

#ifdef CONFIG_FS_LITTLEFS
    ret = hcom_littlefs_support_mount_format(partCounter);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: Initial mount failed '%s' to '%s' for type '%s' on PartitionID %d Error %d\n",
            __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partCounter, ret);
    }
#endif
  }

  f7syslog(LOG_INFO, "fs->File system mount complete. File system creation successfully completed\n");
  return OK;
}

//==============================================================================
// This method is called to mount either one partition or the only file system 
int hcom_fs_helper_mount_file_system(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId,
                                        const char *mountCommand)
{
  if (_shutting_down)
    return OK;

  int ret;
  char *finalSourceName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

#ifdef CONFIG_MTD_PARTITION
  // e.g. /dev/smart0 or dev/little0
  int stringLen = snprintf(finalSourceName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s0p%d", sourceDevice, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  // e.g. /meadow0
  stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d", targetDevice, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

  f7syslog(LOG_INFO, "fs->Attempting to mount partition %d as '%s' to '%s' type '%s'\n",
           partitionId, finalSourceName, fullMountPtName, fileSystemType);
#else
  // e.g. mount("/dev/little", "/meadow", "littlefs", 0, NULL);
  DEBUGASSERT(strlen(sourceDevice) < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  strcpy(finalSourceName, sourceDevice);
  DEBUGASSERT(strlen(targetDevice) < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  strcpy(fullMountPtName, targetDevice);

  f7syslog(LOG_INFO, "fs->Attempting to mount '%s' to '%s' type '%s'\n",
           finalSourceName, fullMountPtName, fileSystemType);
#endif

  // e.g. mount("/dev/ram0", "/mnt", "vfat", 0, NULL);  // Needs backing block device
  // e.g. mount(NULL, "/mnt", "nxffs", 0, NULL);        // When no backing block device
  ret = mount(finalSourceName, fullMountPtName, fileSystemType, 0, mountCommand);
  if (ret < 0)
  {
    // The mount function puts the error code into errno
    int errnumb = get_errno();
    // Return to the specific file system code to determine if formatting needed
    free(finalSourceName);
    free(fullMountPtName);
    return -errnumb;
  }

  _mountedPartitionId[partitionId] = true;

  f7syslog(LOG_INFO, "fs->Successfully mounted '%s' to '%s' for type '%s'\n",
        finalSourceName, fullMountPtName, fileSystemType);
  free(finalSourceName);
  free(fullMountPtName);

  return OK;
}

//=====================================================================
//
bool hcom_fs_helper_is_fs_mounted(uint32_t partitionId)
{
  return (_mountedPartitionId[partitionId]);
}

//=====================================================================
int hcom_fs_helper_get_list_files_in_partition(uint32_t partitionId, char *csvList, int csvListLen)
{
  int csvBufferOff = 0;
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *fileListBuff = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

#ifdef CONFIG_MTD_PARTITION
  int stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  strcpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET);
#endif

  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    f7syslog(LOG_ERR, "ERROR: opendir(\"%s\") failed with errno=%d\n", fullMountPtName, errno);
    free(fullMountPtName);
    free(fileListBuff);
    return -1;
  }

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      // Get the next file name
#ifdef CONFIG_MTD_PARTITION
      f7syslog(LOG_INFO, "fs->Found file '%s' in partition %d\n", direntry->d_name, partitionId);
#else
      f7syslog(LOG_INFO, "fs->Found file '%s'\n", direntry->d_name);
#endif
      int fileNameLen;
      if(firstFile)
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", fullMountPtName, direntry->d_name);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, ",%s/%s", fullMountPtName, direntry->d_name);
      }

      DEBUGASSERT(fileNameLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      if(csvBufferOff + fileNameLen > csvListLen - 1)
      {
        f7syslog(LOG_ERR, "ERROR: while building file name list, ran out of buffer space.\n");
        closedir(dirp);
        free(fullMountPtName);
        free(fileListBuff);
        return -1;
      }

      // Add this file name to the list
      strcpy(csvList + csvBufferOff, fileListBuff);
      csvBufferOff += fileNameLen;
    }
  }

  csvList[csvBufferOff] = '\0';
  closedir(dirp);

  free(fullMountPtName);
  free(fileListBuff);
  return OK;
}

//=====================================================================
int hcom_fs_helper_get_list_files_in_partition_and_crc(uint32_t partitionId, char *csvList, int csvListLen)
{
  int csvBufferOff = 0;
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *fileListBuff = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  char *completeNameBuf = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  int stringLen;
  bool firstFile = true;
  DIR *dirp;
  struct dirent *direntry;

#ifdef CONFIG_MTD_PARTITION
  stringLen = snprintf(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d", HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
  DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#else
  strcpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET);
#endif

  dirp = opendir(fullMountPtName);
  if ( !dirp )
  {
    f7syslog(LOG_ERR, "ERROR: opendir '%s' failed with errno=%d\n", fullMountPtName, errno);
    free(fullMountPtName);
    free(fileListBuff);
    free(completeNameBuf);
    return -1;
  }

  while((direntry = readdir(dirp)) != NULL)
  {
    if(DIRENT_ISFILE(direntry->d_type))
    {
      stringLen = snprintf(completeNameBuf, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", fullMountPtName, direntry->d_name);
      DEBUGASSERT(stringLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      
      // Find the CRC checksum
      uint32_t crcChecksum = hcom_file_commands_calc_crc_for_file(completeNameBuf);

#ifdef CONFIG_MTD_PARTITION
      f7syslog(LOG_INFO, "fs->Found file '%s' in partition %d with checksum 0x%08x\n", direntry->d_name, partitionId, crcChecksum);
#else
      f7syslog(LOG_INFO, "fs->Found file '%s'with checksum 0x%08x\n", direntry->d_name, crcChecksum);
#endif

      // Add this file to the csv list 
      int fileNameLen = 0;

      if(firstFile)
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s [0x%08x]",
            fullMountPtName, direntry->d_name, crcChecksum);
        firstFile = false;
      }
      else
      {
        fileNameLen = snprintf(fileListBuff, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, ",%s/%s [0x%08x]",
            fullMountPtName, direntry->d_name, crcChecksum);
      }

      DEBUGASSERT(fileNameLen < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
      if(csvBufferOff + fileNameLen > csvListLen - 1)
      {
        f7syslog(LOG_ERR, "ERROR: while building file name list, ran out of buffer space.\n");
        closedir(dirp);
        free(fullMountPtName);
        free(fileListBuff);
        free(completeNameBuf);
        return -1;
      }

      // Add this file name to the list
      strcpy(csvList + csvBufferOff, fileListBuff);
      csvBufferOff += fileNameLen;
    }
  }

  csvList[csvBufferOff] = '\0';
  closedir(dirp);

  free(fullMountPtName);
  free(fileListBuff);
  free(completeNameBuf);

  return OK;
}

//======================================================================
// Partitions the entire flash chip with the number of partitions specified.
// This sets the size of each partition
int hcom_fs_helper_init_fs_partitions(FAR struct mtd_dev_s *master_flash_mtd, uint32_t numberOfPartitions)
{
#ifndef CONFIG_MTD_PARTITION
  _mtdPartArray[0] = master_flash_mtd;
#else
  FAR struct mtd_geometry_s geo;
  off_t partitionId;

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

  f7syslog(LOG_DEBUG, "MTD Geo info - numb erase sectors %u, erasesize %u page size %u\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t pagesPerErase = geo.erasesize / geo.blocksize;
  off_t nPages = (geo.neraseblocks / numberOfPartitions) * pagesPerErase;
  size_t partsize = nPages * geo.blocksize;

  off_t offset = 0;
  for (partitionId = 0; partitionId < numberOfPartitions; partitionId++)
  {
    _mtdPartArray[partitionId] = mtd_partition(master_flash_mtd, offset, nPages);
    offset += nPages;
    if (!_mtdPartArray[partitionId])
    {
      f7syslog(LOG_ERR, "%s() ERROR: mtd_partition failed. offset=%lu nPages=%lu\n",
               __func__, (unsigned long)offset, (unsigned long)nPages);
    }

    f7syslog(LOG_INFO, "fs->Partition %d created at offset %d with size = %d bytes\n",
             partitionId, offset, partsize);
  }
#endif
  return OK;
}

//=====================================================================================
int hcom_fs_helper_fs_initialize_proxy(uint32_t partitionId)
{
  int ret;

#ifdef CONFIG_MTD_PARTITION

#ifdef CONFIG_FS_SMARTFS
  ret = hcom_smartfs_support_init_part_fs(partitionId, _mtdPartArray[partitionId]);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing partition %d\n",
              __func__, ret, partitionId);
    return ret;
  }
#endif

#ifdef CONFIG_FS_LITTLEFS
  // For LittleFS an additional initialization step is needed.
  ret = hcom_little_support_init_master_fs(_master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS returned error '%d' while initializing master mtd\n",
              __func__, ret);
    return ret;
  }

  ret = hcom_littlefs_support_init_part_fs(partitionId, _mtdPartArray[partitionId]);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS returned error '%d' while initializing partition %d\n",
              __func__, ret, partitionId);
    return ret;
  }
#endif

#else

#ifdef CONFIG_FS_SMARTFS
  ret = hcom_smartfs_support_init_part_fs(0, _master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: SmartFS returned error '%d' while initializing\n",
              __func__, ret);
    return ret;
  }
#endif

#ifdef CONFIG_FS_LITTLEFS
  // For LittleFS an additional initialization step is needed.
  ret = hcom_little_support_init_master_fs(_master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS returned error '%d' while initializing master mtd\n",
              __func__, ret);
    return ret;
  }

#ifdef CONFIG_MTD_PARTITION
  ret = hcom_littlefs_support_init_part_fs(0, _master_mtd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: LittleFS returned error '%d' while initializing\n",
              __func__, ret);
    return ret;
  }
#endif

#endif

#endif

  return OK;
}

//=====================================================================================
int hcom_fs_helper_format_fs_proxy(uint32_t partitionId)
{
  int ret;
  
#ifdef CONFIG_FS_SMARTFS
  ret = hcom_smartfs_support_format(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format SmartFS failed for partition %d\n", __func__, partitionId);
    return ret;
  }
#endif

#ifdef CONFIG_FS_LITTLEFS
  ret = hcom_fs_helper_mount_file_system(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            HCOM_FILE_MOUNT_FORCE_FORMAT);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format of LittleFS failed for partition %d\n", __func__, partitionId);
    return ret;
  }
#endif

  return OK;
}

