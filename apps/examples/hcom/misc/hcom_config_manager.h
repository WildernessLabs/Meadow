/****************************************************************************
 * hcom_config_manager.h
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

//  The methods and data structures in this file provide access to the
//  configuration of the meadow board.

#include <stdlib.h>
#include <string.h>
#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <nuttx/semaphore.h>
#include <meadow/hcom_shared_common.h>


// This struct holds the version number obtained from the nuttx side
// Based on 256.256.256.256 the max length is 15 chars + null
#define HCOM_VERSION_NUMBER_MAX_LENGTH (16)
struct hcom_config_version_information_s
{
  bool esp32_version_available;
  char esp32_version[HCOM_VERSION_NUMBER_MAX_LENGTH];
  bool meadow_version_available;
  char meadow_version[HCOM_VERSION_NUMBER_MAX_LENGTH];
  bool hardware_version_available;
  char hardware_version[HCOM_VERSION_NUMBER_MAX_LENGTH];
  bool mono_version_available;
  char mono_version[HCOM_VERSION_NUMBER_MAX_LENGTH];
};
typedef struct hcom_config_version_information_s hcom_config_version_information_t;

meadow_configuration_t *hcom_config_get_pointer(void);
int hcom_get_software_version_info(hcom_config_version_information_t *version_info);
void hcom_config_free_resources(meadow_configuration_t *);