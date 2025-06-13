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

#if defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>

#include "network_tests.h"
#include "../hcom_nx/hcom_nx_config_manager.h"
#include <meadow/meadow_unit_test_framework.h>

/****************************************************************************
 * Local defines.
 ****************************************************************************/

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

/****************************************************************************
 * Name: meadow_kt_ethernet_load_test_web_page
 *
 * Description:
 *  Load test downloading a simple web page.
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
void meadow_kt_ethernet_get_web_resource(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "Testing the download of multiple web pages\n");

    if (unit_tests_configuration == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: No unit tests configuration.\n");
        return;
    }

    uint16_t port = string_to_server_port(UNIT_TESTS_CONFIG_SERVER_PORT);
    network_test_get_web_resource(arg, UNIT_TESTS_CONFIG_SERVER_IP, port, UNIT_TESTS_CONFIG_RESOURCE);

    syslog(LOGGING_LEVEL, "Download of multiple web pages test completed.\n");
}
/****************************************************************************
 * Name: meadow_kt_ethernet_tests
 *
 * Description:
 *  Execute any ethernet tests.
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
void meadow_kt_ethernet_tests(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "Executing ethernet network tests.\n");

    if (unit_tests_configuration == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: No unit tests configuration.\n");
        return;
    }

    uint16_t port = string_to_server_port(UNIT_TESTS_CONFIG_SERVER_PORT);
    network_test_get_web_resource(arg, UNIT_TESTS_CONFIG_SERVER_IP, port, UNIT_TESTS_CONFIG_RESOURCE);

    syslog(LOGGING_LEVEL, "Ethernet tests completed.\n");
}

#endif /* defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS) */
