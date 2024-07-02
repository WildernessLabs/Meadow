/****************************************************************************
 * \include\meadow\bootloader\meadow_os_partitions.h
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
 *   Author:  Mark Stevens
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

#ifndef __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PARTITIONS_H
#define __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PARTITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

//    NUTTX DEFINITIONS
#define NUTTX_PRI_LOC               0x08040000
#define NUTTX_SEC_QSPI_LOC          0x300000    // sync with HCOM_NX_FS_MONO_RAW_PARTITION_SIZE
#define NUTTX_SIZE                  0x1C0000    //    (2MB - 256KB)
#define NUTTX_PRI_CRC_LOC           (NUTTX_PRI_LOC + NUTTX_SIZE - 4)

//    SDRAM DEFINITIONS
#define SDRAM_LOC                   0xC0000000
#define SDRAM_SIZE                  0x2000000

//    QSPI DEFINITIONS
#define QSPI_FLASH_LOC_INTERNAL     0x0000000
#define QSPI_FLASH_SIZE             0x2000000

/**
 * @brief Size of the OTA data in flash.
 * 
 * Note: This defaults to the size of the erase flash size on the F7v2.
 */
#define OTA_DATA_SIZE               0x1000

/**
 * @brief Location of the OTA data in flash.
 */
#define OTA_DATA_LOC                (NUTTX_SEC_QSPI_LOC + NUTTX_SIZE)

// OS persistent data location etc.
#define OS_PERSISTENT_DATA_SIZE     0x1000
#define OS_PERSISTENT_DATA_LOC      (OTA_DATA_LOC + OTA_DATA_SIZE)

#define IO_BLOCK_SIZE               0x40000

#ifdef __cplusplus
}
#endif


#endif  // __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PARTITIONS_H
