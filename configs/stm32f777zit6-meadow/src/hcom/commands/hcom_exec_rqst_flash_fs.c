/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_exec_rqst_flash_fs.c
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

#include "../hcom_common.h"

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
static char *thisFile = __FILE__;

static FAR struct mtd_dev_s *_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_exec_flash_fs_get_file_list(uint32_t partitionId, bool getChecksum);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_flash_fs_setup(FAR struct mtd_dev_s *mtd)
{
  _mtd = mtd;
  return OK;
}

//=======================================================================================
void hcom_exec_flash_fs_partition(uint32_t numberOfPartitions)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, thisFile, __LINE__);
  return;
#else
  int ret;
#ifndef CONFIG_MTD_PARTITION
  char *partMsg = "Partitioning is not supported in this version of Meadow. This step not necessary.";
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, partMsg,
           thisFile, __LINE__);

#else

  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;

  if(numberOfPartitions != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "Currently, the number of partitions is hardcode at %d partitions. Please retry with this value.",
      HCOM_NUMBER_OF_FS_PARTITIONS);    

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
             thisFile, __LINE__);
    return;
  }

  f7syslog(LOG_NOTICE, "Partitioning of Flash beginning\n");

  // Partitions the entire flash chip with the number of partitions provided
  ret = hcom_fs_init_partitions(_mtd, numberOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Flash part/format:%d\n", , thisFile, __LINE__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Partition and format completed for QSPI Flash with error %d.", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                "%d partitions created", numberOfPartitions);
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  f7syslog(LOG_NOTICE, "Partitioning completed\n\n");
#endif
#endif
}

//=======================================================================================
void hcom_exec_flash_fs_mount(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, thisFile, __LINE__);
  return;
#else
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

#ifndef CONFIG_MTD_PARTITION
  DEBUGASSERT(partitionId == 0);
#endif

  f7syslog(LOG_NOTICE, "F/S mount begin\n");

  // Mount the entire QSPI flash as defined in HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET
  // and HCOM_FILE_MOUNT_FILE_SYS_TYPE
  ret = hcom_fs_mount_file_system(HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            NULL);
  if (ret < 0)
  {
#ifdef CONFIG_MTD_PARTITION
    f7syslog(LOG_ERR, "%%s@%d-Error:mounting '%s' to '%s' for type '%s' on PartitionID %d err:%d\n",
             thisFile, __LINE__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
             HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                      "Failed to mount '%s' to '%s' for type '%s' on PartitionID %d err:%d\0\n",
                      HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
#else
    f7syslog(LOG_ERR, "%s@%d-Error:mounting '%s' to '%s' for type '%s' err:%d\n",
             thisFile, __LINE__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
             HCOM_FILE_MOUNT_FILE_SYS_TYPE, ret);

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
                      "Failed to mount '%s' to '%s' for type '%s' err:%d",
                      HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, ret);
#endif
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Flash file system mount completed. No errors reported");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  f7syslog(LOG_NOTICE, "F/S mount success\n\n");
  #endif
}

//=======================================================================================
void hcom_exec_flash_fs_initialize(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, thisFile, __LINE__);
  return;
#else

  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int sHCOM_USE_SIMPLE_CLI_FILE_SYSTEM_COMMANDS
  int ret;

  f7syslog(LOG_NOTICE, "Initialize Flash File System beginning\n");

  ret = hcom_fs_initialize_proxy(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d Error:Init F/S err:%d\n", thisFile, __LINE__, ret);

    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Init F/S failed with error:%d", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
             "Init F/S complete");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  f7syslog(LOG_NOTICE, "Init F/S completed\n\n");
#endif
}

//=======================================================================================
void hcom_exec_flash_fs_format(uint32_t partitionId)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, thisFile, __LINE__);
  return;
#else
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

  // Comes from hcom message
  f7syslog(LOG_NOTICE, "F/S format begin\n");

  ret = hcom_fs_format_proxy(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d Error:Format F/S:%d\n", thisFile, __LINE__, ret);
    
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "F/S Format failed with error %d", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "F/S Format completed");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
         thisFile, __LINE__);
  f7syslog(LOG_NOTICE, "Format F/S completed\n\n");
#endif
}

//=======================================================================================
void hcom_exec_flash_fs_create(uint32_t numbOfPartitions)
{
#ifdef HCOM_IGNORE_UNNECESSARY_FILE_SYSTEM_COMMANDS
  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_REJECTED, 0, thisFile, __LINE__);
  return;
#else
  // This single call will partition, initialize, format (if needed) and mount the file system
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  int ret;

#ifdef CONFIG_MTD_PARTITION
  if(numbOfPartitions != HCOM_NUMBER_OF_FS_PARTITIONS)
  {
    // p-m This message makes no sense - investigate when partitioning activated
    DEBUGASSERT(false);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "Part %d not equal to %d. Please retry with %d",
      numbOfPartitions, HCOM_NUMBER_OF_FS_PARTITIONS, HCOM_NUMBER_OF_FS_PARTITIONS);    
    
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
             thisFile, __LINE__);
    return;
  }
#else
  if(numbOfPartitions > 0)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "This version of Meadow does not support partitions");
  
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
    return;
  }
#endif

  f7syslog(LOG_NOTICE, "Create F/S start\n");

  ret = hcom_fs_create_partition_initialize_and_mount_fs(_mtd, numbOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Creation of F/S error:%d\n", , thisFile, __LINE__, ret);
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Create F/S failed:%d", ret);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "File System created");
  }

  // Send text message to host
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);

  f7syslog(LOG_NOTICE, "F/S create completed\n\n");
  #endif
}

//=======================================================================================
// userData contains the partition number, if partitioning is in use
void hcom_exec_flash_fs_part_renew_file_system(uint32_t partitionId)
{
  int ret;
  
  // Send the concluded message after recreating the file system and restarting Meadow
  hcom_utils_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  int sectorOffset = hcom_fs_1st_erase_sector_of_partition(partitionId);

  // Erase the first few sectors of the partition and restart the MCU
  ret = MTD_ERASE(_mtd, sectorOffset, 16);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s@%d-flash erase SectorOffset %d, err:%d\n",
          thisFile, __LINE__, sectorOffset, ret);

  char *sendMsgToHost = "File system renewed. Restarting F7 Micro";
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
           thisFile, __LINE__);

  // Tell host to begin to reconnect
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_RECONNECT, 0, sendMsgToHost,
           thisFile, __LINE__);

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
//
void hcom_exec_flash_fs_get_file_list(uint32_t partitionId, bool getChecksum)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

#ifdef CONFIG_MTD_PARTITION
  f7syslog(LOG_NOTICE, "Get file list for part %d begin. Will%sadd crc\n",
      partitionId, getChecksum ? " " : " not ");
#else
  DEBUGASSERT(partitionId == 0);
  f7syslog(LOG_NOTICE, "Get file list. Will%sadd crc\n",
      getChecksum ? " " : " NOT ");
#endif

  hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_LIST_HEADER, 0, thisFile, __LINE__);

  if(getChecksum)
    ret = hcom_fs_get_list_files_in_partition_and_crc(partitionId);
  else
    ret = hcom_fs_get_list_files_in_partition(partitionId);
  
  if(ret != OK)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:No results:%d\n", thisFile, __LINE__, ret);

    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "No results available");
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);
  }

  f7syslog(LOG_NOTICE, "File list exit\n");
}

//=======================================================================================
void hcom_exec_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
    uint32_t partitionId)
{
  int ret;
  char *hostMsg = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);

  size_t fileNameLength = recvPacketDataSize - HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET;
  char *fileNameBuffer = zalloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + HCOM_PROTOCOL_REQUEST_HEADER_FILE_NAME_OFFSET, fileNameLength);

  ret = hcom_file_commands_delete_by_name(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s@%d-Error from call to delete:%d\n",
        thisFile, __LINE__, ret);
    hcom_comms_send_header_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, thisFile, __LINE__);
  }

  // Send text message to host
  int stringLen = snprintf(hostMsg, HCOM_MAX_HOST_STRING_BUFF_LENGTH,
        "Delete success %s", fileNameBuffer);

  DEBUGASSERT(stringLen < HCOM_MAX_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  free(fileNameBuffer);
  free(hostMsg);
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

  ret = _mtd->ioctl(_mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:IOCTL MTDIOC_BULKERASE Err:%d\n",
        thisFile, __LINE__, ret);
    int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Bulk Erase of QSPI Flash error %d.", ret);

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
             thisFile, __LINE__);
    return;
  }

  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            "Bulk erase completed", thisFile, __LINE__);

  f7syslog(LOG_WARNING, "Bulk erase complete\n\n");
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
  ret = _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Read geo for MTD:%d\n", thisFile, __LINE__, ret);
    return;
  }

  f7syslog(LOG_INFO, "Verify MTD erase, sectors %d, era size %d pagesize %d\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t writeable_pages_per_sector = geo.erasesize / geo.blocksize;
  uint8_t *readBuffer = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  uint8_t *baseReference = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  memset(baseReference, 0xff, geo.blocksize * writeable_pages_per_sector); // base line for erased flash

  notErasedSectors = 0;
  for (sectorCounter = 0; sectorCounter < geo.neraseblocks; sectorCounter++)
  {
    size_t pagesRead = MTD_BREAD(_mtd, sectorCounter * writeable_pages_per_sector,
                                  writeable_pages_per_sector, readBuffer);
    if (pagesRead == writeable_pages_per_sector)
    {
      ret = memcmp(baseReference, readBuffer, geo.blocksize * writeable_pages_per_sector);
      if (ret != 0)
      {
        f7syslog(LOG_WARNING, "%s@%d-Warning:4k sector at offset %04d is not erased\n",
            thisFile, __LINE__, sectorCounter);
        notErasedSectors++;
      }
      continue;
    }

    if (pagesRead == 0)
      break; // End of data

    f7syslog(LOG_ERR, "%s@%d-Error:Expected %d pages but read %d\n",
            thisFile, __LINE__, writeable_pages_per_sector, pagesRead);
    break;
  }

  f7syslog(LOG_INFO, "Verified %04d bytes (%d of %d sectors)\n",
           sectorCounter * geo.erasesize, sectorCounter, geo.neraseblocks);
  f7syslog(LOG_NOTICE, "Verify complete, %d non-erased 4KB sectors\n\n", notErasedSectors);

  // Send text message to host
  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
        "Verified %04d bytes (%d of %d sectors), found %d not erased.",
        sectorCounter * geo.erasesize, sectorCounter, geo.neraseblocks, notErasedSectors);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg_err(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
           thisFile, __LINE__);

  free(baseReference);
  free(readBuffer);
}
