/****************************************************************************
 * \apps\examples\hcom\tests\developer_tests.c
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

#include <nuttx/config.h>

#include <stdint.h>

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private types
 ****************************************************************************/

/*
 *  Prototype for the test methods.
 */
typedef void (*test_method_t)(uint32_t);

/*
 *  Structure (and associated type definition) for the test methods and their IDs.
 */
struct meadow_test_s
{
    /*
    *  ID of a test.
    */
    uint16_t testId;

    /*
    *  Description of the test.
    */
    char *description;

    /*
    *  Method to be executed.
    */
    test_method_t testMethod;
};
typedef struct meadow_test_s meadow_test_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static meadow_test_t _tests[] = 
{
  //
  //  Miscellaneous tests 0 - 999
  //
//#if HCOM_INCLUDE_SNPRINTF_ON_NUTTX_TESTS_IN_BUILD > 0
#if defined(CONFIG_SNPRINTF_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "1 - snprintf tests are enabled."
    { 1, "sprintf_chk tests", diag_misc_tests_snprintf_on_nuttx },
#endif

// #if HCOM_INCLUDE_GPIO_DIAG_TESTS_IN_BUILD > 0
#if defined(CONFIG_GPIO_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "2 - GPIO tests are enabled."
    { 2, "GPIO tests", hcom_meadow_diag_gpio_tests },
#endif

// #if defined(CONFIG_EXAMPLES_SQLITE_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
#if defined(CONFIG_EXAMPLES_SQLITE_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "3 - SQLLite tests are enabled."
    { 3, "NuttX SQLLite tests", hcom_meadow_sqlite_tests },
#endif

// #if HCOM_INCLUDE_OVERLOAD_MCU_TESTS_IN_BUILD > 0
#if defined(CONFIG_MCU_OVERLOAD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "4 - MCU Overload tests are enabled."
    { 4, "MCU Overload tests", diag_misc_tests_overload_mcu },
#endif

// #if MEADOW_ETHERNET_INCLUDE_CHAT_TEST_IN_BUILD > 0
#if defined(CONFIG_CHAT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "5 - Chat tests are enabled."
    { 5, "Char client tests", diag_ethernet_chat_server },
#endif

// #if HCOM_INCLUDE_BATTERY_BACKED_REG_TEST > 0
#if defined(CONFIG_BBR_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    // if(userData == 0)
    //   hcom_bbr_tests();
#endif

  //
  //  ESP tests 1000 - 1200
  //
#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    #warning "1000 - ESP32 Coprocessor tests are enabled."
    { 1000, "All ESP32 tests", meadow_kt_espcp_tests },
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_developer_tests_developer
 *
 * Description:
 *  Locate the requested test and execute the test passing the user data to
 *  the test method.
 * 
 *  If the test is not compiled into the system then then nothing will be
 *  executed and the user will be informed that the test(s) is/are not
 *  available.
 * 
 *  If level is 0 then the test ID and description will be sent to CLI.
 *
 * Input Parameters:
 *  level - The level passed using the -d parameter.  This is used to
 *          determine which test should be executed.
 *  value - User data specified using the -v parameter.  This will be
 *          used by the test method.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void hcom_developer_tests_developer(uint16_t level, uint32_t value)
{
    bool found = false;
    char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);

    if ((level == 0) && (hostMsg != NULL))
    {
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "Available tests:\n", __FILE__, __LINE__);
    }

    if (sizeof(_tests) > 0)
    {
        for (int index = 0; index < sizeof(_tests) / sizeof(meadow_test_t); index++)
        {
            if (level == 0)
            {
                if (hostMsg != NULL)
                {
                    snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "    %d - %s\n", _tests[index].testId, _tests[index].description);
                    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);
                }
                found = true;
            }
            else
            {
                if (_tests[index].testId == level)
                {
                    _tests[index].testMethod(value);
                    found = true;
                    break;
                }
            }
        }
    }
    else
    {
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "    No tests built into the system.\n", __FILE__, __LINE__);
    }

    if ((!found) && (level != 0))
    {
        if (hostMsg != NULL)
        {
            snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Test %u cannot be found.  Check that the test has been compiled into the system.\n", level);
            hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);
        }
    }

    free(hostMsg);
}