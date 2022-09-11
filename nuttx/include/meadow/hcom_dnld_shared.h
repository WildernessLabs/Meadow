/****************************************************************************
 * \include\meadow\hcom_dnld_shared.h
 * 
 *   Copyright (C) 2022 Wilderness Labs. All rights reserved.
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
// #include <nuttx/compiler.h>
#include <stdint.h>

enum hcom_download_stm32f7_packet_state
{
  HcomStm32F7DnldStateNone = 0,
  HcomStm32F7DnldStateStarting = 1,
  HcomStm32F7DnldStateFileXfer = 2,
};

// May add ESP32 enum here too
// And below - may add ESP32 info to struct

struct hcom_dnld_shared_s
{
    int currentF7DnldState;
    char *simpleFileName;
    char *fullFileName;
    uint32_t xferRecvFullFileCrc;
    uint32_t xferRecvFullFileSize;    // File size based on received data
    uint32_t xferCalcFullFileSize;    // This is the size of the original
    uint32_t xferMeadowCalcCrc;       // This is over all the payload (original data)
    uint32_t partitionId;
    int lastPercentSent;
    bool stateErrShown;
};

typedef struct hcom_dnld_shared_s hcom_dnld_shared_t;

int hcom_dnld_shared_init();
int hcom_dnld_shared_free();


#endif  // __INCLUDE_HCOM_DOWNLOAD_SHARED__H
