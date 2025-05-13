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

#include <meadow/meadow_unit_test_framework.h>
#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>
#include <meadow/meadow_kt_dispatcher.h>

#include "../diag/hcom_diag_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static meadow_test_names_t _testNames[] =
{
    //
    //  Miscellaneous tests 1 - 999
    //
#if defined(CONFIG_SNPRINTF_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SNPRINTF, "sprintf_chk tests" },
#endif

#if defined(CONFIG_GPIO_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_GPIO, "GPIO tests" },
#endif

#if defined(CONFIG_EXAMPLES_SQLITE_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SQLLITE, "NuttX SQLLite tests" },
#endif

#if defined(CONFIG_MCU_OVERLOAD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_MCU_OVERLOAD, "MCU Overload tests" },
#endif

#if defined(CONFIG_CHAT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS) || (MEADOW_INCLUDE_ETHERNET_CHAT_TESTS_IN_BUILD > 0)
    { MEADOW_TEST_CHAT_CLIENT, "Chat client tests" },
#endif

#if defined(CONFIG_BBR_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_BBR, "Battery backed register tests" },
#endif

#if defined(CONFIG_SD_CARD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SD_CARD, "SD card tests" },
#endif

#if defined(CONFIG_POWER_MANAGEMENT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_POWER_MANAGEMENT, "Power management tests" },
#endif

#if defined(CONFIG_ISO8601_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ISO8601, "ISO8601 tests" },
#endif

#if defined(CONFIG_QUICK_MISC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_MISC, "Quick and Misc tests" },
#endif

#if defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_TENSORFLOW, "Tensorflow tests" },
#endif

#if defined(CONFIG_DIR_MGMT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_DIRECTORY_MANAGEMENT, "Directory mgmt tests" },
#endif

#if defined(CONFIG_ADC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ADC, "Analog to Digital tests" },
#endif

#if defined(CONFIG_DAC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_DAC, "Digital to Analog tests" },
#endif

#if defined(CONFIG_MEADOW_INTERRUPT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_INTERRUPT, "Meadow interrupt tests" },
#endif

#if defined(CONFIG_SPI_DMA_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SPI_DMA, "SPI DMA tests" },
#endif

#if (defined(CONFIG_ROTARY_ENCODER_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)) && (MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0)
    { MEADOW_TEST_ROTARY_ENCODER, "Rotary Encoder tests" },
#endif

#if defined(CONFIG_MEASURE_FREQUENCY_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_FREQUENCY_MEASUREMENT, "Measure Frequency tests" },
#endif

    //
    //  Meadow OS tests (900-999)
    //
#if defined(CONFIG_MEADOW_OS_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_USER_SPACE_ASSERT, "User space assert" },
    { MEADOW_TEST_USER_SPACE_RESET, "User space meadow_os_reset_board" },
    { MEADOW_TEST_KERNEL_ASSERT, "Kernel space assert" },
    { MEADOW_TEST_BBD_REGISTER, "Battery Backed Domain register tests" },
    { MEADOW_TEST_BBD_WRITE_AFTER_RESET, "Battery Backed Domain write and reset test" },
    { MEADOW_TEST_READ_AFTER_RESET, "Battery Backed Domain read after reset test" },
#endif

    //
    //  ESP tests 1000 - 1199
    //
#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ALL_ESP32, "All ESP32 tests" },
    { MEADOW_TEST_ESP_WEB_PAGE_LOAD_TEST, "ESP32 get web resource n times (use -v <count> to specify number of iterations, default = 1)" },
#endif

    //
    //  Ethernet tests 1200 - 1399
    //
#if defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ETHERNET, "All ethernet tests" },
    { MEADOW_TEST_ETHERNET_WEB_PAGE_LOAD_TEST, "Ethernet get web resource n times (use -v <count> to specify number of iterations, default = 1)" },
#endif

    //
    //  BG77 tests 1400 - 1599
    //
#if defined(CONFIG_BG77_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_BG77, "All BG77 tests" },
#endif
};

static meadow_test_methods_t _userspaceTests[] =
{
    //
    //  Miscellaneous tests 1 - 999
    //
#if defined(CONFIG_SNPRINTF_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SNPRINTF, diag_misc_tests_snprintf_on_nuttx },
#endif

#if defined(CONFIG_GPIO_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_GPIO, hcom_meadow_diag_gpio_tests },
#endif

#if defined(CONFIG_EXAMPLES_SQLITE_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SQLLITE, hcom_meadow_sqlite_tests },
#endif

#if defined(CONFIG_MCU_OVERLOAD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_MCU_OVERLOAD, diag_misc_tests_overload_mcu },
#endif

#if (defined(CONFIG_CHAT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)) && (MEADOW_INCLUDE_ETHERNET_CHAT_TESTS_IN_BUILD > 0)
    { MEADOW_TEST_CHAT_CLIENT, diag_ethernet_chat_server },
#endif

#if defined(CONFIG_BBR_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_BBR, hcom_bbr_tests },
#endif

#if defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_TENSORFLOW, tensorflow_tests_hello_world },
#endif

#if defined(CONFIG_DIR_MGMT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_DIRECTORY_MANAGEMENT, meadow_dir_mgmt_tests },
#endif

#if defined(CONFIG_MEADOW_OS_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    //
    //  Meadow OS tests (900-999)
    //
    { MEADOW_TEST_USER_SPACE_ASSERT, meadow_os_userspace_assert_test },
    { MEADOW_TEST_USER_SPACE_RESET, meadow_os_userspace_board_reset_test },
    { MEADOW_TEST_BBD_REGISTER, meadow_bbd_write_read_test },
    { MEADOW_TEST_BBD_WRITE_AFTER_RESET, meadow_bbd_write_and_reset_test },
    { MEADOW_TEST_READ_AFTER_RESET, meadow_bbd_read_after_reset_test },
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_developer_tests_userspace_dispatcher
 *
 * Description:
 *  Locate the requested userspace test and execute the test passing the user
 *  data to the test method.
 *
 *  If the test is not compiled into the system then then nothing will be
 *  executed.
 *
 * Input Parameters:
 *  param - The param passed using the -p parameter.  This is used to
 *          determine which test should be executed.
 *  value - User data specified using the -v parameter.  This will be
 *          used by the test method.
 *
 * Returned Value:
 *  TEST_ERR_OK: Test found and executed.
 *  TEST_ERR_NOT_FOUND: Test not found.
 *  TEST_ERR_INVALID_CONFIG: Invalid configuration file.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int hcom_developer_tests_userspace_dispatcher(uint16_t param, uint32_t value)
{
    int result = TEST_ERR_NOT_FOUND;

    syslog(LOGGING_LEVEL, "Checking for userspace test param: %u - value: %lu\n", param, value);
    if (sizeof(_userspaceTests) > 0)
    {
        for (int index = 0; index < sizeof(_userspaceTests) / sizeof(meadow_test_methods_t); index++)
        {
            if (_userspaceTests[index].testId == param)
            {
                _userspaceTests[index].testMethod(value);
                result = TEST_ERR_OK;
                break;
            }
        }
    }

    return(result);
}

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
 *  If param is 0 then the test ID and description will be sent to CLI.
 *
 * Input Parameters:
 *  param - The param passed using the -p parameter.  This is used to
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
void hcom_developer_tests_developer(uint16_t param, uint32_t value)
{
    char *hostMsg = malloc(HCOM_LARGE_HOST_STRING_BUFF_LENGTH);

    if (hostMsg == NULL)
    {
        syslog(2, "Developer tests: failed to allocate memory for hostMsg\n");
        return;
    }

    syslog(2, "Developer test param: %u - value: %lu\n", param, value);

    if (param == 0)
    {
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "Available tests:\n", __FILE__, __LINE__);
        if (sizeof(_testNames) > 0)
        {
            for (int index = 0; index < sizeof(_testNames) / sizeof(meadow_test_names_t); index++)
            {
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "    %d - %s\n", _testNames[index].testId, _testNames[index].description);
                hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);
            }
        }
        else
        {
            hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, "    No tests built into the system.\n", __FILE__, __LINE__);
        }
    }
    else
    {
        for (int index = 0; index < sizeof(_testNames) / sizeof(meadow_test_names_t); index++)
        {
            if (_testNames[index].testId == param)
            {
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Executing %s\n", _testNames[index].description);
                hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);
                break;
            }
        }
        int result = hcom_developer_tests_userspace_dispatcher(param, value);

        #if defined(CONFIG_KERNEL_TESTS_SYSCALL)
        if (result == TEST_ERR_NOT_FOUND)
        {
            result = meadow_kt_dispatcher((uint32_t) param, value);
        }
        #endif

        switch (result)
        {
            case TEST_ERR_OK:
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Test %u completed successfully.\n", param);
                break;

            case TEST_ERR_NOT_FOUND:
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Test %u cannot be found.  Check that the test has been compiled into the system.\n", param);
                break;

            case TEST_ERR_INVALID_CONFIG:
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Invalid configuration file.\n");
                break;

            default:
                snprintf_chk(hostMsg, HCOM_LARGE_HOST_STRING_BUFF_LENGTH, "Unknown error.\n");
                break;
        }
        hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg, __FILE__, __LINE__);
    }

    free(hostMsg);
}
