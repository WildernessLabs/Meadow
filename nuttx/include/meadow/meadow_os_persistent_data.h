/****************************************************************************
 * \include\meadow\bootloader\meadow_os_persistent_data.h
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

#ifndef __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PERSISTENT_DATA_H
#define __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PERSISTENT_DATA_H

/****************************************************************************
 * 
 * The persistent data storage is split into two sections.  The first section
 * is considered safe safe data.  The second half of the sector is considered
 * unsafe data.
 * 
 * Safe dat is data that will not prevent the system from starting.  This is
 * data such as the reboot count, power cycle count etc.  So these are simple
 * numbers that provide information.
 * 
 * Unsafe data is data that could prevent the system from starting.  This
 * could be configuration data that may be used at startup.
 * 
 * The split between the two types of data has been made so that safe data
 * will persist even if the flash is erased.  Unsafe data will be reset when
 * the flash is erased.
 * 
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 * @brief Current persistent data structure version.
 */
#define OS_PERSISTENT_DATA_VERSION      1

/**
 * @brief Size of the persistent data in flash.  This defaults to the flash erase
 *        page length of 4096 bytes.
 */
#define OS_PERSISTENT_DATA_SIZE         4096

/**
 * @brief Amount of storage reserved for data that is considered safe.
 */
#define OS_PERSISTENT_DATA_SAFE_SIZE    OS_PERSISTENT_DATA_SIZE

/****************************************************************************
 * Public type definitions.
 ****************************************************************************/

/**
 * @brief Data structure holding the data that should persist across reboots.
 */
struct meadow_os_persistent_data_s
{
    //
    //  Used to determine the version of the data in this structure.
    //  This will allow for future changes.
    //
    uint32_t version;

    //
    //  How many times has the board been power cycled?
    //
    uint32_t power_cycle_count;
    
    //
    //  How many times has the board been reset?
    //
    //  This is different to the number of power cycles.  All power cycles
    //  will increment the power cycle count and the reset counts.  Resets
    //  will not necessarily increment the power cycle count.
    //
    uint32_t reset_count;

    //
    //  Pad out the rest of the safe data area with unused data.
    //
    uint8_t unused_safe_data[OS_PERSISTENT_DATA_SAFE_SIZE - (3 * sizeof(uint32_t))];
} __attribute((packed));
typedef struct meadow_os_persistent_data_s meadow_os_persistent_data_t;

#endif // __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PERSISTENT_DATA_H