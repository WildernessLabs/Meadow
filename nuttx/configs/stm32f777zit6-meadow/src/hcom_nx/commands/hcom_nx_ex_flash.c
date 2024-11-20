/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\commands\hcom_nx_ex_flash.c
 * 
 *   Copyright (C) 2019 - 2024 Wilderness Labs. All rights reserved.
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

// This module contains code that access the external (i.e. not part of
// the STM32F7) flash memory

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/meadow_hw_version.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include "stm32_uid.h"          // stm32_get_uniqueid()

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

#include <sys/stat.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Steps to run the following tests:
// 1. Use the python script Meadow.OS/alphaTooling/create_mock_sysfiles.py to
//     create files that contain text that is used to verify partition
//     boundaries.
// 2. Edit hcom_shared_common.h's MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH
//     entry to cause the test code to be built.
// 3. Build using './build.sh --force --clean --unittests=misc'
// 4. Use 'meadow developer -p 10 -v 1' to populate 5MB with known data
// 5. Use 'meadow developer -p 10 -v 2' to run the test which outputs via syslog
//     be ready to capture the output and verify the boundaries manually.
#if (MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH)
#pragma GCC optimize("O0")    // Prevent code optimization
#pragma message "(--) hcom_nx_ex_flash.c"
#endif

#define error(x, ...)                                                                          \
  do                                                                                           \
  {                                                                                            \
    if (cmdData)                                                                               \
    {                                                                                          \
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE, x, ##__VA_ARGS__); \
    }                                                                                          \
    else                                                                                       \
    {                                                                                          \
      char buf1[256];                                                                          \
      snprintf(buf1, sizeof(buf1), x, ##__VA_ARGS__);                                          \
      syslog(LOG_ERR, buf1);                                                                   \
    }                                                                                          \
  } while (0);

#if defined(USE_MEADOW_DEBUG_HELPERS)
//
//  Output information messages when debugging.
//
#define info(x, ...)                                                  \
  do                                                                  \
  {                                                                   \
    if (cmdData)                                                      \
    {                                                                 \
      char buf2[256];                                                 \
      snprintf(buf2, sizeof(buf2), "%s@d-%s\n", thisFile, __LINE__, x); \
      snprintf(buf2, sizeof(buf2), buf2, ##__VA_ARGS__);              \
      cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,   \
                             buf2, thisFile, __LINE__);               \
    }                                                                 \
    else                                                              \
    {                                                                 \
      char buf3[256];                                                 \
      snprintf(buf3, sizeof(buf3), x, ##__VA_ARGS__);                 \
      syslog(LOG_ERR, buf3);                                          \
    }                                                                 \
  } while (0);
#else
//
//  No output when not debugging.
//
#define info(x, ...)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 * @brief Name of this file.
 */
static char *thisFile = __FILE__;

/**
 * @brief Pointer to the _mtd driver for the flash memory on the board.
 */
static FAR struct mtd_dev_s *_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_setup
 *
 * Description:
 *  Copy the pointer to the flash driver into a static local variable.
 *
 * Input Parameters:
 *  mtd - Pointer to the flash driver.
 *
 * Returned Value:
 *  OK
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_setup(FAR struct mtd_dev_s *mtd)
{
  _mtd = mtd;
  return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_erase_ex_flash
 *
 * Description:
 *  Process the CLI request to erase all of the flash memory.
 *
 * Input Parameters:
 *  cmdData - Pointer to the command data structure holding the parameters
 *            (arguments) from CLI.
 *
 * Returned Value:
 *  OK on success, negative error code on error.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_erase_ex_flash(struct hcom_nx_cmd_data *cmdData)
{
  int ret;

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Bulk erase started (~2 minutes)", thisFile, __LINE__);

  syslog(LOG_INFO, "Bulk erase of External Flash begun\n");

  ret = _mtd->ioctl(_mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-IOCTL MTDIOC_BULKERASE Err:%d\n", thisFile, __LINE__, ret);

    char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];
    snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
              "Bulk Erase of QSPI Flash error:%d", ret);
    cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
    return ret;
  }

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Bulk erase complete", thisFile, __LINE__);

  syslog(LOG_INFO, "Bulk erase complete\n\n");
  return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_verify_ex_flash
 *
 * Description:
 *  Verify that the external flash has been erased.
 *
 * Input Parameters:
 *  cmdData - Pointer to the command data structure holding the parameters
 *            (arguments) from CLI.
 *
 * Returned Value:
 *  OK on success, negative error code on error.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_verify_ex_flash(struct hcom_nx_cmd_data *cmdData)
{
  FAR struct mtd_geometry_s geo;
  int sectorCounter;
  int notErasedSectors;
  int ret;

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "External flash verification started", thisFile, __LINE__);

  syslog(LOG_NOTICE, "Verification of External Flash Erased beginning\n");

  // Get geometry of QSPI Flash
  ret = _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-Read geo for MTD:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  syslog(LOG_INFO, "Verifying MTD erase, sectors %d, era size %d pagesize %d\n",
           geo.neraseblocks, geo.erasesize, geo.blocksize);

  uint32_t writeable_pages_per_sector = geo.erasesize / geo.blocksize;
  uint8_t *readBuffer = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  if(readBuffer == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }
  
  uint8_t *baseReference = (uint8_t *)malloc(geo.blocksize * writeable_pages_per_sector);
  if(baseReference == NULL)
  {
    free(readBuffer);
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }
  
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
        syslog(LOG_WARNING, "%s@%d-4k sector at offset %04d is not erased\n",
            thisFile, __LINE__, sectorCounter);
        notErasedSectors++;
      }
      continue;
    }

    if (pagesRead == 0)
      break; // End of data

    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-Expected %d pages but read %d\n",
            thisFile, __LINE__, writeable_pages_per_sector, pagesRead);
    break;
  }

  syslog(LOG_INFO, "Verified %04d bytes (%d of %d sectors)\n",
           sectorCounter * geo.erasesize, sectorCounter, geo.neraseblocks);
  syslog(LOG_NOTICE, "Verify complete, %d non-erased 4KB sectors\n\n", notErasedSectors);

  // Send text message to host
  char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];
  snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE,
            "Verified %04d bytes (%d of %d sectors), found %d not erased.",
            sectorCounter * geo.erasesize, sectorCounter, geo.neraseblocks, notErasedSectors);
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);

  free(baseReference);
  free(readBuffer);
  return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_renew_file_system
 *
 * Description:
 *  Process the CLI request to renew the file system.
 * 
 *  This method erases the first 16 blocks of the file system.  This makes
 *  the file system appear as if it is new and no files are present.
 *
 * Input Parameters:
 *  cmdData - userData in cmdData contains the partition id.
 *
 * Returned Value:
 *  OK on success, negative error code on error.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_renew_file_system(struct hcom_nx_cmd_data *cmdData)
{
  int ret;

  // userData has partition id
  int sectorOffset = hcom_nx_fs_1st_erase_sector_of_partition(cmdData->userData);

  // Erase the first few sectors of the partition and restart the MCU
  ret = MTD_ERASE(_mtd, sectorOffset, 16);
  if (ret < 0)
  {
    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-flash erase SectorOffset %d, err:%d\n", thisFile, __LINE__, sectorOffset, ret);
    return ret;
  }
   return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_get_block_size
 *
 * Description:
 *  Get the block size for the flash memory.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Block size in bytes.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
uint32_t hcom_nx_exec_ex_flash_get_block_size(void)
{
  struct mtd_geometry_s geo;

  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t) &geo));
  return(geo.blocksize);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_get_erase_block_size
 *
 * Description:
 *  Get the erase blocksize for the flash memory.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Size of the erase block for the current flash memory.
 * 
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
uint32_t hcom_nx_exec_ex_flash_get_erase_block_size(void)
{
  struct mtd_geometry_s geo;

  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t) &geo));
  return(geo.erasesize);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_get_geometry
 *
 * Description:
 *  Get a pointer to the geometry structure for the flash memory.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, -1 otherwise.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_get_geometry(struct mtd_geometry_s *geometry)
{
  int result = OK;

  if (geometry != NULL)
  {
    if (_mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t) geometry)) != 0)
    {
      result = -1;
    }
  }

  return(result);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_read_absolute_block
 *
 * Description:
 *  Read a block from flash memory.
 *
 * Input Parameters:
 *  blockNumber - The block number to read.
 *  destinationAddress - Address in memory to place the data from the flash 
 *                       memory.
 *
 * Returned Value:
 *  OK
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_read_absolute_block(uint32_t blockNumber, void *destinationAddress)
{
  MTD_BREAD(_mtd, blockNumber, 1, destinationAddress);
  return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_copy_blocks_to_memory
 *
 * Description:
 *  Read a number of blocks from flash memory into a data buffer.
 * 
 *  The data is read twice, once into the destination buffer and then
 *  a second time into a local buffer for verification against the first
 *  read operation.
 *
 * Input Parameters:
 *  startBlock - The block number to start reading from.
 *  destinationAddress - Address in memory to place the data from the flash.
 *  numberOfBlocks - The number of blocks to read.
 *
 * Returned Value:
 *  OK on success, -1 if the data cannot be verified or if the data fails
 *  verification.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *  The destinationAddress must have enough space to hold the data.
 *
 ****************************************************************************/
// #pragma GCC optimize("O0")
int hcom_nx_exec_ex_flash_copy_blocks_to_memory(uint32_t startBlock, void *destinationAddress, uint32_t numberOfBlocks)
{
  uint32_t currentBlock = startBlock;
  uint32_t blocksRemaining = numberOfBlocks;
  void *destination = destinationAddress;
  uint32_t blockSize = hcom_nx_exec_ex_flash_get_block_size();
  int result = OK;

  while (blocksRemaining > 0)
  {
    MTD_BREAD(_mtd, currentBlock, 1, destination);
    currentBlock++;
    destination += blockSize;
    blocksRemaining--;
  }
  //
  //  Now verify the copy.
  //
  void *buffer = kmm_malloc(blockSize);
  if (buffer == NULL)
  {
    result = -1;
  }
  else
  {
    currentBlock = startBlock;
    blocksRemaining = numberOfBlocks;
    destination = destinationAddress;
    while (blocksRemaining > 0)
    {
      MTD_BREAD(_mtd, currentBlock, 1, buffer);
      if (memcmp(buffer, destination, blockSize) != 0)
      {
        result = -1;
        break;
      }
      currentBlock++;
      destination += blockSize;
      blocksRemaining--;
    }
    kmm_free(buffer);
  }

  return(result);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_mono_flash
 *
 * Description:
 *  Copy the OS runtime binary into the correct location in flash memory.
 *
 * Input Parameters:
 *  cmdData - Pointer to the command data structure holding the parameters
 *           (arguments) from CLI.
 *
 * Returned Value:
 *  OK on success, -1 or negated errno on error.
 *
 * Assumptions/Limitations:
 *  _mtd is set for the current flash driver.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_mono_flash(struct hcom_nx_cmd_data *cmdData)
{
  int ret;
  cmdData->userData = 0;
  int lastPercentSent = 0;

  // Check for Mono runtime binary on filesystem.
#ifdef CONFIG_MTD_PARTITION
  const char runtimePath[] = "/meadow0/" HCOM_NX_FS_MONO_RUNTIME_FILENAME;
#else
  const char runtimePath[] = "/meadow/" HCOM_NX_FS_MONO_RUNTIME_FILENAME;
#endif

  int filefd = open(runtimePath, O_RDONLY);
  if (filefd == -1)
  {
    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-Mono runtime was not found in %s.", thisFile, __LINE__,
            runtimePath);
    return -1;
  }

  struct stat fileStatus;
  ret = fstat(filefd, &fileStatus);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-fstat of %s failed errno:%d\n",
           thisFile, __LINE__, runtimePath, errno);
    return -errno;
  }

  off_t fileSize = fileStatus.st_size;
  if (fileSize != HCOM_NX_FS_MONO_RAW_PARTITION_SIZE)
  {
    cmdData->logLevel = LOG_ERR;
    // Don't use snprintf_chk here
    cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
            "%s@%d-Mono runtime binary has invalid size.", thisFile, __LINE__);
    return -1;
  }

  struct mtd_geometry_s geo;
  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));

  const char monoEraseFlashMsg1[] = "Erasing mono flash memory\n";
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoEraseFlashMsg1, thisFile, __LINE__);

  // This assumes that the space for Meadow.OS.Runtime.bin is the very first
  // thing in the external flash memory.
  size_t numBlocksToErase = fileSize / geo.erasesize;
  MTD_ERASE(_mtd, 0, numBlocksToErase);

  const char monoEraseFlashMsg2[] = "Mono memory erase success\n";
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoEraseFlashMsg2, thisFile, __LINE__);

  uint8_t buf[geo.blocksize];
  uint8_t *verify = NULL;
  size_t numBlocksToWrite = fileSize / geo.blocksize;
  for (int i = 0; i < numBlocksToWrite; i++)
  {
    if (read(filefd, buf, geo.blocksize) < 0)
    {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error reading from %s.\n", thisFile, __LINE__,
              HCOM_NX_FS_MONO_RUNTIME_FILENAME);
      goto cleanup;
    }

    ssize_t writtenBlocks = MTD_BWRITE(_mtd, i, 1, buf);
    if (writtenBlocks != 1)
    {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error while writing block %d to flash.\n", thisFile, __LINE__, i);
      goto cleanup;
    }

    // 10%, 20% etc
    int percentDone = (i * 100) / numBlocksToWrite;
    if(percentDone / 10 != lastPercentSent)
    {
      char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];
      lastPercentSent = percentDone / 10;

      snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE, "Flashing %d%% complete", percentDone);
      cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
              hostMsg, thisFile, __LINE__);
    }
  }
  //
  //  Now lets verify what has been written.
  //
  if (lseek(filefd, 0, SEEK_SET) != 0)
  {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error resetting file pointer for file %s.\n", thisFile, __LINE__,
              HCOM_NX_FS_MONO_RUNTIME_FILENAME);
      goto cleanup;
  }

  verify = malloc(geo.blocksize);
  if (verify == NULL)
  {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error allocating memory for verification operation.\n", thisFile, __LINE__);
      goto cleanup;
  }

  const char monoStartingVerificationOperation[] = "Verifying runtime flash operation.\n";
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoStartingVerificationOperation, thisFile, __LINE__);

  lastPercentSent = 0;
  for (int i = 0; i < numBlocksToWrite; i++)
  {
    if (read(filefd, buf, geo.blocksize) < 0)
    {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error reading from %s.\n", thisFile, __LINE__,
              HCOM_NX_FS_MONO_RUNTIME_FILENAME);
      goto cleanup;
    }

    MTD_BREAD(_mtd, i, 1, verify);
    if (memcmp(buf, verify, geo.blocksize) != 0)
    {
      cmdData->logLevel = LOG_ERR;
      // Don't use snprintf_chk here
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Error verifying %s.\n", thisFile, __LINE__,
              HCOM_NX_FS_MONO_RUNTIME_FILENAME);
      goto cleanup;
    }

    // 10%, 20% etc
    int percentDone = (i * 100) / numBlocksToWrite;
    if(percentDone / 10 != lastPercentSent)
    {
      char hostMsg[HCOM_NX_CMD_HOST_MSG_SIZE];
      lastPercentSent = percentDone / 10;

      snprintf_chk(hostMsg, HCOM_NX_CMD_HOST_MSG_SIZE, "Verifying %d%% complete", percentDone);
      cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
              hostMsg, thisFile, __LINE__);
    }
  }
  free(verify);
  verify = NULL;

  const char monoSuccessFlashMsg[] = "Runtime flashed successfully\n";
  syslog(LOG_INFO, monoSuccessFlashMsg);
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoSuccessFlashMsg, thisFile, __LINE__);

  cleanup:
    close(filefd);
    if (verify != NULL)
    {
      free(verify);
    }
    if(cmdData->logLevel != LOG_NONE)
    {
      if (cmdData->logLevel == LOG_ERR)
      {
        cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, cmdData->logMsg, thisFile, __LINE__);
      }
      return -1;
    }
    return OK;
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_write_buffer_to_flash
 *
 * Description:
 *   Write the buffer of data to the specified location in flash.
 * 
 *   The data is written into flash one block at a time.  The data is then
 *   read back from flash into a temporary buffer and the original and the
 *   version from flash are then verified.
 *
 * Input Parameters:
 *   data_buf - Pointer to the data to be written to flash.
 *   size - Amount of data to write to flash.
 *   offset - Offset in flash to write the data.
 *
 * Returned Value:
 *  OK if successful, -1 if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int  hcom_nx_exec_ex_flash_write_buffer_to_flash(uint8_t* data_buf, off_t size, off_t offset)
{
  struct hcom_nx_cmd_data *cmdData = NULL;
  struct mtd_geometry_s geo;
  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));

  info("Erasing flash memory\n");

  int offsetInPages = offset / geo.blocksize;
  int offsetInEraseBlocks = offset / geo.erasesize;
  size_t numBlocksToWrite = size / geo.blocksize;
  size_t numBlocksToErase = size / geo.erasesize;
  int blockserased = MTD_ERASE(_mtd, offsetInEraseBlocks, numBlocksToErase);
  if (blockserased < 0)
  {
    return -1;
  }
  info("Erase success\n");

  uint8_t *buf = calloc(geo.blocksize, 1);

  for (int i = 0; i < numBlocksToWrite; i++)
    {
      memcpy(buf,data_buf, geo.blocksize);

      ssize_t writtenBlocks = MTD_BWRITE(_mtd, i + offsetInPages, 1, buf);
      if (writtenBlocks != 1)
      {
        error("Error while writing block %d to flash\n", i);
        free(buf);
        return -1;
      }

      // verify
      MTD_BREAD(_mtd, i + offsetInPages, 1, buf);
      if (memcmp(buf, data_buf, geo.blocksize) != 0)
      {
        syslog(LOG_ERR, "Error while verifying block %d.\n", i);
        free(buf);
        return -1;
      }
      data_buf+= geo.blocksize;
    }
  free(buf);

  return OK;
}

/****************************************************************************
 * Name: flash_file
 *
 * Description:
 *   Write the contents of the specified file into the location (offset) in
 *   flash.
 * 
 * Input Parameters:
 *   path - Path to the file to be written to flash.
 *   size - Amount of data to write to flash.
 *   offset - Offset in flash to write the data.
 *
 * Returned Value:
 *  OK if successful, -1 or -errno if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int flash_file(const char *path, off_t size, off_t offset)
{
  struct hcom_nx_cmd_data *cmdData = NULL;
  error("Flashing that file %s\n", path);
  int ret;

  int filefd = open(path, O_RDONLY);
  if (filefd == -1)
  {
    error("%s@%d-File not found: %s.\n", thisFile, __LINE__, path);
    return -errno;
  }

  struct stat fileStatus;
  ret = fstat(filefd, &fileStatus);
  if (ret < 0)
  {
    error("%s@%d-fstat of %s failed errno:%d\n", thisFile, __LINE__, path, errno);
    return -errno;
  }

  off_t fileSize = fileStatus.st_size;
  if (fileSize != size)
  {
    error("File has invalid size. Expected %d, got %d\n", size, fileSize);
    return -1;
  }

  struct mtd_geometry_s geo;
  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));

  info("Erasing flash memory\n");

  int offsetInEraseBlocks = offset / geo.erasesize;

  size_t numBlocksToErase = fileSize / geo.erasesize;
  int blockserased = MTD_ERASE(_mtd, offsetInEraseBlocks, numBlocksToErase);
  if (blockserased < 0)
  {
    return -1;
  }
  info("Erase success\n");

  uint8_t buf[geo.blocksize];
  size_t numBlocksToWrite = fileSize / geo.blocksize;
  size_t numBlocksToSkip = offset / geo.blocksize;
  for (int i = 0; i < numBlocksToWrite; i++)
  {
    if (read(filefd, buf, geo.blocksize) < 0)
    {
      error("Error reading from %s\n", path);
      goto cleanup;
    }

    ssize_t writtenBlocks = MTD_BWRITE(_mtd, i + numBlocksToSkip, 1, buf);
    if (writtenBlocks != 1)
    {
      error("Error while writing block %d to flash\n", i);
      goto cleanup;
    }

    // 10%, 20% etc
    int lastPercentSent = 0;
    int percentDone = (i * 100) / numBlocksToWrite;
    if (percentDone / 10 != lastPercentSent)
    {
      lastPercentSent = percentDone / 10;

      info("Flashing %d%% complete\n", percentDone);
    }

#define NUTTX_UPDATE_VERIFY 0
#if NUTTX_UPDATE_VERIFY > 0
    uint8_t verify[geo.blocksize];
    //
    //  Don't we need to add numBlocksToSkip here?
    //
    MTD_BREAD(_mtd, i, 1, verify);

    if (memcmp(buf, verify, geo.blocksize) != 0)
    {
      syslog(LOG_ERR, "Error while verifying block %d.\n", i);
      goto cleanup;
    }
#endif
  }
  info("Flash operation complete\n");
cleanup:
  close(filefd);
  // if(cmdData->logLevel != LOG_NONE)
  //   return -1;
  return OK;
}

// mirroring the individual flags in bootloader/Core/Inc/ota_data.h
typedef struct
{
  uint8_t update;
  uint8_t rollback;
  uint8_t backup_flag;
  uint8_t update_failure_flag;
  uint8_t rollback_failure;
  uint8_t backup_failure;
  uint8_t rollback_on_fail;
  uint8_t reserved[0x1000 - 7]; // min struct size = flash geo.erasesize
} OTAState;

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_OS_update_flash1
 *
 * Description:
 *   Part 1 update - AUpdate the operating system.
 * 
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *  OK if successful, -1 or -errno if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_OS_update_flash1(void)
{
  int ret;
  OTAState state = {0};
  ret = flash_file(UPDATE_OS_DIR HCOM_NX_FS_NUTTX_UPDATE_FILENAME, HCOM_NX_FS_NUTTX_UPDATE_SIZE, HCOM_NX_FS_MONO_RAW_PARTITION_SIZE);
  if (ret)
    return ret;

#if (MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH > 0)
  // At boot-up this is called. For this testing disable.
  syslog(2, "Not deleting Meadow.OS.Update.bin because testing is active\n");
  return OK;
#endif
  //
  //  Save the current OTA state to flash
  //
  state.update = 0x1;
  ret = hcom_nx_exec_ex_flash_write_buffer_to_flash((uint8_t*)&state, sizeof(OTAState), HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + HCOM_NX_FS_NUTTX_UPDATE_SIZE);
  if (ret)
    return ret;
  ret = unlink(UPDATE_OS_DIR HCOM_NX_FS_NUTTX_UPDATE_FILENAME);
  if (ret)
    return ret;
  hcom_nx_common_utils_host_restart_meadow();
  return 0; // restarts; never actually returns
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_OS_update_flash2
 *
 * Description:
 *   Part 2 update - Update the runtime system.
 * 
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *  OK if successful, -1 or -errno if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_OS_update_flash2(void)
{
  int ret;
  ret = flash_file(UPDATE_OS_DIR HCOM_NX_FS_MONO_RUNTIME_FILENAME, HCOM_NX_FS_MONO_RAW_PARTITION_SIZE, 0x0);
  if (ret)
    return ret;
#if (MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH > 0)
  // At boot-up this is called. For this testing disable.
  syslog(2, "Not deleting Meadow.OS.Runtime.bin because testing is active\n");
  return OK;
#endif
  ret = unlink(UPDATE_OS_DIR HCOM_NX_FS_MONO_RUNTIME_FILENAME);
  if (ret)
    return ret;
  hcom_nx_common_utils_host_restart_meadow();
  return 0; // restarts; never actually returns
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_persistent_data_location
 *
 * Description:
 *   Get the location of the OS persistent data in flash memory.  This is one
 *   erase block past the start of the OtA data block.
 * 
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *  Location in flash of the OS persistent data.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
uint32_t hcom_nx_exec_ex_flash_persistent_data_location(void)
{
  uint32_t location = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + HCOM_NX_FS_NUTTX_UPDATE_SIZE;
  location += hcom_nx_exec_ex_flash_get_erase_block_size();
  return(location);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_assert_data_location
 *
 * Description:
 *   Get the location of the assertion data from up_assert in flash memory.
 *   this is 2 erase block past the start of the OtA data block.
 * 
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *  Location in flash of the OS persistent data.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
uint32_t hcom_nx_exec_ex_flash_assert_data_location(void)
{
  uint32_t location = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + HCOM_NX_FS_NUTTX_UPDATE_SIZE;
  location += (2 * hcom_nx_exec_ex_flash_get_erase_block_size());
  return(location);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_write_assertion_data
 *
 * Description:
 *   Write the assertion data to flash.
 * 
 * Input Parameters:
 *   data - pointer to the assertion data to be written to flash.
 *   length - amount of the data to be written.
 *
 * Returned Value:
 *  OK if successful, -1 or -errno if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_write_assertion_data(const char *data, uint32_t length)
{
  int result;

  if (data == NULL)
  {
    result = -EINVAL;
  }
  else
  {
    uint32_t location = hcom_nx_exec_ex_flash_assert_data_location();
    off_t amount_to_write = length > HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE ? HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE : length;
    result = hcom_nx_exec_ex_flash_write_buffer_to_flash((uint8_t *) data, amount_to_write, location);
  }

  return(result);
}

/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_read_assertion_data
 *
 * Description:
 *   Read the assertion data from the last call to up_assert from flash.
 * 
 * Input Parameters:
 *   data - Block of memory large enough to hold the assertion data.
 *
 * Returned Value:
 *  OK if successful, -1 or -errno if there was a problem.
 *
 * Assumptions/Limitations:
 *  data must be large enough to hold the assertion data.
 *
 ****************************************************************************/
int hcom_nx_exec_ex_flash_read_assertion_data(const char *data)
{
  int result = OK;

  if (data == NULL)
  {
    result = -EINVAL;
  }
  else
  {
    uint32_t location = hcom_nx_exec_ex_flash_assert_data_location();
    uint32_t pageLocation = location / hcom_nx_exec_ex_flash_get_block_size();
    size_t pagesToRead = HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE / hcom_nx_exec_ex_flash_get_block_size();
    
    size_t pagesRead = MTD_BREAD(_mtd, pageLocation, pagesToRead, data);
    if (pagesRead != pagesToRead)
    {
      result = -EIO;
    }
  }

  return(result);
}

#if (HCOM_NX_EX_FLASH_SHOW_FLASH_STATS > 0)
/****************************************************************************
 * Name: hcom_nx_exec_ex_flash_syslog_external_flash_regions
 *
 * Description:
 *   Mimicking the above code, to output, via syslog, the name, start and size
 *   of each of the external flash's non-file system regions. Also, output
 *   the available flash memory.
 * 
 *   Based on the above code this is the current use of the top 5MB of flash
 *   Runtime       - Size: 3145728 ( 3072 KB), Offset:0x00000000
 *   Meadow update - Size: 1835008 ( 1792 KB), Offset:0x00300000
 *   OTA State     - Size:    4096 (    4 KB), Offset:0x004c0000
 *   OS Persisted  - Size:    4096 (    4 KB), Offset:0x004c1000
 *   Assert Data   - Size:   32768 (   32 KB), Offset:0x004c2000
 *   Unused        - Size:  221184 (  216 KB), Offset:0x004ca000
 *   File System   - Size:61865984 (60416 KB), Offset:0x00500000
 *
 * Input Parameters:
 *  none.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  This function is executed before the flash chip is fully initialized.
 *  Therefore, the flash erase size is hard coded as 4096 (0x1000).
 *
 ****************************************************************************/
void hcom_nx_exec_ex_flash_syslog_external_flash_regions(FAR struct mtd_dev_s *mtd)
{
  uint32_t thisOffset = 0;
  uint32_t flashEraseSize;
  uint32_t totalReserved = (HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + \
                       HCOM_NX_FS_OTA_RESERVED_SPACE);
  // uint32_t meadowVersion = meadow_hw_version_get();
  struct mtd_geometry_s geo;

  mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  
  flashEraseSize = geo.erasesize;

  // Segment 0 - Meadow.OS.Runtime.bin
  syslog(1, "Runtime       - Size:%8lu (%5lu KB), Offset:0x%08x\n",
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE,
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE/1024,
            thisOffset);

  // Segment 1 - Meadow.OS.Update.bin
  thisOffset += HCOM_NX_FS_MONO_RAW_PARTITION_SIZE;
  syslog(1, "Meadow Update - Size:%8lu (%5lu KB), Offset:0x%08x\n",
            HCOM_NX_FS_NUTTX_UPDATE_SIZE,
            HCOM_NX_FS_NUTTX_UPDATE_SIZE/1024,
            thisOffset);

  // Segment 2 - OTAState
  thisOffset += HCOM_NX_FS_NUTTX_UPDATE_SIZE;
  syslog(1, "OTA State     - Size:%8lu (%5lu KB), Offset:0x%08x\n",
            sizeof(OTAState),
            sizeof(OTAState)/1024,
            thisOffset);

  // Segment 3 - OS Persisted Data
  thisOffset += flashEraseSize;
  syslog(1, "OS Persisted  - Size:%8lu (%5lu KB), Offset:0x%08x\n",
          flashEraseSize,
          flashEraseSize/1024,
          thisOffset);

  // Segment 4 - Assert Data
  // See hcom_nx_exec_ex_flash_persistent_data_location();
  thisOffset += flashEraseSize;
  syslog(1, "Assert Data   - Size:%8lu (%5lu KB), Offset:0x%08x\n",
          HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE,
          HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE/1024,
          thisOffset);

  // Segment 5 - Unused flash space
  thisOffset += HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE;
  syslog(1, "Unused        - Size:%8lu (%5lu KB), Offset:0x%08x\n",
          totalReserved - thisOffset,
          (totalReserved - thisOffset)/1024,
          thisOffset);
  
  // Segment 6 - available for file system
  // Get the flash chip size based on the hardware version
  int fileSystemSize = meadow_hw_version_flash_size() - totalReserved;
  thisOffset += (totalReserved - thisOffset);
  syslog(1, "File System   - Size:%8lu (%5lu KB), Offset:0x%08x\n",
          fileSystemSize,
          fileSystemSize/1024,
          thisOffset);
}
#endif    // #if (HCOM_NX_EX_FLASH_SHOW_FLASH_STATS > 0)

#if MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH > 0
//===========================================================================
// Verifies specific pattern has been written in the first 5 MB of flash
// correctly. It uses syslog to display the data on both side of the layout
// boundaries. It is called using 'developer -d 10 -v 2'.
void hcom_nx_exec_ex_flash_verify_segments()
{
#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
  #define HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY (2)

  int ret;
  uint32_t segmentLen;
  uint32_t prevUseSpace = 0;
  uint32_t full5MBFlash = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + \
            HCOM_NX_FS_OTA_RESERVED_SPACE;
  uint32_t startBlock;

  // Find the boundary address and dump 512 bytes (256 before to 256 after).
  uint32_t block_size = hcom_nx_exec_ex_flash_get_block_size();

  uint8_t *flashBuffer = zalloc(HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY * block_size);
  if(flashBuffer == NULL)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }

  // The segments all begin and end on block boundaries, therefore at least
  // 2 blocks must normally be read and the last bit of the first and the
  // first bit of the second are output separately. 
  //----------------------------------------------------------------------
  // 1 - Special case starts at offset = 0, so only 1 block shown, the first
  startBlock = prevUseSpace / block_size;
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }
  syslog(2, "\nTop of 1st (Runtime)\n");
  hcom_nx_diag_print_buffer(flashBuffer, block_size, 1);

  prevUseSpace = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE;

  //----------------------------------------------------------------------
  // 2 - 
  startBlock = prevUseSpace / block_size;
  // Start reading 1 block before the beginning of the segment and end after
  // 2 blocks have been displayed.
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2; // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 1st and Top of 2nd (Meadow Update)\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  prevUseSpace += HCOM_NX_FS_NUTTX_UPDATE_SIZE;

  //----------------------------------------------------------------------
  // 3
  segmentLen = sizeof(OTAState);

  // Backup 1/2 256 so boundary is at the center of the dump
  startBlock = prevUseSpace / block_size;
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2; // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 2nd and Top of 3rd (OTA State)\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  prevUseSpace += segmentLen;

  //----------------------------------------------------------------------
  // 4
  segmentLen = hcom_nx_exec_ex_flash_get_erase_block_size();

  // Backup 1/2 256 so boundary is at the center of the dump
  startBlock = prevUseSpace / block_size;
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2; // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 3rd and Top of 4th (OS Persisted)\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  prevUseSpace += segmentLen;

  //----------------------------------------------------------------------
  // 5
  segmentLen = HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE;

  // Backup 1/2 256 so boundary is at the center of the dump
  startBlock = prevUseSpace / block_size;
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2; // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 4th and Top of 5th (Assert Data)\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  prevUseSpace += segmentLen;
  
  //----------------------------------------------------------------------
  // 6 - Whatever is left. Need to add code to show end of segment too
  segmentLen = full5MBFlash - prevUseSpace;

  // Backup 1/2 256 so boundary is at the center of the dump
  startBlock = prevUseSpace / block_size;
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2;   // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 5th and Top of 6th (Reserved)\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  prevUseSpace += segmentLen;   // Now offset is start of file system

  // ----------
  // Show the end too + 256 of file system
  startBlock = prevUseSpace / block_size;
  startBlock -= HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY / 2;   // 1 block before, 1 after

  // Read the block before and the block after
  ret = hcom_nx_exec_ex_flash_copy_blocks_to_memory(startBlock,
            (void *) flashBuffer, HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret);
    usleep(3000 * 1000);
    free(flashBuffer);
    return;
  }

  syslog(2, "\nBottom of 6th and Start of File System\n");
  hcom_nx_diag_print_buffer(flashBuffer,
            block_size * HCOM_NX_EX_FLASH_BLOCKS_TO_VERIFY, 1);

  free(flashBuffer);
#endif    // #if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
}

//----------------------------------------------------------------
// Populates buffer for flashing
static void hcom_nx_exec_ex_fill_layout_buffer(uint32_t writeBufLen, char *writeBuf,
          char *titleMsg, int fillChar)
{
  int offset;
  uint32_t titleLen = strlen(titleMsg);

  if(titleLen > writeBufLen)
  {
    syslog(2, "%s@%d-Error:titleLen > writeBufLen\n", thisFile, __LINE__);
    return;
  }

  for(offset = 0; offset < titleLen; offset++)
    writeBuf[offset] = titleMsg[offset];
  
  ASSERT(offset == titleLen);

  for(; offset < writeBufLen; offset++)
    writeBuf[offset] = fillChar;
}

/****************************************************************************
 * Name: int hcom_nx_exec_ex_flash_fill_5mb_of_flash()
 *
 * Description:
 *   Fills all 6 regions of the 5MB space in the external flash with easily
 *   detectable data.
 *  Called using 'developer -d 10 -v 1'
 *
 * Input Parameters:
 *   none.
 *
 * Returned Value:
 *   none.
 *
 * Assumptions/Limitations:
 *  Fake files for Meadow.OS.Runtime.bin and Meadow.OS.Update.bin. are ready
 *  to be read and used to populate the related spaces.
 *  Assumes that we will be using this after a restart in test mode so we do
 *  not need to check the zalloc calls for failure as there will be plenty of
 *  memory in the system.
 *
 ****************************************************************************/
  // This function 
void hcom_nx_exec_ex_flash_fill_5mb_of_flash()
{  
  int ret;
  char *titleMsg;
  char *writeBuf;
  uint32_t writeBufLen;
  uint32_t prevUseSpace = 0;

  syslog(2, "%s@%d-**>>Please wait, 5 MB of flash being populated<<**\n", thisFile, __LINE__);

  //----------------------------------------------------------------
  // First is 3MB for runtime. Assumes Meadow.OS.Runtime.bin has been replaced
  // with a fake version.
  // File is filled with a title string then filled with the 'A' character
  syslog(2, "%s@%d-Segment 1 Writing %7lu bytes at offset 0x%08x\n",
            thisFile, __LINE__,
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE, prevUseSpace);
  usleep(100 * 1000);
  
  // Can't use hcom_nx_exec_ex_flash_OS_update_flash2 because it deletes the
  // file. This is copied from there.
  ret = flash_file(UPDATE_OS_DIR HCOM_NX_FS_MONO_RUNTIME_FILENAME,
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE, 0x0);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); 
    return;
  }
  prevUseSpace = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE;

  //----------------------------------------------------------------
  // Second is a 1835008 byte file Meadow.OS.Update.bin
  // File is filled with 'B' characters after title string
  syslog(2, "%s@%d-Segment 2 Writing %7lu bytes at offset 0x%08x\n",
            thisFile, __LINE__,
            HCOM_NX_FS_NUTTX_UPDATE_SIZE, prevUseSpace);
  usleep(100 * 1000);

  // Can't use hcom_nx_exec_ex_flash_OS_update_flash1 because it deletes the
  // file. This is copied from there.
  ret = flash_file(UPDATE_OS_DIR HCOM_NX_FS_NUTTX_UPDATE_FILENAME,
            HCOM_NX_FS_NUTTX_UPDATE_SIZE,
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }

  prevUseSpace += HCOM_NX_FS_NUTTX_UPDATE_SIZE;

  //----------------------------------------------------------------
  // Third is the OTAState
  writeBufLen = sizeof(OTAState);
  titleMsg = "This is Segment 3 used for OTAState, fill with D.";
  syslog(2, "%s@%d-Segment 3 Writing %7lu bytes at offset 0x%08x\n", thisFile, __LINE__,
            writeBufLen, prevUseSpace);
  usleep(100 * 1000);

  writeBuf = zalloc(writeBufLen);
  // Write the title message to the buffer, then the fill character
  hcom_nx_exec_ex_fill_layout_buffer(writeBufLen, writeBuf, titleMsg, 'D');

  // Write the writeBuf to the flash
  // See hcom_nx_exec_ex_flash_OS_update_flash1() for rational
  ASSERT(HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + HCOM_NX_FS_NUTTX_UPDATE_SIZE == prevUseSpace);
  ret = hcom_nx_exec_ex_flash_write_buffer_to_flash((uint8_t*) writeBuf, writeBufLen,
            HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + HCOM_NX_FS_NUTTX_UPDATE_SIZE);

  free(writeBuf);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }

  prevUseSpace += writeBufLen;

  //----------------------------------------------------------------
  // Fourth is a region for persisted data.
  titleMsg = "This is Segment 4 used for persisted data, fill with H.";
  writeBufLen = hcom_nx_exec_ex_flash_get_erase_block_size();
  syslog(2, "%s@%d-Segment 4 Writing %7lu bytes at offset 0x%08x\n", thisFile, __LINE__,
            writeBufLen, prevUseSpace);
  usleep(100 * 1000);

  writeBuf = zalloc(writeBufLen);
  hcom_nx_exec_ex_fill_layout_buffer(writeBufLen, writeBuf, titleMsg, 'H');

  uint32_t fieldLocation = hcom_nx_exec_ex_flash_persistent_data_location();
  ret = hcom_nx_exec_ex_flash_write_buffer_to_flash((uint8_t *)writeBuf, writeBufLen, fieldLocation);
  free(writeBuf);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }
  prevUseSpace += writeBufLen;

  //----------------------------------------------------------------
  // Fifth is a 32KB region for assert data
  titleMsg = "This is Segment 5 used for assertion data, fill with P.";
  writeBufLen = HCOM_NX_MAXIMUM_ASSERTION_DATA_SIZE;
  syslog(2, "%s@%d-Segment 5 Writing %7lu bytes at offset 0x%08x\n", thisFile, __LINE__,
            writeBufLen, prevUseSpace);
  usleep(100 * 1000);

  writeBuf = zalloc(writeBufLen);
  hcom_nx_exec_ex_fill_layout_buffer(writeBufLen, writeBuf, titleMsg, 'P');

  ret = hcom_nx_exec_ex_flash_write_assertion_data(writeBuf, writeBufLen);
  free(writeBuf);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }

  prevUseSpace += writeBufLen;

  //----------------------------------------------------------------
  // 6th is all remaining space in the 5MB of flash
  // All the remaining flash space within the 5MB allocation
  titleMsg = "This is Segment 6 currently not used, fill with Q.";
  uint32_t reservedFlashLen = HCOM_NX_FS_MONO_RAW_PARTITION_SIZE + \
            HCOM_NX_FS_OTA_RESERVED_SPACE;
  writeBufLen = reservedFlashLen - prevUseSpace;
  syslog(2, "%s@%d-Segment 6 Writing %7lu bytes at offset 0x%08x\n", thisFile, __LINE__,
              writeBufLen, prevUseSpace);
  usleep(100 * 1000);

  writeBuf = zalloc(writeBufLen);
  hcom_nx_exec_ex_fill_layout_buffer(writeBufLen, writeBuf, titleMsg, 'Q');

  // offset is the offset after the previous segment.
  ret = hcom_nx_exec_ex_flash_write_buffer_to_flash((uint8_t *)writeBuf, writeBufLen,
            prevUseSpace);
  free(writeBuf);
  if(ret < 0)
  {
    syslog(2, "%s@%d-Error:%d\n", thisFile, __LINE__, ret); usleep(3000 * 1000);
    return;
  }

  // Test flash. It should all be filled
  ASSERT((prevUseSpace + writeBufLen) == reservedFlashLen);
}
#endif  // #if MEADOW_INCLUDE_CODE_FOR_TESTING_5MB_OF_FLASH > 0
