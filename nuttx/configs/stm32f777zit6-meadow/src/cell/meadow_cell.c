/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\cell\meadow_cell.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

#include "meadow_cell.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

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
 * Name: meadow_cell_turn_on_the_cell_module
 *
 * Description:
 *  Function to turn on the cell module, which can vary according to
 *  the meadow device pinout and modem model used.
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
void meadow_cell_turn_on_the_cell_module()
{
    uint32_t module_id; 
    uint32_t turn_on_pin;
    module_id = hcom_nx_config_get_cell_module_id();
    turn_on_pin = hcom_nx_config_get_cell_turn_on_pin();

    if (turn_on_pin > 0)
    {
        switch (module_id)
        {
            case CELL_BG770A_MODULE:
                // Low pulse for 3 seconds to turn on the Quectel BG770A-GL cell module
                syslog(LOG_INFO, "Turning on BG770A module\n");
                stm32_configgpio(GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | turn_on_pin); 
                stm32_gpiowrite(turn_on_pin, false);
                usleep(3000000);
                stm32_gpiowrite(turn_on_pin, true);
                stm32_gpiowrite(turn_on_pin, false);
            break;

            case CELL_M95_MODULE:
                syslog(LOG_INFO, "Turning on M95 module\n");
                stm32_configgpio(GPIO_OUTPUT | turn_on_pin);
                stm32_gpiowrite(turn_on_pin, true);
            break;

            case CELL_BG95M3_MODULE:
                syslog(LOG_INFO, "Turning on BG95-M3 module\n");
                stm32_configgpio(GPIO_OUTPUT | turn_on_pin);
                stm32_gpiowrite(turn_on_pin, true);
                usleep(3000000);
                stm32_gpiowrite(turn_on_pin, false);
            break;

            case CELL_EG21GL_MODULE:
                // Low pulse for 500 milliseconds to turn on the Quectel EG21-GL cell module
                syslog(LOG_INFO, "Turning on EG21-GL module\n");
                stm32_configgpio(GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | turn_on_pin); 
                stm32_gpiowrite(turn_on_pin, false);
                usleep(500000);
                stm32_gpiowrite(turn_on_pin, true);
            break;

            default:
                syslog(LOG_INFO, "Failed to identify and turn on the cell module\n");
            break;
        }
    }
    else
    {
        syslog(LOG_INFO, "Failed to turn on the cell module\n");
    }
}
