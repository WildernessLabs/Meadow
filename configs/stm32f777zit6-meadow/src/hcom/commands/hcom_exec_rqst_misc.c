/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_exec_utility_request.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#include <nuttx/userspace.h>
#include <nuttx/kthread.h>
#include "stm32_uid.h"          // stm32_get_uniqueid()

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/


/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static FAR struct mtd_dev_s *_mtd;
static int nsh_pid;
static bool nsh_enabled;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_exec_rqst_misc_setup(FAR struct mtd_dev_s *mtd)
{
  _mtd = mtd;
  nsh_pid = 0;
  nsh_enabled = false;
  return OK;
}

//=======================================================================================
void hcom_exec_rqst_misc_change_trace_level(uint32_t userData)
{
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;

  int syslogmask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ERR) |
                   LOG_MASK(LOG_WARNING);

  switch (userData)
  {
    case HCOM_TRACE_LEVEL_NOTICE:
      syslogmask |= LOG_MASK(LOG_NOTICE);
      break;

    case HCOM_TRACE_LEVEL_NOTICE_INFO:
      syslogmask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO);
      break;

    case HCOM_TRACE_LEVEL_NOTICE_INFO_DEBUG:
      syslogmask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_DEBUG);
      break;
    
    case HCOM_TRACE_LEVEL_DEFAULT:
    default:    // use already calculated syslogmask
      break;
  }

  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_SYSLOG_MASK, syslogmask);

  // Does the user care about the old trace level returned as a mask?
  int newTraceLevel = setlogmask(syslogmask);

  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
          "Trace level changed from 0x%02x to 0x%02x",
          newTraceLevel, syslogmask);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);

  hcom_utils_f7syslog(LOG_NOTICE, "Trace from 0x%02x to 0x%02x\n\n", newTraceLevel, syslogmask);
}

//=======================================================================================

#ifndef CONFIG_BUILD_PROTECTED
int nsh_main(int argc, char *argv[]);
#endif

void hcom_exec_rqst_misc_enable_disable_nsh(uint32_t userData)
{  
#ifdef CONFIG_SYSTEM_NSH
  // 0 = disable, 1= enable
  char *sendMsgToHost;

  if(nsh_enabled)
  {
    sendMsgToHost = "NSH already enabled";
    hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
            thisFile, __LINE__);
    return;
  }

  if(userData == 1)
  {
    // Create a unique task for NSH to isolate its threads from those
    // here in hcom.
#ifdef CONFIG_BUILD_PROTECTED
    DEBUGASSERT(USERSPACE->us_entrypoint != NULL);
    nsh_pid = 0;
    nsh_pid = task_create("nshTask", CONFIG_USERMAIN_PRIORITY,
                        CONFIG_USERMAIN_STACKSIZE,
                        USERSPACE->us_entrypoint,
                        (FAR char * const *)NULL);
#else
    nsh_pid = task_create("nshTask", CONFIG_USERMAIN_PRIORITY,
                        CONFIG_USERMAIN_STACKSIZE,
                        (main_t)CONFIG_USER_ENTRYPOINT,
                        (FAR char * const *)NULL);
#endif
    DEBUGASSERT(nsh_pid > 0);
    nsh_enabled = true;
  }
  else if(userData == 0)
  {
    if(nsh_pid > 0)
    {
      // This call returns 0 (i.e. OK) but if NSH is relaunch, it's not useable.
      // Not supported by CLI at this time
      task_delete(nsh_pid);
      nsh_pid = 0;
    }
  }
  else
  {
    hcom_utils_f7syslog(LOG_WARNING, "%s@%d-userData %d meaningless\n", thisFile, __LINE__, userData);
  }

  sendMsgToHost = "NSH enabled";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
          thisFile, __LINE__);

#else

  char *sendMsgToHost = "NuttShell (NSH) not configured in MeadowOS";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost,
          thisFile, __LINE__);
#endif
}

//=======================================================================================
void hcom_exec_rqst_misc_mcu_restart(uint32_t userData)
{
  // Set flag for testing on restart
  hcom_utils_bbreg_set_bit(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  char *sendMsgToHost = "Restarting F7 Micro"; 
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, userData,
          sendMsgToHost, thisFile, __LINE__);

  // Tell host to begin to reconnect
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          sendMsgToHost, thisFile, __LINE__);

  usleep(500 * 1000);
  // From arch/arm/src/armv7-m/up_systemreset.c
  up_systemreset();
}

//=======================================================================================
// Disable Mono from running on next MCU reset
void hcom_exec_rqst_misc_mono_disable(uint32_t userData)
{
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACCESS, HCOM_MONO_MAIN_ACCESS_KEY);
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACTION, HCOM_MONO_MAIN_ACTION_ENABLE_KEY);
  
  char *sendMsgToHost = "Mono disabled. Restarting Meadow";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);

  hcom_utils_bbreg_set_bit(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  // Tell host to begin to reconnect
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          sendMsgToHost, thisFile, __LINE__);

  usleep(500 * 1000);
  up_systemreset();
}

//=======================================================================================
// Enable Mono to run on next MCU reset
void hcom_exec_rqst_misc_mono_enable(uint32_t userData)
{
  // Clean to enable mono
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACCESS, 0);
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACTION, 0);
  
  char *sendMsgToHost = "Mono being enabled. Restarting F7 Micro";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          sendMsgToHost, thisFile, __LINE__);

  hcom_utils_bbreg_set_bit(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);
  
  // Tell host to begin to reconnect
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData,
          sendMsgToHost, thisFile, __LINE__);

  usleep(500 * 1000);
  up_systemreset();
}


//======================================================================================
// The host has ask for the mono startup state
void hcom_exec_rqst_misc_mono_run_state(uint32_t userData)
{
  char *monoStartupMsg;

  if(hcom_utils_is_mono_disabled())
    monoStartupMsg = "On reset, mono will not run app.exe";
  else
    monoStartupMsg = "On reset, mono will run app.exe";
  
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          monoStartupMsg, thisFile, __LINE__);
}

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

//======================================================================================
void hcom_exec_rqst_misc_mono_flash(uint32_t userData)
{
  const char monoFlashMsg[] = "Flashing Mono from filesystem to external flash.";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoFlashMsg, thisFile, __LINE__);

  // Check for Mono runtime binary on filesystem.
#ifdef CONFIG_MTD_PARTITION
  const char runtimePath[] = "/meadow0/" HCOM_FS_MONO_RUNTIME_FILENAME;
#else
  const char runtimePath[] = "/meadow/" HCOM_FS_MONO_RUNTIME_FILENAME;
#endif

  FILE* file = fopen(runtimePath, "r");
  if (file == NULL)
  {
    hcom_utils_f7syslog(LOG_ERR, "Mono runtime was not found in %s.\n", runtimePath);
    return;
  }

  fseek(file, 0L, SEEK_END);
  int fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  if (fileSize != HCOM_FS_MONO_RAW_PARTITION_SIZE)
  {
    hcom_utils_f7syslog(LOG_ERR, "Mono runtime binary has invalid size.\n");
    return;
  }

  struct mtd_geometry_s geo;
  _mtd->ioctl(_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));

  size_t numBlocksToErase = fileSize / geo.erasesize;
  MTD_ERASE(_mtd, 0, numBlocksToErase);

  uint8_t buf[geo.blocksize];
  size_t numBlocksToWrite = fileSize / geo.blocksize;
  for (int i = 0; i < numBlocksToWrite; i++)
  {
    if (fread(buf, geo.blocksize, 1, file) != 1)
    {
      hcom_utils_f7syslog(LOG_ERR, "Error reading from %s.\n", HCOM_FS_MONO_RUNTIME_FILENAME);
      goto cleanup;
    }

    ssize_t writtenBlocks = MTD_BWRITE(_mtd, i, 1, buf);
    if (writtenBlocks != 1)
    {
      hcom_utils_f7syslog(LOG_ERR, "Error while writing block %d to flash.\n", i);
      goto cleanup;
    }

#define VERITY 0
#if VERIFY > 0
    uint8_t verify[geo.blocksize];
    MTD_BREAD(_mtd, i, 1, verify);

    if (memcmp(buf, verify, geo.blocksize) != 0)
    {
      hcom_utils_f7syslog(LOG_ERR, "Error while verifying block %d.\n", i);
      goto cleanup;
    }
#endif
  }

  const char monoSuccessFlashMsg[] = "Mono runtime successfully flashed.\n";
  hcom_utils_f7syslog(LOG_INFO, monoSuccessFlashMsg);
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          (char*)monoSuccessFlashMsg, thisFile, __LINE__);

  cleanup:
    fclose(file);
}

//======================================================================================
void hcom_exec_rqst_misc_get_device_info(uint32_t userData)
{
  char *csvDevInfo;
  int stringLen;

  csvDevInfo = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);
  if(csvDevInfo == NULL)
  {
    hcom_utils_f7syslog(LOG_ERR, "%s@%d-Alloc failed\n", thisFile, __LINE__);
    stringLen = snprintf(csvDevInfo, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Memory allocation error. No results will be sent");
    
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            csvDevInfo, thisFile, __LINE__);

    hcom_utils_f7syslog(LOG_NOTICE, "Get device info error\n");
    return;
  }

  // 96 bit unique chip id as 12 bytes
  uint8_t uniqueId[12];
  char strChipId[128];

  stm32_get_uniqueid(uniqueId);
  snprintf(strChipId, 128, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
    uniqueId[0], uniqueId[1], uniqueId[2], uniqueId[3], uniqueId[4], uniqueId[5],
    uniqueId[6], uniqueId[7], uniqueId[8], uniqueId[9], uniqueId[10], uniqueId[11]);

  uint8_t serialNumb[6];
  serialNumb[0] = uniqueId[11];                 // 95-88
  serialNumb[1] = uniqueId[10] + uniqueId[2];   // 87-80 + 23-16
  serialNumb[2] = uniqueId[9];                  // 79-72
  serialNumb[3] = uniqueId[8] + uniqueId[0] + 10;    // 71-64 + 7-0 + magic 10
  serialNumb[4] = uniqueId[7];                  // 63-56 
  serialNumb[5] = uniqueId[6];                  // 55-48

  char strChipSN1[128];
  snprintf(strChipSN1, 128, "%02X%02X%02X%02X%02X%02X", 
  serialNumb[0], serialNumb[1], serialNumb[2], serialNumb[3], serialNumb[4], serialNumb[5]);

  // Save for reference - produces same result as above but needs the magic 10 added
  // #define STM32F7_SYSMEM_UID ((uint32_t *)STM32_SYSMEM_UID)
  // uint32_t chipId0 = STM32F7_SYSMEM_UID[0];
  // uint32_t chipId1 = STM32F7_SYSMEM_UID[1];
  // uint32_t chipId2 = STM32F7_SYSMEM_UID[2];
  // chipId0 += chipId2;
  // char strChipSN2[128];
  // snprintf(strChipSN2, 128, "%08X%04X", chipId0, chipId1 >> 16);

  stringLen = snprintf(csvDevInfo, HCOM_MAX_HOST_STRING_BUFF_LENGTH,
    "%s, Model: %s, MeadowOS Version: %s (%s %s), Processor: %s, Processor Id: %s, Serial Number: %s, CoProcessor: %s, CoProcessor OS Version: %s",
    HCOM_DEVICE_INFO_PRODUCT, HCOM_DEVICE_INFO_MODEL,
    HCOM_DEVICE_INFO_MEADOW_OS_VERSION, __DATE__, __TIME__,
    HCOM_DEVICE_INFO_PROCESSOR_TYPE, strChipId, strChipSN1, 
    HCOM_DEVICE_INFO_COPROCESSOR_TYPE, HCOM_DEVICE_INFO_COPROCESSOR_OS_VERSION);

  DEBUGASSERT(stringLen < HCOM_MAX_HOST_STRING_BUFF_LENGTH);
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0,
          csvDevInfo, thisFile, __LINE__);
    
  free(csvDevInfo);
}

//======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_exec_rqst_misc_enter_dfu_mode(uint32_t userData)
{
  char * hostMsg = "DFU mode is not implemented";
  hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0,
          hostMsg, thisFile, __LINE__);
}

