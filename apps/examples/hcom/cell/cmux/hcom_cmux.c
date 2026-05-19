/****************************************************************************
 * \apps\examples\hcom\cell\hcom_cmux.c
 *
 *   Copyright (C) 2025 Wilderness Labs. All rights reserved.
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

#include <nuttx/config.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pty.h>

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>
#include "../../hcom_common.h"

#include "hcom_cmux.h"
#include "netutils/cmux.h"

#define HCOM_CMUX_NUMBER_OF_PORTS   (4)
#define HCOM_CMUX_TASK_STACKSIZE    (3078)
#define HCOM_CMUX_TASK_PRIORITY     (150)
#define HCOM_CMUX_TTY_DEVNODE ("/dev/ttyS1")
static char *thisFile = __FILE__;

static char  g_cmux_script[] = 
  "ECHO ON "
  "TIMEOUT 30 "
  "\"\" ATE0 "
  "OK AT+IFC=2,2 "
  "OK AT+IPR=115200 "
  "OK AT+CMUX=0,0,5,127,10,3,30,10,2 "
  "OK \\c";

int hcom_cmux_start(void)
{
    int ret = -ENODATA;
    pthread_t cmux_thread_id;
    struct cmux_settings_s hcom_cmux;
    meadow_configuration_t *config = meadow_os_deep_copy_config();

    if ((config != NULL) && (config->default_interface != NULL))
    {
        if (config->default_interface->interface_type != MEADOW_IFT_CELL)
        {
            hcom_cmux.total_channels = (int) HCOM_CMUX_NUMBER_OF_PORTS;
            hcom_cmux.script = g_cmux_script;
            hcom_cmux.tty_name = HCOM_CMUX_TTY_DEVNODE;

            ret = cmux_create(&hcom_cmux);
            hcom_logging_syslog(LOG_INFO, "%s-%d-Cell CMUX created: %d\n", thisFile, __LINE__, ret);
        }
    }
    meadow_os_config_free_resources(config);

    return ret;
}