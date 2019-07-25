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
// #include "stm32_dfumode.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Read/write blocks are 256 bytes and erase blocks are 4096 bytes
#define HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK (16)

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct mtd_dev_s *_master_mtd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int hcom_exec_utility_verify_erased_flash_worker(FAR struct mtd_dev_s *full_block_mtd);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_exec_utility_request_setup(FAR struct mtd_dev_s *mtd)
{
  _master_mtd = mtd;
  return OK;
}

//=======================================================================================
// Erase the entire qspi flash chip
// The only reason this is safe is because there is only one thread that receives, sends
// and processes all host communications.
void hcom_exec_utility_request_flash_bulk_erase(uint32_t userData)
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
void hcom_exec_utility_request_flash_verify_erase(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  int strLen;
  int ret;

  f7syslog(LOG_NOTICE, "** Verification of QSPI Flash Erased state beginning\n");
  int errorCount = hcom_exec_utility_verify_erased_flash_worker(_master_mtd);
  f7syslog(LOG_NOTICE, "** Verified Erased Flash completed and found %d non-erased partitions.\n\n", errorCount);

  // Send text message to host
  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Testing Erased Flash found %d non-erased partitions.\0", errorCount);
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }
}

//======================================================================
// Verify that the entire chip contains 0xff
int hcom_exec_utility_verify_erased_flash_worker(FAR struct mtd_dev_s *entire_flash_mtd)
{
  FAR struct mtd_geometry_s geo;
  int blockCounter;
  int errorBlocks;
  int ret;

  // Get geometry of QSPI Flash
  ret = entire_flash_mtd->ioctl(entire_flash_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Read geometry for MTD failed: %d\n", __func__, ret);
    return ret;
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
    size_t blocksRead = MTD_BREAD(entire_flash_mtd, blockCounter * HCOM_WRITE_BLOCKS_IN_ERASE_BLOCK,
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

  return errorBlocks;
}

//=======================================================================================
void hcom_exec_utility_request_change_trace_level(uint32_t userData)
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
  ret = setlogmask(syslogmask);
  f7syslog(LOG_DEBUG, "Changed trace level from 0x%02x to 0x%02x\n", ret, syslogmask);

  strLen = snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Trace level changed from 0x%02x to 0x%02x\0",
      ret, syslogmask);
  ret = hcom_transmitter_send_text(hostMsg, strLen);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s() ERROR: hcom_transmitter_send_text failed %d\n", __func__, ret);
  }

  f7syslog(LOG_NOTICE, "** Changing Trace Level completed\n\n");
}

//=======================================================================================
void hcom_exec_utility_request_mcu_restart(uint32_t userData)
{
  // From arch/arm/src/armv7-m/up_systemreset.c
  up_systemreset();
}

//=======================================================================================
void hcom_exec_utility_request_enable_disable_nsh(uint32_t userData)
{
  // 0 = disable, 1= enable
}

//=======================================================================================
// Enter the dfu mode so the user can flash the internal flash with the OS
void hcom_exec_utility_request_enter_dfu_mode(uint32_t userData)
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
// Experimental Code
void hcom_exec_utility_developer_1(uint32_t userData)
{
  char actionRqst[16];

  // 0 = read, 1 = write, 2 = erase.
  switch(userData)
  {
    case 0: // read
      snprintf(actionRqst, 16, "read from");
      break;

    case 1: // write
      snprintf(actionRqst, 16, "write to");
      break;

    case 2: // erase
      snprintf(actionRqst, 16, "erase");
      break;
    default:
      syslog(0, "Entered %s() - But invalid request %d received. Will quit.\n", __func__, userData); sleep(1);
      return;
  }

  syslog(0, "Entered %s() - Will %s flash\n", __func__, actionRqst); sleep(1);

  if(actionRqst[0] == 'e')
  {
    syslog(0, "%s() - Erase beginning\n", __func__); sleep(1);
    int ret = MTD_ERASE(_master_mtd, 0, 1);    // Erase block offset 0, 1 erase sector = 4096 bytes
    syslog(0, "%s() - Erase completed. Returned %d\n", __func__, ret); sleep(1);
    return;
  }

  // Joao's code (modified) to duplicate behavior
  unsigned char *buff = (FAR unsigned char *)malloc(4096);
  unsigned char *buff2 = (FAR unsigned char *)malloc(4096 * 2);
  //unsigned char *buff3 = (FAR unsigned char *)malloc(4096 * 2);

  // Establish known pattern in buff
  for(int i = 0; i < 4096; i++) {
    buff[i] = i;
  }

  if(actionRqst[0] == 'w')
  {
    // Write pattern to first 4096 of flash
    syslog(0, "%s() - Write pattern to flash\n", __func__); sleep(1);
    MTD_BWRITE(_master_mtd, 0, 1, buff);
  }

  // Read first 4096 bytes from flash
  syslog(0, "%s() - Read flash. Initial buffer all 0x00\n", __func__); sleep(1);

  memset(buff2, 0, 4096 * 2);
  MTD_BREAD(_master_mtd, 0, 1, buff2);    // After
  hcom_diag_print_buffer(buff2, 4096 * 2, LOG_INFO);
      sleep(1);

  syslog(0, "%s() - Comparing first 4096 bytes\n", __func__); sleep(1);

  // Compare
  int ret = memcmp(buff, buff2, 4096);
  if (ret != 0)
  {
    f7syslog(LOG_WARNING, "%s() Data read does not match\n", __func__);
    hcom_diag_print_buffer(buff2, 4096, LOG_INFO);
    sleep(1);
  }
  else
  {
    f7syslog(LOG_WARNING, "%s() Tested Block MATCHED\n", __func__);
  }

  free (buff);
  //syslog(0, "%s() - freed buff\n", __func__); sleep(1);

  free (buff2);
  //syslog(0, "%s() - freed buff2\n", __func__); sleep(1);
  //free (buff3);

// The following is the orginal code in stm32_boot.c
// #if 0
    // // Debugging code to output first 16 bytes of qspi flash
    // uint8_t *buffer = (uint8_t*) malloc (4096);
    // size_t bytes;
    // int i;
//     // read the first block
//     // ssize_t MTD_BREAD(FAR struct mtd_dev_s *dev, off_t startblock,
//     //  size_t nblocks, FAR uint8_t *buffer);
//     bytes = MTD_BREAD(mtd, 0, 1, buffer);
//     printf ("\nQSPI Flash initialized -- dumping first 16 bytes\n");
//     for (i = 0; i < 16; i++)
//     {
//       printf ("%02x ", buffer[i]);
//     }
//     printf("\n\n");

// // Writes following 8 byte pattern to first 16 bytes
//     const uint8_t payload[8] = { 0xf8, 0xc8, 0x10, 0xa8, 0xe7, 0x6f, 0x9e, 0x8c };
//     memcpy(buffer, payload, sizeof(payload));
//     memcpy(buffer+sizeof(payload), payload, sizeof(payload));

//     // write back the block
//     bytes = MTD_BWRITE(mtd, 0, 1, buffer);
//     printf ("wrote block\n");

//     bytes = MTD_BREAD(mtd, 0, 1, buffer);
//     printf ("\nread back -- dumping first 16 bytes\n");
//     for (i = 0; i < 16; i++)
//     {
//       printf ("%02x ", buffer[i]);
//     }
//     printf("\n");

//     // Trial code
//     //struct fat_format_s fmt = FAT_FORMAT_INITIALIZER;
//     //mkfatfs /dev/mtdblock0
//     //mkfatfs("/dev/mtdblock0", &fmt);
// #endif

syslog(0, "%s() - Exit\n", __func__); sleep(1);
}

void hcom_exec_utility_developer_2(uint32_t userData)
{
}

void hcom_exec_utility_developer_3(uint32_t userData)
{
}

void hcom_exec_utility_developer_4(uint32_t userData)
{

}