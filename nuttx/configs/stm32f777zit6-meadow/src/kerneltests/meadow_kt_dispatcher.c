/****************************************************************************
 * meadow_kt_dispatcher.c
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

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <meadow/meadow_unit_test_framework.h>
#include <meadow/meadow_kernel_tests.h>
#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Local defines.
 ****************************************************************************/

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

/****************************************************************************
 * Private variables and associated macros.
 ****************************************************************************/

/****************************************************************************
 * Private types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

 /**
  * @brief Kernel test methods array.
  */
static meadow_test_methods_t _kernelTests[] =
{
#if defined(CONFIG_SD_CARD_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SD_CARD, meadow_kt_sd_card_tests },
#endif

#if defined(CONFIG_POWER_MANAGEMENT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_POWER_MANAGEMENT, meadow_kt_power_management_tests },
#endif

#if defined(CONFIG_ISO8601_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ISO8601, meadow_kt_iso8601_tests },
#endif

#if defined(CONFIG_QUICK_MISC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_MISC, meadow_kt_quick_misc_tests },
#endif

#if defined(CONFIG_ADC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ADC, meadow_kt_adc_tests },
#endif

#if defined(CONFIG_DAC_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_DAC, meadow_kt_dac_tests },
#endif

#if defined(CONFIG_MEADOW_INTERRUPT_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_INTERRUPT, meadow_kt_meadow_interrupt_tests },
#endif

#if defined(CONFIG_SPI_DMA_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_SPI_DMA, meadow_kt_spi_dma_tests },
#endif

#if (defined(CONFIG_ROTARY_ENCODER_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)) && (MEADOW_INCLUDE_CODE_FOR_ROTARY_ENCODER > 0)
    { MEADOW_TEST_ROTARY_ENCODER, meadow_kt_rotary_encoder_tests },
#endif

#if defined(CONFIG_MEASURE_FREQUENCY_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_FREQUENCY_MEASUREMENT, meadow_kt_measure_freq_tests },
#endif

#if defined(CONFIG_MEADOW_OS_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_KERNEL_ASSERT, meadow_kt_assert_test },
#endif

#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_ALL_ESP32, meadow_kt_espcp_tests },
    { MEADOW_TEST_ESP_WEB_PAGE_LOAD_TEST, meadow_kt_espcp_test_get_web_resource },
#endif

#if (defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS))
    { MEADOW_TEST_ETHERNET, meadow_kt_ethernet_tests },
    { MEADOW_TEST_ETHERNET_WEB_PAGE_LOAD_TEST, meadow_kt_ethernet_get_web_resource },
#endif

#if defined(CONFIG_BG77_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)
    { MEADOW_TEST_BG77, meadow_kt_ethernet_tests },
#endif
};

/**
 * @brief Pointer to the network test configuration structure.
 */
network_tests_configuration_t *network_tests_configuration = NULL;

/****************************************************************************
 * Public functions.
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_kt_dispatcher
 *
 * Description:
 *  Locate the requested kernel test and execute the test passing the user
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
int meadow_kt_dispatcher(uint32_t param, uint32_t value)
{
    int result = TEST_ERR_NOT_FOUND;

    syslog(LOGGING_LEVEL, "Checking for kernel test param: %u - value: %lu\n", param, value);
    if (sizeof(_kernelTests) > 0)
    {
        for (int index = 0; index < sizeof(_kernelTests) / sizeof(meadow_test_methods_t); index++)
        {
            if (_kernelTests[index].testId == param)
            {
                network_tests_configuration = process_network_test_configuration_file();
                if (network_tests_configuration == NULL)
                {
                    syslog(LOGGING_LEVEL, "Failed to load network test configuration file.\n");
                    return(TEST_ERR_INVALID_CONFIG);
                }
                _kernelTests[index].testMethod(value);
                result = TEST_ERR_OK;
                break;
            }
        }
    }

    return(result);
}