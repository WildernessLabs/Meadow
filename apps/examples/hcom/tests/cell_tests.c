/****************************************************************************
 * /apps/examples/hcom/tests/cell_tests.c
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

#if defined(CONFIG_CELL_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <meadow/hcom_shared_common.h>

#include "netutils/chat.h"

/****************************************************************************
 * Local defines.
 ****************************************************************************/

#define TTY_NAME "/dev/ttyS1" // COM4
#define CHAT_TIMEOUT 30

const char g_cell_signal_quality[] = { "TIMEOUT 30 \"\" AT+CSQ PAUSE 3 OK \\c"};

const char g_cell_scan_operator[] = { "TIMEOUT 30 \"\" AT+COPS=? PAUSE 3 OK \\c"};

const char g_cell_gps[] =
{
        "TIMEOUT 30 \"\" "
        "AT+QGPS=1,2,180,1 PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",1,64,1,0,0,1 PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",3,64,1,1 PAUSE 3 OK "
        "AT+QGPSCFG=\\\"nmeasrc\\\",1 PAUSE 120 OK "
        "AT+QGPSGNMEA=\\\"GSV\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"GGA\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"RMC\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"GSA\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"VTG\\\" PAUSE 3 OK "
        "AT+QGPSEND PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",1,64,1,0,0,1 PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",3,64,0,1 PAUSE 3 OK "
        "\\c"
};

char cell_cmd_output[128] = {0};
static struct chat_ctl cell_chat;

typedef enum
{
    CELL_SIGNAL_QUALITY = 0,
    CELL_SCAN,
    CELL_GPS,
} cell_script_e;

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL 1

/****************************************************************************
 * Name: cell_get_script
 *
 * Description:
 *  Select the cell script.
 *
 * Input Parameters:
 *   userData - Argument passed to test via CLI
 *
 * Returned Value:
 *   Script according to the userData value.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static char *cell_get_script(uint32_t userData)
{
    switch (userData)
    {
    case CELL_SIGNAL_QUALITY:
        return g_cell_signal_quality;
    case CELL_SCAN:
        return g_cell_scan_operator;
    case CELL_GPS:
        return g_cell_gps;
    }
    return NULL;
}

/****************************************************************************
 * Name: cell_script_tests
 *
 * Description:
 *  Execute the script.
 *
 * Input Parameters:
 *   userData - Argument passed to test via CLI
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void cell_script_tests(uint32_t userData)
{
    syslog(LOGGING_LEVEL, "\n");

    syslog(LOGGING_LEVEL, "Executing Cell network tests.\n");

    char *cell_script = cell_get_script(userData);
    if (cell_script == NULL)
    {
        syslog(LOGGING_LEVEL, "Failed to execute the test.\n");
        return;
    }

    cell_chat.echo = true;
    cell_chat.verbose = true;
    cell_chat.timeout = CHAT_TIMEOUT;

    cell_chat.fd = open(TTY_NAME, O_RDWR);
    if (cell_chat.fd < 0)
    {
        syslog(LOGGING_LEVEL, "Failed to open %s.\n", TTY_NAME);
        return;
    }

    int flags = 0;
    flags = fcntl(cell_chat.fd, F_GETFL, 0);
    if (flags < 0)
    {
        syslog(LOGGING_LEVEL, "Failed to get file status, ret = %d.\n", flags);
        return;
    }

    flags = fcntl(cell_chat.fd, F_SETFL, flags | O_NONBLOCK);
    if (flags < 0)
    {
        syslog(LOGGING_LEVEL, "Failed to set file, ret = %d.\n", flags);
        return;
    }

    int ret = chat(&cell_chat, cell_script, &cell_cmd_output);
    syslog(LOGGING_LEVEL, "Cell Output: %s \n", cell_cmd_output);
    close(cell_chat.fd);

    syslog(LOGGING_LEVEL, "Cell tests completed.\n");
}
#endif