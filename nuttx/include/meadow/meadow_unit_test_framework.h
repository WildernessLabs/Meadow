/****************************************************************************
 * nuttx/include/meadow/meadow_unit_test_framework.h
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
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef MEADOW_UNIT_TEST_FRAMEWORK_H
#define MEADOW_UNIT_TEST_FRAMEWORK_H

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

//---------------------------------------------------------------------------
//      Test IDs.
//---------------------------------------------------------------------------

/**
 * @brief snprintf test ID.
 */
#define MEADOW_TEST_SNPRINTF                        1

/**
 * @brief GPIO test ID.
 */
#define MEADOW_TEST_GPIO                            2

/**
 * @brief SQLLite test ID.
 */
#define MEADOW_TEST_SQLLITE                         3

/**
 * @brief MCU overload test ID.
 */
#define MEADOW_TEST_MCU_OVERLOAD                    4

/**
 * @brief Chat client test ID.
 */
#define MEADOW_TEST_CHAT_CLIENT                     5

/**
 * @brief Battery backed register test ID.
 */
#define MEADOW_TEST_BBR                             6

/**
 * @brief SD card test ID.
 */
#define MEADOW_TEST_SD_CARD                         7

/**
 * @brief Power management test ID.
 */
#define MEADOW_TEST_POWER_MANAGEMENT                8

/**
 * @brief ISO8601 test ID.
 */
#define MEADOW_TEST_ISO8601                         9

/**
 * @brief Miscellaneous (quick) test ID.
 */
#define MEADOW_TEST_MISC                            10

/**
 * @brief TensorFlow test ID.
 */
#define MEADOW_TEST_TENSORFLOW                      11

/**
 * @brief Directory management test ID.
 */
#define MEADOW_TEST_DIRECTORY_MANAGEMENT            13

/**
 * @brief ADC test ID.
 */
#define MEADOW_TEST_ADC                             14

/**
 * @brief DAC test ID.
 */
#define MEADOW_TEST_DAC                             15

/**
 * @brief Interrupt test ID.
 */
#define MEADOW_TEST_INTERRUPT                       16

/**
 * @brief SPI DMA test ID.
 */
#define MEADOW_TEST_SPI_DMA                         17

/**
 * @brief Rotary encoder test ID.
 */
#define MEADOW_TEST_ROTARY_ENCODER                  18

/**
 * @brief Frequency measurement test ID.
 */
#define MEADOW_TEST_FREQUENCY_MEASUREMENT           19

/**
 * @brief Meadow OS test ID.
 */
#define MEADOW_TEST_MEADOW_OS                      900

/**
 * @brief User space assert test ID.
 */
#define MEADOW_TEST_USER_SPACE_ASSERT               900

/**
 * @brief User space reset test ID.
 */
#define MEADOW_TEST_USER_SPACE_RESET                901

/**
 * @brief Kernel space assert test ID.
 */
#define MEADOW_TEST_KERNEL_ASSERT                   902

/**
 * @brief Battery backed domain test ID.
 */
#define MEADOW_TEST_BBD_REGISTER                    903

/**
 * @brief Battery backed domain write after reset test ID.
 */
#define MEADOW_TEST_BBD_WRITE_AFTER_RESET           904

/**
 * @brief Battery backed domain read after reset test ID.
 */
#define MEADOW_TEST_READ_AFTER_RESET                905

/**
 * @brief ESP32 (all tests) test ID.
 */
#define MEADOW_TEST_ALL_ESP32                       1000

/**
 * @brief ESP32 web page load test ID.
 */
#define MEADOW_TEST_ESP_WEB_PAGE_LOAD_TEST          1001

/**
 * @brief ESP32 binary file load test ID.
 */
#define MEADOW_TEST_ESP_BINARY_FILE_LOAD_TEST       1002

/**
 * @brief Ethernet (all tests) test ID.
 */
#define MEADOW_TEST_ETHERNET                        1200

/**
 * @brief Ethernet web page load test ID.
 */
#define MEADOW_TEST_ETHERNET_WEB_PAGE_LOAD_TEST     1201

/**
 * @brief Ethernet binary file load test ID.
 */
#define MEADOW_TEST_ETHERNET_BINARY_FILE_LOAD_TEST  1202

/**
 * @brief BG77 (all tests) test ID.
 */
#define MEADOW_TEST_BG77                            1400

/****************************************************************************
 * Private types
 ****************************************************************************/

/*
 * @brief Prototype for the test methods.
 */
typedef void (*test_method_t)(uint32_t);

/*
 * @brief Structure to hold the test names and their IDs.
 */
struct meadow_test_names_s
{
    /*
    *  ID of a test.
    */
    uint16_t testId;

    /*
    *  Description of the test.
    */
    char *description;
};
typedef struct meadow_test_names_s meadow_test_names_t;

/**
 * @brief Structure to hold the methods associated with a particular test ID.
 */
struct meadow_test_methods_s
{
    /*
    *  ID of a test.
    */
    uint16_t testId;

    /*
    *  Method to be executed.
    */
    test_method_t testMethod;
};
typedef struct meadow_test_methods_s meadow_test_methods_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

 /****************************************************************************
 * Public Function prototypes
 ****************************************************************************/


#endif /* MEADOW_UNIT_TEST_FRAMEWORK_H */