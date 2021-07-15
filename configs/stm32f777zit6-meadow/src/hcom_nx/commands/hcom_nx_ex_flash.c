/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\commands\hcom_nx_ex_flash.c
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

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include "stm32_uid.h"          // stm32_get_uniqueid()

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

#include <sys/stat.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/


/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static FAR struct mtd_dev_s *_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_nx_exec_ex_flash_setup(FAR struct mtd_dev_s *mtd)
{
  _mtd = mtd;
  return OK;
}

//=======================================================================================
// Called from host PC
int hcom_nx_exec_ex_flash_erase_ex_flash(struct hcom_nx_cmd_data *cmdData)
{
  int ret;

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Bulk erase begun. Will take 2-3 minutes.", thisFile, __LINE__);

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
              "Bulk Erase of QSPI Flash error:%d.", ret);
    cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
            thisFile, __LINE__);
    return ret;
  }

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Bulk erase completed successfully", thisFile, __LINE__);

  syslog(LOG_INFO, "Bulk erase complete\n\n");
  return OK;
}

//========================================================================
// Called from host PC
int hcom_nx_exec_ex_flash_verify_ex_flash(struct hcom_nx_cmd_data *cmdData)
{
  FAR struct mtd_geometry_s geo;
  int sectorCounter;
  int notErasedSectors;
  int ret;

  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "External flash verification begun", thisFile, __LINE__);

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

//=======================================================================================
// userData contains the partition number, if partitioning is in use
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

//======================================================================================
// Called from host PC
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

#define MONO_VERIFY 0
#if MONO_VERIFY > 0
    uint8_t verify[geo.blocksize];
    MTD_BREAD(_mtd, i, 1, verify);

    if (memcmp(buf, verify, geo.blocksize) != 0)
    {
      syslog(LOG_ERR, "Error while verifying block %d.\n", i);
      goto cleanup;
    }
#endif
  }

  const char monoSuccessFlashMsg[] = "Mono runtime successfully flashed.\n";
  syslog(LOG_INFO, monoSuccessFlashMsg);
  cmdData->send_host_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoSuccessFlashMsg, thisFile, __LINE__);

  cleanup:
    close(filefd);
    if(cmdData->logLevel != LOG_NONE)
      return -1;
    return OK;
}
