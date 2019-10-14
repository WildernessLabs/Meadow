/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_request_action.c
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
#include <crc8.h>

#include "stm32_qspi.h"
#include <nuttx/spi/qspi.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct mtd_dev_s *_master_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_exec_flash_fs_get_file_list(uint32_t partitionId, bool getChecksum);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_flash_fs_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  return OK;
}

//=======================================================================================
void hcom_exec_flash_fs_partition(uint32_t numberOfPartitions)
{
  int ret;
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestRejected, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  return;
#else
#ifndef CONFIG_MTD_PARTITION
  char *partMsg = "Partitioning is not supported in this version of Meadow. This step not necessary.";
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, partMsg);
  if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

#else

  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;

  if(numberOfPartitions != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "Currently, the number of partitions is hardcode at %d partitions. Please retry with this value.",
      HCOM_NUMBER_OF_FS_PARTITIONS);    

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

    return;
  }

  f7syslog(LOG_NOTICE, "Partitioning of Flash beginning\n");

  // Partitions the entire flash chip with the number of partitions provided
  ret = hcom_fs_helper_init_fs_partitions(_master_mtd, numberOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Flash file system partition and format failed: %d\n", __func__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Partition and format completed for QSPI Flash with error %d.", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Partitioned into %d partitions completed for QSPI Flash. No errors reported",
                      numberOfPartitions);
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, hostMsg);
  if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Partitioning of Flash completed\n\n");
#endif
#endif
}

//=======================================================================================
void hcom_exec_flash_fs_mount(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  int ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestRejected, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  return;
#else
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

#ifndef CONFIG_MTD_PARTITION
  DEBUGASSERT(partitionId == 0);
#endif

  f7syslog(LOG_NOTICE, "Flash File System mount beginning\n");

  // Mount the entire QSPI flash as defined in HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET
  // and HCOM_FILE_MOUNT_FILE_SYS_TYPE
  ret = hcom_fs_helper_mount_file_system(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            NULL);
  if (ret < 0)
  {
#ifdef CONFIG_MTD_PARTITION
    f7syslog(LOG_ERR, "%s() ERROR: Failed to mount '%s' to '%s' for type '%s' on PartitionID %d errno:%d\n",
             __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
             HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                      "Failed to mount '%s' to '%s' for type '%s' on PartitionID %d errno:%d\0\n",
                      __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
#else
    f7syslog(LOG_ERR, "%s() ERROR: Failed to mount '%s' to '%s' for type '%s' errno:%d\n",
             __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
             HCOM_FILE_MOUNT_FILE_SYS_TYPE, ret);

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                      "Failed to mount '%s' to '%s' for type '%s' errno:%d",
                      __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, ret);
#endif
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Flash file system mount completed. No errors reported");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Mounting of Flash File System completed\n\n");
  #endif
}

//=======================================================================================
void hcom_exec_flash_fs_initialize(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  int ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestRejected, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  return;
#else

  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int sHCOM_USE_SIMPLE_CLI_FILE_SYSTEM_COMMANDS
  int ret;

  f7syslog(LOG_NOTICE, "Initialize Flash File System beginning\n");

  ret = hcom_fs_helper_fs_initialize_proxy(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Initialize File System failed with error: %d\n", __func__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Initialize file system failed with error %d.", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Initialize file system completed. No errors reported");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Initialization of File System completed\n\n");
#endif
}

//=======================================================================================
void hcom_exec_flash_fs_format(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  int ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestRejected, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  return;
#else
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

  // Comes from hcom message
  // VaHCOM_USE_SIMPLE_CLI_FILE_SYSTEM_COMMANDSeater than HCOM_FLASH_FILE_PARTITION_COUNT_MAX
  f7syslog(LOG_NOTICE, "Format Flash File System beginning\n");

  ret = hcom_fs_helper_format_fs_proxy(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format File System failed with error: %d\n", __func__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Format file system failed with error %d.", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Format file system completed. No errors reported");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Format File System completed\n\n");
    #endif
}

//=======================================================================================
void hcom_exec_flash_fs_create(uint32_t numbOfPartitions)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  int ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestRejected, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  return;
#else
  // This single call will partition, initialize, format (if needed) and mount the file system
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

#ifdef CONFIG_MTD_PARTITION
  if(numbOfPartitions != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "HCOM_USE_SIMPLE_CLI_FILE_SYSTEM_COMMANDScode at %d partitions. Please retry with this value.",
      HCOM_NUMBER_OF_FS_PARTITIONS);    
    
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

    return;
  }
#else
  if(numbOfPartitions > 0)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "This version of Meadow does not support partitions");
  
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

    return;
  }
#endif

  f7syslog(LOG_NOTICE, "Create Flash File System beginning\n");

  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(_master_mtd, numbOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Creation of File System failed error: %d\n", __func__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Create file system failed with error %d", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "File system created successfully");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Create File System completed\n\n");
  #endif
}

//=======================================================================================
// userData contains the partition number, if partitioning is in use
void hcom_exec_flash_fs_part_renew_file_system(uint32_t partitionId)
{
  int ret;
  
  int sectorOffset = hcom_fs_helper_1st_erase_sector_of_partition(partitionId);

  // Erase the first few sectors of the partition and restart the MCU
  ret = MTD_ERASE(_master_mtd, sectorOffset, 16);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() MTD_ERASE failed to erase SectorOffset %d, error %d.\n", __func__, sectorOffset, ret);
  }

  char *sendMsgToHost = "File system renewed. Restarting F7 Micro";
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestInformation, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  // Send the ended flag after recreating the file system and restarting Meadow
  hcom_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_ENDED_BIT_FLAG);

  usleep(500 * 1000);
  up_systemreset();
}

//=======================================================================================
// userData contains the partition number, if partitioning is in use
void hcom_exec_flash_fs_return_file_list(uint32_t partitionId)
{
  hcom_exec_flash_fs_get_file_list(partitionId, false);
}

//=======================================================================================
// userData contains the partition number, if partitioning is in use
void hcom_exec_flash_fs_return_file_list_with_crc(uint32_t partitionId)
{
  hcom_exec_flash_fs_get_file_list(partitionId, true);
}

//=======================================================================================
// userData contains the partition number, if partitioning is in use
void hcom_exec_flash_fs_get_file_list(uint32_t partitionId, bool getChecksum)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

#ifdef CONFIG_MTD_PARTITION
  f7syslog(LOG_NOTICE, "Getting file list for partition %d beginning. Will%sadd crc\n",
      partitionId, getChecksum ? " " : " NOT ");
#else
  DEBUGASSERT(partitionId == 0);
  f7syslog(LOG_NOTICE, "Getting file list beginning. Will%sadd crc\n",
      getChecksum ? " " : " NOT ");
#endif

  ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestFileListHeader, 0);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  if(getChecksum)
    ret = hcom_fs_helper_get_list_files_in_partition_and_crc(partitionId);
  else
    ret = hcom_fs_helper_get_list_files_in_partition(partitionId);
  
  if(ret != OK)
  {
    f7syslog(LOG_ERR, "%s() Error: No results available (%d).\n", __func__, ret);

    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "No results available");
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  }

  f7syslog(LOG_NOTICE, "Getting file list exiting\n");
}

//=======================================================================================
void hcom_exec_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
    uint32_t partitionId)
{
  char *hostMsg;
  int ret;

  hostMsg = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);

  size_t fileNameLength = recvPacketDataSize - HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET;
  char *fileNameBuffer = zalloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET, fileNameLength);

  ret = hcom_file_commands_delete_by_name(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() Error returned from call to hcom_file_commands_delete_by_name: %d\n", __func__, ret);
    ret = hcom_host_msg_bldr_send_information_msg(HcomProtoCtrlRequestError, 0);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  }

  free(hostMsg);
  free(fileNameBuffer);
}

//=======================================================================================
// Erase the entire qspi flash chip
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_exec_flash_fs_flash_bulk_erase(uint32_t userData)
{
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int ret;

  f7syslog(LOG_WARNING, "Bulk erase of QSPI Flash begun\n");

  ret = _master_mtd->ioctl(_master_mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: IOCTL MTDIOC_BULKERASE failed. Returned %d\n", __func__, ret);
    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Bulk Erase of QSPI Flash error %d.", ret);
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestError, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
  }
  f7syslog(LOG_WARNING, "Bulk erase of QSPI Flash completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_exec_flash_fs_flash_verify_erase(uint32_t userData)
{
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  FAR struct mtd_geometry_s geo;
  int sectorCounter;
  int notErasedSectors;
  int ret;

  f7syslog(LOG_NOTICE, "Verification of QSPI Flash Erased state beginning\n");

  // Get geometry of QSPI Flash
  ret = _master_mtd->ioctl(_master_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Read geometry for MTD failed: %d\n", __func__, ret);
    return;
  }

  f7syslog(LOG_INFO, "Verifying if entire MTD is erased, nerasesectors %d, sectorsize %d pagesize %d\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t writeable_pages_per_sector = geo.erasesize / geo.blocksize;
  uint8_t *readBuffer = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  uint8_t *baseReference = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  memset(baseReference, 0xff, geo.blocksize * writeable_pages_per_sector); // base line for erased flash

  notErasedSectors = 0;
  for (sectorCounter = 0; sectorCounter < geo.neraseblocks; sectorCounter++)
  {
    size_t pagesRead = MTD_BREAD(_master_mtd, sectorCounter * writeable_pages_per_sector,
                                  writeable_pages_per_sector, readBuffer);
    if (pagesRead == writeable_pages_per_sector)
    {
      ret = memcmp(baseReference, readBuffer, geo.blocksize * writeable_pages_per_sector);
      if (ret != 0)
      {
        f7syslog(LOG_WARNING, "%s() WARNING: 4k byte sector at offset %04d is not erased\n", __func__, sectorCounter);
        notErasedSectors++;
      }
      continue;
    }

    if (pagesRead == 0)
      break; // End of data

    f7syslog(LOG_ERR, "%s() ERROR: Expected to read %d pages but read %d pages\n",
             __func__, writeable_pages_per_sector, pagesRead);
    break;
  }

  f7syslog(LOG_INFO, "Verified %04d bytes (%d of %d sectors)\n",
           sectorCounter * geo.erasesize, sectorCounter, geo.neraseblocks);

  free(baseReference);
  free(readBuffer);

  f7syslog(LOG_NOTICE, "Verified Erased Flash completed and found %d non-erased 4096 byte sectors.\n\n", notErasedSectors);

  // Send text message to host
  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Testing Erased Flash found %d non-erased 4096-byte sectors.",
   notErasedSectors);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_host_msg_bldr_send_short_str_msg(HcomProtoCtrlRequestEnded, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
}
