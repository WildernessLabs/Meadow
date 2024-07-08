/****************************************************************************
 * meadow_os_battery_backed_domain.c
 *
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs (Mark Stevens)
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

#include <nuttx/config.h>

#include <nuttx/kmalloc.h>

#include <arch/board/board.h>

#include <meadow/meadow_os_battery_backed_domain.h>
#include <meadow/hcom_bbreg_defn.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Address of the battery backed domain SRAM.
 */
#define BATTERY_BACKED_DOMAIN_SRAM_ADDRESS          0x40024000

/**
 * @brief Maximum register number (register numbers start at 0).
 */
#define MEADOW_OS_BBD_REGISTER_NUMBER_MAX           (STM32_RTC_BKCOUNT - 1)

/**
 * @brief Base address of the battery backed domain registers.
 */
#define MEADOW_OS_BBR_BASE_ADDRESS                  STM32_RTC_BK0R

/**
 * @brief Maximum address of the battery backed domain registers.
 */
#define MEADOW_OS_BBR_MAXIMUM_ADDRESS               (MEADOW_OS_BBR_BASE_ADDRESS + (MEADOW_OS_BBD_REGISTER_NUMBER_MAX * 4))

/**
 * @brief Length of the battery backed domain SRAM.
 */
#define BATTERY_BACKED_DOMAIN_SRAM_LENGTH           4092

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_bbd_is_bbr_address_valid
 *
 * Description:
 *  Check if the register address is valid.
 *
 * Input Parameters:
 *  address - Address of the register to check.
 *
 * Returned Value:
 *  true if the register address is valid, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static bool meadow_os_bbd_is_bbr_address_valid(uint32_t address)
{
    bool result = true;

    if ((address < MEADOW_OS_BBR_BASE_ADDRESS) || (address > MEADOW_OS_BBR_MAXIMUM_ADDRESS))
    {
        result = false;
    }
    else
    {
        if ((address % 4) != 0)
        {
            result = false;
        }
    }

    return(result);
}

/****************************************************************************
 * Name: meadow_os_bbd_is_bbr_number_valid
 *
 * Description:
 *  Check if the register number is valid.
 *
 * Input Parameters:
 *  register_number - Register number to be checked.
 *
 * Returned Value:
 *  true if the register number is valid, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static bool inline meadow_os_bbd_is_bbr_number_valid(utin32_t number)
{
    return(number <= MEADOW_OS_BBD_REGISTER_NUMBER_MAX);
}

/****************************************************************************
 * Name: meadow_os_bbd_bbr_number_to_address
 *
 * Description:
 *  Convert a battery backed register number into an address.
 *
 * Input Parameters:
 *  number - Register number to be converted.
 *
 * Returned Value:
 *  Address of the battery backed register.
 *
 * Assumptions/Limitations:
 *  Register number has been validated before this method is called.
 *
 ****************************************************************************/
static uint32_t inline *meadow_os_bbd_bbr_number_to_address(uint32_t number)
{
    return(MEADOW_OS_BBR_BASE_ADDRESS + (register_number * 4));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_bbd_strdup_to_sram
 *
 * Description:
 *  Copy the message into battery backed RAM.
 * 
 *  The message will be truncated to the 4095 bytes (allowing 1 byte for the
 *  terminating NULL).
 * 
 *  The battery backed domain SRAM is cleared before the message is copied.
 *
 * Input Parameters:
 *  message - String to be saved.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_os_bbd_strdup_to_sram(const char *message)
{
    meadow_os_bbd_clear_sram();
    strncpy((char *) BATTERY_BACKED_DOMAIN_SRAM_ADDRESS, message, BATTERY_BACKED_DOMAIN_SRAM_LENGTH - 1);
}

/****************************************************************************
 * Name: meadow_os_bbd_sram_strdup
 *
 * Description:
 *  Duplicate the string held in battery backed RAM
 * 
 *  The message will be truncated to the 4095 bytes (allowing 1 byte for the
 *  terminating NULL).
 *
 * Input Parameters:
 *  message - String to be saved.
 *
 * Returned Value:
 *  Pointer to the duplicated string.
 *
 * Assumptions/Limitations:
 *  Caller will release the memory holding the duplicated string.
 *
 ****************************************************************************/
char *meadow_os_bbd_sram_strdup(void)
{
    return(strndup((char *) BATTERY_BACKED_DOMAIN_SRAM_ADDRESS), BATTERY_BACKED_DOMAIN_SRAM_LENGTH - 1);
}

/****************************************************************************
 * Name: meadow_os_bbd_clear_sram
 *
 * Description:
 *  Clear the battery backed domain SRAM.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_os_bbd_clear_sram(void)
{
    memset((void *) BATTERY_BACKED_DOMAIN_SRAM_ADDRESS, 0, BATTERY_BACKED_DOMAIN_SRAM_LENGTH);
}

/****************************************************************************
 * Name: meadow_os_bbd_register_get_value
 *
 * Description:
 *  Get a value from a battery backed register.
 *
 * Input Parameters:
 *  register_number - Register number to read.
 *  value - Location to store the value.
 *
 * Returned Value:
 *  OK if the register number is valid, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_bbd_register_get_value(uint32_t register_number, uint32_t *value)
{
    int result = OK;

    if (!meadow_os_nnd_is_bbr_number_valid(register_number))
    {
        result = ERROR;
    }
    else
    {
        *value = getreg32(meadow_os_bbd_bbr_number_to_address(register_number));
    }

    return(result);
}

/****************************************************************************
 * Name: meadow_os_bbd_register_set_value
 *
 * Description:
 *  Set the value in the specified battery backed register.
 *
 * Input Parameters:
 *  register_number - Register number to write.
 *  value - Value to write.
 *
 * Returned Value:
 *  OK if the register number is valid, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_bbd_register_set_value(uint32_t register_number, uint32_t value)
{
    int result = OK;

    if (!meadow_os_nnd_is_bbr_number_valid(register_number))
    {
        result = ERROR;
    }
    else
    {
        setreg32(meadow_os_bbd_bbr_number_to_address(register_number), value);
    }

    return(result);
}

/****************************************************************************
 * Name: meadow_os_bbd_register_set_bits
 *
 * Description:
 *  Set bit in the specified battery backed register.
 *
 * Input Parameters:
 *  register_number - Register number to write.
 *  value - Bits to be set.
 *
 * Returned Value:
 *  OK if the register number is valid, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_bbd_register_set_bits(uint32_t register_number, uint32_t value)
{
    int result = OK;

    if (!meadow_os_nnd_is_bbr_number_valid(register_number))
    {
        result = ERROR;
    }
    else
    {
        uint32_t *address = meadow_os_bbd_bbr_number_to_address(register_number);
        uint32_t current_value = getreg32(address);
        current_value |= value;
        setreg32(address, current_value);
    }

    return(result§);
}

/****************************************************************************
 * Name: meadow_os_bbd_register_clear_bits
 *
 * Description:
 *  Clear the specified bit in the battery backed register.
 *
 * Input Parameters:
 *  register_number - Register number to write.
 *  value - Bits to be cleared.
 *
 * Returned Value:
 *  OK if the register number is valid, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_bbd_register_clear_bits(uint32_t register_number, uint32_t value)
{
    int result = OK;

    if (!meadow_os_nnd_is_bbr_number_valid(register_number))
    {
        result = ERROR;
    }
    else
    {
        uint32_t *address = meadow_os_bbd_bbr_number_to_address(register_number);
        uint32_t current_value = getreg32(address);
        current_value &= ~value;
        setreg32(address, current_value);
    }

    return(result);
}

