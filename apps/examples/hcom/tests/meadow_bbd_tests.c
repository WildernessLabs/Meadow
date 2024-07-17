/****************************************************************************
 * \apps\examples\hcom\tests\meadow_bbd_tests.c
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

#if defined(CONFIG_MEADOW_OS_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>

#include "../hcom_common.h"
#include <meadow/meadow_os.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os_battery_backed_domain.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 * @brief Test bit pattern for the battery backed registers.
 */
#define BBD_TEST_VALUE_AA       (0xaaaaaaaa)
#define BBD_TEST_VALUE_55       (0x55555555)

/**
 * @brief First and last BBR that will be used for testing.
 */
#define BBR_FIRST               0
#define BBR_LAST                31
#define BBR_TEST_REGISTER       15

/**
 * @brief Logging level to use for syslog calls.
 */
#define LOGGING_LEVEL           1

/**
 * @brief Simple test message string.
 */
 #define TEST_MESSAGE_STRING     "This is a test message"

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
 * Name: fill_all_registers
 *
 * Description:
 *  Fill all of the battery backed registers with the same value.
 *
 * Input Parameters:
 *  value - the value to put into the registers.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void fill_all_registers(uint32_t value)
{
    for (int index = BBR_FIRST; index <= BBR_LAST; index++)
    {
        meadow_os_bbd_register_set_value(index, value);
    }
}

/****************************************************************************
 * Name: check_all_registers
 *
 * Description:
 *  Fill all of the battery backed registers with the same value.
 *
 * Input Parameters:
 *  expected_value - the value that should be in the registers.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int check_all_registers(uint32_t expected_value)
{
    int result = OK;

    for (int index = BBR_FIRST; index <= BBR_LAST; index++)
    {
        uint32_t value;
        if (meadow_os_bbd_register_get_value(index, &value) != OK)
        {
            result = ERROR;
        }
        else
        {
            if (value != expected_value)
            {
                result = ERROR;
            }
        }
    }

    return(result);
}

/****************************************************************************
 * Name: fill_bkpsram
 *
 * Description:
 *  Fill the BKPSRAM area with the specified value.
 *
 * Input Parameters:
 *  value - the value that should written to BKPSRAM.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void fill_bkpsram(uint8_t value)
{
    char *buffer = (char *) malloc(MEADOW_OS_BBD_SRAM_SIZE);
    memset(buffer, value, MEADOW_OS_BBD_SRAM_SIZE);
    meadow_os_bbd_strdup_to_sram(buffer);
    free(buffer);
}

/****************************************************************************
 * Name: check_bkpsram
 *
 * Description:
 *  Check that the BKPSRAM area contains the specified value.
 *
 * Input Parameters:
 *  expected_value - the value that should be in BKPSRAM.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static int check_bkpsram(uint8_t expected_value)
{
    int result = OK;

    char *buffer = (char *) malloc(MEADOW_OS_BBD_SRAM_SIZE);
    meadow_os_bbd_strdup_from_sram(buffer, MEADOW_OS_BBD_SRAM_SIZE);
    for (int index = 0; index < MEADOW_OS_BBD_SRAM_SIZE - 1; index++)
    {
        if (buffer[index] != expected_value)
        {
            result = ERROR;
        }
    }
    if (buffer[MEADOW_OS_BBD_SRAM_SIZE - 1] != 0)
    {
        result = ERROR;
    }
    free(buffer);

    return(result);
}

/****************************************************************************
 * Name: test_registers
 *
 * Description:
 *  Test register read and write access.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void test_registers(void)
{
    uint32_t old_values[BBR_LAST + 1];
    uint32_t value;

    //
    //  Save the old values.
    //
    for (int index = BBR_FIRST; index <= BBR_LAST; index++)
    {
        meadow_os_bbd_register_get_value(index, & old_values[index]);
    }

    //
    //  Write and then read back 0xAAAAAAAA followed by 0x55555555 to all the BBRs.
    //
    fill_all_registers(BBD_TEST_VALUE_AA);
    if (check_all_registers(BBD_TEST_VALUE_AA) == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing 0x%08x to all registers\n", BBD_TEST_VALUE_AA);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing 0x%08x to all registers\n", BBD_TEST_VALUE_AA);
    }
    fill_all_registers(BBD_TEST_VALUE_55);
    check_all_registers(BBD_TEST_VALUE_55);
    if (check_all_registers(BBD_TEST_VALUE_55) == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing 0x%08x to all registers\n", BBD_TEST_VALUE_55);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing 0x%08x to all registers\n", BBD_TEST_VALUE_55);
    }

    //
    //  Now for some bit twiddling, we will assume get and set work as they were tested above.
    //
    meadow_os_bbd_register_set_value(BBR_TEST_REGISTER, 0xFFFFFFFF);
    if (meadow_os_bbd_register_clear_bits(BBR_TEST_REGISTER, BBD_TEST_VALUE_55) == OK)
    {
        meadow_os_bbd_register_get_value(BBR_TEST_REGISTER, &value);
        if (value == BBD_TEST_VALUE_AA)
        {
            syslog(LOGGING_LEVEL, "    PASS: Clearing bits in register 0 (bad value)\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: Clearing bits in register 0\n");
        }
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Clearing bits in register 0\n");
    }
    if (meadow_os_bbd_register_set_bits(BBR_TEST_REGISTER, BBD_TEST_VALUE_55) != OK)
    {
        syslog(LOGGING_LEVEL, "    FAIL: Setting bits in register 0\n");
    }
    else
    {
        meadow_os_bbd_register_get_value(BBR_TEST_REGISTER, &value);
        if (value == 0xFFFFFFFF)
        {
            syslog(LOGGING_LEVEL, "    PASS: Setting bits in register 0 (bad value)\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: Setting bits in register 0\n");
        }
    }

    //
    //  Restore the old values.
    //
    for (int index = BBR_FIRST; index <= BBR_LAST; index++)
    {
        meadow_os_bbd_register_set_value(index, old_values[index]);
    }
    bool pass = true;
    for (int index = BBR_FIRST; index <= BBR_LAST; index++)
    {
        meadow_os_bbd_register_get_value(index, & value);
        if (value != old_values[index])
        {
            pass = false;
        }
    }
    if (pass)
    {
        syslog(LOGGING_LEVEL, "    PASS: Restoring old values to all registers\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Restoring old values to all registers\n");
    }

    //
    //  Now check for invalid register numbers.
    //
    if (meadow_os_bbd_register_set_value(100, 0) == ERROR)
    {
        syslog(LOGGING_LEVEL, "    PASS: Setting value in invalid register\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Setting value in invalid register\n");
    }
    if (meadow_os_bbd_register_get_value(100, &value) == ERROR)
    {
        syslog(LOGGING_LEVEL, "    PASS: Getting value from invalid register\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Getting value from invalid register\n");
    }
    if (meadow_os_bbd_register_clear_bits(100, 0) == ERROR)
    {
        syslog(LOGGING_LEVEL, "    PASS: Clearing bits in invalid register\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Clearing bits in invalid register\n");
    }
    if (meadow_os_bbd_register_set_bits(100, 0) == ERROR)
    {
        syslog(LOGGING_LEVEL, "    PASS: Setting bits in invalid register\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Setting bits in invalid register\n");
    }
}

/****************************************************************************
 * Name: test_bkpsram
 *
 * Description:
 *  Test the BKPSRAM area.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void test_bkpsram(void)
{
    //
    //  Write and then read back 0x00 to all of the BKPSRAM.
    //
    fill_bkpsram(0x00);
    if (check_bkpsram(0x00) == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing 0x00 to all of BKPSRAM\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing 0x00 to all of BKPSRAM\n");
    }

    //
    //  Write and then read back 0xAA to all of the BKPSRAM.
    //
    fill_bkpsram(0xAA);
    if (check_bkpsram(0xAA) == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing 0xAA to all of BKPSRAM\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing 0xAA to all of BKPSRAM\n");
    }
    //
    //  Now check that clear works.
    //
    meadow_os_bbd_clear_sram();
    if (check_bkpsram(0x00) == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Clearing BKPSRAM\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Clearing BKPSRAM\n");
    }
    //
    //  Now lets check some strings.
    //
    char *buffer = (char *) malloc(MEADOW_OS_BBD_SRAM_SIZE);
    char *message = TEST_MESSAGE_STRING;
    meadow_os_bbd_strdup_to_sram(message);
    meadow_os_bbd_strdup_from_sram(buffer, MEADOW_OS_BBD_SRAM_SIZE);
    if (strcmp(buffer, TEST_MESSAGE_STRING) == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing and reading back a string\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing and reading back a string\n");
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_bbd_write_read_test
 *
 * Description:
 *  Write values to the BBR registers and then read them back.
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
void meadow_bbd_write_read_test(uint32_t userdata)
{
    syslog(LOGGING_LEVEL, "********** Writing data to registers and SRAM and verifying contents.\n");
    test_registers();
    test_bkpsram();    
}

/****************************************************************************
 * Name: meadow_bbd_write_and_reset_test
 *
 * Description:
 *
 * Input Parameters:
 *  userdata - Value passed to the test from the meadow command line.
 *             See: -v / --value parameter in meadow command line.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  This test assumes that the basic read/write of registers and SRAM have
 *  passed.
 *
 ****************************************************************************/
void meadow_bbd_write_and_reset_test(uint32_t userdata)
{
    syslog(LOGGING_LEVEL, "********** Writing data to registers and SRAM and then resetting the board.\n");
    meadow_os_bbd_register_set_value(BBR_TEST_REGISTER, BBD_TEST_VALUE_55);
    meadow_os_bbd_strdup_to_sram(TEST_MESSAGE_STRING);
    syslog(LOGGING_LEVEL, "    The board will now reset.  Execute the read after reset test when the.\n");
    syslog(LOGGING_LEVEL, "    board has restarted.\n");
    sleep(2);
    meadow_os_reset_board(0);
}

/****************************************************************************
 * Name: meadow_bbd_read_after_reset_test
 *
 * Description:
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
void meadow_bbd_read_after_reset_test(uint32_t userdata)
{
    syslog(LOGGING_LEVEL, "********** Reading data from registers and SRAM after a reset.\n");
    uint32_t value;
    meadow_os_bbd_register_get_value(BBR_TEST_REGISTER, &value);
    if (value == BBD_TEST_VALUE_55)
    {
        syslog(LOGGING_LEVEL, "    PASS: Reading back value from register after reset\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Reading back value from register after reset\n");
    }
    char *buffer = (char *) malloc(MEADOW_OS_BBD_SRAM_SIZE);
    meadow_os_bbd_strdup_from_sram(buffer, MEADOW_OS_BBD_SRAM_SIZE);
    if (strcmp(buffer, TEST_MESSAGE_STRING) == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Reading back string from SRAM after reset\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Reading back string from SRAM after reset\n");
    }
    free(buffer);
}

#endif /* defined(CONFIG_MEADOW_OS_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS) */
