/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_request_action.c
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

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

#ifdef CONFIG_SEMIHOSTING_STAT
#warning "Because CONFIG_SEMIHOSTING_STAT is defined SmartFS formatting will not be possible"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent

// Read/write blocks are 256 bytes and erase blocks are 4096 bytes
#define HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK (16)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct mtd_dev_s *_master_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_exec_flash_fs_get_file_list(uint32_t userData, bool getChecksum);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_flash_fs_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  return OK;
}

//=======================================================================================
void hcom_exec_flash_fs_partition(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

  if(userData != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN,
      "Currently, the number of partitions is hardcode at %d partitions. Please retry with this value.\0",
      HCOM_NUMBER_OF_FS_PARTITIONS);    
    ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed. Returned %d\n", __func__, ret);
    }
    return;
  }

  uint32_t numberOfPartitions = userData;
  f7syslog(LOG_NOTICE, "** Partitioning of Flash beginning\n");

  // Partitions the entire flash chip with the number of partitions provided
  ret = hcom_fs_helper_init_fs_partitions(_master_mtd, numberOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Flash file system partition and format failed: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Partition and format completed for QSPI Flash with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Partitioned into %d partitions completed for QSPI Flash. No errors reported\0",
                      numberOfPartitions);
  }

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed. Returned %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Partitioning of Flash completed\n\n");
}

//=======================================================================================
void hcom_exec_flash_fs_mount(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

  uint32_t partitionId = userData;
  f7syslog(LOG_NOTICE, "** Mount of the Flash File System beginning\n");

  // Mount the entire QSPI flash as defined in HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET
  // and HCOM_FILE_MOUNT_FILE_SYS_TYPE
  ret = hcom_fs_helper_mount_partitioned_fs(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Failed to mount '%s' to '%s' for type '%s' on PartitionID %d errno:%d\n",
             __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
             HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);

    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN,
                      "Failed to mount '%s' to '%s' for type '%s' on PartitionID %d errno:%d\0\n",
                      __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Mount of flash file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Mounting of Flash File System completed\n\n");
}

//=======================================================================================
void hcom_exec_flash_fs_initialize(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

  uint32_t partitionId = userData;
  f7syslog(LOG_NOTICE, "** Initialize Flash File System beginning\n");

  ret = hcom_fs_helper_initialize_fs(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Initialize File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Initialize file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Initialize file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Initialization of File System completed\n\n");
}

//=======================================================================================
void hcom_exec_flash_fs_format(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

#ifdef CONFIG_SEMIHOSTING_STAT
  char *semihostingMsg = "File format is not possible with 'CONFIG_SEMIHOSTING_STAT' configured\0";
  hcom_host_msg_bldr_send_text(semihostingMsg, strlen(semihostingMsg));
  return;
#endif

  // Comes from hcom message
  // Valid partitions are 0 - n, where n is not greater than HCOM_FLASH_FILE_PARTITION_COUNT_MAX
  f7syslog(LOG_NOTICE, "** Format Flash File System beginning\n");

  ret = hcom_fs_helper_format_smartfs(userData);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Format file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Format file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Format File System completed\n\n");
}

//=======================================================================================
void hcom_exec_flash_fs_create(uint32_t userData)
{
  // This single call will partition, initialize, format (if needed) and mount the file system
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

#ifdef CONFIG_SEMIHOSTING_STAT
  char *semihostingMsg = "File system creation is not possible with 'CONFIG_SEMIHOSTING_STAT' configured\0";
  hcom_host_msg_bldr_send_text(semihostingMsg, strlen(semihostingMsg));
  return;
#endif

  if(userData != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN,
      "Currently, the number of partitions is hardcode at %d partitions. Please retry with this value.\0",
      HCOM_NUMBER_OF_FS_PARTITIONS);    
    ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
    }
    return;
  }

  f7syslog(LOG_NOTICE, "** Create entire Flash File System beginning\n");

  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(_master_mtd, userData);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Create File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Create file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Create file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Create File System completed\n\n");
}

//=======================================================================================
// userData contains the partition number
void hcom_exec_flash_fs_return_file_list(uint32_t userData)
{
  hcom_exec_flash_fs_get_file_list(userData, false);
}

//=======================================================================================
// userData contains the partition number
void hcom_exec_flash_fs_return_file_list_with_crc(uint32_t userData)
{
  hcom_exec_flash_fs_get_file_list(userData, true);
}

//=======================================================================================
// userData contains the partition number
void hcom_exec_flash_fs_get_file_list(uint32_t userData, bool getChecksum)
{
  char *csvList;
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

  f7syslog(LOG_NOTICE, "** Getting file list for partition %d beginning. Will%sadd crc\n",
      userData, getChecksum ? " " : " NOT ");

  csvList = malloc(HCOM_MAX_RETURN_TEXT_TO_HOST);
  if(csvList == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Memory allocation failed\n", __func__);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Memory allocation error. No results will be sent\0");
    ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
    f7syslog(LOG_NOTICE, "** Getting file list error exit\n");
    return;
  }

  // The list must begin with "FileList: " for the receiver to know it's not just text
  strcpy(csvList, "FileList: ");
  int preambleLen = strlen("FileList: ");

  if(getChecksum)
    ret = hcom_fs_helper_get_list_files_in_partition_and_crc(userData,
        csvList + preambleLen, HCOM_MAX_RETURN_TEXT_TO_HOST - preambleLen);
  else
    ret = hcom_fs_helper_get_list_files_in_partition(userData,
        csvList + preambleLen, HCOM_MAX_RETURN_TEXT_TO_HOST - preambleLen);
  
  if(ret == OK)
  {
    ret = hcom_host_msg_bldr_send_text(csvList, strlen(csvList));
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Internal error. No results will be sent\0");
    ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  }

  free(csvList);

  if (ret < 0)
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);

  f7syslog(LOG_NOTICE, "** Getting file list exiting\n");
}

//=======================================================================================
void hcom_exec_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
    uint32_t partitionId)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  int ret;

  size_t fileNameLength = recvPacketDataSize - HCOM_PROTOCOL_REQUEST_FILE_HDR_FILENAME_OFFSET;
  char *fileNameBuffer = malloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + HCOM_PROTOCOL_REQUEST_FILE_HDR_FILENAME_OFFSET, fileNameLength);

  ret = hcom_file_commands_delete_by_name(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() Error returned from call to hcom_file_commands_delete_by_name: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Deletaion of file '%s' failed [Error %d]\0",
        fileNameBuffer, ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "File '%s' deleted. No errors reported\0",
        fileNameBuffer);
  }
  
  free(fileNameBuffer);

  // Send text message to host
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
}

//=======================================================================================
// Erase the entire qspi flash chip
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_exec_flash_fs_flash_bulk_erase(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;

  f7syslog(LOG_NOTICE, "** Bulk erase of QSPI Flash beginning\n");
  int ret = _master_mtd->ioctl(_master_mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: IOCTL MTDIOC_BULKERASE failed. Returned %d\n", __func__, ret);
  }

  strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Bulk Erase of QSPI Flash completed.\0");
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed. Returned %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Bulk erase of QSPI Flash completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_exec_flash_fs_flash_verify_erase(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_SHORT_HOST_STRING_LEN];
  int strLen;
  FAR struct mtd_geometry_s geo;
  int blockCounter;
  int errorBlocks;
  int ret;

  f7syslog(LOG_NOTICE, "** Verification of QSPI Flash Erased state beginning\n");

  // Get geometry of QSPI Flash
  ret = _master_mtd->ioctl(_master_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Read geometry for MTD failed: %d\n", __func__, ret);
    return;
  }

  f7syslog(LOG_INFO, "Verifying if entire MTD is erased, neraseblocks %d, erasesize %d blocksize %d\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint8_t *readBuffer = (uint8_t *)malloc(geo.blocksize * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK);
  uint8_t *baseReference = (uint8_t *)malloc(geo.blocksize * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK);
  memset(baseReference, 0xff, geo.blocksize * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK); // base line for erased flash

  errorBlocks = 0;

  for (blockCounter = 0; blockCounter < geo.neraseblocks; blockCounter++)
  {
    // nread   = MTD_BREAD(dev->mtd, startblockOffset, nblocks, readBuffer);
    size_t blocksRead = MTD_BREAD(_master_mtd, blockCounter * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK,
                                  HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK, readBuffer);
    if (blocksRead == HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK)
    {
      ret = memcmp(baseReference, readBuffer, geo.blocksize * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK);
      if (ret != 0)
      {
        f7syslog(LOG_WARNING, "%s() WARNING: Block %04d is not erased\n", __func__, blockCounter);
        errorBlocks++;
      }
      continue;
    }

    if (blocksRead == 0)
      break; // End of data

    f7syslog(LOG_ERR, "%s() ERROR: Expected to read %d blocks but read %d blocks\n",
             __func__, HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK, blocksRead);
    break;
  }

  f7syslog(LOG_INFO, "Verified %04d bytes (%d of %d blocks)\n",
           blockCounter * geo.erasesize, blockCounter, geo.neraseblocks);

  free(baseReference);
  free(readBuffer);

  f7syslog(LOG_NOTICE, "** Verified Erased Flash completed and found %d non-erased partitions.\n\n", errorBlocks);

  // Send text message to host
  strLen = snprintf(hostMsg, HCOM_TEMP_SHORT_HOST_STRING_LEN, "Testing Erased Flash found %d non-erased partitions.\0",
   errorBlocks);
  ret = hcom_host_msg_bldr_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_bldr_send_text failed %d\n", __func__, ret);
  }
}
