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
#include "../diag/hcom_diag_gpio.h"

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
  //  Miscellaneous tests 1 - 999
  //
#if defined(CONFIG_SNPRINTF_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 1, "sprintf_chk tests", diag_misc_tests_snprintf_on_nuttx },
#endif

#if defined(CONFIG_GPIO_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 2, "GPIO tests", hcom_meadow_diag_gpio_tests },
#endif

#if defined(CONFIG_EXAMPLES_SQLITE_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 3, "NuttX SQLLite tests", hcom_meadow_sqlite_tests },
#endif

#if defined(CONFIG_MCU_OVERLOAD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 4, "MCU Overload tests", diag_misc_tests_overload_mcu },
#endif

#if defined(CONFIG_CHAT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS) || MEADOW_INCLUDE_ETHERNET_CHAT_TESTS_IN_BUILD > 0
    { 5, "Chat client tests", diag_ethernet_chat_server },
#endif

#if defined(CONFIG_BBR_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 6, "Battery backed register tests", hcom_bbr_tests },
#endif

#if defined(CONFIG_SD_CARD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 7, "SD card tests", meadow_kt_sd_card_tests },
#endif

#if defined(CONFIG_POWER_MANAGEMENT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 8, "Power management tests", meadow_kt_power_management_tests },
#endif

#if defined(CONFIG_ISO8601_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 9, "ISO8601 tests", meadow_kt_iso8601_tests },
#endif

#if defined(CONFIG_QUICK_MISC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 10, "Quick and Misc tests", meadow_kt_quick_misc_tests },
#endif

#if defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 11, "Tensorflow tests", tensorflow_tests_hello_world },
#endif

#if defined(CONFIG_DIR_MGMT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 13, "Directory mgmt tests", meadow_dir_mgmt_tests },
#endif

#if defined(CONFIG_ADC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 14, "Analog to Digital tests", meadow_kt_adc_tests },
#endif

#if defined(CONFIG_DAC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 15, "Digital to Analog tests", meadow_kt_dac_tests },
#endif

  //
  //  ESP tests 1000 - 1200
  //
#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 1000, "All ESP32 tests", meadow_kt_espcp_tests },
    { 1001, "ESP32 Load Test using simple web page", meadow_kt_espcp_load_test_web_page },
    { 1002, "ESP32 Load Test using a binary file", meadow_kt_espcp_load_test_large_file_download },
#endif

  //
  //  Ethernet tests 1200 - 1399
  //
#if defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 1200, "All ethernet tests", meadow_kt_ethernet_tests },
    { 1201, "Ethernet Load Test using a simple web page", meadow_kt_ethernet_load_test_web_page },
    { 1202, "Ethernet Load Test using a binary file", meadow_kt_ethernet_load_test_large_file_download },
#endif

  //
  //  BG77 tests 1400 - 1599
  //
#if defined(CONFIG_BG77_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { 1400, "All BG77 tests", meadow_kt_ethernet_tests },
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
    
    syslog(2, "developer test Level:%u - userData:%lu\n", level, value);

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