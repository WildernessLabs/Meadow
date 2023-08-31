/****************************************************************************
 * ethernet_tests.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <nuttx/mm/mm.h>
#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "network_tests.h"

/****************************************************************************
 * Local defines.
 ****************************************************************************/

#if defined(CONFIG_BG77_TESTS)
#include "secrets.h"
#else
#define WIFI_NETWORK                "Dummy, do not use"
#define WIFI_PASSWORD               "Use contents of secrets.h"
#define SIMPLE_WEB_SERVER_NAME      "pi4-ubuntu-001"
#define SIMPLE_WEB_PAGE             "/"
#define WEB_SERVER_IP_ADDRESS       "127.0.0.1"
#define WEB_SERVER_PORT             80
#endif

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

/****************************************************************************
 * Name: meadow_kt_bg77_tests
 *
 * Description:
 *  Execute any network tests.
 *
 * Input Parameters:
 *   arg - Argument passed to kernel test via CLI
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void meadow_kt_bg77_tests(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "Executing BG77 network tests.\n");

    //
    //  We can start some actual network tests now we are connected to an 
    //  access point.
    //
    // if (arg == 0)
    // {
    //     arg = 1;
    // }
    // network_test_get_multiple_web_pages(arg, WEB_SERVER_IP_ADDRESS, WEB_SERVER_PORT);

    syslog(LOGGING_LEVEL, "BG77 tests completed.\n");
}