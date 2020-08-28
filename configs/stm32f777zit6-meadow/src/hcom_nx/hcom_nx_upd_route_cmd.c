/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_upd_route_cmd.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

// This is where the CLI commands that need to be executed on the nuttx side
// are handled

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_protocol.h>

#include <assert.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function routes the hcom commands to the approrate code to execute it
int hcom_nx_route_cli_command(struct hcom_nx_cmd_data *cmdData)
{
  int ret;

  switch(cmdData->hcomCmd)
  {
    case HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU:
      up_systemreset();
      return OK;

    case HCOM_MDOW_REQUEST_MONO_FLASH:
      ret = hcom_nx_exec_ex_flash_mono_flash(cmdData);
      return ret;
  
    case HCOM_MDOW_REQUEST_BULK_FLASH_ERASE:
      ret = hcom_nx_exec_ex_flash_erase_ex_flash(cmdData);
      return ret;

    case HCOM_MDOW_REQUEST_VERIFY_ERASED_FLASH:
      ret = hcom_nx_exec_ex_flash_verify_ex_flash(cmdData);
      return ret;

    case HCOM_MDOW_REQUEST_PART_RENEW_FILE_SYS:
      ret = hcom_nx_exec_ex_flash_renew_file_system(cmdData);
      return ret;

    default:
      cmdData->logLevel = LOG_ERR;
      cmdData->logLen = snprintf(cmdData->logMsg, HCOM_NX_CMD_LOG_MSG_SIZE,
              "%s@%d-Undefined command:0x%08x.\n", thisFile, __LINE__, cmdData->hcomCmd);
  }
  return -1;
}


