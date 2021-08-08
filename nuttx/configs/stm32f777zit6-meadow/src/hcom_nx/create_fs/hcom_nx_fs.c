/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\create_fs\hcom_nx_fs.c
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

// This modules contains code to create the files system, partitions and all

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"

#if !defined CONFIG_FS_LITTLEFS
#warning "LittleFS is not configured. No file system can exist"
#endif

#include <errno.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <dirent.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
struct partition_info
{
  struct mtd_dev_s *mtdPart;
  bool isPartMounted;
  off_t partPageOffset;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static FAR struct partition_info _partInfo[HCOM_NX_FLASH_FILE_PARTITION_COUNT_MAX];
static int _totalPartitionCount;
static uint32_t _pagesPerEraSector;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

int hcom_fs_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *mtd,
                                                      uint32_t numbOfPartitions);
int hcom_fs_init_partitions(FAR struct mtd_dev_s *mtd, uint32_t numberOfPartitions);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Entry point for initialization of file system.
// Called at startup to insure correct low-level file system
int hcom_nx_create_fs_initialize(FAR struct mtd_dev_s *mtd)
{
  int ret;

  //_mtd = mtd;
  for(int i = 0; i < HCOM_NX_FLASH_FILE_PARTITION_COUNT_MAX; i++)
  {
    _partInfo[i].mtdPart = NULL;
    _partInfo[i].isPartMounted = false;
    _partInfo[i].partPageOffset = 0;
  }
  
  _totalPartitionCount = 0;
  _pagesPerEraSector = 0;

#ifdef CONFIG_FS_LITTLEFS
  ret = hcom_nx_create_littlefs_support_init_master(mtd);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ERROR:LittleFS F/S init error:%d\n",
              thisFile, __LINE__, ret);
    return ret;
  }
#endif

#if !defined(CONFIG_HCOM_MTD_STRESS_TEST)
  ret = hcom_fs_create_partition_initialize_and_mount_fs(mtd,
        HCOM_NX_NUMBER_OF_FS_PARTITIONS);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-F/S creation, error:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif

  return OK;
}

//=====================================================================
// This function will create partitions, initialize the file system, mount
// the partititions and if necessary format the partitions.
int hcom_fs_create_partition_initialize_and_mount_fs(FAR struct mtd_dev_s *mtd,
                                                      uint32_t numbOfPartitions)
{
  int ret;
  uint32_t partNumb;

  //------------------------------------------------------------------------
  syslog(LOG_INFO, "F/S creating %d partition%s\n", numbOfPartitions, numbOfPartitions > 1 ? "s" : "");

  // Create the partitions in the external flash
  ret = hcom_fs_init_partitions(mtd, numbOfPartitions);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-partitioning FS:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  //------------------------------------------------------------------------
  syslog(LOG_DEBUG, "Partitioning complete. Init partitions\n");

  // Initialize the file system
  for (partNumb = 0; partNumb < numbOfPartitions; partNumb++)
  {

#if HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0
    syslog(LOG_DEBUG, "Init F/S part:%d\n", partNumb);
#endif

#if (defined CONFIG_FS_LITTLEFS && defined CONFIG_MTD_PARTITION)
    ret = hcom_nx_create_littlefs_init_1_part(partNumb, _partInfo[partNumb].mtdPart);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-LittleFS error:%d part:%d\n",
               thisFile, __LINE__, ret, partNumb);
      return ret;
    }
#endif
  }

  //------------------------------------------------------------------------
  syslog(LOG_DEBUG, "F/S Part Init complete, mount & format\n");

  for (partNumb = 0; partNumb < numbOfPartitions; partNumb++)
  {
    
#if HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0
    syslog(LOG_DEBUG, "%s@%d-Mount part %d\n", thisFile, __LINE__, partNumb);
#endif

    // Attempt to mount - if fails format and attempt to mount again
#if defined(CONFIG_FS_LITTLEFS)
    ret = hcom_nx_create_littlefs_mount_format_1_part(partNumb);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-mount failed '%s' to '%s' for type '%s', Part %d error:%d\n",
            thisFile, __LINE__, HCOM_NX_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
            HCOM_NX_FILE_MOUNT_FILE_SYS_TYPE, partNumb, ret);
    }
#endif
  }

  syslog(LOG_NOTICE, "F/S creation complete\n");
  return OK;
}

//==============================================================================
// This method is called from littlefs to mount either a partition or the
// only partition in the file system 
int hcom_nx_create_fs_mount(const char *sourceDevice, const char *targetDevice,
                                        const char *fileSystemType, uint32_t partitionId,
                                        const char *mountCommand)
{
  int ret;
  char *finalSourceName = malloc(HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(finalSourceName == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }
  
  char *fullMountPtName = malloc(HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH);
  if(fullMountPtName == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

#ifdef CONFIG_MTD_PARTITION
  // e.g. /dev/smart0 or dev/little0
  snprintf_chk(finalSourceName, HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s0p%d", sourceDevice, partitionId);

  // e.g. /meadow0
  snprintf_chk(fullMountPtName, HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d", targetDevice, partitionId);

 #if HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0
  syslog(LOG_DEBUG, "Attempt to mount partition %d as '%s' to '%s' type '%s'\n",
           partitionId, finalSourceName, fullMountPtName, fileSystemType);
 #endif

#else  // #ifdef CONFIG_MTD_PARTITION

  // e.g. mount("/dev/little", "/meadow", "littlefs", 0, NULL);
  strcpy(finalSourceName, sourceDevice);
  strcpy(fullMountPtName, targetDevice);

  syslog(LOG_INFO, "Mounting '%s' to '%s' type '%s'\n",
           finalSourceName, fullMountPtName, fileSystemType);
#endif  // #ifdef CONFIG_MTD_PARTITION


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

  _partInfo[partitionId].isPartMounted = true;
  
  syslog(LOG_DEBUG, "%s@%d-Mounted partition %d as '%s' to '%s' for type '%s'\n", thisFile, __LINE__,
        partitionId, finalSourceName, fullMountPtName, fileSystemType);
        
  free(finalSourceName);
  free(fullMountPtName);

  return OK;
}

//=====================================================================
//
bool hcom_nx_fs_is_mounted(uint32_t partitionId)
{
  return (_partInfo[partitionId].isPartMounted);
}

//=====================================================================
// Used to implement RenewFileSys
int hcom_nx_fs_1st_erase_sector_of_partition(uint32_t partitionId)
{
  return (_partInfo[partitionId].partPageOffset / _pagesPerEraSector);
}

//======================================================================
// Partitions the entire flash chip with the number of partitions specified.
// This also sets the size of each partition
int hcom_fs_init_partitions(FAR struct mtd_dev_s *mtd, uint32_t numberOfPartitions)
{
#ifndef CONFIG_MTD_PARTITION
  _partInfo[0].mtdPart = mtd;
  _totalPartitionCount = 1;
#else
  FAR struct mtd_geometry_s geo;
  int partitionId;

  _totalPartitionCount = numberOfPartitions;
  if (numberOfPartitions > HCOM_NX_FLASH_FILE_PARTITION_COUNT_MAX)
  {
    syslog(LOG_ERR, "%s@%d-%d parts too big %d max\n",
             thisFile, __LINE__, numberOfPartitions, HCOM_NX_FLASH_FILE_PARTITION_COUNT_MAX);
    return -1;
  }

  // Get geometry of QSPI Flash
  int ret = mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Read MTD geometry:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0
  syslog(LOG_DEBUG, "MTD Geo info - numb erase sectors %u, erasesize %u page size %u\n",
          geo.neraseblocks, geo.erasesize, geo.blocksize);
#endif

  _pagesPerEraSector = geo.erasesize / geo.blocksize;

  size_t nEraseBlocks = geo.neraseblocks;
  nEraseBlocks -= (HCOM_NX_FS_MONO_RAW_PARTITION_SIZE / geo.erasesize);
  int nPages = (nEraseBlocks / numberOfPartitions) * _pagesPerEraSector;

  // Reserve some size in the flash for Mono raw partition.
  int offsetInPages = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE / geo.blocksize;
  // Reserve some size in flash for OTA update images
  offsetInPages += HCOM_NX_FS_OTA_RESERVED_SPACE / geo.blocksize;

  for (partitionId = 0; partitionId < numberOfPartitions; partitionId++)
  {
    _partInfo[partitionId].partPageOffset = offsetInPages;
    _partInfo[partitionId].mtdPart = mtd_partition(mtd, offsetInPages, nPages);
    offsetInPages += nPages;
    if (!_partInfo[partitionId].mtdPart)
    {
      syslog(LOG_ERR, "%s@%d-Error:mtd_partition, offset=%lu nPages=%lu\n",
               thisFile, __LINE__, (unsigned long)offsetInPages, (unsigned long)nPages);
    }

#if HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0
    size_t partsize = nPages * geo.blocksize;
    syslog(LOG_DEBUG, "Part %d created offset:%lu size:%d bytes\n",
             partitionId, (unsigned long)offsetInPages, partsize);
#endif

  }
#endif
  return OK;
}
