/****************************************************************************
 * \include\meadow\hcom_dnld_shared.h
 * 
 *   Copyright (C) 2022-2023 Wilderness Labs. All rights reserved.
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
#ifndef __INCLUDE_HCOM_DOWNLOAD_SHARED__H
#define __INCLUDE_HCOM_DOWNLOAD_SHARED__H

#include <nuttx/config.h>
#include <stdint.h>

//--------------------------------------------------------------------

#define HCOM_FILE_DNLD_STM32F7_WDOG_TIME (3)

// 8 elements in path will allow up to 6 subdirectories
// '/meadow0/filename' considered 2 mandatory elements
#define HCOM_FILE_DNLD_MAX_NUMB_DIR_ELEMENTS (8)
#define HCOM_FILE_DNLD_MANDATORY_DIR_ELEMENTS (2)
#define HCOM_FILE_DNLD_MAX_NUMB_USER_SUBDIRS \
  (HCOM_FILE_DNLD_MAX_NUMB_DIR_ELEMENTS-HCOM_FILE_DNLD_MANDATORY_DIR_ELEMENTS)

// This enum defines the current processing state of the download code for a
// specific download session. It is also used for file delete.
// It is not used for ESP32 download, only external file system.
// May decide to add ESP32 enum here too
enum hcom_download_stm32f7_packet_state
{
  // The Invalid state indicates that there is no valid information in the
  // hcom_dnld_shared_s structure.
  HcomStm32F7DnldStateInvalid  = 0,
  HcomStm32F7DnldStateNone     = 1,
  HcomStm32F7DnldStateStarting = 2,
  HcomStm32F7DnldStateFileXfer = 3,
};

enum hcom_download_dir_type_identifier
{
  HcomDnldDirTypeUnknown = 0,
  HcomDnldDirTypeMeadow0 = 1,
  HcomDnldDirTypeMmcsd0  = 2,
};

// This enum is used to catorgize CLI file/directory requests
enum hcom_file_msg_cat_e
{
  pathnameNotUsed         = 0xff,    // Flag to indicate Not Used
  pathnameInvalid         = 0,    // Illegal format provided
  pathnameInvalidNoSlash  = 1,    // No '/' found but needed
  pathnameInvalidSlash    = 2,    // '/' found but not wanted
  pathnameOriginal        = 3,    // No '/' found
  pathnameFullMeadow      = 4,    // Starts '/meadow0/'
  pathnameFullSdcard      = 5,    // Starts '/sdcard/'
  pathnameSingleSlash     = 6,    // Just '/'
  pathnameSlashSlash      = 7     // '/text/'
};

// This struct is memset to zero by processing during initialization
struct hcom_dnld_shared_s
{
  // Set by process and maintained during download by file handling
  int dnldCurrentState;               // Tracks the state of the download

  // These are completely managed by file handling code
  uint32_t dnldInitFileCrc;             // CRC that was received from CLI
  uint32_t dnldCalcFileCrc;             // CRC calculated over while receiving
  uint32_t dnldInitFileSize;            // File size based on received CLI data
  uint32_t dnldCalcFileSize;            // This size calculated while receiving
  uint32_t dnldPathNameEleCount;        // Number of elements in pathname
  enum hcom_file_msg_cat_e dnldRqstCat; // Category of file rqst did CLI make?
  int dnldFileFD;                       // For persisting fd
  int dnldPercentSent;                  // Used to calculate the % completed
  // Set by processing and used by file handling (This is always 1)
  uint32_t dnldFilePartId;              // File partition from CLI
  // These are allocated and may be exactly the same string
  char *dnldOrigPathName;               // File name as provided by CLI
  char *dnldFullPathName;               // Full file name (e.g. /meadow0/file.txt)
};
typedef struct hcom_dnld_shared_s hcom_dnld_shared_t;

int hcom_dir_mgmt_free_file_info(hcom_dnld_shared_t *dnldShared);

// The watchdog has a close relationship with hcom CLI message process
int hcom_host_watchdog_initialize(hcom_dnld_shared_t *dnldShared);
void hcom_host_watchdog_stopping(void);
int hcom_host_watchdog_check_execute_if_expired(void);

#endif  // __INCLUDE_HCOM_DOWNLOAD_SHARED__H
