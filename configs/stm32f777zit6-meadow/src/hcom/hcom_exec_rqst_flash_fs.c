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
#include <crc8.h>

#include "stm32_qspi.h"
#include <nuttx/spi/qspi.h>


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
static FAR struct mtd_dev_s *_test_mtd;
static FAR struct mtd_geometry_s _test_geo;
static uint32_t _flash_test_write_page_size;
static uint32_t _flash_test_total_mtd_bytes;
static uint32_t _flash_test_total_write_pages;
static uint32_t _flash_test_total_pages_per_4k_block;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_exec_flash_fs_get_file_list(uint32_t userData, bool getChecksum);
static void hcom_exec_flash_fs_flash_test_erase_1_4k_block(uint32_t blockOffset);
static void hcom_exec_flash_test_find_display_used_pages(bool eraseUsedBlocks, bool displayErasedPages);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_flash_fs_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  _test_mtd = NULL;
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
                                            HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId,
                                            NULL);
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

  ret = hcom_fs_helper_fs_initialize_proxy(partitionId);
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

  // Comes from hcom message
  // Valid partitions are 0 - n, where n is not greater than HCOM_FLASH_FILE_PARTITION_COUNT_MAX
  f7syslog(LOG_NOTICE, "** Format Flash File System beginning\n");

  ret = hcom_fs_helper_format_fs_proxy(userData);
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

  f7syslog(LOG_WARNING, "Bulk erase of QSPI Flash beginning\n");
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
  f7syslog(LOG_WARNING, "Bulk erase of QSPI Flash completed\n\n");
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

//=====================================================================
// The s25fl docs use the term "page" to describe the smallest writtable
// flash unit, here we're not using the nuttx term "block" but page.
#define FLASH_TEST_DISPLAY_INTERVAL 1024

//=====================================================================
static int hcom_exec_flash_initialize_mtd_for_testing(void)
{
  if(_test_mtd != NULL)
    return OK;

  _test_mtd = _master_mtd;

  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&_test_geo));
  DEBUGASSERT(ret == OK);

  // The s25fl docs use the term "page" to describe the smallest writtable
  // flash unit, here we're not using the nuttx term "block" but page.
  _flash_test_write_page_size = _test_geo.blocksize;
  _flash_test_total_mtd_bytes = _test_geo.neraseblocks * _test_geo.erasesize;
  _flash_test_total_write_pages = _flash_test_total_mtd_bytes / _test_geo.blocksize;
  _flash_test_total_pages_per_4k_block = _test_geo.erasesize / _test_geo.blocksize;

  syslog(0, "MTD Geo info - neraseblocks %u, erasesize %u pagesize %u, total pages %u, pages/block %u\n",
           _test_geo.neraseblocks, _test_geo.erasesize, _test_geo.blocksize,
            _flash_test_total_write_pages, _flash_test_total_pages_per_4k_block);
  return OK;
}

//=====================================================================
static void hcom_exec_flash_populate_buffer(uint32_t pageNumber, uint8_t *pageBuffer)
{
  off_t off;
  uint8_t 
  tempBuffer[3];

  // Pattern will be - Each 256 byte page will be divided into 64, 32-bit words. Each 32-bit
  // word will contain the page number 0 - 131072 (0x20000) (bits 0-17), the page offset
  // (bits 18-23) and the crc8 checksum of bytes 0-2 (bits 24-31)

  tempBuffer[0] = pageNumber & 0x000000ff;
  tempBuffer[1] = (pageNumber & 0x0000ff00) >> 8;
  tempBuffer[2] = (pageNumber & 0x00030000) >> 16;

  // syslog(0, "pageNumber = %d buff[0] 0x%02x, buff[1] 0x%02x, buff[2] 0x%02x\n",
  //     pageNumber, tempBuffer[0], tempBuffer[1], tempBuffer[2]);

  for(off = 0; off < _flash_test_write_page_size; off += 4)
  {
    pageBuffer[off]     = tempBuffer[0];
    pageBuffer[off + 1] = tempBuffer[1];
    pageBuffer[off + 2] = tempBuffer[2];
    pageBuffer[off + 2] |= off;  // use offset / 4 (0 - 64)
    pageBuffer[off + 3] = crc8(pageBuffer + off, 3);

    // syslog(0, "  offset = %04d (0x%02x) buff[2] 0x%02x, buff[3] 0x%02x\n", off, ((off >> 2) & 0x3f) << 2,
    //   pageBuffer[off+2], pageBuffer[off+3]);
  }
}

//=====================================================================
// Takes a populated page buffer and verifies that its contents match
// what should be in it.
static bool hcom_exec_flash_verify_buffered_data(uint32_t pageNumber, uint8_t *pageBuffer)
{
  uint8_t testBuffer[_flash_test_write_page_size];

  hcom_exec_flash_populate_buffer(pageNumber, testBuffer);

  // syslog(0, "\n--------- data read ----------\n");
  // hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
  // syslog(0, "\n--------- data calculated ----------\n");
  // hcom_diag_print_buffer(testBuffer, _flash_test_write_page_size, 0);

  if(memcmp(pageBuffer, testBuffer, _flash_test_write_page_size) == 0)
    return true;

  return false;
}

//===========================================================
static void hcom_exec_flash_fs_flash_test_erase_used_4k_blocks(void)
{
  hcom_exec_flash_test_find_display_used_pages(true, false);
}

//===========================================================
static void hcom_exec_flash_test_find_display_erased_pages(void)
{
  hcom_exec_flash_test_find_display_used_pages(false, true);
}

//===========================================================
// This function can display or erase the used blocks
static void hcom_exec_flash_test_find_display_used_pages(bool eraseUsedBlocks, bool displayErasedPages)
{
  off_t pageOff;
  int nread;
  int numbUsed = 0;
  bool patternFailed;
  bool eraseFailed;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  syslog(0, "Checking %d pages to find those used\n", _flash_test_total_write_pages);

  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    patternFailed = false;
    eraseFailed = false;

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);

    // We are only interested in pages that do not have the
    // normal test pattern and are not erased.
    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
      patternFailed = true;

    if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      eraseFailed = true;

    // Erase block if it's not erased
    if(eraseUsedBlocks && (eraseFailed || patternFailed))
    {
      // Note this erases multiple pages
      uint32_t blockOff = pageOff / _flash_test_total_pages_per_4k_block;
      syslog(0, "Page offset %u (0x%08x) used. Erasing associated 4k block %u (0x%08x)\n",
          pageOff, pageOff, blockOff, blockOff);
      hcom_exec_flash_fs_flash_test_erase_1_4k_block(blockOff);
    }
    else if(displayErasedPages && patternFailed && !eraseFailed)
    {
      // Assumes pattern written to entire flash device except where erased.
      // Used to verify erase functionality.
      // Display erased page
      uint32_t blockOff = pageOff / _flash_test_total_pages_per_4k_block;
      syslog(0, "Page offset %u (0x%08x) erased. Associated 4k erase block %u (0x%08x)\n",
          pageOff, pageOff, blockOff, blockOff);
    }
    else if(patternFailed && eraseFailed)
    {
      // Just display if not pattern nor erased
      syslog(0, "\n--------- data read from page# %d----------\n", pageOff);
      hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
      numbUsed++;
    }
  }
  syslog(0, "Found %d used pages\n", numbUsed);
}

//=====================================================================
static bool hcom_exec_flash_test_qspi_data_rw(bool verifyPages)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  off_t pageOff;
  int nread;
  int nfailed = 0;
  
  syslog(0, "QSPI Flash data testing %d pages has begun.\n", _flash_test_total_write_pages);

#if 1 // Disable to allow retesting after power cycle etc.
  int nwrite;
  syslog(0, "MTD initialized to %p. Bulk erasing QSPI flash next.\n", _test_mtd);
  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);
  syslog(0, "Bulk erase completed. Writing data to %d pages\n", _flash_test_total_write_pages);

  // Fill the entire QSPI flash with data
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);

    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Writing to page %d of %d\n", pageOff, _flash_test_total_write_pages);

    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nwrite == 1);
  }
#endif

  if(!verifyPages)
  {
    syslog(0, "Pattern written to %d pages\n", _flash_test_total_write_pages);
    return true;
  }
  
  syslog(0, "Data written. Verifying %d pages\n", _flash_test_total_write_pages);
  
  // Now read and test that the entire qspi files is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Verifying page %d of %d\n", pageOff, _flash_test_total_write_pages);

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);

    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      if(nfailed == 0)
      {
        syslog(0, "Page %d first to fail to compare\n", pageOff);
      }
      else
      {
        if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Page %d also failed to compare\n", pageOff);
      }
      nfailed++;
    }
  }
  syslog(0, "Write / read test completed with %d errors\n", nfailed);

  return nfailed ? true : false;
}

//=====================================================================
static void hcom_exec_flash_qspi_comprehensive_test(void)
{
  int ret;
  off_t pageOff;
  int nwrite, nread;
  off_t beforeOff, afterOff;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  syslog(0, "Comprehensive testing %d pages has begun.\n", _flash_test_total_write_pages);
  syslog(0, "Bulk erasing QSPI flash\n");
  ret = _master_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);

  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Testing page %d of %d\n", pageOff, _flash_test_total_write_pages);

    // Write next page
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);
    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nwrite == 1);

    // And verify that this page has been written correctly
    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);
    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      syslog(0, "Just written page %d failed to compare\n", pageOff);
    }

    // Next verify that all proceeding and subsequent pages are
    // correct. Those before should have data and those following
    // should be erased (0xff).
    for(beforeOff = 0; beforeOff < pageOff; beforeOff++)
    {
      if(beforeOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Testing before page %d of %d\n", beforeOff, _flash_test_total_write_pages);

      nread = MTD_BREAD(_test_mtd, beforeOff, 1, pageBuffer);
      DEBUGASSERT(nread == 1);
      if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
      {
        syslog(0, "Proceeding page %d failed to compare\n", beforeOff);
      }
    }

    for(afterOff = pageOff + 1; afterOff < _flash_test_total_write_pages; afterOff++)
    {
      if(afterOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Testing after page %d of %d\n", afterOff, _flash_test_total_write_pages);

      nread = MTD_BREAD(_test_mtd, afterOff, 1, pageBuffer);
      DEBUGASSERT(nread == 1);
      if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      {
        syslog(0, "Following page %d failed to compare\n", afterOff);
      }
    }
  }
  return;
}

//=======================================================================================
static void hcom_exec_flash_test_read_display_1_page(uint32_t pageOffset)
{
  uint8_t pageBuffer[_flash_test_write_page_size];

  syslog(0, "Reading 256 bytes from QSPI flash at page# %d\n", pageOffset);
  int nread = MTD_BREAD(_test_mtd, pageOffset, 1, pageBuffer);
  DEBUGASSERT(nread == 1);

  syslog(0, "\n--------- data read from page# %d----------\n", pageOffset);
  hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
}

//=======================================================================================
// Erase blocks are 4096 bytes each
static void hcom_exec_flash_fs_flash_test_erase_1_4k_block(uint32_t blockOffset)
{
  int ret;

  syslog(0, "Erasing QSPI flash erase block# %d\n", blockOffset);
  ret = MTD_ERASE(_test_mtd, blockOffset, 1);
  DEBUGASSERT(ret >= 0);
  syslog(0, "Block erase of QSPI flash completed\n");
}

//=======================================================================================
static void hcom_exec_flash_fs_flash_test_erase_entire_flash(void)
{
  int ret;

  syslog(0, "Bulk erasing QSPI flash\n");
  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);
  syslog(0, "Bulk erase of QSPI flash completed\n");
}

//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_flash_fs_flash_test_write_s25fl(uint32_t userData)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }

  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_test_qspi_data_rw(true);
      break;

    case -2:
      hcom_exec_flash_test_qspi_data_rw(false);
      break;

    case -3:
      hcom_exec_flash_qspi_comprehensive_test();
      break;

    default:
      // Populate the buffer with one page number and write it to MTD
      hcom_exec_flash_populate_buffer(userData, pageBuffer);
      ret = MTD_BWRITE(_test_mtd, userData, 1, pageBuffer);
      DEBUGASSERT(ret == 1);
      break;
  }
  syslog(0, "Testing command for flash completed\a\n");

  return;
}
//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_flash_fs_flash_test_init_s25fl(uint32_t userData)
{
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }
  
  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_fs_flash_test_erase_entire_flash();    
      break;

     case -2:
      hcom_exec_flash_fs_flash_test_erase_used_4k_blocks();    
      break;

   default:
      hcom_exec_flash_fs_flash_test_erase_1_4k_block(userData);
      break;
  }
  
  syslog(0, "Erase command for flash completed\a\n");
}

//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_flash_fs_flash_test_read_s25fl(uint32_t userData)
{
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }

  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_test_find_display_used_pages(false, false);
      break;

    case -2:
      hcom_exec_flash_test_find_display_erased_pages();
      break;

    default:
      hcom_exec_flash_test_read_display_1_page(userData);
      break;
  }

  syslog(0, "Read command for flash completed\a\n");
  sleep(5);
}

