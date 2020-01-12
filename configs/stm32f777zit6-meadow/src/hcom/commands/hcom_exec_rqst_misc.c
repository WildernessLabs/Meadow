/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_exec_utility_request.c
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
  int ret;

  f7syslog(LOG_NOTICE, "Changing Trace Level beginning\n");

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

  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Trace level changed from 0x%02x to 0x%02x",
      newTraceLevel, syslogmask);

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  f7syslog(LOG_NOTICE, "Changing Trace Level from 0x%02x to 0x%02x completed\n\n", newTraceLevel, syslogmask);
}

//=======================================================================================
void hcom_exec_rqst_misc_enable_disable_nsh(uint32_t userData)
{
  int ret;
  
#ifdef CONFIG_SYSTEM_NSH
  // 0 = disable, 1= enable
  char *sendMsgToHost;

  if(nsh_enabled)
  {
    sendMsgToHost = "NSH already enabled";
    ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

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
      // This returns 0 (i.e. OK) but if NSH is relaunch, it's not useable.
      // Not supported by CLI at this time
      ret = task_delete(nsh_pid);
      nsh_pid = 0;
    }
  }
  else
  {
    syslog(LOG_WARNING, "Unexpected value of %d passed to %s()\n", userData, __func__);
  }

  sendMsgToHost = "NSH enabled";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

#else

  char *sendMsgToHost = "NuttShell (NSH) not configured in MeadowOS";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
#endif
}

//=======================================================================================
void hcom_exec_rqst_misc_mcu_restart(uint32_t userData)
{
  int ret;

  // Set flag for testing on restart
  hcom_utils_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  char *sendMsgToHost = "Restarting F7 Micro"; 
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, userData, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  // Tell host to begin to reconnect
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  usleep(500 * 1000);
  // From arch/arm/src/armv7-m/up_systemreset.c
  up_systemreset();
}

//=======================================================================================
// Disable Mono from running on next MCU reset
void hcom_exec_rqst_misc_mono_disable(uint32_t userData)
{
  int ret;

  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACCESS, HCOM_MONO_MAIN_ACCESS_KEY);
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACTION, HCOM_MONO_MAIN_ACTION_ENABLE_KEY);
  
  char *sendMsgToHost = "Mono being disabled. Restarting F7 Micro";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  hcom_utils_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);

  // Tell host to begin to reconnect
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  usleep(500 * 1000);
  up_systemreset();
}

//=======================================================================================
// Enable Mono to run on next MCU reset
void hcom_exec_rqst_misc_mono_enable(uint32_t userData)
{
  int ret;

  // Clean to enable mono
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACCESS, 0);
  hcom_utils_bbreg_write(HCOM_BATTERY_BACKED_REG_MONO_ACTION, 0);
  
  char *sendMsgToHost = "Mono being enabled. Restarting F7 Micro";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  hcom_utils_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_RESTART_CONCLUDED_BIT_FLAG);
  
  // Tell host to begin to reconnect
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_RECONNECT, userData, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  usleep(500 * 1000);
  up_systemreset();
}

//======================================================================================
void hcom_exec_rqst_misc_send_diag_to_host(uint32_t userData)
{
  int ret;

  hcom_utils_bbreg_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_DIAG_MSG_TO_HOST_BIT_FLAG);

  char *sendMsgToHost = "Diagnostic messages will be sent";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
}

//======================================================================================
void hcom_exec_rqst_misc_no_diag_msg_to_host(uint32_t userData)
{
  int ret;
  
  char *sendMsgToHost = "Diagnostic messages will not be sent";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, sendMsgToHost);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  hcom_utils_bbreg_bit_clear(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_DIAG_MSG_TO_HOST_BIT_FLAG);
}

//======================================================================================
// Return the mono startup state
void hcom_exec_rqst_misc_mono_run_state(uint32_t userData)
{
  int ret;  
  char *monoStartupMsg;

  if(hcom_utils_is_mono_disabled())
    monoStartupMsg = "On F7 Micro reset, mono will not run applications";
  else
    monoStartupMsg = "On F7 Micro reset, mono will run applications";
  
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, monoStartupMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
}

//======================================================================================
void hcom_exec_rqst_misc_get_device_info(uint32_t userData)
{
  char *csvDevInfo;
  int stringLen;
  int ret;

  csvDevInfo = malloc(HCOM_MAX_HOST_STRING_BUFF_LENGTH);
  if(csvDevInfo == NULL)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Memory allocation failed\n", __func__);
    stringLen = snprintf(csvDevInfo, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Memory allocation error. No results will be sent");
    
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, csvDevInfo);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

    f7syslog(LOG_NOTICE, "Getting device information error exit\n");
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
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0, csvDevInfo);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);
    
  free(csvDevInfo);
}

//======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_exec_rqst_misc_enter_dfu_mode(uint32_t userData)
{
  int ret;

  char * hostMsg = "DFU mode is not implemented";
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_DEVICE_INFO, 0, hostMsg);

  DEBUGASSERT(ret == OK);
  // DFU Mode is on hold
  f7syslog(LOG_INFO, "GOT THIS FAR!  Entered %s()\n", __func__);

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
//   asm("ldr r0, =STM32_RCC_AHB2ENR\n\t"    /* RCC_APB2ENR */
//       "ldr r0, =0x40023844\n\t"    /* RCC_APB2ENR */
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

