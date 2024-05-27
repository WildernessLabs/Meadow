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

#include <stdint.h>

//
//  OS persistent information is stored ina single 4,096 byte block of flash
//  storage after the OTA data.  Full information can be found in the partitions.h
//  header file in this directory.
//
struct os_persistent_data_s
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
} __attribute((packed));

typedef struct os_persistent_data_s os_persistent_data_t;

#endif // __INCLUDE_MEADOW_BOOTLOADER_MEADOW_OS_PERSISTENT_DATA_H