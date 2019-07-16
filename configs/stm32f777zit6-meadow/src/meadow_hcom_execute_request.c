/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow_hcom_request_action.c
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

#include "meadow_hcom_common.h"

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
// #include "stm32_dfumode.h"

#ifdef CONFIG_SEMIHOSTING_STAT
#warning "Because CONFIG_SEMIHOSTING_STAT is defined SmartFS formatting will not be possible"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent
#define HCOM_TEMP_MAX_HOST_STRING_LEN 128 // TODO - remove when host bound working

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct mtd_dev_s *_master_mtd;

static int _currentHcomDataPacketAction;
static uint32_t _xferRecvFullFileCrc;
static uint32_t _xferRecvFullFileSize;
static uint32_t _xferCalcFullFileCrc = 0;  // This is over all the payload (original data)
static uint32_t _xferCalcFullFileSize = 0; // This is the size of the original
static uint32_t _xferCalcPacketCrc = 0;    // This is over all packets
static int _dbgNumbPacketsRecvd = 0;
static bool _fileSystemOpenFailed;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_execute_request_action_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  _fileSystemOpenFailed = false;
  _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
  return OK;
}

//====================================================================
bool hcom_receiver_is_currently_active()
{
  return (_currentHcomDataPacketAction != CurrentHcomDataPacketActionNone);
}

/****************************************************************************
 * Implementation
 ****************************************************************************/

#if HCOM_RECV_DEBUG_TIMING
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;

static uint64_t get_current_time64(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}
#endif

//=======================================================================================
// Erase the entire qspi flash chip
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_bulk_erase(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;

  f7syslog(LOG_NOTICE, "** Bulk erase of QSPI Flash beginning\n");
  int ret = _master_mtd->ioctl(_master_mtd, MTDIOC_BULKERASE, 0);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: IOCTL MTDIOC_BULKERASE failed. Returned %d\n", __func__, ret);
  }

  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Bulk Erase of QSPI Flash completed.\0");
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed. Returned %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Bulk erase of QSPI Flash completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_verify_erase(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  f7syslog(LOG_NOTICE, "** Verification of QSPI Flash Erased state beginning\n");
  int errorCount = hcom_fs_helper_verify_erased_flash(_master_mtd);
  f7syslog(LOG_NOTICE, "** Verified Erased Flash completed and found %d non-erased partitions.\n\n", errorCount);

  // Send text message to host
  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Testing Erased Flash found %d non-erased partitions.\0", errorCount);
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_partition(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  uint32_t numberOfPartitions = userData;
  f7syslog(LOG_NOTICE, "** Partitioning of Flash beginning\n");

  // Partitions the entire flash chip with the number of partitions provided
  ret = hcom_fs_helper_init_fs_partitions(_master_mtd, numberOfPartitions);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Flash file system partition and format failed: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Partition and format completed for QSPI Flash with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Partitioned into %d partitions completed for QSPI Flash. No errors reported\0",
                      numberOfPartitions);
  }

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed. Returned %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Partitioning of Flash completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_mount(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
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

    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN,
                      "Failed to mount '%s' to '%s' for type '%s' on PartitionID %d errno:%d\0\n",
                      __func__, HCOM_FILE_MOUNT_POINT_SOURCE, HCOM_FILE_MOUNT_POINT_TARGET,
                      HCOM_FILE_MOUNT_FILE_SYS_TYPE, partitionId, ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Mount of flash file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Mounting of Flash File System completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_initialize(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  uint32_t partitionId = userData;
  f7syslog(LOG_NOTICE, "** Initialize Flash File System beginning\n");

  ret = hcom_fs_helper_initialize_fs(partitionId);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Initialize File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Initialize file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Initialize file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Initialization of File System completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_format(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

#ifdef CONFIG_SEMIHOSTING_STAT
  char *semihostingMsg = "File format is not possible with 'CONFIG_SEMIHOSTING_STAT' configured\0";
  hcom_transmitter_send_text(semihostingMsg, strlen(semihostingMsg));
  return;
#endif

  // Comes from hcom message
  // Valid partitions are 0 - n, where n is not greater than HCOM_FLASH_FILE_PARTITION_COUNT_MAX
  f7syslog(LOG_NOTICE, "** Format Flash File System beginning\n");

  ret = hcom_fs_helper_format_smartfs(userData);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Format File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Format file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Format file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Format File System completed\n\n");
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_create(uint32_t userData)
{
  // This single call will partition, initialize, format (if needed) and mount the file system
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

#ifdef CONFIG_SEMIHOSTING_STAT
  char *semihostingMsg = "File system creation is not possible with 'CONFIG_SEMIHOSTING_STAT' configured\0";
  hcom_transmitter_send_text(semihostingMsg, strlen(semihostingMsg));
  return;
#endif

  f7syslog(LOG_NOTICE, "** Create entire Flash File System beginning\n");

  ret = hcom_fs_helper_create_partition_initialize_and_mount_fs(_master_mtd, userData);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Create File System failed with error: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Create file system failed with error %d.\0", ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Create file system completed. No errors reported\0");
  }

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
  f7syslog(LOG_NOTICE, "** Create File System completed\n\n");
}

//=======================================================================================
void hcom_execute_request_mcu_restart(uint32_t userData)
{
  // From arch/arm/src/armv7-m/up_systemreset.c
  up_systemreset();
}

//=======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_execute_request_enter_dfu_mode(uint32_t userData)
{
  // WIP -----
  f7syslog(LOG_INFO, "GOT THIS FAR!  Entered %s()\n", __func__);
  
  // Write magic number to RAM and reset the MCU
  *HCOM_MAGIC_NUMBER_DFU_MODE_ADDR = HCOM_MAGIC_NUMBER_DFU_MODE_VALUE1;
  *(HCOM_MAGIC_NUMBER_DFU_MODE_ADDR + 1) = HCOM_MAGIC_NUMBER_DFU_MODE_VALUE2;
  *(HCOM_MAGIC_NUMBER_DFU_MODE_ADDR + 2) = HCOM_MAGIC_NUMBER_DFU_MODE_VALUE3;
  *(HCOM_MAGIC_NUMBER_DFU_MODE_ADDR + 3) = HCOM_MAGIC_NUMBER_DFU_MODE_VALUE4;

  //up_systemreset();
}

// //  *  REVISIT:  STM32_SYSMEM_BASE is not 0x1fff000 for all STM32's.  For F3's
// //  *  The SYSMEM base is at 0x1fffd800
// //  *
// //  *  REVISIT:  RCC_APB2ENR_SYSCFGEN is not bit 14 for all STM32's.  For F3's
// //  *  and L15's, it is bit 0.
// //  *
// //  *  REVISIT:  STM32 F3's do not support the SYSCFG_MEMRMP register.
// //  *

// // RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;   /* Enable the SYSCFG peripheral clock*/
// // SYSCFG->CFGR1 = SYSCFG_CFGR1_MEM_MODE;  /* Remap SRAM at 0x00000000 */

// // void stm32_dfumode(void)
// // {
// // #ifdef CONFIG_DEBUG_WARN
// //   _warn("Entering DFU mode...\n");
// //   sleep(1);
// // #endif

// // Original code from ...\Meadow\Meadow.OS\nuttx\arch\arm\src\stm32\stm32_dfumode.c
// // STM32_RCC_AHB2ENR from ...\Meadow\Meadow.OS\nuttx\arch\arm\src\stm32f7\chip\stm32f76xx77xx_rcc.h
// // STM32_SYSCFG_MEMRMP from ...\Meadow\Meadow.OS\nuttx\arch\arm\src\stm32f7\chip\stm32f76xx77xx_syscfg.h
// // 0x1FF0EDBE from STMicrosystems AN2602 - Table 3 for STM32F76xxx/77xxx
//   asm("ldr r0, =STM32_RCC_AHB2ENR\n\t"    /* RCC_APB2ENR */  // "ldr r0, =0x40023844\n\t"    /* RCC_APB2ENR */
//       "ldr r1, =0x00004000\n\t"    /* Enable SYSCFG clock */
//       "str r1, [r0, #0]\n\t"

//       "ldr r0, =STM32_SYSCFG_MEMRMP\n\t"    /* SYSCFG_MEMRMP */ // "ldr r0, =0x40013800\n\t"    /* SYSCFG_MEMRMP */
//       "ldr r1, =0x00000001\n\t"    /* Map ROM at zero */
//       "str r1, [r0, #0]\n\t"

//       "ldr r0, =0x1ff0edbe\n\t"    /* ROM base */ // "ldr r0, =0x1fff0000\n\t"    /* ROM base */
//       "ldr sp,[r0, #0]\n\t"        /* SP @ 0 */
//       "ldr r0,[r0, #4]\n\t"        /* PC @ 4 */
//       "bx r0\n");

//   __builtin_unreachable();         /* Tell compiler we will not return */
// // }

// //***************************************************************************
// void BootDFU(void)
// {
//   printf('Entering Boot Loader..
// ');

//  SCB_DisableDCache();
//  *((unsigned long *)0x2004FFF0) = 0xDEADBEEF; // 320KB STM32F7xx
//  __DSB();

//  NVIC_SystemReset(); 
// }
// //***************************************************************************
 
// ; Reset handler
// Reset_Handler PROC
//  EXPORT Reset_Handler [WEAK]
//  IMPORT SystemInit
//  IMPORT __main
//  LDR R0, =0x2004FFF0 ; Address for RAM signature
//  LDR R1, =0xDEADBEEF
//  LDR R2, [R0, #0]
//  STR R0, [R0, #0] ; Invalidate
//  CMP R2, R1
//  BEQ Reboot_Loader

//  LDR R0, =SystemInit
//  BLX R0
//  LDR R0, =__main
//  BX R0
//  ENDP

// Reboot_Loader PROC
//  EXPORT Reboot_Loader ; STM32F7xx
//  LDR R0, =0x1FF00000 ; ROM BASE
//  LDR SP, [R0, #0] ; SP @ +0
//  LDR R0, [R0, #4] ; PC @ +4
//  BX R0
//  ENDP ; sourcer32@gmail.com
// ‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍‍
// }

//=======================================================================================
void hcom_execute_request_change_trace_level(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  f7syslog(LOG_NOTICE, "** Changing Trace Level beginning\n");

  int syslogmask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ERR) |
                   LOG_MASK(LOG_WARNING);

  switch (userData)
  {
    case HCOM_DIAG_LOG_NOTICE:
      syslogmask |= LOG_MASK(LOG_NOTICE);
      break;

    case HCOM_DIAG_LOG_NOTICE_INFO:
      syslogmask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO);
      break;

    case HCOM_DIAG_LOG_NOTICE_INFO_DEBUG:
      syslogmask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_DEBUG);
      break;

    default:    // case 0: restores default
      break;
  }

  // Does the user care about the old trace level? It's returned as a mask.
  ret = setlogmask(syslogmask);
  f7syslog(LOG_DEBUG, "Changed trace level from 0x%02x to 0x%02x\n", ret, syslogmask);

  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Trace level changed. No errors reported\0");
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }

  f7syslog(LOG_NOTICE, "** Changing Trace Level completed\n\n");
}

//=======================================================================================
void hcom_execute_request_flash_file_xfer_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
                                                uint32_t partitionId)
{
  off_t msgOffset = 0;
  char *sendStartMsg;

  _xferCalcFullFileCrc = 0; // Setup for checksum calculation of orig file
  _fileSystemOpenFailed = false;

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = get_current_time64();
#endif

  // File size
  _xferRecvFullFileSize = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                          (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Checksum
  _xferRecvFullFileCrc = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // FileName
  size_t fileNameLength = recvPacketDataSize - msgOffset;
  
  char *fileNameBuffer = malloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + msgOffset, fileNameLength);
  msgOffset += fileNameLength;

  _currentHcomDataPacketAction = CurrentHcomDataPacketActionExtFileXfer;

  f7syslog(LOG_INFO, "--------- Header for file transfer -------------\n");
  f7syslog(LOG_INFO, "PartitionId=%d, FullFileSize=%d, FullFileCrc=0x%08x _FileName = %s\n",
           partitionId, _xferRecvFullFileSize, _xferRecvFullFileCrc, fileNameBuffer);
  hcom_diag_print_buffer(recvPacketData, recvPacketDataSize, LOG_DEBUG);

  int ret = hcom_file_processing_open(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    _fileSystemOpenFailed = true;
    f7syslog(LOG_ERR, "%s() Error returned from call to hcom_file_processing_open: %d\n", __func__, ret);
  }
  free(fileNameBuffer);

  // Send text message to host
  if (_fileSystemOpenFailed)
    sendStartMsg = "Failed to open target file\0";
  else
    sendStartMsg = "File transfer header received with no errors\0";
  hcom_transmitter_send_text(sendStartMsg, strlen((char *)sendStartMsg));
}

//=======================================================================================
// Process a end of file transfer message
void hcom_execute_request_flash_file_xfer_end(uint32_t userData)
{
  f7syslog(LOG_NOTICE, "--------- End of File Transfer Trailer -------------\n");

  int ret = hcom_file_processing_close();
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: File close failed %d\n", __func__, ret);
  }

  // Compare results and report to host
  char *sendEndMsgToHost;
  if (_fileSystemOpenFailed)
  {
    sendEndMsgToHost = "File Send Failed, file system could not be opened.\0";
  }
  else if (_xferCalcFullFileCrc == _xferRecvFullFileCrc && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    sendEndMsgToHost = "File Sent Successfully\0";
  }
  else
  {
    if (_xferCalcFullFileCrc != _xferRecvFullFileCrc)
    {
      char crcError[64];
      snprintf(crcError, 64, "Checksum matching error Calc = 0x%08X, Recv = 0x%08X\0",
               _xferCalcFullFileCrc, _xferRecvFullFileCrc);
      sendEndMsgToHost = crcError;
    }
    else if (_xferCalcFullFileSize != _xferRecvFullFileSize)
    {
      char sizeError[64];
      snprintf(sizeError, 64, "Size matching error Calc = %d, Recv = %d\0",
               _xferCalcFullFileSize, _xferRecvFullFileSize);
      sendEndMsgToHost = sizeError;
    }
  }
  // Send text message to host
  hcom_transmitter_send_text(sendEndMsgToHost, strlen((char *)sendEndMsgToHost));

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionEndedAt = get_current_time64();
  f7syslog(LOG_DEBUG, "File transfer %d packets, took %llu mSec, CalcPacketCRC:0x%08x CalcFileCRC:0x%08x\n",
           _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           _xferCalcPacketCrc, _xferCalcFullFileCrc);
#else
  f7syslog(LOG_DEBUG, "Host has sent %d packets\n", _dbgNumbPacketsRecvd);
#endif

  _xferCalcPacketCrc = 0;
  _xferCalcFullFileSize = 0;
  _xferCalcFullFileCrc = 0; // Set to 0 for next message

  _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
}

//=======================================================================================
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_execute_request_flash_fs_delete(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
    uint32_t partitionId)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  size_t fileNameLength = recvPacketDataSize - HCOM_PROTOCOL_REQUEST_FILE_HDR_FILENAME_OFFSET;
  char *fileNameBuffer = malloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + HCOM_PROTOCOL_REQUEST_FILE_HDR_FILENAME_OFFSET, fileNameLength);

  ret = hcom_file_processing_delete_file(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() Error returned from call to hcom_file_processing_delete_file: %d\n", __func__, ret);
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Deletaion of file '%s' failed [Error %d]\0",
        fileNameBuffer, ret);
  }
  else
  {
    strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "File '%s' deleted. No errors reported\0",
        fileNameBuffer);
  }
  
  free(fileNameBuffer);

  // Send text message to host
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
}

//============================================================================
// Process data packet based on currently active state
void hcom_execute_data_packet(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb)
{
  // TODO - insure that packets are numbered sequentially

  int ret;
  int msgOffset = sizeof(uint16_t); // size of sequence number

  if (_fileSystemOpenFailed)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Data Packet received but ignored - previous requested file open failed (seq %d)\n",
             __func__, seqNumb);
    return;
  }

  _dbgNumbPacketsRecvd++;

  // Calculate the running checksum including the sequence number
  _xferCalcPacketCrc = crc32part(packet, packetSize, _xferCalcPacketCrc);

  const uint8_t *recvOrigData = packet + msgOffset;
  const size_t recvOrigDataSize = packetSize - msgOffset;

  // Calculate CRC checksum of the payload without sequence number
  _xferCalcFullFileCrc = crc32part(recvOrigData, recvOrigDataSize, _xferCalcFullFileCrc);
  _xferCalcFullFileSize += recvOrigDataSize;

  // Depending on what we're doing process this data packet
  switch (_currentHcomDataPacketAction)
  {
  case CurrentHcomDataPacketActionExtFileXfer:
    ret = hcom_file_processing_write(recvOrigData, recvOrigDataSize);
    break;

  default:
    ret = -1;
    f7syslog(LOG_ERR, "%s() ERROR: Data Packet (SeqNumb=%d), but Data Packet Action unknown\n",
             __func__, seqNumb);
  }

  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Data Packet received but write failed [%d] for sequence %d\n",
             __func__, ret, seqNumb);
  }
}
