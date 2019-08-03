/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_exec_utility_request.c
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
#include <nuttx/userspace.h>
#include "task/task.h"
// #include "stm32_dfumode.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/


/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct mtd_dev_s *_master_mtd;
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
  _master_mtd = mtd;
  nsh_pid = 0;
  nsh_enabled = false;
  return OK;
}

//=======================================================================================
void hcom_exec_rqst_misc_change_trace_level(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  f7syslog(LOG_NOTICE, "** Changing Trace Level beginning\n");

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

  hcom_persist_trace_level_mask(syslogmask);

  // Does the user care about the old trace level returned as a mask?
  int newTraceLevel = setlogmask(syslogmask);

  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Trace level changed from 0x%02x to 0x%02x\0",
      newTraceLevel, syslogmask);
  ret = hcom_host_msg_builder_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_builder_send_text failed %d\n", __func__, ret);
  }

  f7syslog(LOG_NOTICE, "** Changing Trace Level from 0x%02x to 0x%02x completed\n\n", newTraceLevel, syslogmask);
}

//=======================================================================================
void hcom_exec_rqst_misc_mcu_restart(uint32_t userData)
{
  // From arch/arm/src/armv7-m/up_systemreset.c
  up_systemreset();
}

//=======================================================================================
void hcom_exec_rqst_misc_enable_disable_nsh(uint32_t userData)
{
  // 0 = disable, 1= enable
  int ret;
  char *sendMsgToHost;

  if(nsh_enabled)
  {
    sendMsgToHost = "NSH already enabled\0";
    ret = hcom_host_msg_builder_send_text(sendMsgToHost, strlen((char *)sendMsgToHost));
    if (ret < 0)
    {
      f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_builder_send_text failed %d\n", __func__, ret);
    }
    return;
  }

  if(userData == 1)
  {
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
      ret = task_delete(nsh_pid);
      nsh_pid = 0;
    }
  }
  else
  {
    syslog(LOG_WARNING, "Unexpected value of %d passed to %s()\n", userData, __func__);
  }

  sendMsgToHost = "NSH enabled\0";
  ret = hcom_host_msg_builder_send_text(sendMsgToHost, strlen((char *)sendMsgToHost));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_host_msg_builder_send_text failed %d\n", __func__, ret);
  }
}

//=======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_exec_rqst_misc_enter_dfu_mode(uint32_t userData)
{
  // DFU Mode is on hold
  f7syslog(LOG_INFO, "GOT THIS FAR!  Entered %s()\n", __func__);
  
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

//======================================================================
void hcom_exec_utility_developer_1(uint32_t userData)
{
}

//=============================================================
void hcom_exec_utility_developer_2(uint32_t userData)
{
}

void hcom_exec_utility_developer_3(uint32_t userData)
{
}

void hcom_exec_utility_developer_4(uint32_t userData)
{
}