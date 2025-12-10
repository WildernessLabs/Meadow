/****************************************************************************
 * \apps\examples\hcom\tests\meadow_os_userspace_tests.c
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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

#if defined (CONFIG_MEADOW_OS_TESTS) || defined (CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>

#include "../hcom_common.h"
#include <meadow/meadow_os.h>
#include <meadow/meadow_kernel_tests.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_userspace_assert_test
 *
 * Description:
 *  Execute the userspace assert test in the OS.
 *
 * Input Parameters:
 *  userdata - Value passed to the test from the meadow command line.
 *             See: -v / --value parameter in meadow command line.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_userspace_assert_test(uint32_t userdata)
{
    *((uint32_t *) NULL) = 0;
}

/****************************************************************************
 * Name: meadow_os_userspace_board_reset_test
 *
 * Description:
 *  Check that we can reset the board from user space.
 *
 * Input Parameters:
 *  userdata - Value passed to the test from the meadow command line.
 *             See: -v / --value parameter in meadow command line.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_userspace_board_reset_test(uint32_t userdata)
{
    meadow_os_reset_board(0);
}

/****************************************************************************
 * Name: meadow_os_userspace_send_parameter
 *
 * Description:
 *  Send a parameter to the host for debugging purposes.
 *
 * Input Parameters:
 *  buffer - Buffer to hold the formatted string.
 *  buffer_length - Length of the buffer.
 *  parameter_name - Name of the parameter to send.
 *  parameter_value - Value of the parameter to send.
 *
 * Returned Value
 *   None
 *
 * Assumptions/Limitations:
 *   - The buffer should be large enough to hold the formatted string.
 *   - If the parameter_value is NULL, it will be replaced with "NULL".
 ****************************************************************************/
void meadow_os_userspace_send_parameter(char * const buffer, uint32_t buffer_length, const char * const parameter_name, const char * const parameter_value)
{
    if ((buffer != NULL) && (buffer_length > 0) && (parameter_name != NULL))
    {
        snprintf_chk(buffer, buffer_length, "%s = %s", parameter_name, (parameter_value == NULL) ? "NULL" : parameter_value);
        syslog(LOG_MTEST, "%s", buffer);
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, buffer, __FILE__, __LINE__);
    }
    else
    {
        syslog(LOG_MTEST,
            "Developer tests: Invalid parameters passed to meadow_os_userspace_send_parameter.\n");
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "Invalid parameters passed to meadow_os_userspace_send_parameter.\n", __FILE__, __LINE__);
    }
}

/****************************************************************************
 * Name: meadow_os_userspace_get_test_configuration_test
 *
 * Description:
 *  Get the test configuration from the kernel.
 *
 * Input Parameters:
 *  userdata - Value passed to the test from the meadow command line.
 *             See: -v / --value parameter in meadow command line.
 *
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_userspace_get_test_configuration_test(uint32_t userdata)
{
    char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);

    if (hostMsg == NULL)
    {
        syslog(LOG_MTEST, "Developer tests: failed to allocate memory for hostMsg\n");
        return;
    }

    unit_tests_configuration_t *config = meadow_os_get_unit_tests_config();
    if (config != NULL)
    {
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter1", config->parameter1);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter2", config->parameter2);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter3", config->parameter3);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter4", config->parameter4);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter5", config->parameter5);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter6", config->parameter6);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter7", config->parameter7);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter8", config->parameter8);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter9", config->parameter9);
        meadow_os_userspace_send_parameter(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Parameter10", config->parameter10);
    }
    else
    {
        syslog(LOG_MTEST,
            "Developer tests: failed to get unit tests configuration.\n");
        
            hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
                "Failed to get unit tests configuration.\n", __FILE__, __LINE__);
    }
}

/****************************************************************************
 * Name: meadow_os_cli_timeout_test
 *
 * Description:
 *  Provide a way to test the CLI timeout parameter.
 *
 * Input Parameters:
 *  userdata - Value passed to the test from the meadow command line.
 *             See: -v / --value parameter in meadow command line.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void meadow_os_cli_timeout_test(uint32_t userdata)
{
    char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);

    if (hostMsg == NULL)
    {
        syslog(LOG_MTEST, "Developer tests: failed to allocate memory for hostMsg\n");
        return;
    }

    if (userdata == 0)
    {
        userdata = 60;
    }

    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "CLI timeout test, sleeping for %lu seconds\n", userdata);
    syslog(LOG_MTEST, "%s", hostMsg);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);

    sleep(userdata);

    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Back from sleep.\n");
    syslog(LOG_MTEST, "%s", hostMsg);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);

    free(hostMsg);
}

#endif /* CONFIG_MEADOW_OS_TESTS */
