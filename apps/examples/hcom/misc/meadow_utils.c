/*************************************************************************
 * \include\meadow\hcom_shared_common.h
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#include <sys/ioctl.h>

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/meadow_hw_version.h>

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

/****************************************************************************
 * Name: meadow_copy_mono_runtime_to_ram
 *
 * Description:
 *  Ask NuttX to copy the Mono runtime from the reserved flash area into RAM.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, -1 on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int meadow_copy_mono_runtime_to_ram(void)
{
    int result = OK;

    int fd = open(HCOM_NX_UPD_DRIVER_NAME, O_RDONLY);
    if (fd < 0)
    {
        result = -1;
    }
    else
    {
        result = ioctl(fd, HCOM_NX_UPD_COPY_RUNTIME_TO_RAM, (unsigned long) NULL);
        close(fd);
    }

    if (result < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s:%s()@%d Failed to copy Mono runtime into RAM.\n",
              thisFile, __func__, __LINE__);
    }
    
    return(result);
}
